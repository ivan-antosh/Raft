#include <types.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <netdb.h>

#include <signal.h>
#include <pthread.h>
#include <sys/socket.h>

#include <helper.h>

#define FILENAME_LEN 64

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
	free(state);
}

/* Sends SIGKILL to all threads inside an array of pthread_t 
 * Return: 0 on success and 1 on failure
 */
int killThreads(pthread_t *threads, RequestVoteArgs **threadArgs, int threadCount) {
	for (int i = 0; i < threadCount; i++) {
		/* if thread is still running, cancel it */
		if(pthread_kill(threads[i], 0) == 0) {
			if (pthread_cancel(threads[i]) != 0) {
				perror("pthread_cancel");
				return 1;
			};
			pthread_join(threads[i], NULL); /* make sure to join on cancelled thread */
		}
		/* free args */
		if(threadArgs[i] != NULL) {
			free(threadArgs[i]);
			threadArgs[i] = NULL;
		}
	}
	return 0;
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

/* send LogEntries bytes */
int sendMsgEntries(int s, LogEntry *entries, size_t totalBytesToSend) {
	size_t bytesSent = 0;
	int check;
	char *buffer = (char *)entries;
    
	while(bytesSent < totalBytesToSend) {
		check = send(s, (buffer + bytesSent), (totalBytesToSend - bytesSent), 0);
		if(check <= 0) {
			printf("Error: did not send enough bytes for entries\n");
			return -1;
		}
		bytesSent += check;
	}

	return 0;
}

/* write a servers state (current term, voted for, log entries) to a binary file (state<id>.bin)
 * return 0 on success, -1 of fail
 */
int writeState(int id, int currentTerm, int votedFor, LogEntry *entries, int numEntries) {
	char filename[FILENAME_LEN];
	snprintf(filename, sizeof(filename), "state%d.bin", id);
	/* open file for writing */
	FILE *f = fopen(filename, "wb");
	if(!f) {
		perror("fopen");
		return -1;
	}

	PersistentState state;
	state.currentTerm = currentTerm;
	state.votedFor = votedFor;
	state.numEntries = numEntries;
	/* save state */
	if(fwrite(&state, sizeof(PersistentState), 1, f) != 1) {
		printf("Error: failed to write persistent state\n");
		perror("fwrite");
		fclose(f);
		return -1;
	}
	/* save entries */
	if(numEntries > 0) {
		if(fwrite(entries, sizeof(LogEntry), numEntries, f) != numEntries) {
			printf("Error: failed to write log entries\n");
			perror("fwrite");
			fclose(f);
			return -1;
		}
	}

	fsync(fileno(f)); /* sync file */
	fclose(f);
	return 0;
}

/* read state (current term, voted for, log entries) from binary file (state<id>.bin)
 * returns Log entry pointer. Will be NULL on fail, or if there are no entries stored (if numEntries is 0)
 * stores saved term, vote, and numEntries to pointers passed as params
 */
LogEntry *readState(int id, int *currentTerm, int *votedFor, int *numEntries) {
	char filename[FILENAME_LEN];
	snprintf(filename, sizeof(filename), "state%d.bin", id);
	/* open file for reading */
	FILE *f = fopen(filename, "rb");
	if(!f) {
		perror("fopen");
		return NULL;
	}

	/* read state */
	PersistentState state;
	if(fread(&state, sizeof(PersistentState), 1, f) != 1) {
		printf("Error: failed to read persistent state\n");
		perror("fread");
		return NULL;
	}
	/* read log entries (if there are any, based on numEntries stored)*/
	LogEntry *entries = NULL;
	if(state.numEntries > 0) {
		entries = malloc(sizeof(LogEntry) * state.numEntries);
		if(!entries) {
			perror("malloc");
			fclose(f);
			return NULL;
		}
		if(fread(entries, sizeof(LogEntry), state.numEntries, f) != state.numEntries) {
			printf("Error: failed to read log entries\n");
			free(entries);
			fclose(f);
			return NULL;
		}
	}

	fclose(f);
	/* set values and return log entries */
	*currentTerm = state.currentTerm;
	*votedFor = state.votedFor;
	*numEntries = state.numEntries;
	return entries;
}

/* map RPC header vals to an int for switch case
 * 1: APPEND MSG
 * 2: APPEND REPLY
 * 3: VOTE MSG
 * 4: VOTE REPLY
 * 0: unknown
 */
int mapHeaderToInt(RPCMsgType msgType, RPCType type) {
	if(type == APPEND && msgType == MSG) {
		return 1;
	} else if(type == APPEND && msgType == REPLY) {
		return 2;
	} else if(type == VOTE && msgType == MSG) {
		return 3;
	} else if(type == VOTE && msgType == REPLY) {
		return 4;
	} else {
		return 0;
	}
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
