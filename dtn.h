#include <cnet.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* some constants here, such as maximum frame lengths */
#define ORACLEINTERVAL 6000000 /* oracle broadcast interval in microseconds */
#define ORACLEWAIT (ORACLEINTERVAL*2) /* time a neighbour will be 'live' after a beacon */
#define MINDIST 2
/* This is the maximum size of the PAYLOAD of a datagram, not the datagram including the header! */

#define MAX_FRAME_SIZE WLAN_MAXDATA /* TODO: What is this actually? All other max sizes are based on this. */

/* These are used by the link layer. */
#define FRAME_HEADER_SIZE (sizeof(FRAMETYPE) + (3 * sizeof(int)) + sizeof(size_t))
#define MAX_PACKET_SIZE (MAX_FRAME_SIZE - FRAME_HEADER_SIZE)

/* These are used by the network layer */
#define PACKET_HEADER_SIZE ((2 * sizeof(CnetAddr)) + sizeof(int))
#define MAX_DATAGRAM_SIZE (MAX_PACKET_SIZE - PACKET_HEADER_SIZE) 

/* These are used by the transport layer */
#define DATAGRAM_HEADER_SIZE ((2 * sizeof(uint32_t)) + (4 * sizeof(int)))
#define MAX_FRAGMENT_SIZE (MAX_DATAGRAM_SIZE - DATAGRAM_HEADER_SIZE)

/* TODO: this needs to go! */
#define NUM_NODES 10 

#define LOGDIR "./dtnlog"

/* link.c */
/* I don't know how to do a prototype of an enum, so I just moved the whole thing here */
typedef enum 
{
	DL_DATA, DL_BEACON, DL_RTS, DL_CTS, DL_ACK
} FRAMETYPE;

int get_nbytes_writeable();
void link_send_data( char * msg, int len, CnetAddr recv);
void link_send_info( char * msg, int len, CnetAddr recv);
void link_init();

/* network.c */
int get_public_nbytes_free();
int get_private_nbytes_free();
bool net_send( char * msg, int len, CnetAddr dst);
void net_recv( char * msg, int len, CnetAddr dst);
void net_init();

/* oracle.c */
bool get_nth_best_node(CnetAddr * ptr, int n, CnetAddr dest, size_t messageSize);
void oracle_recv(char * msg, int len, CnetAddr rcv);
void oracle_init();

/* transport.c */
void transport_recv(char * msg, int len, CnetAddr sender);
void transport_datagram(char * msg, int len, CnetAddr destination);
void transport_init();

/* fakeapp.c */
void generate_message();
void receive_message(char* data, int len, CnetAddr sender);

/*dtn.c*/
void message_receive(char* data, int len, CnetAddr sender);



