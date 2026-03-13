#!/bin/bash

# !! DONT RUN YET

NUM_SERVERS=5 # Must be the same as in types.h
EXECUTBLE="./server"

for ((i=0; i<$NUM_SERVERS; i++)); do
	$EXECUTABLE &
	echo "process $i started with pid $!"
done

echo "All processes started"

