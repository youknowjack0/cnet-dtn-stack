/* provide a datagram service to the application layer 
 * handles checksums of data
 */
#include "dtn.h"

/* TODO: define a message structure */

/* handle message from the network layer, 
 * check integrity and pass to the application */
void transport_recv(char * msg, int len) {
	
}

/* called on app init */
void transport_init() {
	/* register handler for application layer events */
}
