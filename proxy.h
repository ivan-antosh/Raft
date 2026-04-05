#ifndef PROXY_H
#define PROXY_H

#define DROP_PROBABILITY 10 /* 10% chance packet gets dropped by the proxy */
#define BUFFER_SIZE 4096

typedef struct {
	int client_fd;
	char target_host[16];
	char target_port[6];
} proxy_args_t;

#endif
