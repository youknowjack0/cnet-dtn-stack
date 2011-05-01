/* Fake application layer
 * Pumps out random message-turds to the transport layer. 
 */
#include "dtn.h"

typedef struct {
	int id;
	char filth[MAXMESSAGESIZE];
} AppMessage;

static int messageCount = 0;

static void megalog(int id, CnetAddr recipient, char * filename) {
	/* TODO: FINISH THIS */
}

static void logsent(int id, CnetAddr recipient) {
	/* TODO: write a separate log file for each node's sent messages, something like: 
	 * <id>,<recipient>,<current_time_of_day>
	 *
	 * use global LOGDIR to prefix saved files
	 */
	char filename[BUFSIZ];
	sprintf(filename, "%s/%s-%d", LOGDIR, "sent", nodeinfo.nodenumber);
	megalog(id, recipient, filename);
}

static void logrcv(int id, CnetAddr sender) {
	/* exact same as logsent, but in a different file
	 */
	char filename[BUFSIZ];
	sprintf(filename, "%s/%s-%d", LOGDIR, "received", nodeinfo.nodenumber);
	megalog(id, sender, filename);
}

static void produce_filth(char * ptr, int len) {
	for(int i=0;i<len;i++) {
		*ptr++ = (char) CNET_rand();
	}
}

/* generate messages of random data and random (bounded) length to pass to the 
 * transport layer
 */
void generate_message() {
	/* generate a message with a unique identifier and some random data of rando
	 * m (bounded) length to a random receipient */
	int len  = CNET_rand() % MAXMESSAGESIZE;
	/* TODO: currently assumes addresses  == node IDs */
	CnetAddr recipient = (CnetAddr)(CNET_rand() % NUM_NODES);
	AppMessage m;
	m.id = messageCount;
	produce_filth(m.filth, len);
	/* save the unique identifier, and the global time at which it was sent */
	logsent(messageCount, recipient);
	messageCount++;
}

/* accept a message from a receiver
 * at this point, the message contents have been verified on the transport layer
 * , so no further verification is required. 
 */
void receive_message(char* data, int len, CnetAddr sender) {
	/* record the received unique identifier, and the global time at which it wa
	 * s received */
	AppMessage * m = (AppMessage*)data;
	/* log then discard */
	logrcv(m->id, sender);
}


