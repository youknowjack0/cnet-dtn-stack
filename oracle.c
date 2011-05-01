/* this file gathers information from neighbour nodes in order
 * to estimate the topology of the network.
 *
 * All nodes transmit their geographical positions, as well 
 * as the positions of all nodes which they have had contact with (ever). 
 * This transmission takes place on a regular timed interval.
 * 
 * Nodes also transmit their own available public buffer space,
 * but do not retransmit this information.
 *
 * Routing is non-flooding, packets are forwarded only once.
 *
 * Packets are routed to any node which is closer than itself to the
 * last known position of the destination node. If no position
 * is known for the destination node, the packet is not forwarded.
 * The exception being, if a neighbour node's buffers are full,
 * then the packet will not be forwarded to that neighbour.
 *
 */
#include "dtn.h"
#include <stdlib.h>

/* todo: globalize this, make it a correct number */
#define MAXNODES 1000

/* structure to represent node and location */
typedef struct {
	CnetAddr addr;
	CnetPosition loc;
} NodeLocation;

/* the packet structure for oracle information transmission */
typedef struct {
	uint32_t checksum; /*crc32 checksum of the oraclepacket including 'locations' payload */
	NodeLocation senderLocation;
	uint32_t freeBufferSpace; /* how many bytes of space available in transmitting nodes' public buffer */
	uint16_t locationsSize; /* how many elements in locations */
	NodeLocation locations[MAXNODES]; /* array of (last known) locations of known hosts */	
} OraclePacket;

/* structure to store information about neighbours */
/* TODO: store distance to this neighbour here to speed up computation */
typedef struct {
	NodeLocation nl;
	uint32_t freeBufferSpace;
	uint64_t lastBeacon; /* when did we last see a bacon from this noodle */
} Neighbour;

//last known addresses for all known nodes
static Neighbour * positionDB;
static int dbsize;

static int compareNL(const void * key, const void * elem) {
	uint32_t k = *((uint32_t *)key);
	uint32_t addr = (uint32_t)(((Neighbour *)(elem))->nl.addr);
	return k-addr ;
}

/* add a position to the positionDB
 * maintaining db sortedness
 * or update the existing position if the address
 * already exists
 */
static void savePosition(NodeLocation n) {
	CnetAddr * np = &(n.addr);
	Neighbour * nbp  = bsearch(np, positionDB, dbsize, sizeof(Neighbour), compareNL);
	if(nbp == NULL) {
		//append and sort
		dbsize++;
		positionDB = realloc(positionDB, sizeof(Neighbour)*dbsize);
		positionDB[dbsize-1].nl = n;
		positionDB[dbsize-1].lastBeacon = 0; 
		qsort(positionDB, dbsize, sizeof(Neighbour), compareNL);
	} else {
		//update
		nbp->nl.loc = n.loc;
	}
}

/* checksum an oracle packet, return the result
 * crc32
 */
static uint32_t checksum_oracle_packet(OraclePacket * p) {
	/* TODO: might need to deal with struct field offset? */
	return CNET_crc32((unsigned char*)p + sizeof(p->checksum), sizeof(p) - sizeof(p->locations) + sizeof(NodeLocation)*p->locationsSize - sizeof(p->checksum));
}

/* broadcast info about this node and other known nodes
 */
static void sendOracleBeacon() {
	/* possible todo: restructure the Neighbour data so that this can be done with one memcpy */
	OraclePacket p;
	for(int i=0;i<dbsize;i++) {
		p.locations[i] = positionDB[i].nl;
	}
	p.freeBufferSpace = get_public_nbytes_free();
	p.locationsSize = dbsize;
	p.senderLocation.addr = nodeinfo.address;
	CnetPosition loc;
	CNET_get_position(&loc, NULL);
	p.senderLocation.loc = loc;	
	char * pp = (char *)(&(p));	
	p.checksum = checksum_oracle_packet(&p);
	/* todo: macro for the packet header */
	link_send_info(pp, sizeof(p) - sizeof(p.locations) + sizeof(NodeLocation)*dbsize, ALLNODES);

	/* send again later */
	CNET_start_timer(EV_TIMER7, (CnetTime)ORACLEINTERVAL, 0);
}

/* process an oracle packet and update local knowledge
 * database
 */
static void processBeacon(OraclePacket * p) {
	for(int i=0;i<p->locationsSize;i++) {
		if((int)p->locations[i].addr != (int)nodeinfo.address) /* if not THIS node */
		savePosition(p->locations[i]);	
	}
	
	/* save some near-neighbour specific info */
	savePosition(p->senderLocation);
	Neighbour * nbp = bsearch(&(p->senderLocation.addr), positionDB, dbsize, sizeof(Neighbour), compareNL);
	nbp->lastBeacon = nodeinfo.time_in_usec; 
	nbp->freeBufferSpace = p->freeBufferSpace;
}

/* find the last known position of a node (a), sets l
 * equal to the last known position
 * note: location (l) will be set to NULL if there is no
 * last known position for l.
 */
static void queryPosition(CnetPosition * l, CnetAddr a) {
	Neighbour * nbp = bsearch(&a, positionDB, dbsize, sizeof(Neighbour), compareNL);
	if(nbp==NULL) {
		l = NULL;
	} else {
		*l = nbp->nl.loc;
	}
}

/* returns true iff: 
 * 	a->c > b->c
 * 	by some interval defined in dtn.h
 */
bool isCloser(CnetPosition a, CnetPosition b, CnetPosition c, int interval) {
	int cax = c.x - a.x;
	int cay = c.y - a.y;
	int cbx = c.x - b.x;
	int cby = c.y - b.y;
	if( cbx*cbx + cby*cby + interval*interval < cax*cax + cay*cay )
		return true;
	else 
		return false;
}

/* Sets ptr to the best intermediate node by which a messag
 * to dest should be delivered.
 * n is indexed from 0
 *
 * Returns false if there is no best node (i.e. it is best to
 * buffer the message), otherwise returns true
 *
 * This function will only 'recommend' a node which is in range and
 * has buffer space to transmit. 
 *
 * Note: function will currently recommend broadcasting to ANY node
 * which causes an improvement, rather than greedily trying to find
 * the BEST node.
 *
 */
bool get_nth_best_node(CnetAddr * ptr, int n, CnetAddr dest, size_t message_size) {
	CnetTime t = nodeinfo.time_in_usec;
	if(n!=0) return false;
	else {
		for(int i=0; i<dbsize;i++) {
			if(t > positionDB[i].lastBeacon + ORACLEWAIT ) continue; /* skip this neighbour if we haven't had a beacon from it recently */ 
			if((int)positionDB[i].freeBufferSpace < message_size) continue; /* enough buffer space for this massage */
			CnetPosition destPos; queryPosition(&destPos, dest); 
			CnetPosition nextPos = positionDB[i].nl.loc;
			CnetPosition myPos; CNET_get_position(&myPos, NULL);	
		 	if( isCloser(myPos, nextPos, destPos, MINDIST) ) {
				*ptr = positionDB[i].nl.addr; 
				return true; 
			}
		}
	}
	return false;

}

/* Messages from other nodes which use link_send_info will
 * be passed up to here from the data link layer 
 */
void oracle_recv(char * msg, int len, CnetAddr rcv) {
	/* parse info from other nodes to estimate topology */
	OraclePacket * p = (OraclePacket *) msg;
	if(checksum_oracle_packet(p)==p->checksum) {
		processBeacon(p);
	}
}

/* function is called on program intialisation 
 */
void oracle_init() {
	CNET_srand(nodeinfo.time_of_day.sec + nodeinfo.nodenumber);
	/* schedule periodic transmission of topology information, or whatever */		
	CNET_set_handler(EV_TIMER7, sendOracleBeacon, 0);
	CNET_start_timer(EV_TIMER7, (CnetTime)((CNET_rand() % ORACLEINTERVAL)), 0);
}
