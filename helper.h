#ifndef HELPER_H
#define HELPER_H

#include <request_vote.h>
#include <pthread.h>

/* used for writing/reading persistent state */
/* will also w/r log entries separately */
typedef struct {
	int currentTerm;
	int votedFor;
	int numEntries;
} PersistentState;

int StateEntryKeyComparator(void *item, void *comparisonArg);
void StateEntryFree(void *itemToBeFreed);

LogEntry *getMsgEntries(int s, size_t totalBytesToRec);
int sendMsgEntries(int s, LogEntry *entries, size_t totalBytesToSend);

int writeState(int id, int currentTerm, int votedFor, LogEntry *entries, int numEntries, pthread_mutex_t *stateLock);
LogEntry *readState(int id, int *currentTerm, int *votedFor, int *numEntries);

int mapHeaderToInt(RPCMsgType msgType, RPCType type);

int get_listener_socket(char *portNum);
void *get_in_addr(struct sockaddr *sa);
void close_connection(int s, fd_set *master, ServerInfo *serverInfo, pthread_mutex_t *lock);

int proxyEnabled();

#endif
