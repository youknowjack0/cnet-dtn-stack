typedef enum {DL_DATA, DL_BEACON, DL_RTS, DL_CTS, DL_ACK} FRAMETYPE;

typedef struct {
	char	data[MAX_MESSAGE_SIZE];
} MSG;

typedef struct {
	FRAMETYPE	kind;
	int			dest;	// Zero if beacon, so broadcasted
	int 		src;
	size_t		len;
	MSG			msg;	// Holds list of recently seen addresses if beacon
} FRAME;

#define FRAME_HEADER_SIZE (sizeof(FRAME) - sizeof(MSG))

#define FRAME_SIZE(f)	  (FRAME_HEADER_SIZE + f.len)
