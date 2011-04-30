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

/* structure to represent node and location */
typedef struct {
	CnetAddress addr;
	CnetLocation loc;
} NodeLocation;

/* the packet structure for oracle information transmission */
typedef struct {
	uint32_t checksum; /*crc32 checksum of the oraclepacket including 'locations' payload */
	NodeLocation senderLocation;
	uint32_t freeBufferSpace; /* how many bytes of space available in transmitting nodes' public buffer */
	uint16_t locationsSize; /* how many elements in locations */
	NodeLocation * locations; /* array of (last known) locations of known hosts */	
} OraclePacket;

/* structure to store information about neighbours */
typedef struct {
	NodeLocation nl;
	uint32_t freeBufferSpace;
	uint64_t lastBeacon; /* when did we last see a bacon from this noodle */
} Neighbour;

//last known addresses for all known nodes
Neighbour * positionDB;
int dbsize;

static int compareNL(const void * key, const void * elem) {
	return (int)(key) - (int)(((Neighbour)(elem))->NodeLocation.addr);
}

/* add a position to the positionDB
 * maintaining db sortedness
 * or update the existing position if the address
 * already exists
 */
static void savePosition(NodeLocation n) {
	CnetAddress * np = &(n.addr);
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
		np->loc = n.loc;
	}
}

/* broadcast info about this node and other known nodes
 */
static void sendOracleBeacon() {

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
	nbp->lastBeacon = 69; //TODO: Timestamp here
	nbp->freeBufferSpace = p->freeBufferSpace;
}

/* find the last known position of a node (a), sets l
 * equal to the last known position
 * note: location (l) will be set to NULL if there is no
 * last known position for l.
 */
static void queryPosition(CnetLocation * l, CnetAddress a) {
	
}


/* Return the nth best intermediate node by which the message
 * to dest should be delivered.
 * n is indexed from 0
 *
 * Returns NULL if there is no best node (i.e. it is best to
 * buffer the message), otherwise returns the CnetAddr of the 
 * best intermediate node.
 *
 * Note: this function should only return the address of a node
 * which is currently reachable. So if there are no good nodes in
 * range, the function should return NULL until a good node comes
 * in range.
 *
 */
CnetAddr get_nth_best_node(int n, CnetAddr dest) {
	return NULL;
}

/* Messages from other nodes which use link_send_info will
 * be passed up to here from the data link layer 
 */
void oracle_recv(char * msg, int len, CnetAddr rcv) {
	/* parse info from other nodes to estimate topology */
}

/* function is called on program intialisation 
 */
void oracle_init() {
	/* schedule periodic transmission of topology information, or whatever */		
}
