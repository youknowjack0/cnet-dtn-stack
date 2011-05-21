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

static int msg_num_counter;

/*
 * Called by the network layer.
 *
 * Buffer datagrams until all fragments have arrived, then rebuild
 * the message and pass it to the application layer. 
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
	receive_message(d->msg_frag, d->msg_size, sender);
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

			d->checksum = CNET_crc32(((unsigned char *) d) + offsetof(DATAGRAM, msg_size), sizeof(DATAGRAM) - sizeof(d->checksum)); 

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
}
