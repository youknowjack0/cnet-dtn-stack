/* provide a datagram service to the application layer 
 * handles checksums of data
 */
#include "dtn.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>




/* a message structure */
typedef struct 
{
	uint32_t checksum;
	/* the size of msg_frag */
	uint32_t msg_size;
	/* the original sender */
	int source;
	/* the serial number on the message */
	int msg_num;
	/* the sequence number of this fragment within the message */
	int frag_num;
	/* the number of fragments in this message */
	int frag_count;
	/* the message fragment */
	char msg_frag[MAX_DATAGRAM_SIZE];
} DATAGRAM;

/*
 * A structure for the elements of the TRANSQUEUE.
 */
struct QUEUE_EL
{
	int num_frags_needed;
	int num_frags_gotten;
	int key;
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
 * counter for the serial numbers of messages
 */
static int msg_num_counter;

/*
 * The layer's buffer
 */
static TRANSQUEUE* buff;

/*
 * concatenates two ints to make one int
 */
static int make_key(int src, int message_num)
{
	char* tmp = malloc(50 * sizeof(char));
	sprintf(tmp, "%d%d", src, message_num);
	int ret = atoi(tmp);
	return ret;
}

/*
 * these functions mimic what will eventually be done with a
 * red-black tree.
 */


/*
 * Adds an entry to the tree
 */
static void tree_add_entry(int key, struct QUEUE_EL* el)
{
	//at this stage there is no tree, so nothing to add to;
	//int key = make_key(src, message_num);
	return;
}

/*
 * Looks up the adress of the entry for the message in the
 * queue
 */
static struct QUEUE_EL* tree_get_entry(int key)
{
	struct QUEUE_EL* curr_el = buff->bottom;
	while(curr_el->up != NULL)
	{
		if(curr_el->up->key == key)
			return curr_el->up;
		curr_el = curr_el->up;
	}
	return NULL;
}

/*
 * return true if there is an entry in the buffer for the
 * message, else return false.
 */
/* NOT USED
static bool tree_has_entry(int key)
{
	return (tree_get_entry(key) != NULL);
}
*/

/*
 * Deletes the entry for the message in the tree and returns
 * the address of the entry for the message in the queue.
 */
static struct QUEUE_EL* tree_delete_entry(int key)
{
	// at this stage there is no tree, so nothing to delete!
	return tree_get_entry(key);
}
/*
 * End search tree functions
 */

/* 
 * These functions handle the queue 
 */

/*
 * returns a pointer to a new, empty TRANSQUEUE
 */
static TRANSQUEUE* new_queue()
{
	TRANSQUEUE* q = malloc(sizeof(TRANSQUEUE));
	struct QUEUE_EL* el = malloc(sizeof(struct QUEUE_EL));
	el->num_frags_needed = 0;
	el->num_frags_gotten = 0;
	el->key = 0;
	el->up = NULL;
	el->down = NULL;
	el->frags = NULL;
	q->top = el;
	q->bottom = el;
	return q;
}

/*
 * Returns true if the queue is empty
 */
/* NOT USED _YET_
static bool is_empty(TRANSQUEUE* q)
{
	return ((q->top->down == NULL) || (q->bottom->up == NULL));
}
*/

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
	int key = make_key(dat->source, dat->msg_num);
	struct QUEUE_EL* el = tree_get_entry(key);
	if(el == NULL)
	{
		el = malloc(sizeof(struct QUEUE_EL));
		// just make each array element a full sized datagram. Not
		// worth worrying about.
		el->frags = malloc((dat->frag_count) * sizeof(DATAGRAM));
		el->num_frags_needed = dat->frag_count;
		el->num_frags_gotten = 0;
		el->key = make_key(dat->source, dat->msg_num);

		el->down = q->top;
		el->up = NULL;
		q->top->up = el;
		q->top = el;
		tree_add_entry(key, el);
	}

	memcpy(el->frags + el->num_frags_gotten++, dat, sizeof(DATAGRAM) - MAX_DATAGRAM_SIZE + dat->msg_size);
	return (el->num_frags_gotten == el->num_frags_needed);
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
/* NOT USED _YET_
static DATAGRAM* dequeue(TRANSQUEUE* q)
{
	if(is_empty(q))
	{
		return NULL;
	}
	else
	{
		DATAGRAM* d = q->bottom->frags;
		tree_delete_entry(q->bottom->key);
		q->bottom = q->bottom->up;
		free(q->bottom->down);
		q->bottom->down = NULL;
		return d;
	}
}
*/

/*
 * Finds the queue entry for a given message (identified by src
 * and messagenum), removes the entry from the queue and returns
 * the array of datagrams.
 */
static DATAGRAM* queue_delete(TRANSQUEUE* q, int src, int message_num)
{
	struct QUEUE_EL* temp = tree_delete_entry(make_key(src, message_num));
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

		DATAGRAM* ret = temp->frags;
		free(temp);
		return ret;
	}
}
/*
 * End of queue functions
 */

static int comp(const void* one, const void* two)
{
	DATAGRAM* first = (DATAGRAM*) one;
	DATAGRAM* second = (DATAGRAM*) two;
	if(first->frag_num == second->frag_num)
	{
		return 0;
	}
	else if(first->frag_num < second->frag_num)
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
void transport_recv(char * msg, int len, CnetAddr sender) 
{
	DATAGRAM * d = (DATAGRAM*) msg;
	/* check length*/
	if(len != (d->msg_size + sizeof(DATAGRAM) - MAX_DATAGRAM_SIZE))
		return; /* lengths don't match */
	
	/* check integrity */
	int sum = CNET_crc32((unsigned char *)(d) + offsetof(DATAGRAM, msg_size), len - sizeof(d->checksum));
	if(sum != d->checksum) return; /* bad checksum */

	/* pass up */
	if(d->frag_count == 1)
	{
		receive_message(d->msg_frag, d->msg_size, sender);
	}
	else
	{
		bool all_received = enqueue(buff, d);
		if(all_received == true)
		{
			DATAGRAM* frags = queue_delete(buff, d->source, d->msg_num);
			qsort(frags, d->frag_count, sizeof(DATAGRAM), comp); 
			int num_frags = d->frag_count;
			char* built_msg = malloc(num_frags * MAX_DATAGRAM_SIZE * sizeof(char));
			for(int i = 1; i <= num_frags; i++)
			{
				if(i == num_frags)
				{
					memcpy(built_msg + ((i - 1) * MAX_DATAGRAM_SIZE), frags[i - 1].msg_frag, frags[i - 1].msg_size);
				}
				else
				{
					memcpy(built_msg + ((i - 1) * MAX_DATAGRAM_SIZE), frags[i - 1].msg_frag, MAX_DATAGRAM_SIZE);
				}
			}
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
        if ((len % MAX_DATAGRAM_SIZE) == 0)
		extra = 0;
	else
		extra = 1;
	int num_frags_needed = (len / MAX_DATAGRAM_SIZE) + extra;

	int src = nodeinfo.nodenumber;
	int msg_num = ++msg_num_counter;
	int frag_num = 0;

	for(int i = 1; i <= num_frags_needed; i++) 
	{
		/*
		 * if it is the last fragment of the message:
		 */
		if(i == num_frags_needed) 
		{
			int remainder = len % MAX_DATAGRAM_SIZE;
			if (remainder == 0)
				remainder = MAX_DATAGRAM_SIZE;

			DATAGRAM* d = malloc(sizeof(DATAGRAM) - MAX_DATAGRAM_SIZE + remainder); 

			d->msg_size = remainder;
			d->source = src;
			d->msg_num = msg_num;
			d->frag_num = frag_num++;
			d->frag_count = num_frags_needed;

			/*
			 * copy over a part of a the message. parts are not null-terminated 
			 */
			memcpy(d->msg_frag, msg + ((i - 1) * MAX_DATAGRAM_SIZE), remainder);

			d->checksum = CNET_crc32(((unsigned char *) d) + offsetof(DATAGRAM, msg_size), 
					sizeof(DATAGRAM) - sizeof(d->checksum) - MAX_DATAGRAM_SIZE + remainder); 

			net_send(((char*) d), sizeof(DATAGRAM) - MAX_DATAGRAM_SIZE + remainder, destination);
			free(d);
		}
		/*
		 * if it's not the last fragment of the message:
		 */
		else 
		{
			DATAGRAM* d = malloc(sizeof(DATAGRAM));

			d->msg_size = MAX_DATAGRAM_SIZE;
			d->source = src;
			d->msg_num = msg_num;
			d->frag_num = frag_num++;
			d->frag_count = num_frags_needed;

			/*
			 * copy over a part of a the message. parts are not null-terminated 
			 */
			memcpy(d->msg_frag, msg + ((i - 1) * MAX_DATAGRAM_SIZE), MAX_DATAGRAM_SIZE);

			d->checksum = CNET_crc32(((unsigned char *) d) + offsetof(DATAGRAM, msg_size), 
					sizeof(DATAGRAM) - sizeof(d->checksum)); 

			net_send(((char*)d), sizeof(DATAGRAM), destination);
			free(d);
		}
	}
}

/* called on node init */
void transport_init() 
{
	/* register handler for application layer events if required*/
	msg_num_counter = 0;	
	buff = new_queue();
}

/* 
 * TODO: Check for mem leaks.
 * TODO: Implement limited buffer! 
 */
