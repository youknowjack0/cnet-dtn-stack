/* protocol initialisation */
#include "dtn.h"
#include <cnet.h>
#include <stdlib.h>
#include <string.h>

#include "mapping.h"
#include "walking.h"

/*
 * talk every 3 seconds
 */
#define	TALK_FREQUENCY		3000000	
#define	EV_TALKING		EV_TIMER8
#define MAXMESSAGE nodeinfo.maxmessagesize

static WLANRESULT my_WLAN_model(WLANSIGNAL *sig);

EVENT_HANDLER(start_sending)
{

}

void message_receive(char* data, int len, CnetAddr sender)
{
		size_t msglen = len;
		CHECK(CNET_write_application(data, &msglen));
}

EVENT_HANDLER(app_rdy)
{
		char msg[MAXMESSAGE];
		int dest;
		size_t len = MAXMESSAGE;
		CHECK(CNET_read_application(&dest, &msg, &len));
		transport_datagram(msg, len, dest);
}

EVENT_HANDLER(reboot_node)
{
		char    **argv  = (char **)data;

		/*
		 * ENSURE THAT WE'RE RUNNING THE CORRECT VERSION OF cnet
		 */
		CNET_check_version(CNET_VERSION);

		/*
		 * ENSURE THAT WE HAVE A MAPFILE
		 */
		if(argv[0]) {

				/*
				 * WE REQUIRE EACH NODE TO HAVE A DIFFERENT 
				 * STREAM OF RANDOM NUMBERS
				 */
				CNET_srand(nodeinfo.time_of_day.sec + nodeinfo.nodenumber);

				/*
				 * READ AND DRAW THE MAP
				 */
				readmap(argv[0]);

				CHECK(CNET_set_handler(EV_APPLICATIONREADY, app_rdy, 0));
				CHECK(CNET_set_handler(EV_TIMER6, start_sending, 0));

				/*
				 * START LAYERS
				 */
				link_init();
				transport_init();
				net_init();
				oracle_init();

				/*
				 * START WALKING
				 */
				init_walking();
				start_walking();

				/*
				 * PREPARE TO TALK VIA OUR WIRELESS CONNECTION
				 */
				CNET_set_wlan_model( my_WLAN_model );
				CNET_start_timer(EV_TALKING, TALK_FREQUENCY, 0);
				CHECK(CNET_enable_application(ALLNODES));
				CNET_start_timer(EV_TIMER6, 10*ORACLEINTERVAL, 0);
		}
}


//  =======================================================================

//  THERE'S NO NEED TO COMPREHEND NOR CHANGE THIS FUNCTION
#define	SIGNAL_LOSS_PER_OBJECT		12.0		// dBm

static WLANRESULT my_WLAN_model(WLANSIGNAL *sig)
{
		int		dx, dy;
		double	metres;
		double	TXtotal, FSL, budget;

		//  CALCULATE THE TOTAL OUTPUT POWER LEAVING TRANSMITTER
		TXtotal	= sig->tx_info->tx_power_dBm - sig->tx_info->tx_cable_loss_dBm +
				sig->tx_info->tx_antenna_gain_dBi;

		//  CALCULATE THE DISTANCE TO THE DESTINATION NODE
		dx		= (sig->tx_pos.x - sig->rx_pos.x);
		dy		= (sig->tx_pos.y - sig->rx_pos.y);
		metres	= sqrt((double)(dx*dx + dy*dy)) + 0.1;	// just 2D

		//  CALCULATE THE FREE-SPACE-LOSS OVER THIS DISTANCE
		FSL		= (92.467 + 20.0*log10(sig->tx_info->frequency_GHz)) +
				20.0*log10(metres/1000.0);

		//  CALCULATE THE SIGNAL STRENGTH ARRIVING AT RECEIVER
		sig->rx_strength_dBm = TXtotal - FSL +
				sig->rx_info->rx_antenna_gain_dBi - 
				sig->rx_info->rx_cable_loss_dBm;

		//  DEGRAGDE THE WIRELESS SIGNAL BASED ON THE NUMBER OF OBJECTS IT HITS
		int	nobjects = through_N_objects(sig->tx_pos, sig->rx_pos);

		sig->rx_strength_dBm -= (nobjects * SIGNAL_LOSS_PER_OBJECT);

		//  CAN THE RECEIVER DETECT THIS SIGNAL AT ALL?
		budget	= sig->rx_strength_dBm - sig->rx_info->rx_sensitivity_dBm;
		if(budget < 0.0)
				return(WLAN_TOOWEAK);

		//  CAN THE RECEIVER DECODE THIS SIGNAL?
		return (budget < sig->rx_info->rx_signal_to_noise_dBm) ?
				WLAN_TOONOISY : WLAN_RECEIVED;
}
