/* this file handles data link layer functions, including:
 *  - CSMA/CA with binary exponential backoff
 *  - buffers and retries data to be sent
 *  - passes received data frames to the appropriate handlers
 *  - manages address resolution and maintains an address resolution cache
 */
#include "dtn.h"

#define TIMESLOT 			CNET_rand()%freq + 1
#define FRAME_HEADER_SIZE (sizeof(FRAME) - sizeof(MSG))
#define FRAME_SIZE(f)	  (FRAME_HEADER_SIZE + f.len)
#define WAITINGTIME (FRAME_HEADER_SIZE + len)/linkinfo[1].bandwidth + linkinfo[1].propagationdelay
#define	DEFAULT_FREQ 1000000 //(3*FRAME_HEADER_SIZE + MAX_DATAGRAM_SIZE)/linkinfo[1].bandwidth + linkinfo[1].propagationdelay

/* Type definitions*/
typedef enum 
{
	DL_DATA, DL_BEACON, DL_RTS, DL_CTS, DL_ACK
} FRAMETYPE;

typedef struct 
{
	char	data[MAX_MESSAGE_SIZE];
} MSG;

typedef struct 
{
	FRAMETYPE	type;
	int		dest;	// Zero if beacon, so broadcasted
	int 		src;
	size_t		len;
	char*		msg;
} FRAME;


struct node
{
	FRAME f;
	struct node* next;
};

struct queue
{
	struct node* head;
	struct node* tail;
};

/* end type defs */


/* Global var declarations */
static	int64_t	freq	= DEFAULT_FREQ;

static CnetTimerID local_timer;
static CnetTimerID sendTimer;

struct queue* buf; 

static FRAME* info; //info frame buffer
static int numFrames;
static int backoff = 0;
/* End global vars */

/*queue definitions*/
int enqueue(struct queue* q, FRAME f)
{
	struct node* n = malloc(sizeof(struct node));
	if (n == NULL) 
	{
		return -1;
	}
	n->f = f;
	if (q->head == NULL)
		q->head = q->tail = n;
	else 
	{
		q->tail->next = n;
		q->tail = n;
	}
	n->next = NULL;
	return 0;
}

int dequeue(struct queue* q, FRAME* f)
{
	if (!q->head) 
	{
		return -1;
	}
	FRAME* frame = &(q->head->f);
	memcpy(f, frame, sizeof(frame));
	struct node* tmp = q->head;
	if (q->head == q->tail) 
	{
		q->head = NULL;
		q->tail = NULL;
	}
	else
		q->head = q->head->next;
	free(tmp);
	return 0;
}

void create_queue(struct queue* q)
{
	q->head = NULL;
	q->tail = NULL;
}
/*end of queue definitions*/



/* returns max # of bytes of data that link_send_data or link_send_info
 * will currently accept */
int get_nbytes_writeable() 
{
	/* TODO: Use it or lose it - Renee */
	return 10000000;
}

void send_frame(FRAMETYPE type, CnetAddr dest, int len, char* data) 
{
	FRAME f;
	f.type = type;
	f.dest = dest;
	f.src = nodeinfo.nodenumber;
	f.len = len;
	f.msg = malloc(len);
	if(data != NULL)
		memcpy(f.msg, data, len);
	else 
		f.msg = NULL;

	size_t msglen = sizeof(FRAME) + len; // TODO: check that this is the correct size!! I just made this up - Renee.
	//send frame over cnet
	CHECK(CNET_write_physical(1, &f, &msglen));
}

/* send data msg of length len to receiver recv 
 */
void link_send_data( char* msg, int len, CnetAddr recv) 
{
	//put a frame into the frame buffer
	FRAME f;
	f.type = DL_DATA;
	f.dest = recv;
	f.src = nodeinfo.nodenumber;
	f.len = len;
	f.msg = malloc(len);
	memcpy(f.msg, msg, len);
	enqueue(buf, f);
}

/* send info msg of length len to receiver recv
 *
 * Note: This function performs the exact same action as
 * link_send_data, but the frames should be marked as 
 * being info types so the receiver knows to pass them to
 * the oracle
 */
void link_send_info( char* msg, int len, CnetAddr recv) 
{
	FRAME* f = malloc(sizeof(FRAME));
	f->type = DL_DATA;
	f->dest = recv;
	f->src = nodeinfo.nodenumber;
	f->len = len;
	f->msg = malloc(len);
	memcpy(f->msg, msg, len);
	info = f;
}

static EVENT_HANDLER(collision) 
{
	CNET_stop_timer(sendTimer);
	sendTimer = CNET_start_timer(EV_TIMER2, TIMESLOT* (CNET_rand()%((int)pow(2,backoff))), 0);
	backoff++;
}

static EVENT_HANDLER(send) 
{
	if(CNET_carrier_sense(1)==0) 
	{
		backoff = 0;
		if(info != NULL) 
		{
			FRAME* b = info;
			info = NULL;
			send_frame(DL_BEACON, b->dest, b->len, b->msg);
			sendTimer = CNET_start_timer(EV_TIMER2, TIMESLOT, 0);
		}
		else 
		{
			FRAME f = buf->head->f;
			send_frame(DL_RTS, f.dest, 0, NULL);
		}
		//TODO: Free b?
	}

}

static EVENT_HANDLER(timeout) 
{
	CnetData numTimeouts = 0; // ok to init to zero? I don't know what's going on here - Renee.
	if(CNET_timer_data(local_timer, &numTimeouts) == 0) 
	{ 
		if(numTimeouts > 3) 
		{
			sendTimer = CNET_start_timer(EV_TIMER2, TIMESLOT, 0);
			FRAME* f = malloc(sizeof(FRAME));
			dequeue(buf, f);
			free(f);
		} 
		else 
		{
			sendTimer = CNET_start_timer(EV_TIMER2, TIMESLOT, numTimeouts + 1);
		}
	} 
	else 
	{
		sendTimer = CNET_start_timer(EV_TIMER2, TIMESLOT, 0);
		FRAME* f = malloc(sizeof(FRAME));
		dequeue(buf, f);
		free(f);
	}
}



static EVENT_HANDLER(receive) 
{
	FRAME f;
	size_t len;
	int link;

	//receive the frame
	CHECK(CNET_read_physical(&link, &f, &len));

	switch(f.type) 
	{
		case DL_BEACON:
			oracle_recv(f.msg, f.len, f.src);
			break;
		case DL_RTS:
			if(f.dest == nodeinfo.nodenumber) 
			{
				send_frame(DL_CTS, f.src, 0, NULL);	
			}
			local_timer = CNET_start_timer(EV_TIMER1, WAITINGTIME, 0);
			break;
		case DL_CTS:
			if(f.dest == nodeinfo.nodenumber) 
			{
				FRAME next;
				dequeue(buf, &next);
				send_frame(next.type, next.src, next.len, next.msg); //send DATA
			}
			local_timer = CNET_start_timer(EV_TIMER1, WAITINGTIME, 0);
			break;
		case DL_DATA:
			if(f.dest == nodeinfo.nodenumber) 
			{
				net_recv(f.msg, f.len, f.src);
			}
			local_timer = CNET_start_timer(EV_TIMER1, WAITINGTIME, 0);
			break;
		case DL_ACK:
			if(f.dest == nodeinfo.nodenumber) 
			{
				sendTimer = CNET_start_timer(EV_TIMER2, TIMESLOT, 0);
			}
			CNET_stop_timer(local_timer);
			break;
			//schedule next message (or set some state that means we can send)
	}
}

/* called on program initialisation */
void link_init() 
{
	/* set mac address */
	/* register CNET handlers for physical ready */
	CHECK(CNET_set_handler(EV_PHYSICALREADY, receive, 0));
	CHECK(CNET_set_handler(EV_TIMER1, timeout, 0));
	CHECK(CNET_set_handler(EV_TIMER2, send, 0));
	CHECK(CNET_set_handler(EV_FRAMECOLLISION, collision, 0));

	create_queue(buf);

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
