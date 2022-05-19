#!/bin/bash

WORKING_DIR=~/private/cs739/project2/safs-client/cmake/build

MTBF=4
MTTR=0.01

MAX=10000


i=0
while [ $i -lt $MAX ]
do
  $WORKING_DIR/Server -dir serv &   
  sleep $MTBF
  kill -9 `pgrep Server`
  sleep $MTTR
  i=$[$i+1]
done
