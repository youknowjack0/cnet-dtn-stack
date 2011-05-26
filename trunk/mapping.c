#include <cnet.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "mapping.h"

typedef struct {
    char	*text;
    double	x0;
    double	y0;
    double	x1;
    double	y1;
} OBJECT;

static	OBJECT	*objects	= NULL;
static	int	nobjects	= 0;

#define	FOREACH_OBJECT	for(n=0, op=objects ; n<nobjects ; ++n, ++op)

static	CnetPosition	mapsize;
static	double		mapscale;

#define	SCALE(p)	((int)((p) / mapscale))

static void add_object(char *text, double x0, double y0, double x1, double y1)
{
    objects	= realloc(objects, (nobjects+1)*sizeof(OBJECT));

    if(objects) {
	extern char *strdup(const char *str);

	OBJECT	*new	= &objects[nobjects];

	memset(new, 0, sizeof(OBJECT));
	if(text)
	    new->text	= strdup(text);

	if(x0 == x1)
	    ++x1;
	if(y0 == y1)
	    ++y1;
	new->x0			= x0;
	new->y0			= y0;
	new->x1			= x1;
	new->y1			= y1;
	++nobjects;
    }
    else
	nobjects = 0;
}

void draw_walk(CnetPosition *now, CnetPosition *newdest)
{
    TCLTK("$map create line %d %d %d %d -fill red -width 3",
	SCALE(now->x), SCALE(now->y), SCALE(newdest->x), SCALE(newdest->y) );
}

static void draw_objects(void)
{
    int		n;
    OBJECT	*op;

    TCLTK("font create bfont -family Helvetica -size 18 -weight normal");
    FOREACH_OBJECT {
	if(op->text)
	    TCLTK("$map create text %d %d -font bfont -text \"%s\"",
		    SCALE(op->x0), SCALE(op->y0),
		    op->text);
/*
	else
	    TCLTK("$map create rect %d %d %d %d -width 1 -outline %s -fill %s",
		    SCALE(op->x0), SCALE(op->y0), SCALE(op->x1), SCALE(op->y1),
		    COLOUR_OBJECTS, COLOUR_OBJECTS);
 */
    }
}

static char *trim(char *line)
{
    char	*s = line;

    while(*s) {
	if(*s == COMMENT || *s == '\n' || *s == '\r') {
	    *s	= '\0';
	    break;
	}
	++s;
    }
    s	= line;
    while(isspace(*s))
	++s;
    return s;
}

void readmap(const char *mapfile)
{
    FILE *fp	= fopen(mapfile, "r");

//  EACH NODE READS IN THE MAP DETAILS
    if(fp) {
	char	line[1024], text[1024], *s;

	CNET_get_position(NULL, &mapsize);
	mapscale = CNET_get_mapscale();

	while(fgets(line, sizeof(line), fp)) {
	    s	= trim(line);

	    if(*s) {
		double	x0, y0, x1, y1;

		if(sscanf(s, "object %lf %lf %lf %lf", &x0, &y0, &x1, &y1) == 4)
		    add_object(NULL, x0, y0, x1, y1);
		else if(sscanf(s, "text %lf %lf %s", &x0, &y0, text) == 3)
		    add_object(text, x0, y0, 0, 0);
	    }
	}
	fclose(fp);

//  ALL NODES NEED THE MAP, BUT ONLY ONE NODE DRAWS THE MAP
	if(nodeinfo.nodenumber == 0)
	    draw_objects();
    }
    else {
	fprintf(stderr, "%s: cannot open '%s'\n", nodeinfo.nodename, mapfile);
	exit(EXIT_FAILURE);
    }
}

static bool intersect(	double x0, double y0, double x1, double y1,
			double x2, double y2, double x3, double y3)
{
#define	EPS	0.0001
   double	denom  = (y3-y2) * (x1-x0) - (x3-x2) * (y1-y0);
   double	numera = (x3-x2) * (y0-y2) - (y3-y2) * (x0-x2);
   double	numerb = (x1-x0) * (y0-y2) - (y1-y0) * (x0-x2);

// ARE LINES COINCIDENT?
   if(fabs(numera) < EPS && fabs(numerb) < EPS && fabs(denom) < EPS)
      return true;

// ARE LINES PARALLEL?
   if(fabs(denom) < EPS)
      return false;

// IS INTERSECTION ALONG THE THE SEGMENTS?
   double	mua = numera / denom;
   double	mub = numerb / denom;
   return (mua > 0 && mua < 1 && mub > 0 && mub < 1);
#undef	EPS
}


//  DOES THE PATH FROM S -> D PASS THROUGH AN OBJECT?
bool through_an_object(CnetPosition S, CnetPosition D)
{
    int		n;
    OBJECT	*op;

    FOREACH_OBJECT {
	if(op->text == NULL)		// only interested in objects
	    if(	intersect(S.x, S.y, D.x, D.y, op->x0, op->y0, op->x1, op->y0) ||
		intersect(S.x, S.y, D.x, D.y, op->x1, op->y0, op->x1, op->y1) ||
		intersect(S.x, S.y, D.x, D.y, op->x1, op->y1, op->x0, op->y1) ||
		intersect(S.x, S.y, D.x, D.y, op->x0, op->y1, op->x0, op->y0) )
		return true;
    }
    return false;
}

//  THROUGH HOW MANY OBJECTS DOES THE PATH FROM S -> D PASS?
int through_N_objects(CnetPosition S, CnetPosition D)
{
    int		n;
    OBJECT	*op;
    int	count	= 0;

    FOREACH_OBJECT {
	if(op->text == NULL)		// only interested in objects
	    if(	intersect(S.x, S.y, D.x, D.y, op->x0, op->y0, op->x1, op->y0) ||
		intersect(S.x, S.y, D.x, D.y, op->x1, op->y0, op->x1, op->y1) ||
		intersect(S.x, S.y, D.x, D.y, op->x1, op->y1, op->x0, op->y1) ||
		intersect(S.x, S.y, D.x, D.y, op->x0, op->y1, op->x0, op->y0) )
		++count;
    }
    return count;
}

//  CHOOSE A RANDOM POSITION ON THE MAP, BUT NOT WITHIN ANY OBJECT
void choose_position(CnetPosition *new, int maxdist)
{
    int mdmd	= (maxdist * maxdist);

    for(;;) {
	double	tryx	= CNET_rand() % mapsize.x;
	double	tryy	= CNET_rand() % mapsize.y;
	int	n;
	OBJECT	*op;

//  IF REQUESTED, ENSURE THAT NEW POINT IS CLOSE ENOUGH TO THE OLD POINT
	if(mdmd != 0 &&
	 ((tryx-new->x)*(tryx-new->x) + (tryy-new->y)*(tryy-new->y)) > mdmd)
	    continue;

	FOREACH_OBJECT {
	    if(op->text == NULL &&
		tryx >= op->x0 && tryx <= op->x1 &&
		tryy >= op->y0 && tryy <= op->y1)
		    break;		// oops, inside an object
	}
	if(n == nobjects) {
	    new->x	= tryx;
	    new->y	= tryy;
	    new->z	= 0;
	    break;
	}
    }
}
