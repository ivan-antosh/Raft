#include <types.h>
#include <string.h>
#include <stdlib.h>

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