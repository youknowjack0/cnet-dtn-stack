/* this file has functions which perform the responsibilities
 * of the network layer.
 * In a DTN network, these responsibilities include:
 *  - buffering transitory data from other hosts
 *  - buffering data from this host which is intended
 *    for other hosts
 *  - manage the buffers, shedding load when required
 *  - data which is originating from this host should
 *    be treated separately, so that other hosts cannot
 *    monopolise the reosurces of this host. Two packet
 *    queues should be maintained, one for data originating
 *    from this host, and one for data from other hosts.
 */
#include "dtn.h"

/* TODO: define a network packet structure */

/* amount of space for messages in the public buffer 
 * (that is, messages originating from other hosts)
 */
int get_public_nbytes_free() {
	return 0;
}

/* amount of space for messages in the private buffer
 * (that is, the buffer for messages originating from
 * this host)
 */
int get_private_nbytes_free() {
	return 0;
}

/* Send data msg length len to destination dst.
 * This function is called from the transport layer
 * return false on some error
 */
bool net_send( char * msg, int len, CnetAddr dst) {
	/* call get_nth_best_node and try send the data there,
	 * or buffer it if there is no good node, or if the 
	 * data link layer buffers are full (i.e. the medium
	 * cannot accommodate more traffic)
	 *
	 * Note: get_nth_best_node currently only finds the best
	 * node, so calling on anything other than the 0th node
	 * will return false
	 *
	 */
	return false;
}

/* Handle data message length len to destination dst.
 * This function is called from the data link layer.
 *
 * Note: If this message has arrived at its destination
 * (current host == dst) this simply needs to pass the message
 * to the transport layer.
 *
 * If the destination is another host, then some other action
 * should be taken to try get it there
 */
void net_recv( char * msg, int len, CnetAddr dst) {

}

/* called at program start */
void net_init() {
	/* initialise message queues, etc... */
}



