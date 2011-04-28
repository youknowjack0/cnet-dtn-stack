/* protocol initialisation */
#include "dtn.h"

EVENT_HANDLER(reboot_node)
{
	link_init();
	newtork_init();
	oracle_init();
	transport_init();
}
