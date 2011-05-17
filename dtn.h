#include <cnet.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* some constants here, such as maximum frame lengths */
#define ORACLEINTERVAL 3000000 /* oracle broadcast interval in microseconds */
#define ORACLEWAIT (ORACLEINTERVAL*2) /* time a neighbour will be 'live' after a beacon */
#define MINDIST 2
#define MAX_DATAGRAM_SIZE 50000 /* TODO: max datagram size in bytes */
#define MAXMESSAGESIZE 48699 /* TODO */
#define NUM_NODES 10 /* TODO: this needs to be accurate, since fakeapp will use it to choose a dest */
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
