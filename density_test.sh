#!/bin/bash
#
DURATION="5m"
#
rm -f result.density
#
for f in 1 2 3 4 5 6 7 8
do
	cnet -W -q -T -e $DURATION -s -Q DENSITY/DTNDENS$f	| 
	echo `expr $f + 1` `grep 'Messages *' | cut -d: -f 2`
done > result.density


