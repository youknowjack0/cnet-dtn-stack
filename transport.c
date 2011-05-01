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
	uint32_t msg_size;
	char msg[MAX_DATAGRAM_SIZE];
} Datagram;

/* handle message from the network layer, 
 * check integrity and pass to the application */
void transport_recv(char * msg, int len, CnetAddr sender) {
	Datagram * d = (Datagram*)msg;
	/* check length*/
	if(len != (d->msg_size +sizeof(d->msg_size) + sizeof(d->checksum)))
		return; /* lengths don't match */
	
	/* check integrity */
	int sum = CNET_crc32((unsigned char *)(d) + offsetof(Datagram,msg_size), len - sizeof(d->checksum));
	if(sum != d->checksum) return; /* bad checksum */

	/* pass up */
	receive_message(d->msg, d->msg_size, sender);
}

/* add a header with message length and checksum of data, pass it down
 */
void transport_datagram(char * msg, int len, CnetAddr destination) {
	Datagram d;
	d.msg_size = len;
	assert(len < MAX_DATAGRAM_SIZE);
	memcpy(d.msg, msg, len);
	d.checksum = CNET_crc32((unsigned char *)(&d) + offsetof(Datagram, msg_size), len + sizeof(d.msg_size));
	net_send( (char *)(&d), sizeof(d) - sizeof(d.msg) + len, destination);
}

/* called on node init */
void transport_init() {
	/* register handler for application layer events if required*/

}
