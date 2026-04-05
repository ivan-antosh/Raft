#ifndef PROXY_H
#define PROXY_H

typedef struct {
	int client_fd;
	char target_host[16];
	char target_port[6];
} proxy_args_t;

#endif
