/* Fake application layer
 * Pumps out random message-turds to the transport layer. 
 */



/* generate messages of random data and random (bounded) length to pass to the 
 * transport layer
 */
static void generate_message() {
	/* generate a message with a unique identifier and some random data of rando
	 * m (bounded) length to a random receipient */
	/* save the random identifier, and the global time at which it was sent */
	/* reschedule this function at a random interval */

}

/* accept a message from a receiver
 * at this point, the message contents have been verified on the transport layer
 * , so no further verification is required. 
 */
void receive_message(char* data, CnetAddr sender) {
	/* record the received unique identifier, and the global time at which it wa
	 * s received */

}


