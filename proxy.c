#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <poll.h>

#include "proxy.h"
#include "helper.h"
#include "types.h"

/* Read a complete Raft RPC (header + body) from fd into buffer.
 * Returns: 
 * - number of bytes read on success, <=0 on failure/partial/close. 
 */
int read_full_rpc(int fd, char *buffer, size_t *out_len) {
	int pos = 0;
	const size_t MAX_RPC = 1024 * 64; /* generous; adjust for huge logs */

	/* 1. Header */
	int check = recv(fd, buffer, sizeof(RPCHeader), 0);
	if (check != sizeof(RPCHeader)) {
		return check;
	}
	pos += check;

	RPCHeader *header = (RPCHeader *)buffer;
	uint16_t msgType = ntohs(header->rpcMsgType);
	uint16_t rpcType   = ntohs(header->rpcType);

	/* 2. Body */
	if (rpcType == APPEND && msgType == MSG) {
		/* RPCAppendMsg + variable entries */
		check = recv(fd, buffer + pos, sizeof(RPCAppendMsg), 0);
		if (check != sizeof(RPCAppendMsg)) {
			return check;
		}
		pos += check;

		RPCAppendMsg *appendMsg = (RPCAppendMsg *)(buffer + sizeof(RPCHeader));
		uint32_t numEntries = ntohl(appendMsg->entriesLen);
		size_t entriesBytes = (size_t)numEntries * sizeof(LogEntry);

		if (entriesBytes > 0) {
			if (pos + entriesBytes > MAX_RPC) {
				printf("[Proxy] RPC too large\n");
				return -1;
			}
			check = recv(fd, buffer + pos, entriesBytes, 0);
			if (check != (int)entriesBytes) {
				return check;
			}
			pos += check;
		}
	} else if (rpcType == APPEND && msgType == REPLY) {
		check = recv(fd, buffer + pos, sizeof(RPCAppendReplyMsg), 0);
		if (check != sizeof(RPCAppendReplyMsg)) {
			return check;
		}
		pos += check;
	} else if (rpcType == VOTE && msgType == MSG) {
		check = recv(fd, buffer + pos, sizeof(RPCVoteMsg), 0);
		if (check != sizeof(RPCVoteMsg)) {
			return check;
		}
		pos += check;
	} else if (rpcType == VOTE && msgType == REPLY) {
		check = recv(fd, buffer + pos, sizeof(RPCVoteReplyMsg), 0);
		if (check != sizeof(RPCVoteReplyMsg)) {
			return check;
		}
		pos += check;
	} else {
		printf("[Proxy] unknown RPC type %d and %d\n", rpcType, msgType);
		return -1;
	}

	*out_len = pos;
	return (int)pos;
}

/* get the percent chance a packet gets dropped by the proxy */
int drop_probability() {
	const char *dropProbEnv = getenv("DROP_PROBABILITY");
	if(dropProbEnv != NULL)
		return atoi(dropProbEnv);
	return 0; /* Default - 0% chance of dropping a traffic */
}

/* Calculates whether traffic will be dropped by the network
 * returns: 
 * - 1 Network traffic will be dropped
 * - 0 Network traffic will be send to destination
 */
int drop_traffic(int source, int destination) {
	int n = rand() % 100;

	if (n < drop_probability()) {
		printf("\033[31m[DROP]\033[0m Dropping traffic | src: %d -> dest: %d\n", source, destination);
		return 1;
	}
	return 0;
}

/* Thread to handle a single bidirectional traffic for a single established connection */
void *handle_connection(void *arg) {
	proxy_args_t *args = (proxy_args_t*)arg;
	int server_fd;
	int rv;
	struct addrinfo hints, *servinfo, *p;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC; // IPv4 or IPv6
	hints.ai_socktype = SOCK_STREAM; // TCP

	if((rv = getaddrinfo(args->target_host, args->target_port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return NULL;
	}

	// loop through results and connect to first one
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if((server_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if(connect(server_fd, p->ai_addr, p->ai_addrlen) == -1) {
			close(server_fd);
			continue;
		}
		break;
	}

	freeaddrinfo(servinfo); // done with this

	if(p == NULL) {
		/* nothing found, try again next main iteration */
		close(args->client_fd);
		free(args);
		return NULL;
	}

	/* Handle Raft server handshake */
	HandshakeMsg msg;
	if((recv(args->client_fd, &msg, sizeof(msg), 0)) == -1) {
		perror("Handshake - recv");
		close(args->client_fd);
		close(server_fd);
		free(args);
		return NULL;
	}
	if((send(server_fd, &msg, sizeof(msg), 0)) == -1) {
		perror("Handshake - send");
		close(args->client_fd);
		close(server_fd);
		free(args);
		return NULL;
	}

	printf("[Proxy] Link established: target_port %s\n", args->target_port);

	struct pollfd fds[2];
	fds[0].fd = args->client_fd;
	fds[0].events = POLLIN;
	fds[1].fd = server_fd;
	fds[1].events = POLLIN;

	char buffer[BUFFER_SIZE];

	/* Send bytes back and forth until the connection closes */
	for (;;) {
		if (poll(fds, 2, -1) < 0) {
			break;
		}

		/* Traffic going TO the target Raft server */
		if (fds[0].revents & POLLIN) {
			size_t bytes;
			if (read_full_rpc(args->client_fd, buffer, &bytes) <= 0){
				break;
			}
			if(!drop_traffic(args->client_fd, server_fd)) {
				if (send(server_fd, buffer, bytes, 0) < 0) {
					printf("[Proxy] Server closed connection on fd %d\n", server_fd);
					break;
				}
			}
		}

		/* Traffic going FROM the target Raft server */
		if (fds[1].revents & POLLIN) {
			size_t bytes;
			if (read_full_rpc(server_fd, buffer, &bytes) <= 0) {
				break;
			}
			if(!drop_traffic(args->client_fd, server_fd)) {
				if (send(args->client_fd, buffer, bytes, 0) < 0) {
					printf("[Proxy] Send to client failed on fd %d\n", args->client_fd);
					break;
				}
			}
		}
	}

	close(args->client_fd);
	close(server_fd);
	free(args);
	return NULL;
}

int main(int argc, char *argv[]) {
	struct pollfd listeners[NUM_SERVERS];
	proxy_args_t targetArgs[NUM_SERVERS];

	if (argc != (3 * NUM_SERVERS)+1) {
		printf("Error: invalid number of arguments\n"
			"Usage: ./proxy (<listen port> <target host> <target port>) * (%d servers)\n", NUM_SERVERS);
		return 1;
	}

	srand(time(NULL));

	printf("DROP_PROBABILITY: %d%%\n", drop_probability());

	/* Setup listening socket for each route */
	for (int i = 0; i < NUM_SERVERS; i++) {
		char *listen_port = argv[(i*3)+1];
		strcpy(targetArgs[i].target_host, argv[(i*3)+2]);
		strcpy(targetArgs[i].target_port, argv[(i*3)+3]);

		listeners[i].fd = get_listener_socket(listen_port);
		listeners[i].events = POLLIN;

		printf("Proxy listening on port %s -> forwarding to %s:%s\n", 
				 listen_port, targetArgs[i].target_host, targetArgs[i].target_port);
	}

	for (;;) {
		if (poll(listeners, NUM_SERVERS, -1) > 0) {
			for (int i = 0; i < NUM_SERVERS; i++) {
				if (listeners[i].revents & POLLIN) {
					int client_fd = accept(listeners[i].fd, NULL, NULL);
					targetArgs[i].client_fd = client_fd;

					if (client_fd >= 0) {
						proxy_args_t *threadArg = malloc(sizeof(proxy_args_t));
						memcpy(threadArg, &targetArgs[i], sizeof(proxy_args_t));
						pthread_t tid;
						/* Spawn a thread for this connection and detach it so it cleans up its own memory */
						pthread_create(&tid, NULL, handle_connection, threadArg);
						pthread_detach(tid);
					}
				}
			}
		}
	}

	return 0;
}
