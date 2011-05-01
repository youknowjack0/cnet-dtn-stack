/* provide a datagram service to the application layer 
 * handles checksums of data
 */
#include "dtn.h"

/* a message structure */
typedef struct {
	uint32_t checksum;
	char msg[MAX_DATAGRAM_SIZE];
} Datagram;

/* handle message from the network layer, 
 * check integrity and pass to the application */
void transport_recv(char * msg, int len, CnetAddr sender) {
	
}

/* called on app init */
void transport_init() {
	/* register handler for application layer events */
}
