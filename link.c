/* this file handles data link layer functions, including:
 *  - CSMA/CA with binary exponential backoff
 *  - buffers and retries data to be sent
 *  - passes received data frames to the appropriate handlers
 *  - manages address resolution and maintains an address resolution cache
 */
#include "dtn.h"

typedef enum {DL_DATA, DL_BEACON, DL_RTS, DL_CTS, DL_ACK} FRAMETYPE;

typedef struct {
	char	data[MAX_MESSAGE_SIZE];
} MSG;

typedef struct {
	FRAMETYPE	type;
	int			dest;	// Zero if beacon, so broadcasted
	int 		src;
	size_t		len;
	MSG			msg;	// Holds list of recently seen addresses if beacon
} FRAME;

#define FRAME_HEADER_SIZE (sizeof(FRAME) - sizeof(MSG))

#define FRAME_SIZE(f)	  (FRAME_HEADER_SIZE + f.len)

#define WAITINGTIME (FRAME_HEADER_SIZE + len)/linkinfo[1].bandwidth + linkinfo[1].propagationdelay
#define	DEFAULT_FREQ		(3*FRAME_HEADER_SIZE + MAX_DATAGRAM_SIZE)/linkinfo[1].bandwidth + linkinfo[1].propagationdelay//1000000

static	int64_t	freq	= DEFAULT_FREQ;
#define TIMESLOT 			CNET_rand()%freq + 1

static CnetTimerID timer;
static CnetTimerID sendTimer;

typedef struct {
	FRAME f;
	struct FRAMELIST* succ;
} FRAMELIST;

static FRAMELIST* fBuf; //frame buffer
static FRAMELIST* tail;
static FRAME info; //info frame buffer
static int numFrames;
static int backoff = 0;

/* returns max # of bytes of data that link_send_data or link_send_info
 * will currently accept */
int get_nbytes_writeable();

void add(FRAMELIST * buf, FRAMELIST * tail, int * numEl, FRAME f) {
	if(numFrames < BUFSIZ) {
		if(buf == NULL) {
				buf = malloc(sizeof(FRAMELIST));
				buf->f = f;
				buf->succ = NULL;
			} else {
				tail->succ = malloc(sizeof(FRAMELIST));
				tail = tail->succ;
				tail->f = f;
				tail->succ = NULL;
			}
			*numEl++;
	}
}

/*gets and removes the frame from the front of the queue*/
FRAME removeHead(FRAMELIST * buf, int * numEl) {
	FRAME out = buf->f;
	FRAMELIST* next = buf->succ;
	FRAMELIST* head = buf;
	buf = next;
	free(head);
	*numEl--;
	return out;
}

void send_frame(FRAMETYPE type, CnetAddr dest, int len, char *data) {
	FRAME f;
	f.type = type;
	f.dest = dest;
	f.src = nodeinfo.nodenumber;
	f.len = len;
	if(data != NULL)
		memcpy(f.msg.data, data, len);
	else f.msg.data = NULL;
	
	size_t msglen = FRAMESIZE(f);
	//send frame over cnet
	CHECK(CNET_write_physical(1, &f, &msglen));
}

/* send data msg of length len to receiver recv 
 */
void link_send_data( char * msg, int len, CnetAddr recv) {
	//put a frame into the frame buffer
		FRAME f;
		f.type = DL_DATA;
		f.dest = recv;
		f.src = nodeinfo.nodenumber;
		f.len = len;
		memcpy(f.msg.data, msg, len);
		add(fBuf, tail, &numFrames, f);
}

/* send info msg of length len to receiver recv
 *
 * Note: This function performs the exact same action as
 * link_send_data, but the frames should be marked as 
 * being info types so the receiver knows to pass them to
 * the oracle
 */
void link_send_info( char * msg, int len, CnetAddr recv) {
	FRAME f;
	f.type = DL_DATA;
	f.dest = recv;
	f.src = nodeinfo.nodenumber;
	f.len = len;
	memcpy(f.msg.data, msg, len);
	info = f;
	//send_frame(DL_BEACON, recv, len, msg);
}

static EVENT_HANDLER(collision) {
	CNET_stop_timer(sendTimer);
	sendTimer = CNET_start_timer(EV_TIMER2, TIMESLOT*(CNET_rand()%((int)pow(2,backoff))), 0);
	backoff++;
}

static EVENT_HANDLER(send) {
	if(CNET_carrier_sense(1)==0) {
		backoff = 0;
		if((void*)info != NULL) {
			FRAME b = info;
			info = (FRAMELIST*)NULL;
			send_frame(DL_BEACON, b.dest, b.len, b.msg.data);
			sendTimer = CNET_start_timer(EV_TIMER2, TIMESLOT, 0);
		} else {
			FRAME f = fBuf->f;
			send_frame(DL_RTS, f.dest, 0, NULL);
		}
		//send RTS
		//wait for CTS
		//send data
		//wait for ack
	}
	
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
	int numTimeouts;
	if(CNET_timer_data(timer, &numTimeouts) == 0) { 
		if(numTimeouts > 3) {
			sendTimer = CNET_start_timer(EV_TIMER2, TIMESLOT, 0);
			removeHead(fBuf, &numFrames);
		} else {
			sendTimer = CNET_start_timer(EV_TIMER2, TIMESLOT, numTimeouts + 1);
		}
	} else {
		sendTimer = CNET_start_timer(EV_TIMER2, TIMESLOT, 0);
		removeHead(fBuf, &numFrames);
	}
}



static EVENT_HANDLER(receive) {
	FRAME f;
	size_t len;
	int link;
	
	//receive the frame
	CHECK(CNET_read_physical(&link, &f, &len));
	
	switch(f.type) {
		case DL_BEACON:
			oracle_recv(&(f.msg.data), f.len, f.src);
			break;
		case DL_RTS:
			if(f.dest == nodeinfo.nodenumber) {
				send_frame(DL_CTS, f.src, 0, NULL);	
			}
			timer = CNET_start_timer(EV_TIMER1, WAITINGTIME, 0);
			break;
		case DL_CTS:
			if(f.dest == nodeinfo.nodenumber) {
				FRAME next = removeHead(fBuf, &numFrames);
				send_frame(next.type, next.src, next.len, &next.msg.data); //send DATA
			}
			timer = CNET_start_timer(EV_TIMER1, WAITINGTIME, 0);
			break;
		case DL_DATA:
			if(f.dest == nodeinfo.nodenumber) {
				net_recv(&f.msg.data, f.len, f.src);
			}
			timer = CNET_start_timer(EV_TIMER1, WAITINGTIME, 0);
			break;
		case DL_ACK:
			if(f.dest == nodeinfo.nodenumber) {
				sendTimer = CNET_start_timer(EV_TIMER2, TIMESLOT, 0);
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
	CHECK(CNET_set_handler(EV_PHYSICALREADY, receive, 0));
	CHECK(CNET_set_handler(EV_TIMER1, timeout, 0));
	CHECK(CNET_set_handler(EV_TIMER2, send, 0));
	CHECK(CNET_set_handler(EV_FRAMECOLLISION, collision, 0));
	
	fBuf = malloc(sizeof(FRAMELIST));
	fBuf->f = NULL;
	fBuf->succ = NULL;
	tail = NULL;
		
	info = NULL;
	numFrames = 0;
}

/** RECEIVED DATA **/

/* received data frames should be passed to:
 *  net_recv( char * msg, int len, CnetAddr from);
 *
 * received info frames should be passed to: 
 * 	void oracle_recv(char * msg, int len, CnetAddr from);
 */
