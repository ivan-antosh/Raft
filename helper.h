#ifndef HELPER_H
#define HELPER_H

#include <request_vote.h>

/* used for writing/reading persistent state */
/* will also w/r log entries separately */
typedef struct {
	int currentTerm;
	int votedFor;
	int numEntries;
} PersistentState;

int StateEntryKeyComparator(void *item, void *comparisonArg);
void StateEntryFree(void *itemToBeFreed);

int killThreads(pthread_t *threads, RequestVoteArgs **threadArgs, int threadCount);

LogEntry *getMsgEntries(int s, size_t totalBytesToRec);
int sendMsgEntries(int s, LogEntry *entries, size_t totalBytesToSend);

int writeState(int id, int currentTerm, int votedFor, LogEntry *entries, int numEntries);
LogEntry *readState(int id, int *currentTerm, int *votedFor, int *numEntries);

#endif
