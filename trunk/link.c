/* this file handles data link layer functions, including:
 *  - CSMA/CA with binary exponential backoff
 *  - buffers and retries data to be sent
 *  - passes received data frames to the appropriate handlers
 *  - manages address resolution and maintains an address resolution cache
 */
#include "dtn.h"

#define TIMESLOT 			CNET_rand()%freq + 1

#define WAITINGTIME 1000000000
#define	DEFAULT_FREQ 1000000 //(3*FRAME_HEADER_SIZE + MAX_DATAGRAM_SIZE)/linkinfo[1].bandwidth + linkinfo[1].propagationdelay

/* Type definitions*/

typedef struct 
{
	FRAMETYPE	type;
	int		dest;	// Zero if beacon, so broadcasted
	int 		src;
	size_t		len;
	int checksum;
	char		msg[MAX_PACKET_SIZE];
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


#define FRAME_HEADER_SIZE2  (sizeof(FRAME) - (sizeof(char)*MAX_PACKET_SIZE))
#define FRAME_SIZE(f)      (FRAME_HEADER_SIZE2 + f.len)

/* Global var declarations */
static	int64_t	freq	= DEFAULT_FREQ;

static CnetTimerID local_timer;
static CnetTimerID sendTimer;

struct queue* buf; 

static FRAME* info; //info frame buffer
static bool sent_info = false;
static bool sending_data = false;
static int numFrames;
static int backoff = 0;
static int numTimeouts = 0;
/* End global vars */

/*queue definitions*/
int enqueue(struct queue* q, FRAME f)
{
	/* TODO: whut? there is no way n can be null here */
	struct node* n = malloc(sizeof(struct node));
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
/* NOT USED
int get_nbytes_writeable() 
{
	return 10000000;
}
*/

void send_frame(FRAMETYPE type, CnetAddr dest, size_t len, char* data) 
{
	printf("    Node %d: Sending frame\n", nodeinfo.nodenumber);
	FRAME f;// = malloc(FRAME_HEADER_SIZE + len);
	f.type = type;
	f.dest = dest;
	f.src = nodeinfo.nodenumber;
	f.len = len;
	f.checksum = 0;
	size_t framelen;
	printf("    Node %d: set up header\n", nodeinfo.nodenumber);
	if(data != NULL) {
		memcpy(f.msg, data, len);
		framelen = FRAME_HEADER_SIZE + len;
		printf("    Node %d: copied frame\n", nodeinfo.nodenumber);
	}
	else  {
		f.msg[0] = '\0';
		f.len = strlen(f.msg);
		framelen = FRAME_HEADER_SIZE + len;
	}
	f.checksum  = CNET_ccitt((unsigned char *)&f, (int)framelen);
	printf("Node %d: Sending frame with checksum %d, len = %d\n", nodeinfo.nodenumber, f.checksum, framelen);
	//send frame over cnet
	CHECK(CNET_write_physical(1, &f, &framelen));
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
	free(info);
	FRAME* f = malloc(FRAME_HEADER_SIZE + len);
	f->type = DL_DATA;
	f->dest = recv;
	f->src = nodeinfo.nodenumber;
	f->len = len;
	memcpy(f->msg, msg, len);
	info = f;
	sent_info = false;
}

static EVENT_HANDLER(collision) 
{
	CHECK(CNET_stop_timer(sendTimer));
	sendTimer = CNET_start_timer(EV_TIMER2, TIMESLOT* (CNET_rand()%((int)pow(2,backoff))), 0);
	backoff++;
}

static EVENT_HANDLER(send) 
{
	printf("starting send\n");
	if(CNET_carrier_sense(1)==0) 
	{
		backoff = 0;
		if(sent_info == false && info != NULL)
		{
			printf("trying to send beacon\n");
			send_frame(DL_BEACON, info->dest, info->len, info->msg);
			sent_info = true;
			//sendTimer = CNET_start_timer(EV_TIMER2, TIMESLOT, 0);
		}
		else if(sending_data == false && buf->head != NULL)
		{
			printf("trying to send data\n");
			FRAME f = buf->head->f;
			printf("got frame here\n");
			sending_data = true;
			local_timer = CNET_start_timer(EV_TIMER1, WAITINGTIME, 0);
			send_frame(DL_RTS, f.dest, f.len, f.msg);
		}
	}
	sendTimer = CNET_start_timer(EV_TIMER2, TIMESLOT, 0);
	printf("finished send\n");
}

static EVENT_HANDLER(timeout) 
{
	printf("\t\t\tTiming out!\n");
	numTimeouts++;
		if(numTimeouts > 3) 
		{
			
			/* TODO: not sure what's happening here */
			FRAME* f = malloc(sizeof(FRAME));
			dequeue(buf, f);
			free(f);
			numTimeouts = 0;
			
		} 
		sending_data = false;
		sendTimer = CNET_start_timer(EV_TIMER2, TIMESLOT, 0);
}



static EVENT_HANDLER(receive) 
{
	FRAME f;
	size_t len;
	int link, checksum;

	//receive the frame
	len = MAX_FRAME_SIZE;
	CHECK(CNET_read_physical(&link, &f, &len));
	printf("Node %d: Read physical, len = %d\n", nodeinfo.nodenumber, len);
	
	checksum    = f.checksum;
    f.checksum  = 0;
    printf("Node %d: Receiving frame with checksum %d\n",nodeinfo.nodenumber, checksum);
    int new_check = CNET_ccitt((unsigned char *)&f, len);
    printf("Calculated checksum is: %d\n", new_check);
    if(new_check != checksum) {
        printf("\t\t\t\tBAD checksum\n");
        //sending_data = false;
        //CNET_stop_timer(local_timer);
        //sendTimer = CNET_start_timer(EV_TIMER2, TIMESLOT, 0);
        return;           /* bad checksum, ignore frame */
    }
	
	switch(f.type)
	{
		case DL_BEACON:
			printf("Beacon from: %d\n",f.src);
			//oracle_recv(f.msg, f.len, f.src);
			break;
		case DL_RTS:
			printf("Node %d: Processing RTS\n", nodeinfo.nodenumber);
			if(f.dest == nodeinfo.nodenumber)
			{
				send_frame(DL_CTS, f.src, f.len, f.msg);
			}
			local_timer = CNET_start_timer(EV_TIMER1, WAITINGTIME, 0);
			break;
		case DL_CTS:
			printf("Node %d: Processing CTS\n", nodeinfo.nodenumber);
			if(f.dest == nodeinfo.nodenumber) 
			{
				CHECK(CNET_stop_timer(local_timer));
				FRAME next;
				dequeue(buf, &next);
				send_frame(DL_DATA, f.src, f.len, f.msg); //send DATA
			}
			local_timer = CNET_start_timer(EV_TIMER1, WAITINGTIME, 0);
			break;
		case DL_DATA:
			if(f.dest == nodeinfo.nodenumber) 
			{
				CHECK(CNET_stop_timer(local_timer));
				printf("Recevied from: %d\n",f.src);
				CHECK(CNET_write_application(&f.msg, &f.len));
				//net_recv(f.msg, f.len, f.src);
				send_frame(DL_ACK, f.src, f.len, f.msg);
			}
			local_timer = CNET_start_timer(EV_TIMER1, WAITINGTIME, 0);
			break;
		case DL_ACK:
			if(f.dest == nodeinfo.nodenumber) 
			{
				sending_data = false;
				sendTimer = CNET_start_timer(EV_TIMER2, TIMESLOT, 0);
			}
			CHECK(CNET_stop_timer(local_timer));
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

	buf = malloc(sizeof(struct queue));
	create_queue(buf);

	info = NULL;
	numFrames = 0;
	
	sendTimer = CNET_start_timer(EV_TIMER2, TIMESLOT, 0);
}

/*
EVENT_HANDLER(app_rdy)
{

	FRAME f;
	f.len = MAX_PACKET_SIZE;
	CHECK(CNET_read_application(&f.dest, f.msg, &f.len));
	printf("Node %d: generated message for %d\n", nodeinfo.nodenumber, f.dest);
	link_send_data(f.msg, f.len, f.dest);
}

EVENT_HANDLER(reboot_node)
{
	link_init();
	CHECK(CNET_set_handler(EV_APPLICATIONREADY, app_rdy, 0));
	
	
	if(nodeinfo.nodenumber == 0)
		CNET_enable_application(ALLNODES);
}
*/

/** RECEIVED DATA **/

/* received data frames should be passed to:
 *  net_recv( char * msg, int len, CnetAddr from);
 *
 * received info frames should be passed to: 
 * 	void oracle_recv(char * msg, int len, CnetAddr from);
 */
