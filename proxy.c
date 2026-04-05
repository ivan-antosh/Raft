#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>

#include "proxy.h"
#include "helper.h"

/* Thread to handle a single bidirectional proxy connection */
void *handle_connection(void *arg) {
	proxy_args_t *args = (proxy_args_t*)arg;
	int client_fd = args->client_fd;
	int server_fd;

	free(args);
	return NULL;
}

int main(int argc, char *argv[]) {
	int listener;

	if (argc != 4) {
		printf("Error: invalid number of arguments\n"
			"Usage: ./proxy <listen port> <target host> <target port>\n");
		return 1;
	}

	char *listen_port = argv[1];
	char *target_host = argv[2];
	char *target_port = argv[3];

	listener = get_listener_socket(listen_port);

	printf("Proxy listening on port %s -> forwarding to %s:%s\n", listen_port, target_host, target_port);

	for (;;) {
		struct sockaddr_in client_addr;
		socklen_t client_len = sizeof(client_addr);
		int client_fd = accept(listener, (struct sockaddr*)&client_addr, &client_len);

		if (client_fd >= 0) {
			proxy_args_t *args = malloc(sizeof(proxy_args_t));
			args->client_fd = client_fd;
			strcpy(args->target_host, target_host);
			strcpy(args->target_port, target_port);

			pthread_t tid;
			/* Spawn a thread for this connection and detach it so it cleans up its own memory */
			pthread_create(&tid, NULL, handle_connection, args);
			pthread_detach(tid);
		}
	}

	return 0;
}
