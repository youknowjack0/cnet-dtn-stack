/* protocol initialisation */
#include "dtn.h"

EVENT_HANDLER(reboot_node)
{
	link_init();
	net_init();
	oracle_init();
	transport_init();
}
