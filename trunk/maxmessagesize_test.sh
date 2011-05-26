#!/bin/bash
#
DURATION="5m"
#
rm -f result.messagesize
#
for f in 0 1 2 3 4 5 6 7 8 9
do
	cnet -W -q -T -e $DURATION -s -Q MESSAGESIZE/DTNMESS$f	| 
	echo $[1000 + $f*1000] `grep 'Messages *' | cut -d: -f 2`
done > result.messagesize


