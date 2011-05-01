/* this file handles data link layer functions, including:
 *  - CSMA/CA with binary exponential backoff
 *  - buffers and retries data to be sent
 *  - passes received data frames to the appropriate handlers
 *  - manages address resolution and maintains an address resolution cache
 */
#include "dtn.h"

/* TODO: define a frame structure */

/* returns max # of bytes of data that link_send_data or link_send_info
 * will currently accept */
int get_nbytes_writeable();

/* send data msg of length len to receiver recv 
 */
void link_send_data( char * msg, int len, CnetAddr recv) {
};

/* send info msg of length len to receiver recv
 *
 * Note: This function performs the exact same action as
 * link_send_data, but the frames should be marked as 
 * being info types so the receiver knows to pass them to
 * the oracle
 */
void link_send_info( char * msg, int len, CnetAddr recv) {
}

/* called on program initialisation */
void link_init() {
	/* set mac address */
	/* register CNET handlers for physical ready */
}

/** RECEIVED DATA **/

/* received data frames should be passed to:
 *  net_recv( char * msg, int len, CnetAddr from);
 *
 * received info frames should be passed to: 
 * 	void oracle_recv(char * msg, int len, CnetAddr from);
 */


