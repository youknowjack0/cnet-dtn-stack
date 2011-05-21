/* provide a datagram service to the application layer 
 * handles checksums of data
 */
#include "dtn.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
/* a message structure */
typedef struct {
	uint32_t checksum;
	/* the size of msg */
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

/* handle message from the network layer, 
 * check integrity and pass to the application */
void transport_recv(char * msg, int len, CnetAddr sender) {
	DATAGRAM * d = (DATAGRAM*) msg;
	/* check length*/
	if(len != (d->msg_size + sizeof(DATAGRAM)))
		return; /* lengths don't match */
	
	/* check integrity */
	int sum = CNET_crc32((unsigned char *)(d) + offsetof(DATAGRAM, msg_size), len - sizeof(d->checksum));
	if(sum != d->checksum) return; /* bad checksum */

	/* pass up */
	receive_message(d->msg_frag, d->msg_size, sender);
}

/* add a header with message length and checksum of data, pass it down
 */
void transport_datagram(char* msg, int len, CnetAddr destination) {
	DATAGRAM d;
	d.msg_size = len;
	assert(len < MAX_DATAGRAM_SIZE);
	memcpy(d.msg_frag, msg, len);
	d.checksum = CNET_crc32((unsigned char *)(&d) + offsetof(DATAGRAM, msg_size), len + sizeof(d.msg_size));
	net_send( (char *)(&d), sizeof(d) - sizeof(d.msg_frag) + len, destination);
}

/* called on node init */
void transport_init() {
	/* register handler for application layer events if required*/
	msg_num_counter = 0;	
}
