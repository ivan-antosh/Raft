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

#define STDIN 0
#define DEFAULT_LOG_SIZE 10

typedef struct {
	int headerInt;
	int result;
} RPCHandlerResult;

typedef struct {
	int sockfd;
	int followerId;
	int leaderId;
} AppendEntryThreadArgs;

typedef struct {
	int sockfd;
	int id;
} RequestVoteThreadArgs;

/* STATE INFO */

/* persistent on all servers */
/* lastest term server has seen */
int currentTerm = 0;
pthread_mutex_t termLock = PTHREAD_MUTEX_INITIALIZER; /* lock for current term */
/* candidateId that received vote in current term, -1 if none */
int votedFor = -1;
/* log entries, command for state machine and term when entry received by leader */
LogEntry *logEntries; /* first index is 1 */
int logEntriesSize = DEFAULT_LOG_SIZE; /* size of logEntries, increases when runs out of space */
int logEntryIndex = 0; /* index of last log in logEntries, zero indicate no logs, first index at 1 */

/* volatile on all servers */
/* index of highest log entry known to be committed */
int commitIndex = 0;
/* index of highest log entry applied to state machined */
int lastApplied = 0;
/* current state of the server, follower, candidate, leader */
ServerStateType serverStateType = FOLLOWER;
/* doubly linked list of state machine entries, each are a key value pair */
List *stateMachine;

/* volatile on leaders (reinitialized after election)*/
/* index of next log entry to send for each server */
int nextIndex[NUM_SERVERS]; /* init to leader last log index + 1 */
/* index of highest log entry known to be replicated on each server */
int matchIndex[NUM_SERVERS]; /* init to 0 */

/* OTHER INFO */
ServerInfo servers[NUM_SERVERS - 1];
pthread_mutex_t serversLock = PTHREAD_MUTEX_INITIALIZER; /* lock for server info */
pthread_mutex_t stateLock = PTHREAD_MUTEX_INITIALIZER; /* lock for writing to state */

/* Double log entry space, or to given size */
void increaseLogEntries(int size) {
	if(size > 0) {
		logEntriesSize = size;
	} else {
		logEntriesSize *= 2; /* double size */
	}
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
			printf("Applied PUT for %s with val %d\n", key, val);
			break;
		case GET:
			/* nothing to commit for GET. Only handle as LEADER */
			if(serverStateType == LEADER) {
				if(state == NULL) {
					printf("No value for %s\n", key);
				} else {
					printf("Value for %s is %d\n", key, state->val);
				}
			}
			break;
		case DEL:
			/* delete state if there is one with the same key */
			if(state != NULL) {
				ListRemove(stateMachine);
				StateEntryFree(state);
			}
			printf("Applied DEL for %s\n", key);
			break;
	}
}

/* For RPC req OR resp, if contains term T > current term, need to update term and set to FOLLOWER
 * return 1 if new term, 0 if not
 */
int checkTerm(int term, int forceChange, int id) {
	int changed = 0;
	/* use mutex since called in threads */
	pthread_mutex_lock(&termLock);
	/* if term change or are forcing a change, update to FOLLOWER */
	if (term > currentTerm) {
		currentTerm = term; /* only set when term change */
		printf("Converting server to FOLLOWER\n");
		serverStateType = FOLLOWER;
		votedFor = -1; /* only reset votedFor on new term */
		changed = 1;
	} else if(forceChange) {
		printf("Converting server to FOLLOWER\n");
		serverStateType = FOLLOWER;
		/* no change to save for this condition */
	}
	pthread_mutex_unlock(&termLock);
	
	/* save new state if changed */
	if(changed) {
		if(writeState(id, currentTerm, votedFor, logEntries, logEntryIndex, &stateLock) == -1) {
			printf("Error: failed to write state before replying to append entries\n");
		}
	}
	return changed;
}

/* handle an append message */
void handleAppendMsg(RPCAppendMsg *msg, LogEntry *entries, int numEntries, RPCAppendReplyMsg *replyMsg, int id) {
	int term = ntohl(msg->term);
	/* int leaderId = ntohl(msg->id);*/ /* used for client redirection */
	int prevLogIndex = ntohl(msg->prevLogIndex);
	int prevLogTerm = ntohl(msg->prevLogTerm);
	int leaderCommit = ntohl(msg->leaderCommit);

	/* 1. reply false if term < currentTerm */
	/* 2. reply false if log doesnt have entry at prevLogIndex with same term */
	if(term < currentTerm || prevLogIndex > logEntryIndex || (prevLogIndex > 0 && logEntries[prevLogIndex].term != prevLogTerm)) {
		/* reply */
		replyMsg->success = htonl(0);
		return;
	}
	replyMsg->success = htonl(1);

	/* 3. remove logs at and after a conflicting log */
	int numNonConflicting = 0; /* how many entries not conflicting */
	for(int i = 0; i < numEntries; i++) {
		int curLogIndex = i + prevLogIndex + 1; /* servers logs start at index 1, and checking past prevLogIndex */
		/* if past last log, no more checking needed */
		if(curLogIndex > logEntryIndex) {
			break;
		}
		/* if log entry term does not match new log entry term, remove logs, then stop loop */
		if(logEntries[curLogIndex].term != entries[i].term) {
			memset(&logEntries[curLogIndex], 0, sizeof(LogEntry) * (logEntryIndex - curLogIndex + 1));
			logEntryIndex = curLogIndex - 1;
			break;
		}
		numNonConflicting += 1;
	}

	/* 4. append any new entries not in log */
	int numNewLogs = numEntries - numNonConflicting;
	for(;;) {
		if(logEntryIndex + numNewLogs >= logEntriesSize) {
			/* new logs will exceed log entry space, increase here before continuing */
			/* keep increasing until there is room */
			increaseLogEntries(0);
		} else {
			break;
		}
	}
	/* if there are new entries from rpc to add, add them */
	if(numNewLogs > 0) {
		memcpy(&logEntries[logEntryIndex + 1], &entries[numNonConflicting], sizeof(LogEntry) * numNewLogs);
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

	/* if there were new logs, save state */
	if(numNewLogs > 0) {
		if(writeState(id, currentTerm, votedFor, logEntries, logEntryIndex, &stateLock) == -1) {
			printf("Error: failed to write state before replying to append entries\n");
		}
	}

	return;
}

/* handle a vote message */
void handleVoteMsg(RPCVoteMsg *msg, RPCVoteReplyMsg *replyMsg, int id) {
	int term = ntohl(msg->term);
	int candidateId = ntohl(msg->candidateId);
	int lastLogIndex = ntohl(msg->lastLogIndex);
	int lastLogTerm = ntohl(msg->lastLogTerm);

	int myLastLogTerm = 0;
	if(logEntryIndex > 0) {
		myLastLogTerm = logEntries[logEntryIndex].term;
	}

	if(term < currentTerm) {
		/* 1. dont grant vote if term is less */
		replyMsg->voteGranted = htonl(0);
	} else if((votedFor == -1 || votedFor == candidateId) &&
		((lastLogTerm > myLastLogTerm) || (lastLogTerm == myLastLogTerm && lastLogIndex >= logEntryIndex))) {
		/* 2. grant vote if votedFor is null or candidateId, and candidate log is atleast as up-to-date as log */
		replyMsg->voteGranted = htonl(1);
		votedFor = candidateId;
		/* votedFor changed, save state */
		if(writeState(id, currentTerm, votedFor, logEntries, logEntryIndex, &stateLock) == -1) {
			printf("Error: failed to write state before replying to append entries\n");
		}
	} else {
		/* otherwise, dont grant vote */
		replyMsg->voteGranted = htonl(0);
	}

	return;
}

/* Receive, handle, respond to incoming message
 * Input:
 * 	s: sockfd to rec from
 *  master: master set of sockfds
 * Return: RPCHandlerResult, where headerInt is int representation of message type handled (based on mapHeaderToInt()),
 * 	and result is result int from RPC (-1 when failed)
 */
RPCHandlerResult RPCHandler(int s, fd_set *master, int id) {
	int check;
	RPCHandlerResult result = {0, 0};

	/* rec header */
	RPCHeader header;
	check = recv(s, &header, sizeof(header), 0);
	if(check != sizeof(header)) {
		if(check == 0) {
			close_connection(s, master, servers, &serversLock);
		} else if(check == -1) {
			perror("recv");
			close_connection(s, master, servers, &serversLock);
		} else {
			printf("Error: did not rec full header\n");
			perror("recv");
		}
		result.result = -1;
		return result;
	}
	RPCMsgType msgType = ntohs(header.rpcMsgType);
	RPCType type = ntohs(header.rpcType);

	int headerInt = mapHeaderToInt(msgType, type);
	result.headerInt = headerInt;
	/* rec body */
	switch(headerInt) {
		case 0:
			printf("Error: Unknown header\n");
			result.result = -1;
			return result;
		case 1: /* APPEND MSG */
			RPCAppendMsg appendMsg;
			/* rec */
			check = recv(s, &appendMsg, sizeof(appendMsg), 0);
			if (check != sizeof(appendMsg)) {
				if(check == 0) {
					close_connection(s, master, servers, &serversLock);
				} else if(check == -1) {
					perror("recv");
					close_connection(s, master, servers, &serversLock);
				} else {
					printf("Error: did not rec full append msg\n");
					perror("recv");
				}
				result.result = -1;
				return result;
			}
			/* if frame indicates entries to rec, rec them */
			int numEntries = ntohl(appendMsg.entriesLen);
			LogEntry *entries = NULL;
			if(numEntries > 0) {
				printf("GOT NUMERNTRIES %d\n", numEntries);
				size_t totalBytesToRec = numEntries * sizeof(LogEntry);
				entries = getMsgEntries(s, totalBytesToRec);
				if(!entries) {
					printf("Error: expected to rec log entries, but did not rec\n");
					close(s);
					if(master) {
						FD_CLR(s, master);
					}
					result.result = -1;
					return result;
				}
			}
			
			int msgTerm = ntohl(appendMsg.term);
			checkTerm(msgTerm, 0, id); /* will convert to FOLLOWER if new term */
			RPCAppendReplyMsg replyMsg;
			replyMsg.term = htonl(currentTerm); /* set term */
			replyMsg.success = 0; /* default success to false */
			switch(serverStateType) {
				case(FOLLOWER):
					/* handle append msg for follower */
					handleAppendMsg(&appendMsg, entries, numEntries, &replyMsg, id); /* may set success to true */
					break;
				case(CANDIDATE):
					if(msgTerm == currentTerm) {
						checkTerm(msgTerm, 1, id); /* force to FOLLOWER if APPEND MSG has equal term for candidate */
						handleAppendMsg(&appendMsg, entries, numEntries, &replyMsg, id); /* may set success to true */
					}
					break;
				case(LEADER):
					break;
			}
			if(entries != NULL) {
				free(entries);
			}

			/* reply */
			RPCHeader headerReply;
			headerReply.rpcMsgType = htons(REPLY);
			headerReply.rpcType = htons(APPEND);
			if(send(s, &headerReply, sizeof(headerReply), 0) == -1) {
				if(errno != EPIPE) {
					printf("Error: failed header send on rpc append reply\n");
					perror("send");
				}
				close_connection(s, master, servers, &serversLock);
				result.result = -1;
				return result;
			}
			if(send(s, &replyMsg, sizeof(replyMsg), 0) == -1) {
				if(errno != EPIPE) {
					printf("Error: failed msg send on rpc append reply\n");
					perror("send");
				}
				close_connection(s, master, servers, &serversLock);
				result.result = -1;
				return result;
			}

			break;
		case 2: /* APPEND REPLY */
			RPCAppendReplyMsg appendReplyMsg;
			/* rec */
			check = recv(s, &appendReplyMsg, sizeof(appendReplyMsg), 0);
			if (check != sizeof(appendReplyMsg)) {
				if(check == 0) {
					close_connection(s, master, servers, &serversLock);
				} else if(check == -1) {
					perror("recv");
					close_connection(s, master, servers, &serversLock);
				} else {
					printf("Error: did not rec full append reply msg\n");
					perror("recv");
				}
				result.result = -1;
				return result;
			}

			checkTerm(ntohl(appendReplyMsg.term), 0, id); /* will convert to FOLLOWER if new term */
			switch(serverStateType) {
				case(FOLLOWER):
				case(CANDIDATE):
					break;
				case(LEADER):
					result.result = ntohl(appendReplyMsg.success);
					break;
			}

			break;
		case 3: /* VOTE MSG */
			RPCVoteMsg voteMsg;
			/* rec */
			check = recv(s, &voteMsg, sizeof(voteMsg), 0);
			if (check != sizeof(voteMsg)) {
				if(check == 0) {
					close_connection(s, master, servers, &serversLock);
				} else if(check == -1) {
					perror("recv");
					close_connection(s, master, servers, &serversLock);
				} else {
					printf("Error: did not rec full vote msg\n");
					perror("recv");
				}
				result.result = -1;
				return result;
			}

			checkTerm(ntohl(voteMsg.term), 0, id); /* will convert to FOLLOWER if new term */
			switch(serverStateType) {
				case(FOLLOWER):
				case(CANDIDATE):
				case(LEADER):
					/* handle */
					RPCVoteReplyMsg replyMsg;
					handleVoteMsg(&voteMsg, &replyMsg, id);
					replyMsg.term = htonl(currentTerm);

					/* reply*/
					RPCHeader headerReply;
					headerReply.rpcMsgType = htons(REPLY);
					headerReply.rpcType = htons(VOTE);
					if(send(s, &headerReply, sizeof(headerReply), 0) == -1) {
						if(errno != EPIPE) {
							printf("Error: failed header send on rpc vote reply\n");
							perror("send");
						}
						close_connection(s, master, servers, &serversLock);
						result.result = -1;
						return result;
					}
					if(send(s, &replyMsg, sizeof(replyMsg), 0) == -1) {
						if(errno != EPIPE) {
							printf("Error: failed msg send on rpc vote reply\n");
							perror("send");
						}
						close_connection(s, master, servers, &serversLock);
						result.result = -1;
						return result;
					}
					break;
			}

			break;
		case 4: /* VOTE REPLY */
			RPCVoteReplyMsg voteReplyMsg;
			/* rec */
			check = recv(s, &voteReplyMsg, sizeof(voteReplyMsg), 0);
			if (check != sizeof(voteReplyMsg)) {
				if(check == 0) {
					close_connection(s, master, servers, &serversLock);
				} else if(check == -1) {
					perror("recv");
					close_connection(s, master, servers, &serversLock);
				} else {
					printf("Error: did not rec full vote reply msg\n");
					perror("recv");
				}
				result.result = -1;
				return result;
			}

			checkTerm(ntohl(voteReplyMsg.term), 0, id); /* will convert to FOLLOWER if new term */
			switch(serverStateType) {
				case(FOLLOWER):
					break;
				case(CANDIDATE):
					/* return voteGranted */
					result.result = ntohl(voteReplyMsg.voteGranted);
					break;
				case(LEADER):
					break;
			}

			break;
	}

	return result;
}

/* single time logic for when server converts to leader */
void handleNewLeader() {
	/* init nextIndex and matchIndex vals */
	for(int i = 0; i < NUM_SERVERS; i++) {
		nextIndex[i] = logEntryIndex + 1;
		matchIndex[i] = 0;
	}
}

/* Thread to handle append entry rpc calls */
void *AppendEntryThread(void *args) {
	AppendEntryThreadArgs *threadArgs = (AppendEntryThreadArgs *)args;
	int sockfd = threadArgs->sockfd;
	int followerId = threadArgs->followerId; /* id sending to */
	int leaderId = threadArgs->leaderId; /* own id */

	RPCHandlerResult handlerResult;
	int check;

	int followerNextIndex = nextIndex[followerId - 1];
	if(logEntryIndex < followerNextIndex) {
		int prevLogTerm = (followerNextIndex > 1) ? logEntries[followerNextIndex - 1].term : 0;
		/* Send heartbeat if no log entries to send (default values, w/ commitIndex) */
		check = AppendEntries(sockfd, currentTerm, leaderId, followerNextIndex - 1, prevLogTerm,
			NULL, 0, commitIndex);
		if(check < 0) {
			if(check == -1) {
				printf("Error: sending heartbeat in append entry thread for server %d\n", followerId);
			}
			close_connection(sockfd, NULL, servers, &serversLock);
			return NULL;
		}
		/* keep handling until rec an APPEND REPLY or no longer LEADER */
		for(;;) {
			handlerResult = RPCHandler(sockfd, NULL, leaderId); /* might set server to FOLLOWER */
			if(handlerResult.result == -1) {
				printf("Error: RPC handler error for heartbeat in append entries\n");
				return NULL;
			}
			if(handlerResult.headerInt == 2 || serverStateType != LEADER) {
				return NULL;
			}
		}
	}

	
	int success = 0;
	/* keep trying AppendEntries */
	while(!success) {
		/* mutex around term since other threads call checkTerm() */
		pthread_mutex_lock(&termLock);
		int currentTermSnap = currentTerm;
		pthread_mutex_unlock(&termLock);

		int numEntriesToSend = logEntryIndex - followerNextIndex + 1;
		int prevLogTerm = (followerNextIndex > 1) ? logEntries[followerNextIndex - 1].term : 0;
		check = AppendEntries(sockfd, currentTermSnap, leaderId, followerNextIndex - 1, prevLogTerm,
			&logEntries[followerNextIndex], numEntriesToSend, commitIndex);
		if(check < 0) {
			if(check == -1) {
				printf("Error: sending append entries in append entries thread for server %d\n", followerId);
			}
			close_connection(sockfd, NULL, servers, &serversLock);
			return NULL;
		}
		/* keep handling until rec an APPEND REPLY or no longer LEADER */
		for(;;) {
			handlerResult = RPCHandler(sockfd, NULL, leaderId); /* might set server to FOLLOWER */
			if(handlerResult.result == -1) {
				printf("Error: RPC handler error for heartbeat in append entries\n");
				return NULL;
			}
			if(serverStateType != LEADER) {
				return NULL;
			}
			if(handlerResult.headerInt == 2) {
				break;
			}
		}

		success = handlerResult.result;
		if(!success) {
			followerNextIndex -= 1;
			if(followerNextIndex < 1) {
				printf("decremented follower next index to less than 1, setting to 1\n");
				followerNextIndex = 1;
			}
		}
	}

	nextIndex[followerId - 1] = logEntryIndex + 1;
	matchIndex[followerId - 1] = logEntryIndex;
	return NULL;
}

/* Thread wrapper function to call RequestVote */
void *RequestVoteThread(void *args) {
	RequestVoteThreadArgs *threadArgs = (RequestVoteThreadArgs *)args;
	int sockfd = threadArgs->sockfd;
	int candidateId = threadArgs->id;

	int term = currentTerm;
	int lastLogIndex = logEntryIndex;
	int lastLogTerm = logEntryIndex > 0 ? logEntries[logEntryIndex].term : 0;

	int check = RequestVote(sockfd, term, candidateId, lastLogIndex, lastLogTerm);
	if(check < 0) {
		if(check == -1) {
			printf("Error: request vote\n");
			close_connection(sockfd, NULL, servers, &serversLock);
		}
		return NULL;
	}

	return NULL;
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
int connect_to_server(ServerInfo *serverInfo, fd_set *master, int *fdmax, int id) {
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
    msg.id = htonl(id);
    if(send(sockfd, &msg, sizeof(msg), 0) == -1) {
        perror("send");
        exit(1);
    }
	printf("Connected to %d\n", serverInfo->id);

    return 1;
}

int main(int argc, char *argv[]) {
	signal(SIGPIPE, SIG_IGN); /* have sends return -1 instead of killing process when socket is closed */

	fd_set master, stdin_fd, read_fds, write_fds;
	int fdmax;
	int listener;
	struct timeval tv;
	
	struct timeval electionTimer;
	int electionTimerVal, electionTimerVal_usec, electionTimerVal_sec;

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

	/* clear master, stdin, and temps */
    FD_ZERO(&master); 
	FD_ZERO(&stdin_fd);
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    /* add listener to master */
    listener = get_listener_socket(argv[2]);
    FD_SET(listener, &master);
    fdmax = listener; /* set fdmax */
	/* set stdin */
	FD_SET(STDIN, &stdin_fd);

	/* Connect to all servers before continuing */
	int neededConnections = (NUM_SERVERS / 2) + 1;
	for(;;) {
		printf("needed connections: %d\n", neededConnections);
		if(neededConnections <= 0) {
			break; /* got all connections */
		}
		sleep(1);
		printf("trying connections...\n");
		
		/* attempt to connect to higher id servers */
		for(i = 0; i < (NUM_SERVERS - 1); i++) {
			/* for race condition, only attempt connection for higher ids, listen for lower ids */
			if((servers[i].id < id && !proxyEnabled()) || servers[i].sockfd != -1) {
				continue;
			}
			check = connect_to_server(&servers[i], &master, &fdmax, id);
			if (check == -1) {
				printf("Error: connect to server\n");
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
				/* only check listener */
				if(i == listener) {
					check = handle_new_connection(i, &master, &fdmax);
					printf("check: %d\n", check);
					if(check == 1) {
						neededConnections -= 1;
					}
				}
			}
		}
	}

	/* init timers */
	// srand(time(NULL) ^ id);
	// electionTimerVal = (rand() % 1001 + 1000) * 1000; /* between 1000-2000 ms */
	electionTimerVal = 1000000 + (((id + NUM_SERVERS - 1) % NUM_SERVERS) * 200000);
	electionTimerVal_sec = electionTimerVal / 1000000;
	electionTimerVal_usec = electionTimerVal % 1000000;
	printf("server will use election timeout of %ds %dms\n", electionTimerVal_sec, electionTimerVal_usec / 1000);

	electionTimer.tv_sec = electionTimerVal_sec;
	electionTimer.tv_usec = electionTimerVal_usec;

	/* init values */
	/* if there is saved data, init data with it */
	logEntries = readState(id, &currentTerm, &votedFor, &logEntryIndex);
	if(!logEntries) {
		printf("No saved data. Using default starting values\n");
		logEntries = (LogEntry *)malloc(sizeof(LogEntry) * logEntriesSize);
	} else {
		logEntriesSize = logEntryIndex + 1;
		if(logEntriesSize < DEFAULT_LOG_SIZE) {
			increaseLogEntries(DEFAULT_LOG_SIZE);
		}
	}
	stateMachine = ListCreate();

	for (;;) {
		/* attempt to connect to higher id servers that have lost connection */
		for(i = 0; i < (NUM_SERVERS - 1); i++) {
			/* for race condition, only attempt connection for higher ids, listen for lower ids */
			if((servers[i].id < id && !proxyEnabled()) || servers[i].sockfd != -1) {
				continue;
			}
			check = connect_to_server(&servers[i], &master, &fdmax, id);
			if (check == -1) {
				printf("Error: connect to server\n");
				return 1;
			}
		}

		/* for all server states: */
		/* if commit index is larger than last applied, increase last applied and commit new log */
		while(commitIndex > lastApplied) {
			applyOldestLog();
		}

		/* server state specific logic: */
		switch (serverStateType) {
			case FOLLOWER:
				int resetTimer = 0;
				read_fds = master;
				//printf("follower select, with timer %ld %ld\n", electionTimer.tv_sec, electionTimer.tv_usec);
				check = select(fdmax + 1, &read_fds, NULL, NULL, &electionTimer);
				if(check == -1) {
					perror("select: read on follower");
					exit(4);
				} else if(check == 0) {
					/* timer ran out, no more fds, so no leader heartbeat was rec */
					printf("Converting server to CANDIDATE\n");
					serverStateType = CANDIDATE;
					break;
				} else {
					for(i = 0; i <= fdmax; i++) {
						if(FD_ISSET(i, &read_fds)) {
							if(i == listener) {
								handle_new_connection(i, &master, &fdmax);
							} else {
								RPCHandlerResult result = RPCHandler(i, &master, id);
								/* If handled APPEND MSG, reset election timer */
								if(result.headerInt == 1) {
									resetTimer = 1;
								}
							}
						}
					}
				}
				/* if got an appendEntries call, reset timer */
				if(resetTimer) {
					electionTimer.tv_sec = electionTimerVal_sec;
					electionTimer.tv_usec = electionTimerVal_usec;
				}

				break;
			case CANDIDATE:
				/* start election */
				currentTerm += 1;
				votedFor = id;
				int votesReceived = 1;
				/* reset election timer */
				electionTimer.tv_sec = electionTimerVal_sec;
				electionTimer.tv_usec = electionTimerVal_usec;

				/* Create threads */
				pthread_t threads[NUM_SERVERS-1];
				RequestVoteThreadArgs *threadArgs = (RequestVoteThreadArgs *)malloc(sizeof(RequestVoteThreadArgs) * (NUM_SERVERS - 1));
				if(threadArgs == NULL) {
					printf("Error: failed to alloc request vote args\n");
					perror("malloc");
					break;
				}
				int threadCreated[NUM_SERVERS - 1] = {0};
				/* Send conccurent RequestVote request to all servers */
				for (int i = 0; i < (NUM_SERVERS-1); i++){
					if (servers[i].sockfd == -1){
						continue;
					}
					threadArgs[i].sockfd = servers[i].sockfd;
					threadArgs[i].id = id;
					if (pthread_create(&threads[i], NULL, RequestVoteThread, &threadArgs[i]) != 0) {
						perror("Create request vote thread");
						continue;
					}
					threadCreated[i] = 1;
				}
				/* wait until all threads finished */
				for (i = 0; i < NUM_SERVERS-1; i++) {
					if(!threadCreated[i]) {
						continue;
					}
					pthread_join(threads[i], NULL);
				}
				/* Release thread argument memory */
				free(threadArgs);

				/* Candidate loops until either:
				 * a) Candidate receives the majority of the votes
				 * b) Another server establises itself as a leader. (AKA. receives rpc call with higher term)
				 * c) Election timeout
				 */
				for (;;){
					read_fds = master;
					check = select(fdmax + 1, &read_fds, NULL, NULL, &electionTimer);
					if(check == -1) {
						perror("select: read on candidate");
						exit(4);
					} else if(check == 0) {
						/* Event C: Election timeout elapsed, will restart CANDIDATE loop to start new election */
						break;
					} else {
						for(i = 0; i <= fdmax; i++) {
							if(FD_ISSET(i, &read_fds)) {
								if(i == listener) {
									handle_new_connection(i, &master, &fdmax);
								} else {
									RPCHandlerResult handlerResult = RPCHandler(i, &master, id);
									if(serverStateType == FOLLOWER) {
										/* Event B: Another server had a higher term, end CANDIDATE loop */
										goto done_reading;
									} else if (handlerResult.headerInt == 4 && handlerResult.result != -1) {
										if (handlerResult.result == 1) {
											votesReceived += 1;
										}
										/* Check for majority vote */
										if (votesReceived > (int)(NUM_SERVERS / 2)) {
											/* Event A: Candidate received the majority of the votes, become LEADER and end CANDIDATE loop */
											printf("Converting server to LEADER\n");
											serverStateType = LEADER;
											handleNewLeader();
											goto done_reading;
										}
									}
								}
							}
						}
					}
				}
				done_reading:
					break;
			case LEADER:
				/* read client */
				read_fds = stdin_fd;
				tv.tv_sec = 0;
				tv.tv_usec = 0;
				check = select(STDIN + 1, &read_fds, NULL, NULL, &tv);
				if(check == -1) {
					perror("select: stdin read for leader");
					exit(4);
				} else if(check) {
					char *userLine = NULL;
					size_t userLen = 0;
					int numRead;

					if((numRead = getline(&userLine, &userLen, stdin)) == -1) {
						perror("getline failed");
					} else {
						/* Remove trailing newline */
						if (userLine[numRead -1] == '\n') {
							userLine[numRead -1] = '\0';
						}

						/* convert input to command, add to local log */
						/* TODO: error checking, also prolly regex */
						if(logEntryIndex + 1 >= logEntriesSize) {
							increaseLogEntries(0);
						}
						LogEntry *entry = &logEntries[logEntryIndex + 1];
						entry->term = currentTerm;
						char *token = strtok(userLine, " ");
						int gotCommand = 1;
						if(strcmp(token, "put") == 0) {
							entry->cmd.type = PUT;
							token = strtok(NULL, " ");
							strcpy(entry->cmd.x, token);
							token = strtok(NULL, " ");
							int y = atoi(token);
							entry->cmd.y = y;
						} else if(strcmp(token, "get") == 0) {
							entry->cmd.type = GET;
							token = strtok(NULL, " ");
							strcpy(entry->cmd.x, token);
							entry->cmd.y = 0;
						} else if(strcmp(token, "del") == 0) {
							entry->cmd.type = DEL;
							token = strtok(NULL, " ");
							strcpy(entry->cmd.x, token);
							entry->cmd.y = 0;
						} else {
							gotCommand = 0;
							printf("Error: unknown command from user input\n");
						}
						if(gotCommand) {
							logEntryIndex += 1;
							if(writeState(id, currentTerm, votedFor, logEntries, logEntryIndex, &stateLock) == -1) {
								printf("Error: failed to write state before replying to append entries\n");
							}
						}
					}
					free(userLine);
				}

				/* send + handle append entries rpc calls in parallel to each other server */
				pthread_t appendEntriesThreads[NUM_SERVERS-1];
				AppendEntryThreadArgs *appendEntriesThreadArgs =
					(AppendEntryThreadArgs *)malloc(sizeof(AppendEntryThreadArgs) * (NUM_SERVERS - 1));
				if (appendEntriesThreadArgs == NULL) {
					perror("Allocate memory for append entries thread args (leader)");
					break;
				}

				int appendEntriesThreadCreated[NUM_SERVERS - 1] = {0};
				for(int i = 0; i < (NUM_SERVERS-1); i++) {
					//printf("For id %d, have socket %d\n", i, servers[i].sockfd);
					if(servers[i].sockfd == -1) {
						//printf("NOT making thread for id %d\n", i);
						continue;
					}
					appendEntriesThreadArgs[i].sockfd = servers[i].sockfd;
					appendEntriesThreadArgs[i].followerId = servers[i].id;
					appendEntriesThreadArgs[i].leaderId = id;
					/* create thread */
					if (pthread_create(&appendEntriesThreads[i], NULL, AppendEntryThread, &appendEntriesThreadArgs[i]) != 0) {
						perror("Create thread");
						continue;
					}
					appendEntriesThreadCreated[i] = 1;
				}
				/* wait until all threads finished */
				for (i = 0; i < NUM_SERVERS-1; i++) {
					if(!appendEntriesThreadCreated[i]) {
						continue;
					}
					pthread_join(appendEntriesThreads[i], NULL);
				}
				/* Release thread argument memory */
				free(appendEntriesThreadArgs);
				/* thread may have converted server to follower, if so don't continue */
				if(serverStateType == FOLLOWER) {
					break;
				}

				/* update commit index once all appendentries done */
				int n = commitIndex;
				/* find n that n > commitIndex, majority of matchIndex[i]>=n, log[n].term == currentTerm */
				/* then set commitIndex to that n */
				for(;;) {
					n += 1;
					int checkServers = 0;
					for(int i = 1; i <= NUM_SERVERS; i++) {
						/* dont check own matchIndex */
						if(i == id) {
							continue;
						}
						if(matchIndex[i-1] >= n) {
							checkServers += 1;
						}
					}
					if(checkServers < (int)(NUM_SERVERS / 2)) {
						break;
					}
					if(n > logEntryIndex || logEntries[n].term != currentTerm) {
						break;
					}
				}
				n -= 1; /* sub 1 since loop adds 1 then breaks if doesn't satisfy constraints */
				commitIndex = n; /* if no such n, then will stay the same */

				break;
			default:
				perror("Invalid server state");
		}
	}

	return 0;
}
