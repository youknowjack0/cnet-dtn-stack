/* this file handles data link layer functions, including:
 *  - CSMA/CA with binary exponential backoff
 *  - buffers and retries data to be sent
 *  - passes received data frames to the appropriate handlers
 *  - manages address resolution and maintains an address resolution cache
 */
#include "dtn.h"


#define IDLE_TIMESLOT 			CNET_rand()%idle_freq + 1
#define ACT_TIMESLOT			CNET_rand()%active_freq + 1

#define WAITINGTIME 50
#define IDLE_FREQ 1000000 
#define ACTIVE_FREQ 100

/*
 * Struct for a frame
 */
typedef struct 
{
	FRAMEHEADER h;
	char		msg[MAX_PACKET_SIZE];
} FRAME;


/*
 ************************
 * STRUCTURES FOR QUEUE *
 ************************
 */

/*
 * And element of a queue
 */
struct node
{
	FRAME f;
	struct node* next;
};

/*
 * A queue
 */
struct queue
{
	struct node* head;
	struct node* tail;
};
/*
 ************************
 * END QUEUE STRUCTURES *
 ************************
 */


#define FRAME_SIZE(f)      (FRAME_HEADER_SIZE + f.len)

/*
 ********************************
 * GLOBAL VARIABLE DECLARATIONS *
 ********************************
 */
static	int64_t	idle_freq	= IDLE_FREQ;
static 	int64_t active_freq = ACTIVE_FREQ;

static CnetTimerID local_timer;
static CnetTimerID sendTimer;

struct queue* buf; 

static FRAME* info = NULL;
static bool sent_info = false;
static bool sending_data = false;
static int numFrames;
static int backoff = 0;
static int numTimeouts = 0;
/*
 ************************
 * END GLOBAL VARIABLES *
 ************************
 */

/*
 **********************************
 * FUNCTIONS FOR QUEUE MANAGEMENT *
 * ********************************
 */

/*
 * Place a frame on the queue
 */
int enqueue(struct queue* q, FRAME f)
{
	struct node* n = malloc(sizeof(struct node));
	n->f = f;
	if (q->head == NULL)
	{
		q->head = q->tail = n;
	}
	else 
	{
		q->tail->next = n;
		q->tail = n;
	}
	n->next = NULL;
	return 0;
}

/*
 * Remove a frame from the queue
 */
int dequeue(struct queue* q, FRAME* f)
{
	if (!q->head) 
	{
		return -1;
	}
	FRAME* frame = &(q->head->f);
	memcpy(f, frame, sizeof(*frame));
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

/*
 * Create a new queue
 */
void create_queue(struct queue* q)
{
	q->head = NULL;
	q->tail = NULL;
}
/*
 ***********************
 * END QUEUE FUNCTIONS *
 ***********************
 */


/*
 * Resets the send timer
 */
void reset_send_timer() 
{
	if(buf->head != NULL) 
	{
		sendTimer = CNET_start_timer(EV_TIMER2, ACT_TIMESLOT, 0);
	}
	else 
	{
		sendTimer = CNET_start_timer(EV_TIMER2, IDLE_TIMESLOT, 0);
	}
}

/*
 * Sends a frame to the physical layer
 */
void send_frame(FRAMETYPE type, CnetAddr dest, size_t len, char* data) 
{
	assert(len <= MAX_PACKET_SIZE);
	FRAME f;
	f.h.type = type;
	f.h.dest = dest;
	f.h.src = nodeinfo.nodenumber;
	f.h.len = len;
	f.h.checksum = 0;
	size_t framelen;
	if(data != NULL) 
	{
		memcpy(f.msg, data, len);
		framelen = FRAME_HEADER_SIZE + len;
	}
	else  
	{
		f.h.len = 0;
		framelen = FRAME_HEADER_SIZE;
	}
	f.h.checksum  = CNET_crc32((unsigned char *)&f, (int)framelen);
	CHECK(CNET_write_physical(1, &f, &framelen));
}

/*
 * send data msg of length len to receiver recv 
 */
void link_send_data( char* msg, int len, CnetAddr recv) 
{
	FRAME f;
	f.h.type = DL_DATA;
	f.h.dest = recv;
	f.h.src = nodeinfo.nodenumber;
	f.h.len = len;
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
	if(info != NULL)
		free(info);
	FRAME* f = malloc((FRAME_HEADER_SIZE) + len);
	f->h.type = DL_BEACON;
	f->h.dest = recv;
	f->h.src = nodeinfo.nodenumber;
	f->h.len = len;
	memcpy(f->msg, msg, len);
	info = f;
	sent_info = false;
}

/*
 * Called in the event of a collision
 */
static EVENT_HANDLER(collision) 
{
	CHECK(CNET_stop_timer(sendTimer));
	sendTimer = CNET_start_timer(EV_TIMER2, 
		IDLE_TIMESLOT* (CNET_rand()%((int)pow(2,backoff))), 0);
	backoff++;
}

/*
 * Called on sending
 */
static EVENT_HANDLER(send) 
{
	if(CNET_carrier_sense(1)==0) 
	{
		backoff = 0;
		if(sent_info == false && info != NULL)
		{
			send_frame(DL_BEACON, info->h.dest, info->h.len, info->msg);
			free(info);
			info = NULL;
			sent_info = true;
		}
		else if(sending_data == false && buf->head != NULL)
		{
			FRAME f = buf->head->f;
			sending_data = true;
			local_timer = CNET_start_timer(EV_TIMER1, WAITINGTIME, 0);
			send_frame(DL_RTS, f.h.dest, 0, NULL);
		}
	}
	reset_send_timer();
}

/*
 * Called when a timer times out
 */
static EVENT_HANDLER(timeout) 
{
	numTimeouts++;
		if(numTimeouts > 3) 
		{
			FRAME* f = malloc(sizeof(FRAME));
			dequeue(buf, f);
			free(f);
			numTimeouts = 0;
			
		} 
		sending_data = false;
		CNET_stop_timer(sendTimer);
		reset_send_timer();
}

/*
 * Called upon receiving a frame
 */
static EVENT_HANDLER(receive) 
{
	FRAME f;
	size_t len;
	int link; 
	uint32_t checksum;

	len = MAX_FRAME_SIZE;
	CHECK(CNET_read_physical(&link, &f, &len));
	
	checksum    = f.h.checksum;
    f.h.checksum  = 0;
    uint32_t new_check = CNET_crc32((unsigned char *)&f, len);
    if(new_check != checksum) {
        return;
    }
	
	switch(f.h.type)
	{
		case DL_BEACON:
			oracle_recv(f.msg, f.h.len, f.h.src);
			break;
		case DL_RTS:
			if(f.h.dest == nodeinfo.nodenumber)
			{
				send_frame(DL_CTS, f.h.src, 0, NULL);
			}
			local_timer = CNET_start_timer(EV_TIMER1, WAITINGTIME, 0);
			break;
		case DL_CTS:
			if(f.h.dest == nodeinfo.nodenumber) 
			{
				CNET_stop_timer(local_timer);
				FRAME next;
				dequeue(buf, &next);
				send_frame(DL_DATA, next.h.dest, next.h.len, next.msg);
			}
			local_timer = CNET_start_timer(EV_TIMER1, WAITINGTIME, 0);
			break;
		case DL_DATA:
			if(f.h.dest == nodeinfo.nodenumber) 
			{
				CNET_stop_timer(local_timer);
				net_recv(f.msg, f.h.len, f.h.src);
				send_frame(DL_ACK, f.h.src, 0, NULL);
			}
			local_timer = CNET_start_timer(EV_TIMER1, WAITINGTIME, 0);
			break;
		case DL_ACK:
			if(f.h.dest == nodeinfo.nodenumber) 
			{
				sending_data = false;
				reset_send_timer();
			}
			CNET_stop_timer(local_timer);
			break;
	}
}

/* 
 * called on program initialisation 
 * */
void link_init() 
{
	/* set mac address */
	/* register CNET handlers for physical ready */
	CHECK(CNET_set_handler(EV_PHYSICALREADY, receive, 0));
	CHECK(CNET_set_handler(EV_TIMER1, timeout, 0));
	CHECK(CNET_set_handler(EV_TIMER2, send, 0));
	CHECK(CNET_set_handler(EV_FRAMECOLLISION, collision, 0));

	buf = malloc(sizeof(struct queue));
	create_queue(buf);

	info = NULL;
	numFrames = 0;
	
	reset_send_timer();
}

