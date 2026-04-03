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
#include <pthread.h>

#include <append_entries.h>
#include <request_vote.h>
#include <types.h>
#include <helper.h>
#include "list_lib/list.h"

/* STATE INFO */

/* persistent on all servers */
/* lastest term server has seen */
int currentTerm = 0;
/* candidateId that received vote in current term, NULL if none */
int *votedFor = NULL;
/* log entries, command for state machine and term when entry received by leader */
LogEntry *logEntries; /* first index is 1 */
int logEntriesSize = 10; /* size of logEntries, increases when runs out of space */
int logEntryIndex = 0; /* index of last log in logEntries, zero indicate no logs, first index at 1 */
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

/* Double log entry space */
void increaseLogEntries() {
	logEntriesSize *= 2; /* double size */
	LogEntry *newPtr = (LogEntry *)reallocarray(logEntries, logEntriesSize, sizeof(LogEntry));
	if(newPtr == NULL) {
		printf("Error: unable to realloc array for log entries\n");
		perror("realloc");
		exit(1);
	}
	logEntries = newPtr;
}

/* Apply oldest non-committed log based on lastApplied */
void applyOldestLog() {
	/* apply to state machine */
	lastApplied += 1;
	Command cmd = logEntries[lastApplied].cmd;
	char *key = cmd.x;
	int val = cmd.y;
	CommandType cmdType = cmd.type;

	StateEntry *state = ListSearch(stateMachine, StateEntryKeyComparator, key);
	switch (cmdType) {
		case PUT:
			/* PUT new state, or update existing state */
			if(state == NULL) {
				/* create new StateEntry, allocate + set new memory for item */
				StateEntry *newState = (StateEntry *)malloc(sizeof(StateEntry));
				strcpy(newState->key, key);
				newState->val = val;

				ListAppend(stateMachine, newState);
			} else {
				/* update state value */
				state->val = val;
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

/* handle an append message */
void handleAppendMsg(RPCMsg *msg, LogEntry *entries, int numEntries, RPCReplyMsg *replyMsg) {
	int term = ntohl(msg->term);
	/* int leaderId = ntohl(msg->id);*/ /* TODO: for when we do client redirection */
	int prevLogIndex = ntohl(msg->logIndex);
	int prevLogTerm = ntohl(msg->logTerm);
	int leaderCommit = ntohl(msg->leaderCommit);

	/* 1. reply false if term < currentTerm */
	/* 2. reply false if log doesnt have entry at prevLogIndex with same term */
	if(term < currentTerm || (prevLogIndex <= logEntryIndex && logEntries[prevLogIndex].term != prevLogTerm)) {
		/* reply */
		replyMsg->result = htonl(0);
		return;
	}
	replyMsg->result = htonl(1);

	/* 3. remove logs at and after a conflicting log */
	int lastNonConflictIndex = 0;
	for(int i = 1; i <= numEntries; i++) {
		int logIndex = i + prevLogIndex;
		/* check if index out of bounds */
		if(logIndex >= logEntriesSize) {
			/* past bounds, increase size to prepare for new entries */
			increaseLogEntries();
			break;
		}
		/* check if past last log */
		if(logEntryIndex < logIndex) {
			break;
		}
		LogEntry entry = logEntries[logIndex];
		/* if log entry term does not match new log entry term, remove logs */
		if(entry.term != entries[i].term) {
			memset(&logEntries[logIndex], 0, sizeof(LogEntry) * (logEntryIndex - (logIndex) + 1));
			logEntryIndex = logIndex - 1;
			break;
		}
		/* no conflict, continue loop and indicate last non conflicting index */
		lastNonConflictIndex += 1;
	}
	/* 4. append any new entries not in log */
	int numNewLogs = numEntries - lastNonConflictIndex;
	if(logEntryIndex + numNewLogs >= logEntriesSize) {
		/* new logs will exceed log entry space, increase here before continuing */
		increaseLogEntries();
	}
	/* if there are new entries from rpc to add, add them */
	if(lastNonConflictIndex != numEntries) {
		memcpy(&logEntries[logEntryIndex], &entries[lastNonConflictIndex], sizeof(LogEntry) * numNewLogs);
	}
	logEntryIndex += numNewLogs;
	/* 5. update commitIndex if leaderCommit is larger by min(leaderCommit, index of last new entry) */
	if(leaderCommit > commitIndex) {
		if(leaderCommit < logEntryIndex) {
			commitIndex = leaderCommit;
		} else {
			commitIndex = logEntryIndex;
		}
	}

	/* then update term */
	checkTerm(term);

	return;
}

/* handle a vote message */
void handleVoteMsg(RPCMsg *msg, RPCReplyMsg *replyMsg) {
	int term = ntohl(msg->term);
	int candidateId = ntohl(msg->id);
	int lastLogIndex = ntohl(msg->logIndex);
	int lastLogTerm = ntohl(msg->logTerm);

	if(term < currentTerm) {
		/* 1. dont grant vote if term is less */
		replyMsg->result = htonl(0);
	} else if((votedFor == NULL || *votedFor == candidateId) &&
		((lastLogTerm > currentTerm) || (lastLogTerm == currentTerm && lastLogIndex >= logEntryIndex))) {
		/* 2. grant vote if votedFor is null or candidateId, and candidate log is atleast as up-to-date as log */
		replyMsg->result = htonl(1);
	} else {
		/* otherwise, dont grant vote */
		replyMsg->result = htonl(0);
	}

	/* then update term */
	checkTerm(term);

	return;
}

/* rec totalBytesToRec amount of bytes, convert to LogEntry list and return */
LogEntry *getMsgEntries(int s, size_t totalBytesToRec) {
	size_t bytesRec = 0;
	int check;
	char *buffer = malloc(totalBytesToRec);
	if(!buffer) {
		perror("malloc");
		return NULL;
	}

	while(bytesRec < totalBytesToRec) {
		check = recv(s, (buffer + bytesRec), (totalBytesToRec - bytesRec), 0);
		if(check <= 0) {
			printf("Error: did not rec enough bytes for entries\n");
			free(buffer);
			return NULL;
		}
		bytesRec += check;
	}
	
	return (LogEntry *)buffer;
}

/* Receive, handle, respond to incoming message
 * Input:
 * 	s: sockfd to rec from
 *  master: master set of sockfds
 * Return: 1 if rec Append type, 0 if rec non-Append type, -1 on error
 */
int respondToRPC(int s, fd_set *master) {
	RPCMsg msg;
	int check;

	/* receive msg frame */
	check = recv(s, &msg, sizeof(msg), 0);
	if (check != sizeof(msg)) {
		printf("Error: did not rec full msg\n");
		perror("recv");
		close(s);
		FD_CLR(s, master);
		return -1;
	}

	/* if frame indicates entries to rec, rec them */
	LogEntry *entries = NULL;
	int numEntries = ntohl(msg.entriesLen);
	if(numEntries > 0) {
		size_t totalBytesToRec = numEntries * sizeof(LogEntry);
		entries = getMsgEntries(s, totalBytesToRec);
		if(!entries) {
			printf("Error: expected to rec log entries, but did not rec\n");
			close(s);
			FD_CLR(s, master);
			return -1;
		}
	}
	/* TODO: remove, just for checking */
	if(entries == NULL) {
		printf("Entries: NULL");
	} else {
		for(int i = 0; i < numEntries; i++) {
			printf("Entry %d: x: %s, y: %d", i, entries[i].cmd.x, entries[i].cmd.y);
		}
	}

	/* based on rpc type, handle msg differently */
	RPCType type = ntohs(msg.rpcType);
	RPCReplyMsg replyMsg;
	replyMsg.term = htonl(currentTerm);
	int intType = 0;
	if(type == APPEND) {
		handleAppendMsg(&msg, entries, numEntries, &replyMsg);
		intType = 1;
	} else if(type == VOTE) {
		handleVoteMsg(&msg, &replyMsg);
	} else {
		printf("Error: unknown rpc type on msg rec\n");
		return -1;
	}

	/* reply */
	if(send(s, &replyMsg, sizeof(replyMsg), 0) == -1) {
		printf("Error: failed send on rpc reply\n");
		perror("send");
		close(s);
		FD_CLR(s, master);
		return -1;
	}

	return intType;
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
	
	struct timeval electionTimer;
	int electionTimerVal;

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

	/* init timers */
	electionTimerVal = (rand() % 151 + 150) * 1000; /* between 150-300 ms */
	printf("server will use election timeout of %dms", electionTimerVal);
	electionTimer.tv_sec = 0;
	electionTimer.tv_usec = electionTimerVal;

	/* init lists */
	logEntries = (LogEntry *)malloc(sizeof(LogEntry) * logEntriesSize);
	stateMachine = ListCreate();

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
				printf("Server is in the follower state\n");

				/* TODO: Respond to RPC from candidates and leaders */
				int resetTimer = 0;
				read_fds = master;
				check = select(fdmax + 1, &read_fds, NULL, NULL, &electionTimer);
				if(check == -1) {
					perror("select: read on follower");
					exit(4);
				} else if(check == 0) {
					/* timer ran out, no more fds, so no leader heartbeat was rec */
					serverStateType = CANDIDATE;
					break;
				} else {
					for(i = 0; i <= fdmax; i++) {
						if(FD_ISSET(i, &read_fds)) {
							if(i == listener) {
								handle_new_connection(i, &master, &fdmax);
							} else {
								check = respondToRPC(i, &master);
								if(check == 1) {
									resetTimer = 1;
								}
							}
						}
					}
				}
				/* if got an appendEntries call, reset timer */
				if(resetTimer) {
					electionTimer.tv_sec = 0;
					electionTimer.tv_usec = electionTimerVal;
				}

				break;
			case CANDIDATE:
				/* TODO: Implement Candidate Case */

				/* start election */
				currentTerm += 1;
				votedFor = &id;
				int votesReceived = 1;
				/* reset election timer */
				electionTimer.tv_sec = 0;
				electionTimer.tv_usec = electionTimerVal;

				/* Create threads */
				pthread_t threads[NUM_SERVERS-1];
				RequestVoteArgs *threadArgs[NUM_SERVERS-1];

				/* Send conccurent RequestVote request to all servers */
				for (int i = 0; i < (NUM_SERVERS-1); i++){
					if (servers[i].sockfd == -1){
						continue;
					}
					/* Allocate memory for thread aruguments */
					threadArgs[i] = (RequestVoteArgs *)malloc(sizeof(RequestVoteArgs));
					if (threadArgs[i] == NULL) {
						perror("Allocate memory for thread args");
					}

					threadArgs[i]->sockfd = servers[i].sockfd;
					threadArgs[i]->msg.rpcType = VOTE;
					threadArgs[i]->msg.term = currentTerm;
					threadArgs[i]->msg.id = id;
					threadArgs[i]->msg.logIndex = lastApplied;
					/* TODO: Update logTerm */
					threadArgs[i]->msg.logTerm = 0;

					if (pthread_create(&threads[i], NULL, RequestVoteThread, threadArgs[i]) != 0) {
						perror("Create thread");
					}
				}

				/* Candidate loops until either:
				 * a) Candidate receives the majority of the votes
				 * b) Another server establises itself as a leader. (AKA. receives a AppendEntries call)
				 * c) Election timeout
				 */
				for (;;){
					/* TODO: Update to non-blocking operation */
					/* check if any threads finished */
					for (i = 0; i < NUM_SERVERS-1; i++) {
						RPCReplyMsg *result = NULL;
						pthread_join(threads[i], (void *)&result);

						if (result != NULL && result->result) {
							votesReceived += 1;
							free(result);
						}

						/* Release thread argument memory */
						free(threadArgs[i]);
					}

					/* Check for majority vote */
					if (votesReceived > NUM_SERVERS/2) {
						killThreads(threads, NUM_SERVERS-1);
						serverStateType = LEADER;
						break;
					}

					/* TODO: check for AppendEntries RPC */

					/* TODO: if election timeout elapses, start new election */
				}

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
