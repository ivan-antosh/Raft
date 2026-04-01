#include <stdio.h>
#include <stdlib.h>
#include <list.h>

/* Gloabal variable definitions defined here
 * to keep them out of the header file
 */

List *LIST_FREE_HEAD = NULL;
Node *NODE_FREE_HEAD = NULL;

List *LIST_POOL = NULL;
Node *NODE_POOL = NULL;

int CURR_LIST_POOL_AMOUNT = 0;
int CURR_NODE_POOL_AMOUNT = 0;

int USED_LISTS = 0;
int USED_NODES = 0;

/* Internal check if pools have been initialized yet */
int hasInitPools = 0;

/* Doubles the lists in the List Pool
 * Output: pointer to new List Pool after realloc
 * Post: LIST_POOL may be moved to different memory. The size of LIST_POOL is
 *	doubled. FREE_LIST_HEAD and CURR_LIST_POOL_AMOUNT are updated. The new
 *	space is filled with empty Lists
 */
List *DoubleLists() {
	int i;
	List *newListPool;
	size_t listSize = sizeof(List);
	int newListPoolAmount = CURR_LIST_POOL_AMOUNT * 2;

	/* double the pool size */
	newListPool = realloc(LIST_POOL, listSize * newListPoolAmount);
	if (newListPool == NULL) {
		printf("Error: couldn't double List size\n");
		return LIST_POOL;
	}

	/* initialize all new Lists in pool */
	for (i = CURR_LIST_POOL_AMOUNT; i < newListPoolAmount; i++) {
		List *currList = &newListPool[i];
		currList->count = 0;
		currList->head = NULL;
		currList->tail = NULL;
		currList->currNode = NULL;
		currList->isUsed = 0;
		if (i+1 < newListPoolAmount) {
			currList->nextFree = &newListPool[i+1];
		} else {
			currList->nextFree = NULL;
		}
	}
	/* set the free head and new pool amount */
	LIST_FREE_HEAD = &newListPool[CURR_LIST_POOL_AMOUNT];
	CURR_LIST_POOL_AMOUNT = newListPoolAmount;

	return newListPool;
}

List *ListCreate() {
	List *list;
	
	/* Initialize List and Node pools for List library
	 * if it has not been done yet
	 */
	if (!hasInitPools) {
		int i;

		/* only malloc call made her on initialization */
		LIST_POOL = (List *)malloc(MIN_LISTS * sizeof(List));
		NODE_POOL = (Node *)malloc(MIN_NODES * sizeof(Node));

		/* setup lists */
		for (i = 0; i < MIN_LISTS; i++) {
			List *currList = &LIST_POOL[i];
			currList->count = 0;
			currList->head = NULL;
			currList->tail = NULL;
			currList->currNode = NULL;
			currList->isUsed = 0;
			if (i+1 < MIN_LISTS) {
				currList->nextFree = &LIST_POOL[i+1];
			} else {
				currList->nextFree = NULL;
			}
		}
		LIST_FREE_HEAD = &LIST_POOL[0];
		CURR_LIST_POOL_AMOUNT = MIN_LISTS;

		/* setup nodes */
		for (i=0; i< MIN_NODES; i++) {
			Node *currNode = &NODE_POOL[i];
			currNode->item = NULL;
			currNode->next = NULL;
			currNode->previous = NULL;
			currNode->isUsed = 0;
			if (i+1 < MIN_NODES) {
				currNode->nextFree = &NODE_POOL[i+1];
			} else {
				currNode->nextFree = NULL;
			}
		}
		NODE_FREE_HEAD = &NODE_POOL[0];
		CURR_NODE_POOL_AMOUNT = MIN_NODES;

		hasInitPools = 1;
	}

	list = LIST_FREE_HEAD;
	/* if list is NULL, then that means we
	 * have no remaining free Lists to give
	 */
	if (list == NULL) {
		LIST_POOL = DoubleLists();
		list = LIST_FREE_HEAD;
	}
	LIST_FREE_HEAD = list->nextFree;
	
	/* set list as used */
	list->nextFree = NULL;
	list->isUsed = 1;
	USED_LISTS+=1;

	return list;
}

int ListCount(List *list) {
	if (list == NULL) {
		printf("Error in procedure ListCount: invalid parameter list\n");
		return 0;
	}

	return list->count;
}

void *ListSearch(List *list, Comparator comparator, void *comparisonArg) {
	Node *currNode;
	void *currItem;
	int found;

	if (list == NULL) {
		printf("Error in procedure ListSearch: "
			"invalid parameter list\n");
		return NULL;
	} else if (comparator == NULL) {
		printf("Error in procedure ListSearch: "
			"invalid parameter comparator\n");
		return NULL;
	} else if (comparisonArg == NULL) {
		printf("Error in procedure ListSearch: "
			"invalid parameter comaprisonArg\n");
		return NULL;
	}

	/* start at the head of list. traverse to the end calling the comparator
	 * on each item, and return the item found
	 */
	currNode = list->head;
	while (currNode != NULL) {
		/* sets the current node on current iteration to leave it where we
		 * find a match
		 */
		list->currNode = currNode;
		currItem = currNode->item;
		found = (*comparator)(currItem, comparisonArg);
		if (found) {
			return currItem;
		}
		currNode = currNode->next;
	}

	return NULL;
}

