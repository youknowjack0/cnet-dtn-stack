#include frame.c

/* this file handles data link layer functions, including:
 *  - CSMA/CA with binary exponential backoff
 *  - buffers and retries data to be sent
 *  - passes received data frames to the appropriate handlers
 *  - manages address resolution and maintains an address resolution cache
 */
#include "dtn.h"

/* TODO: define a frame structure */

static bool attemptingToSend = false;
char* nextMsg;

/* returns max # of bytes of data that link_send_data or link_send_info
 * will currently accept */
int get_nbytes_writeable();

void send_frame(FRAMETYPE type, CnetAddr dest, int len, char *data) {
	FRAME f;
	f.kind = type;
	f.dest = dest;
	f.src = nodeinfo.nodenumber;
	f.len = len;
	//probably won't need seq or msg
	
	//send frame over cnet
}

/* send data msg of length len to receiver recv 
 */
void link_send_data( char * msg, int len, CnetAddr recv) {
	FRAME f;
	//send RTS
	//wait for CTS
	//send data
	//wait for ack
}

/* send info msg of length len to receiver recv
 *
 * Note: This function performs the exact same action as
 * link_send_data, but the frames should be marked as 
 * being info types so the receiver knows to pass them to
 * the oracle
 */
void link_send_info( char * msg, int len, CnetAddr recv) {
}

/* called on program initialisation */
void link_init() {
	/* set mac address */
	/* register CNET handlers for physical ready */
}

static EVENT_HANDLER(recieve) {
	FRAME f;
	size_t len;
	int link;
	
	//recieve the frame
	
	switch(f.kind) {
		case DL_RTS:
			send_frame(DL_CTS, f.src, f.len, NULL);
		case DL_CTS:
			if(attemptingToSend)
				send_frame(DL_DATA, f.src, f.len, nextMsg);
			//otherwise wait
		case DL_DATA:
			if(f.dest == nodeinfo.nodenumber) {
				net_recv(f.msg, f.len, f.src);
			}
		case DL_ACK:
			if(attemptingToSend)
				//schedule next message (or set some state that means we can send)
	}
}

/** RECEIVED DATA **/

/* received data frames should be passed to:
 *  net_recv( char * msg, int len, CnetAddr from);
 *
 * received info frames should be passed to: 
 * 	void oracle_recv(char * msg, int len, CnetAddr from);
 */
