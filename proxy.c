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

/* Thread to handle a single bidirectional traffic for a single established connection */
void *handle_connection(void *arg) {
	proxy_args_t *args = (proxy_args_t*)arg;
	int server_fd;
	int rv;
	struct addrinfo hints, *servinfo, *p;
	char s[INET6_ADDRSTRLEN];

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
		inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof(s));

		if(connect(server_fd, p->ai_addr, p->ai_addrlen) == -1) {
			close(server_fd);
			continue;
		}
		break;
	}
	if(p == NULL) {
		/* nothing found, try again next main iteration */
		close(args->client_fd);
		return NULL;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof(s));

	freeaddrinfo(servinfo); // done with this
	
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
			int bytes = recv(args->client_fd, buffer, BUFFER_SIZE, 0);
			if (bytes <= 0) {
				break;
			}
			/* TODO: Implement Corruption */
			send(server_fd, buffer, bytes, 0);
		}

		/* Traffic going FROM the target Raft server */
		if (fds[1].revents & POLLIN) {
			int bytes = recv(server_fd, buffer, BUFFER_SIZE, 0);
			if (bytes <= 0) {
				break;
			}
			/* TODO: Implement Corruption */
			send(args->client_fd, buffer, bytes, 0);
		}
	}

	close(args->client_fd);
	close(server_fd);
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
						pthread_t tid;
						/* Spawn a thread for this connection and detach it so it cleans up its own memory */
						pthread_create(&tid, NULL, handle_connection, &targetArgs[i]);
						pthread_detach(tid);
					}
				}
			}
		}
	}

	return 0;
}
