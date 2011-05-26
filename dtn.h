#include <cnet.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* some constants here, such as maximum frame lengths */
#define ORACLEINTERVAL 3000000 /* oracle broadcast interval in microseconds */
#define ORACLEWAIT (ORACLEINTERVAL*2) /* time a neighbour will be 'live' after a beacon */
#define MINDIST 2
/* This is the maximum size of the PAYLOAD of a datagram, not the datagram including the header! */

#define MAX_FRAME_SIZE WLAN_MAXDATA /* TODO: What is this actually? All other max sizes are based on this. */

typedef enum 
{
	DL_DATA, DL_BEACON, DL_RTS, DL_CTS, DL_ACK
} FRAMETYPE;

typedef struct
{
	FRAMETYPE	type;
	int		dest;	// Zero if beacon, so broadcasted
	int 		src;
	size_t		len;
	uint32_t	checksum;
} FRAMEHEADER;


/* These are used by the link layer. */
#define FRAME_HEADER_SIZE sizeof(FRAMEHEADER)
#define MAX_PACKET_SIZE (MAX_FRAME_SIZE - FRAME_HEADER_SIZE)

/* 
 * network packet structure 
 */
typedef struct 
{
	CnetAddr source;
	CnetAddr dest;
	/*
	 * length of msg 
	 */
	int len;

} PACKETHEADER;

/* These are used by the network layer */
#define PACKET_HEADER_SIZE (sizeof(PACKETHEADER))
#define MAX_DATAGRAM_SIZE (MAX_PACKET_SIZE - PACKET_HEADER_SIZE) 

typedef struct {
	PACKETHEADER h;
	char msg[MAX_DATAGRAM_SIZE];
} PACKET;

/*
 **************************************************
 * The datagram structure.			  *
 * This is the header and message that gets sent  *
 * from the transport layer (this) to the network *
 * layer.					  *
 **************************************************
* 
 * Data received from the network layer should be
 * of this type.
 *
 * Messages down from the application layer need
 * to be reduced to fragments of legal size and
 * then added to this header before they are sent
 * to the network layer.
 */
typedef struct
{
	uint32_t checksum;
	/* the size of msg_frag */
	uint32_t msg_size;
	/* the original sender */
	int source;
	/* the serial number on the message */
	int msg_num;
	/* the sequence number of this fragment within the message */
	int frag_num;
	/* the number of fragments in this message */
	int frag_count;
} DATAGRAMHEADER;

/* These are used by the transport layer */
#define DATAGRAM_HEADER_SIZE (sizeof(DATAGRAMHEADER))
#define MAX_FRAGMENT_SIZE ((MAX_DATAGRAM_SIZE - DATAGRAM_HEADER_SIZE))

typedef struct 
{
	DATAGRAMHEADER h;
	/* the message fragment */
	char msg_frag[MAX_FRAGMENT_SIZE];
} DATAGRAM;


#define LOGDIR "./dtnlog"

/* link.c */

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
void net_send_buffered();

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



