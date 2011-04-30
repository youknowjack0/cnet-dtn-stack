#include <cnet.h>

/* some constants here, such as maximum frame lengths */
#define ORACLEINTERVAL 3000000 /* oracle broadcast interval in microseconds */
#define ORACLEWAIT (ORACLEINTERVAL*2) /* time a neighbour will be 'live' after a beacon */
#define MINDIST 2;

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
CnetAddr get_nth_best_node(int n, CnetAddr dest);
void oracle_recv(char * msg, int len, CnetAddr rcv);
void oracle_init();

/* transport.c */
void transport_recv(char * msg, int len);
void tranpsort_init();
