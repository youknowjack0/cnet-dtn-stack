/* provide a datagram service to the application layer 
 * handles checksums of data
 */
#include "dtn.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define TRANSPORT_BUFF_SIZE 1000000



/*
 ****************************
 * STRUCTURES FOR THE QUEUE *
 ****************************
 */

/*
 * A structure for the elements of the TRANSQUEUE.
 */
struct QUEUE_EL
{
	int num_frags_needed;
	int num_frags_gotten;
	long long int key;
	DATAGRAM* frags;
	struct QUEUE_EL* down;
	struct QUEUE_EL* up;
};


/* A queue structure that holds arrays of datagrams, one array per 
 * message. Each element also contains information about how many
 * fragments are in its message and how many fragments have been
 * received so far.
 *
 * At this point I haven't done anything about duplicate datagrams.
 * TODO: will they be an issue?
 *
 * Rebuilding might be made easier by  quicksorting the array before
 * reassembly?
 */
typedef struct 
{
	struct QUEUE_EL* top;
	struct QUEUE_EL* bottom;
} TRANSQUEUE;

/*
 ************************
 * END QUEUE STRUCTURES *
 ************************
 */


/*
 ********************************
 * GLOBAL VARIABLE DECLARATIONS *
 ********************************
 */

/*
 * counter for the serial numbers of messages
 */
static int msg_num_counter;

/*
 * Available buffer space in bytes
 */
static int free_bytes;

/*
 * The layer's buffer
 */
static TRANSQUEUE* buff;

/*
 * Tracks whether to use right_min or left_max for
 * deletion from red-black tree
 */
static bool max_or_min;

/*
 ************************
 * END GLOBAL VARIABLES *
 ************************
 */


/*
 ***************************************
 * DECLARATIONS FOR DEPENDENCY REASONS *
 ***************************************
 */

static DATAGRAM* dequeue(TRANSQUEUE*);

/*
 ****************************
 * RED-BLACK TREE FUNCTIONS *
 ****************************
 */

/*
 * Looks up the adress of the entry for the message in the
 * queue
 */
static struct QUEUE_EL* tree_get(int key)
{
		struct QUEUE_EL* curr_el = buff->bottom;
		if(curr_el == NULL)
		{
			return NULL;
		}
		while(curr_el->up != NULL)
		{
				if(curr_el->up->key == key)
						return curr_el->up;
				curr_el = curr_el->up;
		}
		return NULL;
}	
/*
 * Deletes the entry for the message in the tree and returns
 * the address of the entry for the message in the queue.
 */
/* Not used
static struct QUEUE_EL* tree_delete(int key)
{
		struct QUEUE_EL* ret =  tree_get(key);
		if(ret->down != NULL)
		{
				ret->down->up = ret->up;
		}
		if(ret->up != NULL)
		{
				ret->up->down = ret->down;
		}
		return ret;
}
*/
/*
 ********************************
 * END RED-BLACK TREE FUNCTIONS *
 ********************************
 */



/* 
 *******************
 * QUEUE FUNCTIONS *
 *******************
 */

/*
 * concatenates two ints to make one int
 */
static long long int make_key(int src, int message_num)
{
	char* tmp = malloc(50 * sizeof(char));
	sprintf(tmp, "%d%d", src * 10000, message_num);
	long long int ret = atoll(tmp);
	return ret;
}

/*
 * returns a pointer to a new, empty TRANSQUEUE
 */
static TRANSQUEUE* new_queue()
{
	TRANSQUEUE* q = malloc(sizeof(TRANSQUEUE));
	free_bytes -= sizeof(TRANSQUEUE);
	q->top = NULL;
	q->bottom = NULL;
	return q;
}

/*
 * Returns true if the queue is empty
 */
static bool is_empty(TRANSQUEUE* q)
{
		return ((q->top == NULL) || (q->bottom == NULL));
}

/*
 * Removes the element at the front of the queue. Returns the
 * array of datagrams so that the entry for this message in the
 * binary search TREE can be REMOVED(!) (currently done internally).
 * This should (probably) only be used for dropping messages when
 * the buffer is full. Otherwise use delete().
 *
 * NOTE: On dropping messages, we may want to check that the new
 * datagram that overflows the buffer is not the datagram that
 * will complete the message we would normally drop, as that
 * would be an unneccessary waste.
 */
static DATAGRAM* dequeue(TRANSQUEUE* q)
{
		if(is_empty(q))
		{
				return NULL;
		}
		else
		{
				DATAGRAM* d = q->bottom->frags;
				q->bottom = q->bottom->up;
				if(q->bottom != NULL)
				{
					free(q->bottom->down);
					q->bottom->down = NULL;
				}
				free_bytes += sizeof(struct QUEUE_EL);
				free_bytes += (d->h.frag_count * sizeof(DATAGRAM));
				return d;
		}
}

/*
 * Put a datagram on the buffer. If it's not part of a message that
 * already has an entry in the buffer, then add a new QUEUE_EL. If 
 * it is part of a message that already has an entry in the buffer, 
 * find the entry and add the datagram to the array for that message. 
 * If this datagram makes up the full message then return true. Else 
 * return false.
 */
static bool enqueue(TRANSQUEUE* q, DATAGRAM* dat)
{
	int key = make_key(dat->h.source, dat->h.msg_num);
	struct QUEUE_EL* el = tree_get(key);
	if(el == NULL)
	{
		el = malloc(sizeof(struct QUEUE_EL));
		el->frags = malloc((dat->h.frag_count) * sizeof(DATAGRAM));

		int bytes_used = sizeof(struct QUEUE_EL) +  (dat->h.frag_count * sizeof(DATAGRAM));
		free_bytes -= bytes_used;
		while(free_bytes < bytes_used)
		{
			dequeue(q);
		}

		el->num_frags_needed = dat->h.frag_count;
		el->num_frags_gotten = 0;
		el->key = make_key(dat->h.source, dat->h.msg_num);

		el->down = q->top;
		el->up = NULL;
		if(is_empty(q))
		{
			q->bottom = el;
		}
		else
		{
			q->top->up = el;
		}
		q->top = el;
	}

	memcpy(&(el->frags[el->num_frags_gotten]), dat, DATAGRAM_HEADER_SIZE + dat->h.msg_size);
	el->num_frags_gotten++;
	return (el->num_frags_gotten == el->num_frags_needed);
}


/*
 * Finds the queue entry for a given message (identified by src
 * and messagenum), removes the entry from the queue and returns
 * the array of datagrams.
 */
static DATAGRAM* queue_delete(TRANSQUEUE* q, int src, int message_num)
{
	struct QUEUE_EL* temp = tree_get(make_key(src, message_num));
	if (temp == NULL)
		return NULL;
	else
	{
		if(temp->down != NULL)
		{
			temp->down->up = temp->up;
		}
		if(temp->up != NULL)
		{
			temp->up->down = temp->down;
		}

		free_bytes += (sizeof(struct QUEUE_EL) + (temp->num_frags_needed * sizeof(DATAGRAM)));
		DATAGRAM* ret = temp->frags;
		free(temp);
		return ret;
	}
}
/*
 ***********************
 * END QUEUE FUNCITONS *
 ***********************
 */

static int comp(const void* one, const void* two)
{
	DATAGRAM* first = (DATAGRAM*) one;
	DATAGRAM* second = (DATAGRAM*) two;
	if(first->h.frag_num == second->h.frag_num)
	{
		return 0;
	}
	else if(first->h.frag_num < second->h.frag_num)
	{
		return -1;
	}
	else
	{
		return 1;
	}
}

/*
 * Called by the network layer.
 *
 * Buffer datagrams until all fragments have arrived, then rebuild
 * the message and pass it to the application layer. If a message
 * has only one fragment, do not buffer it.
 * 
 * If the buffer for this layer fills up, delete all fragments from 
 * the message which received its first fragment the earliest. 
 *
 * All this should be able to be achieved with a Queue of arrays, 
 * each array containing the fragments of one message, and a binary 
 * search tree that maps <msg_num, source> pairs (probably implemented 
 * by concatenating the two elements into a string)  to links in the 
 * Queue.
 */
void transport_recv(char* msg, int len, CnetAddr sender) 
{
	printf("Node %d: transport_recv\n", nodeinfo.nodenumber);
	DATAGRAM* d = (DATAGRAM*) msg;
	/* 
	 * Check length
	 */
	//if(len != (d->msg_size + DATAGRAM_HEADER_SIZE));
		//return; 

	//printf("Node %d: transport got past length check\n", nodeinfo.nodenumber);

	/* 
	 * Check integrity 
	 */
	int oldsum = d->h.checksum;
	d->h.checksum = 0;
	int sum = CNET_crc32((unsigned char *)(d), len);
	if(sum != oldsum) {
		printf("network layer checksum failed\n");
		return;
	}

	
	printf("Node %d: transport_recv, got past checksum\n", nodeinfo.nodenumber);
	/* 
	 * Pass up 
	 */
	printf("Node %d: transport_recv, frag_count = %d\n", nodeinfo.nodenumber, d->h.frag_count);
	if(d->h.frag_count == 1)
	{
		message_receive(d->msg_frag, d->h.msg_size, sender);
	}
	else
	{
		bool all_received = enqueue(buff, d);
		printf("Node %d: transport_recv, enqueued\n", nodeinfo.nodenumber);
		if(all_received == true)
		{
			/*
			 * Reassemble message
			 */
			DATAGRAM* frags = queue_delete(buff, d->h.source, d->h.msg_num);
			printf("Node %d: transport_recv, deleted from queue\n", nodeinfo.nodenumber);
			qsort(frags, d->h.frag_count, sizeof(DATAGRAM), comp);
			printf("Node %d: transport_recv, qsorted\n", nodeinfo.nodenumber);
			int num_frags = d->h.frag_count;
			char* built_msg = malloc(num_frags * MAX_FRAGMENT_SIZE * sizeof(char));
			int built_msg_size = 0;
			for(int i = 1; i <= num_frags; i++)
			{
				//if(i == num_frags)
				//{
					printf("Node %d Transport: message %d, frag %d, len %d\n", nodeinfo.nodenumber, frags[i-1].h.msg_num, 
						frags[i-1].h.frag_num, frags[i-1].h.msg_size);
					memcpy(built_msg + ((i - 1) * MAX_FRAGMENT_SIZE), frags[i - 1].msg_frag, frags[i - 1].h.msg_size);
					built_msg_size += frags[i - 1].h.msg_size;
				/*
				}
				else
				{
					memcpy(built_msg + ((i - 1) * MAX_FRAGMENT_SIZE), frags[i - 1].msg_frag, MAX_FRAGMENT_SIZE);
					built_msg_size += MAX_FRAGMENT_SIZE;
				}
				*/
			}
			printf("Node %d Transport: reassembled message %d of len %d\n", nodeinfo.nodenumber, d->h.msg_num, built_msg_size);
			message_receive(built_msg, built_msg_size, sender);
			free(built_msg);
			free(frags);
		}

	}
}

/* 
 * Called by the application layer. Receives a message originating at this node,
 * fragments it if necessary, computes the checksum, builds the header so that
 * error checking and reassembly of fragments is possible and sends it to the
 * network layer.
 */
void transport_datagram(char* msg, int len, CnetAddr destination) 
{
	/*
	 * Determine how many fragments will be needed
	 */
	int extra;
	if ((len % MAX_FRAGMENT_SIZE) == 0)
		extra = 0;
	else
		extra = 1;
	int num_frags_needed = (len / MAX_FRAGMENT_SIZE) + extra;

	int src = nodeinfo.nodenumber;
	int msg_num = ++msg_num_counter;
	int frag_num = 0;
	printf("Node %d Transport: got message %d of len %d\n", nodeinfo.nodenumber, msg_num ,len);
	printf("Node %d Transport: init trans_datagram, num_frags = %d\n", nodeinfo.nodenumber, num_frags_needed);
	/*
	 * Break the message into fragments
	 */
	for(int i = 0; i < num_frags_needed; i++) 
	{
		/*
		 * if it is the last fragment of the message:
		 */
		if((i == num_frags_needed - 1)) 
		{ 
			int remainder = len % MAX_FRAGMENT_SIZE;
			if (remainder == 0)
				remainder = MAX_FRAGMENT_SIZE;

			/*
			 * Make a new datagram
			 */
			DATAGRAM* d = malloc(DATAGRAM_HEADER_SIZE + remainder); 

			d->h.msg_size = remainder;
			d->h.source = src;
			d->h.msg_num = msg_num;
			d->h.frag_num = frag_num++;
			d->h.frag_count = num_frags_needed;

			printf("Node %d Transport: message %d, frag %d, len %d\n", nodeinfo.nodenumber, msg_num, d->h.frag_num, remainder);

			/*
			 * copy over a part of a the message. parts are not null-terminated 
			 */
			memcpy(d->msg_frag, msg + (i * MAX_FRAGMENT_SIZE), remainder);

			/*
			 * Set the checksum
			 */
			d->h.checksum = 0;
			d->h.checksum = CNET_crc32(((unsigned char *) d), DATAGRAM_HEADER_SIZE + d->h.msg_size); 

			/*
			 * Send it and free the memory
			 */
			printf("Node %d Transport: about to net_send\n", nodeinfo.nodenumber);
			assert(DATAGRAM_HEADER_SIZE + remainder < MAX_PACKET_SIZE);
			net_send(((char*) d), DATAGRAM_HEADER_SIZE + remainder, destination);
			free(d);
		}
		/*
		 * if it's not the last fragment of the message:
		 */
		
		else 
		{
			
			 // Make a new datagram
			 
			DATAGRAM* d = malloc(sizeof(DATAGRAM));

			d->h.msg_size = MAX_FRAGMENT_SIZE;
			d->h.source = src;
			d->h.msg_num = msg_num;
			d->h.frag_num = frag_num++;
			d->h.frag_count = num_frags_needed;

			
			 // copy over a part of a the message. parts are not null-terminated 
			 
			memcpy(d->msg_frag, msg + (i * MAX_FRAGMENT_SIZE), MAX_FRAGMENT_SIZE);

						 // Set the checksum 
			d->h.checksum = 0;
			d->h.checksum = CNET_crc32(((unsigned char *) d), DATAGRAM_HEADER_SIZE + d->h.msg_size); 
			
			 //Send it and free the memory
			printf("Node %d Transport: about to net_send\n", nodeinfo.nodenumber);
			assert(sizeof(DATAGRAM) <= MAX_DATAGRAM_SIZE);
			net_send(((char*)d), sizeof(DATAGRAM), destination);
			free(d);
		}
	}
}

/*
 * called on node init 
 */
void transport_init() 
{
	/* 
	 * register handler for application layer events if required
	 */
	msg_num_counter = 0;	
	free_bytes = TRANSPORT_BUFF_SIZE;
	buff = new_queue();
	max_or_min = false;
}

/* 
 * TODO: Implement limited buffer! 
 */
