#!/bin/bash

NUM_SERVERS=5 # Must be the same as in types.h
HOST_NAME=$(hostname)
SERVER_EXE="./server"
PROXY_EXE="./proxy"
SERVER_PORT_NUM=33000
PROXY_PORT_NUM=34000

# Check for -proxy flag
PROXY_FLAG=false
if [ "$1" == "-proxy" ]; then
	PROXY_FLAG=true
fi

getProxy() {
	echo "$((PROXY_PORT_NUM + $1))" "$HOST_NAME" "$((SERVER_PORT_NUM + $1))"
}

getServer() {
	id=$(((($1 + $2) % 5) + 1))
	if $PROXY_FLAG; then
		echo "$id" "$HOST_NAME" "$((PROXY_PORT_NUM + id))"
	else
		echo "$id" "$HOST_NAME" "$((SERVER_PORT_NUM + id))"
	fi
}

# start detached tmux session
SESSION="raft_servers"
tmux new-session -d -s $SESSION

# Run proxy servers when -proxy flag is given to setup.bash
if $PROXY_FLAG; then
	CMD="$PROXY_EXE $(getProxy 1) $(getProxy 2) $(getProxy 3) $(getProxy 4) $(getProxy 5)"

	tmux send-keys -t $SESSION:1 "$CMD" C-m

	echo "proxy server $i started with pid $!"
	# for ((i = 1; i < (NUM_SERVERS + 1); i++)); do
	# done
fi

for ((i = 1; i < (NUM_SERVERS + 1); i++)); do
	tmux new-window -t $SESSION -n "server$i"

	CMD="$SERVER_EXE $i $((SERVER_PORT_NUM + i)) $(getServer $i 0) $(getServer $i 1) $(getServer $i 2) $(getServer $i 3)"

	tmux send-keys -t $SESSION:"server$i" "$CMD" C-m

	echo "process $i started with pid $!"
done

echo "All processes started in tmux session $SESSION"

tmux a -t $SESSION
