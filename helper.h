#ifndef HELPER_H
#define HELPER_H

int StateEntryKeyComparator(void *item, void *comparisonArg);
void StateEntryFree(void *itemToBeFreed);

int killThreads(pthread_t *threads, int threadCount);

LogEntry *getMsgEntries(int s, size_t totalBytesToRec);
int sendMsgEntries(int s, LogEntry *entries, size_t totalBytesToSend);

#endif
