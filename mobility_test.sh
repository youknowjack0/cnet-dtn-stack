#!/bin/bash
#
DURATION="5m"
#
rm -f result.mobility
#
for f in 0 1 2 3 4 5 6 7 8 9
do
	cnet -W -q -T -e $DURATION -s -Q MOBILITY/MOBILITY$f	| 
	echo $[10 + $f*20] `grep 'Messages *' | cut -d: -f 2`
done > result.mobility


