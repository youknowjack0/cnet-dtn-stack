/* this file gathers information from neighbour nodes in order
 * to estimate the topology of the network 
 */
#include "dtn.h"

/* TODO: define an oracle info packet structure */

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
