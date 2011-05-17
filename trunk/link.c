#include frame.c

/* this file handles data link layer functions, including:
 *  - CSMA/CA with binary exponential backoff
 *  - buffers and retries data to be sent
 *  - passes received data frames to the appropriate handlers
 *  - manages address resolution and maintains an address resolution cache
 */
#include "dtn.h"

/* TODO: define a frame structure */

#define WAITINGTIME (FRAME_HEADER_SIZE + len)/linkinfo.bandwidth + linkinfo.propegationdelay
#define BUFSIZ  1000
#define	DEFAULT_FREQ		(3*FRAME_HEADER_SIZE + MAX_DATAGRAM_SIZE)/linkinfo.bandwidth + linkinfo.propegationdelay//1000000

static	int64_t	freq	= DEFAULT_FREQ;
#define TIMESLOT 			CNET_rand()%freq + 1

static bool attemptingToSend = false;
char* nextMsg;

static CnetTimerID timer;
static CnetTimerID sendTimer;

static FRAME fBuf[BUFSIZ];
static int numFrames;
static int backoff = 0;

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
	
	int msglen = FRAMESIZE(f);
	//send frame over cnet
	CHECK(CNET_write_physical(1, &f, &msglen));
}

/* send data msg of length len to receiver recv 
 */
void link_send_data( char * msg, int len, CnetAddr recv) {
	//put a frame into the frame buffer
	if(numFrames < BUFSIZ) {
		FRAME f;
		f.type = DL_DATA;
		f.dest = recv;
		f.src = nodeinfo.nodenumber;
		f.len = len;
		f.msg = msg;
		fBuf[numFrames] = f;
		numFrames++;
	}
}

/* send info msg of length len to receiver recv
 *
 * Note: This function performs the exact same action as
 * link_send_data, but the frames should be marked as 
 * being info types so the receiver knows to pass them to
 * the oracle
 */
void link_send_info( char * msg, int len, CnetAddr recv) {
	send_frame(DL_BEACON, recv, len, msg);
}

static EVENT_HANDLER(collision) {
	CNET_stop_timer(sendTimer);
	sendTimer = CNET_start_timer(EV_TIMER1, TIMESLOT*(CNET_rand()%((int)pow(2,backoff))), 0);
	backoff++;
}

static EVENT_HANDLER(send) {
	if(CNET_carrier_sense(1)==0 && fBuf[0] != NULL) {
		//send RTS
		backoff = 0;
		FRAME f = fBuf[0];
		send_frame(DL_RTS, f.dest, 0, NULL);
		
		//wait for CTS
		//send data
		//wait for ack
	}
	sendTimer = CNET_start_timer(EV_TIMER2, TIMESLOT, 0);
}

static EVENT_HANDLER(timeout) {
	/*
	FRAMETYPE type;
	if(CNET_timer_data(timer, &type) == 0) {
		switch(type) {
			case DL_RTS:
				sendTimer = CNET_start_timer(EV_TIMER2, 0, 0);
				break;
			
		}
	}
	*/
	//for now if we timeout, just drop the front frame
}

/*gets and removes the frame from the front of the queue*/
FRAME removeFrontFrame() {
	FRAME out = fBuf[0];
	int i = 0;
	while(numFrames > 0 && fBuf[i] != NULL && i < BUFSIZ) {
		fBuf[i] = fBuf[i+1];
		i++;
	}
	numFrames--;
	return out;
}

static EVENT_HANDLER(recieve) {
	FRAME f;
	size_t len;
	int link;
	
	//recieve the frame
	CHECK(CNET_read_physical(1, &f, &len));
	
	switch(f.kind) {
		case DL_BEACON:
			oracle_recv(&f.msg, f.len, f.src);
			break;
		case DL_RTS:
			if(f.dest == nodeinfo.nodenumber) {
				send_frame(DL_CTS, f.src, f.len, NULL);	
			}
			FRAMETYPE type = DL_RTS;
			timer = CNET_start_timer(EV_TIMER1, WAITINGTIME, &type);
			break;
		case DL_CTS:
			if(f.dest == nodeinfo.nodenumber) {
				FRAME next = removeFrontFrame();
				send_frame(next.type, next.src, next.len, &next.msg.data); //send DATA
			}
			FRAMETYPE type = DL_CTS;
			timer = CNET_start_timer(EV_TIMER1, WAITINGTIME, &type);
			break;
			//otherwise wait
		case DL_DATA:
			if(f.dest == nodeinfo.nodenumber) {
				net_recv(f.msg.data, f.len, f.src);
			}
			FRAMETYPE type = DL_DATA;
			timer = CNET_start_timer(EV_TIMER1, WAITINGTIME, &type);
			break;
		case DL_ACK:
			if(f.dest == nodeinfo.nodenumber) {
				//do something
				//shedule next message
				sendTimer = CNET_start_timer(EV_TIMER2, 0, 0);
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
	
	fBuf = malloc(sizeof(FRAME) * BUFSIZ);
	for(int i = 0; i < BUFSIZ; i++)
		fBuf[i] = NULL;
	numFrames = 0;
}

/** RECEIVED DATA **/

/* received data frames should be passed to:
 *  net_recv( char * msg, int len, CnetAddr from);
 *
 * received info frames should be passed to: 
 * 	void oracle_recv(char * msg, int len, CnetAddr from);
 */
