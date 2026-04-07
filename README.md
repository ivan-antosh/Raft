# Raft

Raft consensus algorithm implementation in the context of replicated state machines

# Setup

First build executables, run
> make

To run the Raft consensus algorithm:

> ./setup.bash

This uses tmux to attach separate terminals each running their own server instance.
Make sure tmux is installed before running. Use:
- Ctrl + B, then # of terminal to switch to that terminal window
- Ctrl + D to delete the terminal window

Options with setup.bash:
1. -proxy \<Drop Probability>  
Sets up a proxy server that emulates an unreliable network connection between the raft servers with a \<Drop Probability>% chance of dropping network traffic.

To run manually:

for each server, run (this implementation requires exactly 5 servers):
> ./server \<id> \<port number> (\<id> \<host name> \<port number>) * (4 servers)
each server must use a unique id, in the range of 1-5 (inclusive)

# Use

When a server is elected as LEADER, it can take client request through terminal input stdin. The input are commands which will be added to the server's log entries, and then applied to the state machine.

Commands:
put x y: set x to value y
get x: print x
del x: remove x

x: a 32 character string, representing the key
y: an integer, representing the key's value

To simulate a failing server, can terminate a server, which the other server's will react according to the Raft algorithm. A server can be restarted and will then resume operation. A cluster of 5 servers can handle 2 servers failing.
