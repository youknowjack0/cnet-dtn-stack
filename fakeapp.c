/* Fake application layer
 * Pumps out random message-turds to the transport layer. 
 */
#include "dtn.h"

/*
 * Structure for application layer message
 */
typedef struct {
	int id;
	char filth[MAXMESSAGESIZE];
} AppMessage;

/*
 * Holds the number of messages sent
 */
static int messageCount = 0;


static void logsent(int id, CnetAddr recipient) {
	char filename[BUFSIZ];
	sprintf(filename, "%s/%s-%d", LOGDIR, "sent", nodeinfo.nodenumber);
}

static void logrcv(int id, CnetAddr sender) {
	/*
	 * exact same as logsent, but in a different file
	 */
	char filename[BUFSIZ];
	sprintf(filename, "%s/%s-%d", LOGDIR, "received", nodeinfo.nodenumber);
}

/*
 *  Generates random data
 */
static void produce_filth(char * ptr, int len) {
	for(int i=0;i<len;i++) {
		*ptr++ = (char) CNET_rand();
	}
}

/* 
 * generate messages of random data and random (bounded) length to pass to the 
 * transport layer
 */
void generate_message() {
	/* 
	 * generate a message with a unique identifier and some random data of 
	 * random (bounded) length to a random receipient 
	 */
	int len  = CNET_rand() % MAXMESSAGESIZE;
	CnetAddr recipient = (CnetAddr)(CNET_rand() % NUM_NODES);
	AppMessage m;
	m.id = messageCount;
	produce_filth(m.filth, len);
	/* 
	 * save the unique identifier, and the global time at which it was sent 
	 */
	logsent(messageCount, recipient);
	messageCount++;
}

/* 
 * accept a message from a receiver
 * at this point, the message contents have been verified on the transport 
 * layer, so no further verification is required. 
 */
void receive_message(char* data, int len, CnetAddr sender) {
	/* 
	 * record the received unique identifier, and the global time at which it was 
	 * received 
	 * */
	AppMessage * m = (AppMessage*)data;
	/* log then discard */
	logrcv(m->id, sender);
}
