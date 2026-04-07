#!/bin/bash

NUM_SERVERS=5 # Must be the same as in types.h
HOST_NAME=$(hostname)
SERVER_EXE="./server"
PROXY_EXE="./proxy"
SERVER_PORT_NUM=33000
PROXY_PORT_NUM=34000

# Default Environment Variable Values
PROXY_ENABLED=0
ELECTION_TIME=1
DROP_PROBABILITY=0
DELAY_PROBABILITY=0
DELAY_LENGTH=1.5

print_usage() {
	echo "Usage: ./setup.bash [OPTIONS]"
	echo ""
	echo "  -p              Enable proxy server"
	echo "  -e TIME         Election timeout in seconds (default: 1)"
	echo "  -r PROB         Drop probability 0-100% (default: 0)"
	echo "  -d PROB         Delay probability 0-100% (default: 0)"
	echo "  -l LENGTH       Delay length in seconds (default: 1.5)"
	echo "  -h              Show this help"
	exit 2
}

numCheck='^([0-9]+([.][0-9]*)?|[.][0-9]+)$'
while getopts 'pe:r:d:l:h' flag; do
	case "${flag}" in
	p) PROXY_ENABLED=1 ;;
	e)
		ELECTION_TIME=${OPTARG}
		if ! [[ $ELECTION_TIME =~ $numCheck ]]; then
			print_usage
		fi
		;;
	r)
		DROP_PROBABILITY=${OPTARG}
		if ! [[ $DROP_PROBABILITY =~ $numCheck ]]; then
			print_usage
		elif (($(echo "$DROP_PROBABILITY < 0 || $DROP_PROBABILITY > 100" | bc -l))); then
			print_usage
		fi
		;;
	d)
		DELAY_PROBABILITY=${OPTARG}
		if ! [[ $DELAY_PROBABILITY =~ $numCheck ]]; then
			print_usage
		elif (($(echo "$DELAY_PROBABILITY < 0 || $DELAY_PROBABILITY > 100" | bc -l))); then
			print_usage
		fi
		;;
	l)
		DELAY_LENGTH=${OPTARG}
		if ! [[ $DELAY_LENGTH =~ $numCheck ]]; then
			print_usage
		fi
		;;
	h)
		print_usage
		;;
	*)
		print_usage
		;;
	esac
done

# gets formated proxy args
getProxy() {
	echo "$((PROXY_PORT_NUM + $1))" "$HOST_NAME" "$((SERVER_PORT_NUM + $1))"
}

# gets formated server args
getServer() {
	id=$(((($1 + $2) % 5) + 1))
	if [ $PROXY_ENABLED -eq 1 ]; then
		echo "$id" "$HOST_NAME" "$((PROXY_PORT_NUM + id))"
	else
		echo "$id" "$HOST_NAME" "$((SERVER_PORT_NUM + id))"
	fi
}

# start detached tmux session
SESSION="raft_servers"
tmux new-session -d -s $SESSION -n "temp"

# Run proxy servers when -proxy flag is given to setup.bash
if [ $PROXY_ENABLED -eq 1 ]; then
	tmux new-window -t $SESSION -n "proxy"

	ENV="DROP_PROBABILITY=$DROP_PROBABILITY DELAY_PROBABILITY=$DELAY_PROBABILITY DELAY_LENGTH=$DELAY_LENGTH"
	CMD="$ENV $PROXY_EXE $(getProxy 1) $(getProxy 2) $(getProxy 3) $(getProxy 4) $(getProxy 5)"

	tmux send-keys -t $SESSION:proxy "$CMD" C-m

	echo "proxy server $i started with pid $!"
fi

# Run each server in a individiual window
for ((i = 1; i < (NUM_SERVERS + 1); i++)); do
	tmux new-window -t $SESSION -n "server$i"

	ENV="PROXY_ENABLED=$PROXY_ENABLED ELECTION_TIME=$ELECTION_TIME"
	CMD="$ENV $SERVER_EXE $i $((SERVER_PORT_NUM + i)) $(getServer $i 0) $(getServer $i 1) $(getServer $i 2) $(getServer $i 3)"

	tmux send-keys -t $SESSION:"server$i" "$CMD" C-m

	echo "process $i started with pid $!"
done

# Clean up the initial window
tmux kill-window -t $SESSION:temp 2>/dev/null || true

echo "All processes started in tmux session $SESSION"

tmux a -t $SESSION
