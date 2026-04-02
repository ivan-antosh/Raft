#ifndef HELPER_H
#define HELPER_H

int StateEntryKeyComparator(void *item, void *comparisonArg);
void StateEntryFree(void *itemToBeFreed);

int killThreads(pthread_t *threads, int threadCount);

#endif
