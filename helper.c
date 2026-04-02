#include <types.h>
#include <string.h>
#include <stdlib.h>

#include <signal.h>
#include <pthread.h>

#include <helper.h>

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
	free(state);
}

/* Sends SIGKILL to all threads inside an array of pthread_t 
 * Return: 0 on success and 1 on failure
 */
int killThreads(pthread_t *threads, int threadCount) {
	for (int i = 0; i < threadCount; i++) {
		if (pthread_kill(threads[i], 9) != 0){
			return 1;
		};
	}
	return 0;
}
