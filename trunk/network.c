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

/* The size of the buffer for this layer */
#define NETWORK_BUFF_SIZE 1000000



struct STACK_EL 
{
	PACKET* p;
	struct STACK_EL* down;
	struct STACK_EL* up;
};

typedef struct 
{
	struct STACK_EL* top;
	struct STACK_EL* bottom;
} STACK;


static int free_bytes;
static STACK* buff;


int get_public_nbytes_free() 
{
	return free_bytes;
}

int get_private_nbytes_free() 
{
	return free_bytes;
}

static STACK* new_stack() 
{
	STACK* s = malloc(sizeof(STACK));
	struct STACK_EL* el = malloc(sizeof(struct STACK_EL));
/*
	el->up = NULL;
	el->down = NULL;
	el->p = NULL;
	s->top = el;
	s->bottom = el;
*/
	s->top = NULL;
	s->bottom = NULL;
	return s;
}


/*
 * returns true if a stack is empty 
 */
static bool is_empty(STACK* s) 
{
	return ((s->top == NULL) || (s->bottom == NULL));
}


/*
 * delete a packet from the bottom of the stack 
 */
static PACKET* dequeue(STACK* s) 
{
	if(is_empty(s)) 
	{
		return NULL;
	}
	else 
	{
		QUEUE_EL* del = s->bottom;
		PACKET* tmp = del->p;
		s->bottom = del->up;
		free(del);
		if(s->bottom != NULL)
		{
			s->bottom->down = NULL;
		}
		free_bytes += (sizeof(struct STACK_EL) + PACKET_HEADER_SIZE + tmp->h.len);
		return tmp;
	}
}

/*
 * push a packet onto a stack 
 */
static void push(STACK* s, PACKET* pack) 
{
	int mem_used = (sizeof(struct STACK_EL) + PACKET_HEADER_SIZE + pack->h.len);

	while(get_public_nbytes_free() < mem_used) 
	{
		free(dequeue(s));
	}

	struct STACK_EL* e = malloc(sizeof(struct STACK_EL));
	e->p = pack;
	e->down = s->top;
	e->up = NULL;
	if(is_empty(s))
	{
		s->bottom = e;
	}
	else
	{
		s->top->up = e;
	}
	s->top = e;
	free_bytes -= mem_used;
}


/*
 * remove a packet from a stack 
 */
static PACKET* pop(STACK* s) 
{
	if (is_empty(s)) 
	{
		return NULL;
	}
	else 
	{
		struct STACK_EL* tmp = s->top;
		PACKET* ret = tmp->p;	
		s->top = tmp->down;
		free(tmp);
		if(s->top != NULL)
		{
			s->top->up = NULL;
		}
		free_bytes += (sizeof(struct STACK_EL) + PACKET_HEADER_SIZE + tmp->h.len);
		return ret;
	}
}

/*
 * returns but does not remove the top of a stack
 * NOT USED
 *
 static packet* peek(STACK* s) 
 {
 if (is_empty(s)) 
 {
   return NULL;
   }
   else 
   {
   return s->top->p;
   }
   }
 *
 */

/*
 * Use the oracle to find the best link on which
 * to forward the message. If such a link does exist, send
 * it to the data link layer and free the memory 
 * (free(pack)). If no such link exists
 * then buffer pack
 */
static void try_to_send(PACKET* pack, STACK* s) 
{
	printf("Node%d Network: Trying to send\n", nodeinfo.nodenumber);
	int mem_used = PACKET_HEADER_SIZE + pack->h.len;
	CnetAddr add_p;
	bool can_send = get_nth_best_node(&add_p, 0, pack->h.dest, mem_used);

	if (can_send) 
	{
		/*
		 * send it to the DLL and free the memory (free(pack)).
		 * At this point, I'm not doing anything about checking 
		 * the DLL buffer.
		 * TODO: check the DLL buffer
		 */
		printf("Node%d Network: Can send, to %d\n", nodeinfo.nodenumber, add_p);
		/* todo: permanent solution */
		if(mem_used <= MAX_PACKET_SIZE) {
			link_send_data((char*) pack, mem_used, add_p);
			free(pack);
		} else {
			printf("tried to send overlarge packet!");
		}
	}
	else 
	{
		/* 
		 * buffer it.
		 */
		printf("Node%d Network: Couldn't send buffering\n", nodeinfo.nodenumber);
		push(s, pack);
	}
}

/*
 * Called by oracle when topology data is updated (beacon is received)
 */
void net_send_buffered() 
{
	/*
	 * Attempt to send buffered messages. Use a temporary stack to 
	 * store packets that are popped but can't be sent. Then, when
	 * when we're done, simply push the whole temporary stack back
	 * onto the buffer. Free the memory for any packets that are sent.
	 */
	printf("Node%d Network: Clearing buffer\n", nodeinfo.nodenumber);
	STACK* temp_stack = new_stack();

	PACKET* tmp = pop(buff);
	while(tmp != NULL) 
	{
		printf("Attempting to send old packet to %d\n", tmp->h.dest);
		try_to_send(tmp, temp_stack);
		tmp = pop(buff);
	}

	tmp = pop(temp_stack);
	while(tmp != NULL) 
	{
		push(buff, tmp);
		tmp = pop(temp_stack);
	}

	free(temp_stack);
}

/*
 * Send data msg length len to destination dst.
 * This function is called from the transport layer
 * return false on some error
 */
bool net_send(char* msg, int len, CnetAddr dst) 
{
	/*
	 * call get_nth_best_node and try send the data there,
	 * or buffer it if there is no good node, or if the 
	 * data link layer buffers are full (i.e. the medium
	 * cannot accommodate more traffic)
	 *
	 * Note: get_nth_best_node currently only finds the best
	 * node, so calling on anything other than the 0th node
	 * will return false
	 *
	 */
	//net_send_buffered();
	printf("Node %d Network: got msg with dest = %d\n", nodeinfo.nodenumber, dst);
	int mem_used = PACKET_HEADER_SIZE + len;
	PACKET* pack = malloc(mem_used);
	pack->h.source = nodeinfo.nodenumber;
	pack->h.dest = dst;
	pack->h.len = len;
	memcpy(pack->msg, msg, len);
	printf("Node %d Network: copied message\n",nodeinfo.nodenumber);

	/*
	 * attempt to send message. if it can not be sent, buffer
	 * it on buff 
	 */
	try_to_send(pack, buff);
	/*
	 * TODO: something about the return value, I guess 
	 */
	return true;
}

/*
 * Handle data message length len to destination dst.
 * This function is called from the data link layerr
 *
 * The data link layer header has been stripped, so
 * just cast *msg to a packet.
 *
 * Note: If this message has arrived at its destination
 * (pack->dest == nodeinfo.nodenumber)this simply needs 
 * to strip the network layer header and pass the message 
 * to the transport layer.
 *
 * If the destination is another host try to send it or 
 * buffer it.
 */
/*
 * TODO: check that len is the actual length of msg (i.e. the packet)
 */
void net_recv(char* msg, int len, CnetAddr src) 
{
	/*
	 * in this case, the message received from the DLL is just a
	 * packet. The size of the packet is len
	 */
	printf("Node %d: net_recv\n", nodeinfo.nodenumber);
	int mem_used = len;
	PACKET* pack = malloc(mem_used);
	memcpy(pack, msg, mem_used);
	/*
	 * if the destination is this node
	 */
	if(nodeinfo.nodenumber == pack->h.dest) 
	{
		/*
		 * pass it right on up to the transport layer.
		 * free the memory (free(pack)).
		 */
		transport_recv(pack->msg, pack->h.len, pack->h.source);
		free(pack);
	}
	else 
	{
		/*
		 * attempt to send message. if it can not be sent, buffer
		 * it on buff
		 */
		try_to_send(pack, buff);
	}
}



/*
 * called at program start
 */
void net_init() 
{
	free_bytes = NETWORK_BUFF_SIZE;
	buff = 	new_stack();
}
