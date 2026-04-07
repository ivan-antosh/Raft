#ifndef PROXY_H
#define PROXY_H

#define BUFFER_SIZE 4096

typedef struct {
	int client_fd;
	char target_host[16];
	char target_port[6];
} proxy_args_t;

#endif
