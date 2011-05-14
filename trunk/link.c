#include frame.c

/* this file handles data link layer functions, including:
 *  - CSMA/CA with binary exponential backoff
 *  - buffers and retries data to be sent
 *  - passes received data frames to the appropriate handlers
 *  - manages address resolution and maintains an address resolution cache
 */
#include "dtn.h"

/* TODO: define a frame structure */

#define WAITINGTIME 

static bool attemptingToSend = false;
char* nextMsg;

CnetTimerID timer;
CnetTimerID sendTimer;

FRAME* fBuf;

/* returns max # of bytes of data that link_send_data or link_send_info
 * will currently accept */
int get_nbytes_writeable();

void send_frame(FRAMETYPE type, CnetAddr dest, int len, char *data) {
	FRAME f;
	f.kind = type;
	f.dest = dest;
	f.src = nodeinfo.nodenumber;
	f.len = len;
	memcpy(f.msg, data, len);
	
	//send frame over cnet
	CHECK(CNET_write_physical(1, &f, &len));
}

/* send data msg of length len to receiver recv 
 */
void link_send_data( char * msg, int len, CnetAddr recv) {
	//put a frame into the frame buffer
}

/* send info msg of length len to receiver recv
 *
 * Note: This function performs the exact same action as
 * link_send_data, but the frames should be marked as 
 * being info types so the receiver knows to pass them to
 * the oracle
 */
void link_send_info( char * msg, int len, CnetAddr recv) {
	//send a beacon frame
}

static EVENT_HANDLER(collision) {
	//binary exponential backoff
}

static EVENT_HANDLER(send) {
	if(CNET_carrier_sense(1)==0 /*&& we have something to send*/) {
		//send RTS
		//wait for CTS
		//send data
		//wait for ack
	}
}

static EVENT_HANDLER(timeout) {
	FRAMETYPE type;
	if(CNET_timer_data(timer, &type) == 0) {
		switch(type) {
			case DL_RTS:
				sendTimer = CNET_start_timer(EV_TIMER2, 0, 0);
				break;
			
		}
	}
}

static EVENT_HANDLER(recieve) {
	FRAME f;
	size_t len;
	int link;
	
	//recieve the frame
	CHECK(CNET_read_physical(1, &f, &len));
	
	switch(f.kind) {
		case DL_BEACON:
			oracle_recv(&f.msg, len, f.src);
			break;
		case DL_RTS:
			if(f.dest == nodeinfo.nodenumber) {
				send_frame(DL_CTS, f.src, len, NULL);	
			}
			FRAMETYPE type = DL_RTS;
			timer = CNET_start_timer(EV_TIMER1, (2*FRAME_HEADER_SIZE + len)/linkinfo.bandwidth + linkinfo.propegationdelay
				, &type);
			break;
		case DL_CTS:
			if(f.dest == nodeinfo.nodenumber) {
				send_frame(DL_DATA, f.src, len, nextMsg);
			}
			FRAMETYPE type = DL_CTS;
			timer = CNET_start_timer(EV_TIMER1, (FRAME_HEADER_SIZE + len)/linkinfo.bandwidth + linkinfo.propegationdelay, &type);
			break;
			//otherwise wait
		case DL_DATA:
			if(f.dest == nodeinfo.nodenumber) {
				net_recv(f.msg, len, f.src);
			}
			FRAMETYPE type = DL_DATA;
			timer = CNET_start_timer(EV_TIMER1, (FRAME_HEADER_SIZE + len)/linkinfo.bandwidth + linkinfo.propegationdelay, &type);
			break;
		case DL_ACK:
			if(f.dest == nodeinfo.nodenumber) {
				//do something
			}
			CNET_stop_timer(timer);
			break;
				//schedule next message (or set some state that means we can send)
	}
}

/* called on program initialisation */
void link_init() {
	/* set mac address */
	/* register CNET handlers for physical ready */
	CHECK(CNET_set_handler(EV_PHYSICALREADY, recieve, 0));
	CHECK(CNET_set_handler(EV_TIMER1, timeout, 0));
	CHECK(CNET_set_handler(EV_TIMER2, send, 0));
	CHECK(CNET_set_handler(EV_FRAMECOLLISION, collision, 0);
}

/** RECEIVED DATA **/

/* received data frames should be passed to:
 *  net_recv( char * msg, int len, CnetAddr from);
 *
 * received info frames should be passed to: 
 * 	void oracle_recv(char * msg, int len, CnetAddr from);
 */
