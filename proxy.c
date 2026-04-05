#include <stdio.h>
#include <stdlib.h>

#include "proxy.h"
#include "helper.h"

int main(int argc, char *argv[]) {
	int listener;

	if (argc != 4) {
		printf("Error: invalid number of arguments\n"
			"Usage: ./proxy <listen port> <target host> <target port>\n");
		return 1;
	}

	char* listen_port = argv[1];
	char* target_host = argv[2];
	char* target_port = argv[3];

	listener = get_listener_socket(listen_port);

	printf("Proxy listening on port %s -> forwarding to %s:%s\n", listen_port, target_host, target_port);

	return 0;
}
