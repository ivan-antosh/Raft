#!/bin/bash

# !! DONT RUN YET

NUM_SERVERS=5 # Must be the same as in types.h
EXECUTABLE="./server"
HOST_NAME=$(hostname)
PORT_NUM=33000

#ptyxis --execute "bash -c \"$EXECTUABLE; exec bash\"" &

getServer() {
	id=$(((($1 + $2) % 5) + 1))
	echo "$id" "$HOST_NAME" "$((PORT_NUM + id))"
}

for ((i = 1; i < (NUM_SERVERS + 1); i++)); do
	$EXECUTABLE $i $((PORT_NUM + i)) $(getServer $i 0) $(getServer $i 1) $(getServer $i 2) $(getServer $i 3) &
	echo "process $i started with pid $!"
done

echo "All processes started"
