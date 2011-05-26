#include <cnet.h>

#define	COMMENT		'#'
#define	COLOUR_OBJECTS	"#aaaaaa"

//  READ THE OBJECTS ON OUR MAP FROM THE INDICATED FILE
extern	void	readmap(const char *filenm);

//  CHOOSE A LOCATION THAT IS NOT INSIDE ANY OBJECT
extern	void	choose_position(CnetPosition *new, int maxdist);

//  DOES THE PATH FROM S -> D PASS THROUGH AN OBJECT?
extern	bool	through_an_object(CnetPosition S, CnetPosition D);

//  THROUGH HOW MANY OBJECTS DOES THE PATH FROM S -> D PASS?
extern	int	through_N_objects(CnetPosition S, CnetPosition D);

//  DRAWS THE PATH TO BE TALEN BY A NODE
extern void	draw_walk(CnetPosition *now, CnetPosition *newdest);
