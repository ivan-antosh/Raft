#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <poll.h>

#include "proxy.h"
#include "helper.h"
#include "types.h"

/* Thread to handle a single bidirectional traffic for a single established connection */
void *handle_connection(void *arg) {
	proxy_args_t *args = (proxy_args_t*)arg;
	int client_fd = args->client_fd;
	int server_fd;

	printf("TEST THREAD\n");

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
