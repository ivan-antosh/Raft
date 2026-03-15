#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#include <append_entries.h>
#include <request_vote.h>
#include <types.h>

/* STATE INFO */

/* persistent on all servers */
/* lastest term server has seen */
int currentTerm = 0;
/* candidateId that received vote in current term, NULL if none */
int *votedFor = NULL;
/* log entries, command for state machine and term when entry received by leader */
LogEntry *logEntries; /* first index is 1 */

/* volatile on all servers */
/* index of highest log entry known to be committed */
int commitIndex = 0;
/* index of highest log entry applied to state machined */
int lastApplied = 0;

/* volatile on leaders (reinitialized after election)*/
/* index of next log entry to send for each server */
int nextIndex[NUM_SERVERS]; /* init to leader last log index + 1 */
/* index of highest log entry known to be replicated on each server */
int matchIndex[NUM_SERVERS]; /* init to 0 */

/* OTHER INFO */
ServerInfo serverInfo[NUM_SERVERS - 1];


int main(int argc, char *argv[]) {
	int portNum, id;
	int i;
	int usedIds[NUM_SERVERS] = {0}; /* to ensure all ids passed are unique */

	/* arg validation */
	if(argc != (3 + (3 * (NUM_SERVERS-1)))) {
		printf("Error: invalid number of arguments\n"
			"Usage: ./router <id> <port number> (<id> <host name> <port number>) * (%d servers)\n",
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
	/* validate all server args, set serverInfo */
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

		serverInfo[i].id = otherId;
		serverInfo[i].portNum = otherPortNum;
		serverInfo[i].sockfd = -1; /* -1 until set */
		strncpy(serverInfo[i].hostname, argv[index + 1], HOST_LEN);
	}

	/* TODO: remove, just to check */
	printf("id: %d, portNum: %d\n", id, portNum);
	for(i = 0; i < NUM_SERVERS-1; i++) {
		printf("id: %d, portNum: %d, sockfd: %d, hostname: %s\n", serverInfo[i].id, serverInfo[i].portNum, serverInfo[i].sockfd, serverInfo[i].hostname);
	}

	/* TODO: attempt to connect to higher id servers, select() for listener (with timeout)
	 * once all servers connected, continue to next step
	 */

	return 0;
}
