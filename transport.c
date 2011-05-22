/* provide a datagram service to the application layer 
 * handles checksums of data
 */
#include "dtn.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>


/*
 * The datagram structure.
 * This is the header and message that gets sent
 * from the transport layer (this) to the network
 * layer.
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
	/* the message fragment */
	char msg_frag[MAX_DATAGRAM_SIZE];
} DATAGRAM;

/*
 * STRUCTURES FOR THE QUEUE
 */
/*
 * A structure for the elements of the TRANSQUEUE.
 */
struct QUEUE_EL
{
	int num_frags_needed;
	int num_frags_gotten;
	int key;
	DATAGRAM* frags;
	struct QUEUE_EL* down;
	struct QUEUE_EL* up;
};


/* A queue structure that holds arrays of datagrams, one array per 
 * message. Each element also contains information about how many
 * fragments are in its message and how many fragments have been
 * received so far.
 *
 * At this point I haven't done anything about duplicate datagrams.
 * TODO: will they be an issue?
 *
 * Rebuilding might be made easier by  quicksorting the array before
 * reassembly?
 */
typedef struct 
{
	struct QUEUE_EL* top;
	struct QUEUE_EL* bottom;
} TRANSQUEUE;

/*
 * END QUEUE STRUCTURES
 */


/*
 * STRUCTURES FOR THE RED-BLACK BINARY SEARCH TREE
 * This tree is used as a map from keys (concatenation
 * of original source host and message number) to
 * elements in the queue.
 */

/*
 * The possible colors of elements in the red-black tree
 */
typedef enum
{
	RED, BLACK
} RED_OR_BLACK;


/*
 * A structure for the elements of the red-black tree
 */
struct RB_TREE_NODE
{
	struct RB_TREE_NODE* parent;
	struct RB_TREE_NODE* left_child;
	struct RB_TREE_NODE* right_child;
	int key;
	struct QUEUE_EL* value;
	RED_OR_BLACK col;
	bool is_leaf;
};

/*
 * A red-black tree structure
 */
typedef struct
{
	struct RB_TREE_NODE* root;
} RED_BLACK_TREE;

/*
 * END STRUCTURES FOR RED-BLACK TREE
 */

/*
 * GLOBAL VARIABLE DECLARATIONS
 */
/*
 * counter for the serial numbers of messages
 */
static int msg_num_counter;

/*
 * The layer's buffer
 */
static TRANSQUEUE* buff;

/*
 * The red-black tree based map
 */
static RED_BLACK_TREE* tree_map;

/*
 * END GLOBAL VARIABLES
 */

/*
 * RED-BLACK TREE FUNCTIONS
 */

/*
 * concatenates two ints to make one int
 */
static int make_key(int src, int message_num)
{
	char* tmp = malloc(50 * sizeof(char));
	sprintf(tmp, "%d%d", src, message_num);
	int ret = atoi(tmp);
	return ret;
}

/*
 * Make a new red-black tree
 */
static RED_BLACK_TREE* new_red_black_tree()
{
	RED_BLACK_TREE* t = malloc(sizeof(RED_BLACK_TREE));
	t->root = NULL;
	return t;
}

/*
 * Looks up the adress of the entry for the message in the
 * queue
 */
static struct QUEUE_EL* tree_get_entry(struct RB_TREE_NODE* node, int key)
{
	if(node == NULL)
	{
		return NULL;
	}
	else if(node->is_leaf == true)
	{
		return NULL;
	}
	else if(node->key == key)
	{
		return node->value;
	}
	else if(key < node->key)
	{
		return tree_get_entry(node->left_child, key);
	}
	else
	{
		return tree_get_entry(node->right_child, key);
	}
}

/*
 * Looks up the adress of the entry for the message in the
 * queue
 */
static struct QUEUE_EL* tree_get(RED_BLACK_TREE* tree, int key)
{
	return tree_get_entry(tree->root, key);
}

/*
 * return true if there is an entry in the buffer for the
 * message, else return false.
 */
static bool tree_has_entry(RED_BLACK_TREE* tree, int key)
{
	return (tree_get(tree, key) != NULL);
}

/*
 * Returns the grandparent of a node in a red-black tree
 */
static struct RB_TREE_NODE* get_grandparent(struct RB_TREE_NODE* node)
{
	if((node != NULL) && (node->parent != NULL))
		return node->parent->parent;
	else
		return NULL;
}

/*
 * Return the sibling of the parent of a node in a red-black tree
 */
static struct RB_TREE_NODE* get_uncle(struct RB_TREE_NODE* node)
{
	struct RB_TREE_NODE* grand = get_grandparent(node);
	if(grand == NULL)
		return NULL;
	else if(grand->left_child == node->parent)
		return grand->right_child;
	else
		return grand->left_child;
}	

/*
 * Left tree rotation
 */
static void rotate_left(struct RB_TREE_NODE* root)
{
	struct RB_TREE_NODE* q = root->right_child;
	/*
	 * Make q the new root
	 */
	if(root->parent != NULL)
	{
		if(root->parent->right_child == root)
		{
			root->parent->right_child = q;
		}
		else
		{
			root->parent->left_child = q;
		}
	}
	q->parent = root->parent;

	/*
	 * Set original root's right child to be q's left child
	 */
	root->right_child = q->left_child;
	root->right_child->parent = root;

	/*
	 * Set q's left child to be the original root
	 */
	q->left_child = root;
	q->left_child->parent = q;
}

/*
 * Right tree rotation
 */
static void rotate_right(struct RB_TREE_NODE* root)
{
	struct RB_TREE_NODE* p = root->left_child;
	/*
	 * Make p the new root
	 */
	if(root->parent != NULL)
	{
		if(root->parent->right_child == root)
		{
			root->parent->right_child = p;
		}
		else
		{
			root->parent->left_child = p;
		}
	}
	p->parent = root->parent;

	/*
	 * Set the original root's left child to be p's right child
	 */
	root->left_child = p->right_child;
	root->left_child->parent = root;

	/*
	 * Set p's right child to be the original root
	 */
	p->right_child = root;
	p->right_child->parent = p;
}

/*
 * Declaration for dependency reasons. Defined a bit further down.
 */
static void tree_add_c1(struct RB_TREE_NODE*);

/*
 * If inserting under a red parent that has a black sibling
 */
static void tree_add_c5(struct RB_TREE_NODE* node)
{
	struct RB_TREE_NODE* grand = get_grandparent(node);

	node->parent->col = BLACK;
	grand->col = RED;
	/*
	 * If the node is the left child of the parent and the parent
	 * is the left child of the grandparent
	 */
	if((node == node->parent->left_child) && (node->parent == grand->left_child))
	{
		rotate_right(grand);
	}
	/*
	 * If the node is the right child of the parent and the parent
	 * is the right child of the grandparent
	 */
	else
	{
		rotate_left(grand);
	}
}

/*
 * If inserting under a red parent that has a black sibling
 */
static void tree_add_c4(struct RB_TREE_NODE* node)
{
	struct RB_TREE_NODE* grand = get_grandparent(node);

	/*
	 * If the node is the right child of the parent and the parent is the left child
	 * of the grandparent
	 */
	if((node == node->parent->right_child) && (node->parent == grand->left_child))
	{
		rotate_left(node->parent);
		node = node->left_child;
	}
	/*
	 * If the node is the left child of the parent and the parent is the right child of
	 * the grandparent
	 */
	else if((node == node->parent->left_child) && (node->parent == grand->right_child))
	{
		rotate_right(node->parent);
		node = node->right_child;
	}
	tree_add_c5(node);
}

/*
 * If inserting under a red parent
 */
static void tree_add_c3(struct RB_TREE_NODE* node)
{
	struct RB_TREE_NODE* uncle = get_uncle(node);
	struct RB_TREE_NODE* grand;

	/*
	 * If uncle is also red. I don't think uncle can
	 * possibly be null, but wikipedia knows all. (TODO)
	 */
	if((uncle != NULL) && (uncle->col == RED))
	{
		node->parent->col = BLACK;
		uncle->col = BLACK;
		grand = get_grandparent(node);
		grand->col = RED;
		tree_add_c1(grand);
	}
	else
	{
		tree_add_c4(node);
	}
}

/*
 * If not inserting at the root
 */
static void tree_add_c2(struct RB_TREE_NODE* node)
{
	/*
	 * If the parent is black
	 */
	if(node->parent->col == BLACK)
		return;
	else
		tree_add_c3(node);
}

/*
 * If inserting at the root
 */
static void tree_add_c1(struct RB_TREE_NODE* node)
{
	if(node->parent == NULL)
		node->col = BLACK;
	else
		tree_add_c2(node);
}	

/*
 * Makes a leaf node
 */
static void make_leaf(struct RB_TREE_NODE* l)
{
	l->key = 0;
	l->value = NULL;
	l->col = BLACK;
	l->is_leaf = true;
	l->left_child = NULL;
	l->right_child = NULL;
}

/*
 * makes a new node to be insterted into the tree
 */
static void new_tree_node(int key, struct QUEUE_EL* el, struct RB_TREE_NODE* new)
{
	new->parent = NULL;
	new->key = key;
	new->value = el;
	new->col = RED;
	new->is_leaf = false;

	struct RB_TREE_NODE* new_left = malloc(sizeof(struct RB_TREE_NODE));
	make_leaf(new_left);
	new_left->parent = new;

	struct RB_TREE_NODE* new_right = malloc(sizeof(struct RB_TREE_NODE));
	make_leaf(new_right);
	new_right->parent = new;

	new->left_child = new_left;
	new->right_child = new_right;
}

/*
 * Adds an entry to the tree
 * Pointer to pointer works because we only ever
 * insert at a leaf node, which means that there are no
 * children to be redirected.
 */
static void tree_add_entry(struct RB_TREE_NODE** root, int key, struct QUEUE_EL* el)
{

	if(*root == NULL)
	{
		struct RB_TREE_NODE* new = malloc(sizeof(struct RB_TREE_NODE));
		new_tree_node(key, el, new);

		*root = new;
		tree_add_c1(*root);
	}
	else if((*root)->is_leaf == true)
	{
		struct RB_TREE_NODE* new = malloc(sizeof(struct RB_TREE_NODE));
		new_tree_node(key, el, new);
		new->parent = (*root)->parent;
		free(*root);
		*root = new;
		tree_add_c1(*root);
	}
	else if(key < (*root)->key)
	{
		tree_add_entry(&((*root)->left_child), key, el);
	}
	else if(key > (*root)->key)
	{
		tree_add_entry(&((*root)->right_child), key, el);
	}
}

/*
 * Adds an entry to the tree
 */
static void tree_add(RED_BLACK_TREE* tree, int key, struct QUEUE_EL* el)
{
	if(tree_has_entry(tree, key) == false)
	{
		tree_add_entry(&(tree->root), key, el);
	}
}


static struct QUEUE_EL* tree_delete_entry(struct RB_TREE_NODE* node, int key)
{
	// at this stage there is no tree, so nothing to delete!
	return tree_get_entry(node, key);
}
/*
 * Deletes the entry for the message in the tree and returns
 * the address of the entry for the message in the queue.
 */
static struct QUEUE_EL* tree_delete(RED_BLACK_TREE* tree, int key)
{
	// at this stage there is no tree, so nothing to delete!
	return tree_delete_entry(tree->root, key);
}
/*
 * End search tree functions
 */
/*
 * END RED-BLACK TREE FUNCTIONS
 */



/* 
 * QUEUE FUNCTIONS
 */

/*
 * returns a pointer to a new, empty TRANSQUEUE
 */
static TRANSQUEUE* new_queue()
{
	TRANSQUEUE* q = malloc(sizeof(TRANSQUEUE));
	struct QUEUE_EL* el = malloc(sizeof(struct QUEUE_EL));
	el->num_frags_needed = 0;
	el->num_frags_gotten = 0;
	el->key = 0;
	el->up = NULL;
	el->down = NULL;
	el->frags = NULL;
	q->top = el;
	q->bottom = el;
	return q;
}

/*
 * Returns true if the queue is empty
 */
/* NOT USED _YET_
   static bool is_empty(TRANSQUEUE* q)
   {
   return ((q->top->down == NULL) || (q->bottom->up == NULL));
   }
 */

/*
 * Put a datagram on the buffer. If it's not part of a message that
 * already has an entry in the buffer, then add a new QUEUE_EL. If 
 * it is part of a message that already has an entry in the buffer, 
 * find the entry and add the datagram to the array for that message. 
 * If this datagram makes up the full message then return true. Else 
 * return false.
 */
static bool enqueue(TRANSQUEUE* q, DATAGRAM* dat)
{
	int key = make_key(dat->source, dat->msg_num);
	struct QUEUE_EL* el = tree_get(tree_map, key);
	if(el == NULL)
	{
		el = malloc(sizeof(struct QUEUE_EL));
		// just make each array element a full sized datagram. Not
		// worth worrying about.
		el->frags = malloc((dat->frag_count) * sizeof(DATAGRAM));
		el->num_frags_needed = dat->frag_count;
		el->num_frags_gotten = 0;
		el->key = make_key(dat->source, dat->msg_num);

		el->down = q->top;
		el->up = NULL;
		q->top->up = el;
		q->top = el;
		tree_add(tree_map, key, el);
	}

	memcpy(el->frags + el->num_frags_gotten++, dat, sizeof(DATAGRAM) - MAX_DATAGRAM_SIZE + dat->msg_size);
	return (el->num_frags_gotten == el->num_frags_needed);
}

/*
 * Removes the element at the front of the queue. Returns the
 * array of datagrams so that the entry for this message in the
 * binary search TREE can be REMOVED(!) (currently done internally).
 * This should (probably) only be used for dropping messages when
 * the buffer is full. Otherwise use delete().
 *
 * NOTE: On dropping messages, we may want to check that the new
 * datagram that overflows the buffer is not the datagram that
 * will complete the message we would normally drop, as that
 * would be an unneccessary waste.
 */
/* NOT USED _YET_
   static DATAGRAM* dequeue(TRANSQUEUE* q)
   {
   if(is_empty(q))
   {
   return NULL;
   }
   else
   {
   DATAGRAM* d = q->bottom->frags;
   tree_delete(tree_map, q->bottom->key);
   q->bottom = q->bottom->up;
   free(q->bottom->down);
   q->bottom->down = NULL;
   return d;
   }
   }
 */

/*
 * Finds the queue entry for a given message (identified by src
 * and messagenum), removes the entry from the queue and returns
 * the array of datagrams.
 */
static DATAGRAM* queue_delete(TRANSQUEUE* q, int src, int message_num)
{
	struct QUEUE_EL* temp = tree_delete(tree_map, make_key(src, message_num));
	if (temp == NULL)
		return NULL;
	else
	{
		if(temp->down != NULL)
		{
			temp->down->up = temp->up;
		}
		if(temp->up != NULL)
		{
			temp->up->down = temp->down;
		}

		DATAGRAM* ret = temp->frags;
		free(temp);
		return ret;
	}
}
/*
 * END QUEUE FUNCITONS
 */

static int comp(const void* one, const void* two)
{
	DATAGRAM* first = (DATAGRAM*) one;
	DATAGRAM* second = (DATAGRAM*) two;
	if(first->frag_num == second->frag_num)
	{
		return 0;
	}
	else if(first->frag_num < second->frag_num)
	{
		return -1;
	}
	else
	{
		return 1;
	}
}

/*
 * Called by the network layer.
 *
 * Buffer datagrams until all fragments have arrived, then rebuild
 * the message and pass it to the application layer. If a message
 * has only one fragment, do not buffer it.
 * 
 * If the buffer for this layer fills up, delete all fragments from 
 * the message which received its first fragment the earliest. 
 *
 * All this should be able to be achieved with a Queue of arrays, 
 * each array containing the fragments of one message, and a binary 
 * search tree that maps <msg_num, source> pairs (probably implemented 
 * by concatenating the two elements into a string)  to links in the 
 * Queue.
 */
void transport_recv(char * msg, int len, CnetAddr sender) 
{
	DATAGRAM * d = (DATAGRAM*) msg;
	/* check length*/
	if(len != (d->msg_size + sizeof(DATAGRAM) - MAX_DATAGRAM_SIZE))
		return; /* lengths don't match */

	/* check integrity */
	int sum = CNET_crc32((unsigned char *)(d) + offsetof(DATAGRAM, msg_size), len - sizeof(d->checksum));
	if(sum != d->checksum) return; /* bad checksum */

	/* pass up */
	if(d->frag_count == 1)
	{
		receive_message(d->msg_frag, d->msg_size, sender);
	}
	else
	{
		bool all_received = enqueue(buff, d);
		if(all_received == true)
		{
			/*
			 * Reassemble message
			 */
			DATAGRAM* frags = queue_delete(buff, d->source, d->msg_num);
			qsort(frags, d->frag_count, sizeof(DATAGRAM), comp); 
			int num_frags = d->frag_count;
			char* built_msg = malloc(num_frags * MAX_DATAGRAM_SIZE * sizeof(char));
			for(int i = 1; i <= num_frags; i++)
			{
				if(i == num_frags)
				{
					memcpy(built_msg + ((i - 1) * MAX_DATAGRAM_SIZE), frags[i - 1].msg_frag, frags[i - 1].msg_size);
				}
				else
				{
					memcpy(built_msg + ((i - 1) * MAX_DATAGRAM_SIZE), frags[i - 1].msg_frag, MAX_DATAGRAM_SIZE);
				}
			}
			free(frags);
		}

	}
}

/* 
 * Called by the application layer. Receives a message originating at this node,
 * fragments it if necessary, computes the checksum, builds the header so that
 * error checking and reassembly of fragments is possible and sends it to the
 * network layer.
 */
void transport_datagram(char* msg, int len, CnetAddr destination) 
{
	/*
	 * Determine how many fragments will be needed
	 */
	int extra;
	if ((len % MAX_DATAGRAM_SIZE) == 0)
		extra = 0;
	else
		extra = 1;
	int num_frags_needed = (len / MAX_DATAGRAM_SIZE) + extra;

	int src = nodeinfo.nodenumber;
	int msg_num = ++msg_num_counter;
	int frag_num = 0;

	for(int i = 1; i <= num_frags_needed; i++) 
	{
		/*
		 * if it is the last fragment of the message:
		 */
		if(i == num_frags_needed) 
		{
			int remainder = len % MAX_DATAGRAM_SIZE;
			if (remainder == 0)
				remainder = MAX_DATAGRAM_SIZE;

			DATAGRAM* d = malloc(sizeof(DATAGRAM) - MAX_DATAGRAM_SIZE + remainder); 

			d->msg_size = remainder;
			d->source = src;
			d->msg_num = msg_num;
			d->frag_num = frag_num++;
			d->frag_count = num_frags_needed;

			/*
			 * copy over a part of a the message. parts are not null-terminated 
			 */
			memcpy(d->msg_frag, msg + ((i - 1) * MAX_DATAGRAM_SIZE), remainder);

			d->checksum = CNET_crc32(((unsigned char *) d) + offsetof(DATAGRAM, msg_size), 
					sizeof(DATAGRAM) - sizeof(d->checksum) - MAX_DATAGRAM_SIZE + remainder); 

			net_send(((char*) d), sizeof(DATAGRAM) - MAX_DATAGRAM_SIZE + remainder, destination);
			free(d);
		}
		/*
		 * if it's not the last fragment of the message:
		 */
		else 
		{
			DATAGRAM* d = malloc(sizeof(DATAGRAM));

			d->msg_size = MAX_DATAGRAM_SIZE;
			d->source = src;
			d->msg_num = msg_num;
			d->frag_num = frag_num++;
			d->frag_count = num_frags_needed;

			/*
			 * copy over a part of a the message. parts are not null-terminated 
			 */
			memcpy(d->msg_frag, msg + ((i - 1) * MAX_DATAGRAM_SIZE), MAX_DATAGRAM_SIZE);

			d->checksum = CNET_crc32(((unsigned char *) d) + offsetof(DATAGRAM, msg_size), 
					sizeof(DATAGRAM) - sizeof(d->checksum)); 

			net_send(((char*)d), sizeof(DATAGRAM), destination);
			free(d);
		}
	}
}

/* called on node init */
void transport_init() 
{
	/* register handler for application layer events if required*/
	msg_num_counter = 0;	
	buff = new_queue();
	tree_map = new_red_black_tree();
}

/* 
 * TODO: Check for mem leaks.
 * TODO: Implement limited buffer! 
 */
