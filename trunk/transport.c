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

/*
 * A queue structure that holds arrays of datagrams, one array per 
 * message. Each element also contains information about how many
 * fragments are in its message and how many fragments have been
 * received so far.
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
 * Counter for the serial numbers of messages.
 * In reality this would eventually overflow.
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
 ************************
 * END GLOBAL VARIABLES *
 ************************
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
 * Looks up the adress of the entry for the message in the
 * queue
 */
static struct QUEUE_EL* queue_get(long long int key)
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
 * Removes the element at the front of the queue.
 */
static DATAGRAM* dequeue(TRANSQUEUE* q)
{
		if(is_empty(q))
		{
				return NULL;
		}
		else
		{
				struct QUEUE_EL* del = q->bottom;
				DATAGRAM* d = del->frags;
				q->bottom = del->up;
				free(del);
				if(q->bottom != NULL)
				{
						q->bottom->down = NULL;
				}
				else
				{
						q->top = NULL;
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
		long long int key = make_key(dat->h.source, dat->h.msg_num);
		struct QUEUE_EL* el = queue_get(key);
		if(el == NULL)
		{
				el = malloc(sizeof(struct QUEUE_EL));
				el->frags = malloc((dat->h.frag_count) * sizeof(DATAGRAM));

				int bytes_used = sizeof(struct QUEUE_EL) +  
						(dat->h.frag_count * sizeof(DATAGRAM));
				free_bytes -= bytes_used;
				while(free_bytes < bytes_used)
				{
						dequeue(q);
				}

				el->num_frags_needed = dat->h.frag_count;
				el->num_frags_gotten = 0;
				el->key = key; 

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
		memcpy(&(el->frags[el->num_frags_gotten]), dat, 
						DATAGRAM_HEADER_SIZE + dat->h.msg_size);
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
		struct QUEUE_EL* temp = queue_get(make_key(src, message_num));
		if (temp == NULL)
				return NULL;
		else
		{
				if(q->bottom == temp)
				{
						q->bottom = temp->up;
				}
				if(q->top == temp)
				{
						q->top = temp->down;
				}
				if(temp->down != NULL)
				{
						temp->down->up = temp->up;
				}
				if(temp->up != NULL)
				{
						temp->up->down = temp->down;
				}

				free_bytes += (sizeof(struct QUEUE_EL) + 
								(temp->num_frags_needed * sizeof(DATAGRAM)));
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

/*
 * Just a comparison function for quick sorting datagrams
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
 */
void transport_recv(char* msg, int len, CnetAddr sender) 
{

		DATAGRAM * d = (DATAGRAM*) msg;

		/* 
		 * Check integrity 
		 */
		int oldsum = d->h.checksum;
		d->h.checksum = 0;
		int sum = CNET_crc32((unsigned char *)(d), len);
		if(sum != oldsum) {
				return;
		}


		/* 
		 * Pass up 
		 */
		if(d->h.frag_count == 1)
		{
				message_receive(d->msg_frag, d->h.msg_size, sender);
		}
		else
		{
				bool all_received = enqueue(buff, d);
				if(all_received == true)
				{
						/*
						 * Reassemble message
						 */
						DATAGRAM* frags = queue_delete(buff, d->h.source, d->h.msg_num);
						qsort(frags, d->h.frag_count, sizeof(DATAGRAM), comp);
						int num_frags = d->h.frag_count;
						char* built_msg = 
								malloc(num_frags * MAX_FRAGMENT_SIZE * sizeof(char));
						int built_msg_size = 0;
						for(int i = 0; i < num_frags; i++)
						{
								memcpy(built_msg + (i * MAX_FRAGMENT_SIZE), 
												frags[i].msg_frag, frags[i].h.msg_size);
								built_msg_size += frags[i].h.msg_size;
						}
						/*
						 * Send the message to the application layer
						 */
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
						/*
						 * copy over a part of a the message. 
						 */
						memcpy(d->msg_frag, &(msg[i * MAX_FRAGMENT_SIZE]), remainder);

						/*
						 * Set the checksum
						 */
						d->h.checksum = 0;
						d->h.checksum = CNET_crc32(((unsigned char *) d), 
										DATAGRAM_HEADER_SIZE + d->h.msg_size); 

						/*
						 * Send it and free the memory
						 */
						assert(DATAGRAM_HEADER_SIZE + remainder < MAX_PACKET_SIZE);
						net_send(((char*) d), 
										DATAGRAM_HEADER_SIZE + remainder, destination);
						free(d);
				}
				/*
				 * if it's not the last fragment of the message:
				 */
				else 
				{
						/*
						 * Make a new datagram
						 */
						DATAGRAM* d = malloc(sizeof(DATAGRAM));

						d->h.msg_size = MAX_FRAGMENT_SIZE;
						d->h.source = src;
						d->h.msg_num = msg_num;
						d->h.frag_num = frag_num++;
						d->h.frag_count = num_frags_needed;

						/*
						 * copy over a part of a the message. 
						 */
						memcpy(d->msg_frag, &(msg[i * MAX_FRAGMENT_SIZE]),
										MAX_FRAGMENT_SIZE);

						/*
						 * Set the checksum
						 */
						d->h.checksum = 0;
						d->h.checksum = CNET_crc32(((unsigned char *) d), 
										DATAGRAM_HEADER_SIZE + d->h.msg_size); 

						assert(sizeof(DATAGRAM) <= MAX_DATAGRAM_SIZE);
						/*
						 * Send it. Free memeory
						 */
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
}
