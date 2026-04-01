#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#include <append_entries.h>
#include <request_vote.h>
#include <types.h>
#include "list_lib/list.h"

/* STATE INFO */

/* persistent on all servers */
/* lastest term server has seen */
int currentTerm = 0;
/* candidateId that received vote in current term, NULL if none */
int *votedFor = NULL;
/* log entries, command for state machine and term when entry received by leader */
LogEntry *logEntries; /* first index is 1 */
/* doubly linked list of state machine entries, each are a key value pair */
List *stateMachine;

/* volatile on all servers */
/* index of highest log entry known to be committed */
int commitIndex = 0;
/* index of highest log entry applied to state machined */
int lastApplied = 0;
/* current state of the server, follower, candidate, leader */
ServerStateType serverStateType = FOLLOWER;

/* volatile on leaders (reinitialized after election)*/
/* index of next log entry to send for each server */
int nextIndex[NUM_SERVERS]; /* init to leader last log index + 1 */
/* index of highest log entry known to be replicated on each server */
int matchIndex[NUM_SERVERS]; /* init to 0 */

/* OTHER INFO */
ServerInfo servers[NUM_SERVERS - 1];

/* Comparator for ListSearch() lib call to search for state with key == comparisonArg */
int StateEntryKeyComparator(void *item, void *comparisonArg) {
	StateEntry *stateEntry = (StateEntry *)item;
	char *key = (char *)comparisonArg;
	if(strcmp(stateEntry->key, key) == 0) {
		return 1;
	}
	return 0;
}

/* Free a state entry
 * Can be used in ListFree() lib call
 */
void StateEntryFree(void *itemToBeFreed) {
	StateEntry *state = (StateEntry *)itemToBeFreed;
	free(state->key);
	free(state->val);
	free(state);
}

/* Apply oldest non-committed log based on lastApplied */
void applyOldestLog() {
	/* apply to state machine */
	lastApplied += 1;
	Command *cmd = logEntries[lastApplied].cmd;
	char *key = cmd->x;
	void *val = cmd->y;
	CommandType cmdType = cmd->type;

	StateEntry *state = ListSearch(stateMachine, StateEntryKeyComparator, key);
	switch (cmdType) {
		case PUT:
			/* PUT new state, or update existing state */
			if(state == NULL) {
				/* create new StateEntry, allocate + set new memory for values */
				StateEntry *newState = (StateEntry *)malloc(sizeof(StateEntry));
				
				char *newKey = NULL;
				newKey = strdup(val);
				if(newKey == NULL) {
					printf("Error: unable to copy string to new state\n");
					break;
				}
				newState->key = newKey;
				void *newVal = malloc(sizeof(int)); /* TODO: (maybe) just ints for val now, change later */
				if(newVal == NULL) {
					printf("Error: failed to allocate memory for value to new state\n");
					break;
				}
				memcpy(newVal, val, sizeof(int));
				newState->val = newVal;

				ListAppend(stateMachine, newState);
			} else {
				/* update state value, allocate + set new val memory */
				void *newVal = malloc(sizeof(int)); /* TODO: (maybe) just ints for val now, change later */
				if(newVal == NULL) {
					printf("Error: failed to allocate memory for new value to state\n");
					break;
				}
				memcpy(newVal, val, sizeof(int));
				free(state->val);
				state->val = newVal;
			}
			break;
		case GET:
			/* nothing to commit for GET */
			printf("Error: shouldn't have a GET in log\n"); /* maybe */
			break;
		case DEL:
			/* delete state if there is one with the same key */
			if(state != NULL) {
				ListRemove(stateMachine);
				StateEntryFree(state);
			}
			break;
	}
}

/* For RPC req OR resp, if contains term T > current term, need to update term and set to FOLLOWER */
void checkTerm(int term) {
	if (term > currentTerm) {
		currentTerm = term;
		/* if term out of date, then server is a follower */
		serverStateType = FOLLOWER;
	}
}

/* get in addr from sock addr */
void *get_in_addr(struct sockaddr *sa) {
	if(sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/* return listener socket */
int get_listener_socket(char *portNum) {
	struct addrinfo hints, *ai, *p;
	int yes=1;
	int rv, listener;

	/* get socket and bind */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if((rv = getaddrinfo(NULL, portNum, &hints, &ai)) != 0) {
		fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
		exit(1);
	}

	for(p = ai; p != NULL; p = p->ai_next) {
		listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if(listener < 0) {
			continue;
		}
		setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
		if(bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
			close(listener);
			continue;
		}
		break;
	}

	if(p == NULL) {
		/* did not get bound if here */
		fprintf(stderr, "selectserver: failed to bind\n");
		exit(2);
	}
	/* listen */
	if(listen(listener, 10) == -1) {
		perror("listen");
		exit(3);
	}
	return listener;
}

/* accept an incoming connection
 * if the listener has an incoming connection, connect to it and
 * receive an incoming handshake message to verify connection id
 * returns 0 on no connection, 1 on connection
 */
int handle_new_connection(int listener, fd_set *master, int *fdmax)
{
    socklen_t addrlen;
    int sockfd; /* newly accepted sockfd */
    struct sockaddr_storage remoteaddr; /* other server address */
	int i;
    
    addrlen = sizeof(remoteaddr);
    sockfd = accept(listener, (struct sockaddr *)&remoteaddr, &addrlen);
    if (sockfd == -1) {
        perror("accept");
		return 0;
    }

	FD_SET(sockfd, master); /* add to master */
	if (sockfd > *fdmax) { /* track max */
		*fdmax = sockfd;
	}
	/* get handshake message after setting up connection for id */
	HandshakeMsg msg;
	if((recv(sockfd, &msg, sizeof(msg), 0)) == -1) {
		close(sockfd);
		perror("recv");
		FD_CLR(sockfd, master);
		return 0;
	}
	/* set sockfd from handshake message */
	int id = ntohl(msg.id);
	for(i = 0; i < (NUM_SERVERS - 1); i++) {
		if(servers[i].id == id) {
			servers[i].sockfd = sockfd;
			break;
		}
	}
	printf("Connected to %d\n", id);
	return 1;
}

/* Connect to a sever
 * attempt a connect() on a server port number + hostname given
 * if connects, then add server info vals
 * returns -1 on failure, 0 on no connection, 1 on connection
 */
int connect_to_server(ServerInfo *serverInfo, fd_set *master, int *fdmax) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

	char *hostname = serverInfo->hostname;
	int portNum = serverInfo->portNum;
    char portNumStr[10];
    sprintf(portNumStr, "%d", portNum);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP

    if((rv = getaddrinfo(hostname, portNumStr, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    // loop through results and connect to first one
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }
        inet_ntop(p->ai_family,
            get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof(s));
        if(connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            continue;
        }
        break;
    }
    if(p == NULL) {
        /* nothing found, try again next main iteration */
		return 0;
    }

    inet_ntop(p->ai_family,
        get_in_addr((struct sockaddr *)p->ai_addr),
        s, sizeof(s));
    freeaddrinfo(servinfo); // done with this

    FD_SET(sockfd, master); /* add to master */
    if (sockfd > *fdmax) { /* track max */
        *fdmax = sockfd;
    }

    /* new connection with server, set info */
	serverInfo->sockfd = sockfd;

    /* send handshake message to connected server, so it knows which server it connected to */
    HandshakeMsg msg;
    msg.id = htonl(serverInfo->id);
    if(send(sockfd, &msg, sizeof(msg), 0) == -1) {
        perror("send");
        exit(1);
    }
	printf("Connected to %d\n", serverInfo->id);

    return 1;
}

int main(int argc, char *argv[]) {
	fd_set master, read_fds, write_fds;
	int fdmax;
	int listener;
	struct timeval tv;

	int portNum, id;
	int i, check;
	int usedIds[NUM_SERVERS] = {0}; /* to ensure all ids passed are unique */

	/* arg validation */
	if(argc != (3 + (3 * (NUM_SERVERS-1)))) {
		printf("Error: invalid number of arguments\n"
			"Usage: ./server <id> <port number> (<id> <host name> <port number>) * (%d servers)\n",
			(NUM_SERVERS - 1));
		return 1;
	}

	id = atoi(argv[1]);
	if(id <= 0 || id > NUM_SERVERS) {
		printf("Error: invalid id, must be in range 1-%d\n", NUM_SERVERS);
		return 1;
	}
	usedIds[id-1] = 1;
	portNum = atoi(argv[2]);
	if(portNum <= 30000 || portNum >= 40000) {
		printf("Error: invalid port number, must be in range 30001-39999\n");
		return 1;
	}
	/* validate all server args, set servers */
	for(i = 0; i < (NUM_SERVERS - 1); i++) {
		int index = 3 + (i * 3);
		
		int otherId = atoi(argv[index]);
		if(otherId <= 0 || otherId > NUM_SERVERS) {
			printf("Error: invalid id, must be in range 1-%d\n", NUM_SERVERS);
			return 1;
		}
		if(usedIds[otherId-1] == 1) {
			printf("Error: all ids passed through must be unique, in the range 1-%d\n", NUM_SERVERS);
			return 1;
		}
		usedIds[otherId - 1] = 1;

		int otherPortNum = atoi(argv[index + 2]);
		if(otherPortNum <= 30000 || otherPortNum >= 40000) {
			printf("Error: invalid port number, must be in range 30001-39999\n");
			return 1;
		}

		servers[i].id = otherId;
		servers[i].portNum = otherPortNum;
		servers[i].sockfd = -1; /* -1 until set */
		strncpy(servers[i].hostname, argv[index + 1], HOST_LEN);
	}

	/* TODO: remove, just to check */
	printf("id: %d, portNum: %d\n", id, portNum);
	for(i = 0; i < NUM_SERVERS-1; i++) {
		printf("id: %d, portNum: %d, sockfd: %d, hostname: %s\n", servers[i].id, servers[i].portNum, servers[i].sockfd, servers[i].hostname);
	}

	/* clear master and temps */
    FD_ZERO(&master); 
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    /* add listener to master */
    listener = get_listener_socket(argv[2]);
    FD_SET(listener, &master);
    fdmax = listener; /* set fdmax */

	/* Connect to all servers before continuing */
	int neededConnections = NUM_SERVERS - 1;
	for(;;) {
		printf("needed connections: %d\n", neededConnections);
		if(neededConnections <= 0) {
			break; /* got all connections */
		}
		sleep(2); /* TODO: idk like change or something */
		printf("trying connections...\n");
		
		/* attempt to connect to higher id servers */
		for(i = 0; i < (NUM_SERVERS - 1); i++) {
			/* for race condition, only attempt connection for higher ids, listen for lower ids */
			if(servers[i].id < id || servers[i].sockfd != -1) {
				continue;
			}
			check = connect_to_server(&servers[i], &master, &fdmax);
			if (check == -1) {
				printf("Error: connect to neighbour\n");
				return 1;
			} else if (check == 1) {
				neededConnections -= 1;
			}
		}

		/* check for incoming connections on listener */
		read_fds = master;
		tv.tv_sec = 0;
		tv.tv_usec = 0;
		check = select(fdmax + 1, &read_fds, NULL, NULL, &tv);
		if(check == -1) {
			perror("select: read on listener");
			exit(4);
		}
		for(i = 0; i <= fdmax; i++) {
			if(FD_ISSET(i, &read_fds)) {
				if(i == listener) {
					check = handle_new_connection(i, &master, &fdmax);
					printf("check: %d\n", check);
					if(check == 1) {
						neededConnections -= 1;
					}
				} else {
					perror("Unknown read fd??");
				}
			}
		}
	}
	
	/* TODO: remove, just to check */
	printf("CONNECTED!\n");
	printf("id: %d, portNum: %d\n", id, portNum);
	for(i = 0; i < NUM_SERVERS-1; i++) {
		printf("id: %d, portNum: %d, sockfd: %d, hostname: %s\n", servers[i].id, servers[i].portNum, servers[i].sockfd, servers[i].hostname);
	}

	for (;;) {
		/* for all server states: */
		/* if commit index is larger than last applied, increase last applied and commit new log */
		if(commitIndex > lastApplied) {
			applyOldestLog();
		}

		/* server state specific logic: */
		switch (serverStateType) {
			case FOLLOWER:
				/* TODO: Implment Follower Case */

				/* TODO: Respond to RPC from candidates and leaders */

				/* TODO: check timeout and convert to Candidate */
				if (id == 1){
					serverStateType = CANDIDATE;
				}

				printf("Server is in the follower state\n");
				break;
			case CANDIDATE:
				/* TODO: Implement Candidate Case */

				/* start election */
				currentTerm += 1;
				votedFor = &id;
				struct timeval electionTimer = {0, 0};

				/* TODO: Send RequestVote RPC to other servers */

				/* loop till either majority votes received, AppendEntries RPC received or election timeout */
				for (;;){
					/* TODO: Check for majority vote */

					/* TODO: check for AppendEntries RPC */

					/* TODO: if election timeout elapses, start new election */

					if (id == 1) {
						serverStateType = LEADER;
						break;
					}
				}

				printf("Server is in the candidate state\n");
				break;
			case LEADER:
				/* TODO: Implement Leader case */

				printf("Server is in the leader state\n");
				break;
			default:
				perror("Invalid server state");
		}
		sleep(2); /* TODO: remove */
	}

	return 0;
}
