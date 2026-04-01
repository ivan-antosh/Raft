#include <stdio.h>
#include <stdlib.h>
#include <list.h>

/* Halves the node pool
 * Output: Node pointer to the new pool
 * Post: sets NODE_FREE_HEAD to the new head. Changes the NODE_POOL contents
 */
Node *HalfNodes() {
	int i, j, k;
	Node *currNode, *unusedNode, *nextNode;
	Node *newNodePool;
	List *currList;
	size_t nodeSize = sizeof(Node);
	int newNodePoolAmount = CURR_NODE_POOL_AMOUNT / 2;

	/* loop through lists and check their nodes. If any node is out of the
	 * new pool range (first half), then move it to the first freed pool
	 * spot. then update the List/Node pointers referencing the moved Node
	 */
	for (i = 0; i < CURR_LIST_POOL_AMOUNT; i++) {
		currList = &LIST_POOL[i];
		if (currList->count > 0) {
			currNode = currList->head;
			while (currNode != NULL) {
				/* find Node in its Pool. Since we start at the halfway
				 * point, if its found, that means its out of the range of
				 * the new halfed pool and needs to be moved
				 */
				for (j = newNodePoolAmount; j < CURR_NODE_POOL_AMOUNT; j++) {
					if ((&NODE_POOL[j]) == currNode) {
						/* find an unused Node in the Pool that's in range
						 * (first half) to replace with the currNode
						 */
						for (k = 0; k < newNodePoolAmount; k++) {
							if (!(NODE_POOL[k].isUsed)) {
								unusedNode = &NODE_POOL[k];
								unusedNode->item = currNode->item;
								unusedNode->next = currNode->next;
								unusedNode->previous = currNode->previous;
								unusedNode->isUsed = 1;
								unusedNode->nextFree = NULL;

								/* fix List and Node pointers referencing
								 * the old Node position
								 */
								if (currList->head == currNode) {
									currList->head = unusedNode;
								}
								if (currList->tail == currNode) {
									currList->tail = unusedNode;
								}
								if (currList->currNode == currNode) {
									currList->currNode = unusedNode;
								}
								if (unusedNode->next != NULL) {
									unusedNode->next->previous = unusedNode;
								}
								if (unusedNode->previous != NULL) {
									unusedNode->previous->next = unusedNode;
								}

								break;
							}
						}
						break;
					}
				}
				nextNode = currNode->next;
				currNode = nextNode;
			}
		}
	}

	/* Now that all Nodes are in the first half of the Pool, we can half
	 * the Pool without removing any data
	 */
	newNodePool = realloc(NODE_POOL, nodeSize * newNodePoolAmount);
	if (newNodePool == NULL) {
		printf("Error: couldn't half the Node size\n");
		return NODE_POOL;
	}

	/* still have a problem with freed Nodes in the new Pool not linking
	 * correctly, since we used some moving them over, and removed some when
	 * reallocing. Reset the freed Nodes
	 */
	NODE_FREE_HEAD = NULL;
	for (i = 0; i < newNodePoolAmount; i++) {
		if (!(newNodePool[i].isUsed)) {
			currNode = &newNodePool[i];
			currNode->nextFree = NODE_FREE_HEAD;
			NODE_FREE_HEAD = currNode;
		}
	}
	/* set the pool amount to the new amount */
	CURR_NODE_POOL_AMOUNT = newNodePoolAmount;
	
	return newNodePool;
}

/* Halves the list pool
 * Output: List pointer to the new pool
 * Post: sets LIST_FREE_HEAD to the new head. Changes the LIST_POOL contents
 */
List *HalfLists() {
	int i, j;
	List *currList, *unusedList;
	List *newListPool;
	size_t listSize = sizeof(List);
	int newListPoolAmount = CURR_LIST_POOL_AMOUNT / 2;

	/* loop through the lists in the second half of the pool to move to the
	 * first half, so they stay when it reallocs
	 */
	for (i = newListPoolAmount; i < CURR_LIST_POOL_AMOUNT; i++) {
		currList = &LIST_POOL[i];
		if (currList->isUsed) {
			/* find an unused list in the first half to switch with the
			 * current list
			 */
			for (j = 0; j < newListPoolAmount; j++) {
				if (!(LIST_POOL[j].isUsed)) {
					unusedList = &LIST_POOL[j];
					unusedList->currNode = currList->currNode;
					unusedList->head = currList->head;
					unusedList->tail = currList->tail;
					unusedList->count = currList->count;
					unusedList->isUsed = 1;
					unusedList->nextFree = NULL;
				}
			}
		}
	}

	/* Now that all Lists are in the first half of the Pool, we can half
	 * the Pool without removing any data
	 */
	newListPool = realloc(LIST_POOL, listSize * newListPoolAmount);
	if (newListPool == NULL) {
		printf("Error: couldn't half the List size\n");
		return LIST_POOL;
	}

	/* still have a problem with freed Lists in the new Pool not linking
	 * correctly, since we used some moving them over, and removed some when
	 * reallocing. Reset the freed Lists
	 */
	for (i = 0; i < newListPoolAmount; i++) {
		if (!(LIST_POOL[i].isUsed)) {
			currList = &newListPool[i];
			currList->nextFree = LIST_FREE_HEAD;
			LIST_FREE_HEAD = currList;
		}
	}	
	/* set the pool amount to the new amount */
	CURR_LIST_POOL_AMOUNT = newListPoolAmount;

	return newListPool;
}

void *ListRemove(List *list) {
	Node *currNode;
	Node *nextNode, *prevNode;
	void *currItem;

	if (list == NULL) {
		printf("Error in procedure ListRemove: invalid parameter list\n");
		return NULL;
	}

	currNode = list->currNode;
	if (currNode == NULL) {
		printf("Error: no current item in list\n");
		return NULL;
	}
	nextNode = currNode->next;
	prevNode = currNode->previous;

	/* Based on if currNode has neighbors:
	 * 1. has next and prev: then link neighbors together and set currNode to
	 *	next
	 * 2. only has next: then at start of list, so need to set next's prev to
	 *	NULL, reset list's HEAD, and move currentNode
	 * 3. only has prev: the at end of list, so need to set prev's next to
	 *	NULL, reset list's TAIL, and move currNode
	 * 4. no neighbors: then in list with 1 item, so set everything in list to
	 *	NULL
	 *
	 * then decrease the list count
	 */
	if (prevNode != NULL && nextNode != NULL) {
		nextNode->previous = prevNode;
		prevNode->next = nextNode;
		list->currNode = nextNode;
	} else if (nextNode != NULL) {
		nextNode->previous = NULL;
		list->head = nextNode;
		list->currNode = nextNode;
	} else if (prevNode != NULL) {
		prevNode->next = NULL;
		list->tail = prevNode;
		list->currNode = prevNode;
	} else {
		list->head = NULL;
		list->tail = NULL;
		list->currNode = NULL;
	}
	list->count-=1;
	
	/* then need to free the Node */
	currNode->nextFree = NODE_FREE_HEAD;
	NODE_FREE_HEAD = currNode;
	currItem = currNode->item;
	currNode->item = NULL;
	currNode->next = NULL;
	currNode->previous = NULL;
	currNode->isUsed=0;
	USED_NODES-=1;

	/* if there is less than half Nodes remaining in the pool, half the pool */
	if (CURR_NODE_POOL_AMOUNT != MIN_NODES &&
		USED_NODES < (CURR_NODE_POOL_AMOUNT / 2)) {
		NODE_POOL = HalfNodes();
	}

	return currItem;
}

void ListConcat(List *list1, List *list2) {
	Node *l1Head, *l1Tail, *l2Head, *l2Tail;
	if (list1 == NULL) {
		printf("error in procedure ListConcat: invalid parameter list1\n");
		return;
	} else if (list2 == NULL) {	
		printf("error in procedure ListConcat: invalid parameter list2\n");
		return;
	}

	l1Head = list1->head;
	l1Tail = list1->tail;
	l2Head = list2->head;
	l2Tail = list2->tail;

	/* connect l1 TAIL with l2 HEAD to concatenate */
	if (l1Tail != NULL) {
		l1Tail->next = l2Head;
	}
	if (l2Head != NULL) {
		l2Head->previous = l1Tail;
	}

	/* setup the concatenated list's HEAD, TAIL, and currNode */
	if (l1Head == NULL) {
		list1->head = l2Head;
	}
	if (l2Tail != NULL) {
		list1->tail = l2Tail;
	}
	if (list1->currNode == NULL) {
		list1->currNode = list2->currNode;
	}

	/* set the concatenated list's count to the sum of the 2 lists */
	list1->count+=list2->count;

	/* free list2 */
	list2->nextFree = LIST_FREE_HEAD;
	LIST_FREE_HEAD = list2;
	list2->head = NULL;
	list2->tail = NULL;
	list2->currNode = NULL;
	list2->count = 0;
	list2->isUsed = 0;
	USED_LISTS-=1;

	/* if there is less than half Lists remaining in the pool, half the pool */
	if (CURR_LIST_POOL_AMOUNT != MIN_LISTS &&
		USED_LISTS < (CURR_LIST_POOL_AMOUNT / 2)) {
		LIST_POOL = HalfLists();
	}

	return;
}

void ListFree(List *list, ItemFree itemFree) {
	Node *currNode, *nextNode;

	if (list == NULL) {
		printf("Error in procedure ListFree: invalid parameter list\n");
		return;
	} else if (itemFree == NULL) {
		printf("Error in procedure ListFree: invalid parameter itemFree\n");
		return;
	}

	/* free all nodes in list */
	currNode = list->head;
	while (currNode != NULL) {
		(*itemFree)(currNode->item);
		nextNode = currNode->next;
		
		currNode->nextFree = NODE_FREE_HEAD;
		NODE_FREE_HEAD = currNode;
		currNode->item = NULL;
		currNode->next = NULL;
		currNode->previous = NULL;
		currNode->isUsed = 0;	
		USED_NODES-=1;

		/* if there is less than half Nodes remaining in the pool,
		 * half the pool
		 */
		if (CURR_NODE_POOL_AMOUNT != MIN_NODES &&
			USED_NODES < (CURR_NODE_POOL_AMOUNT / 2)) {
			NODE_POOL = HalfNodes();
		}

		currNode = nextNode;
	}

	/* free list itself */
	list->nextFree = LIST_FREE_HEAD;
	LIST_FREE_HEAD = list;
	list->currNode = NULL;
	list->head = NULL;
	list->tail = NULL;
	list->count = 0;
	list->isUsed = 0;
	USED_LISTS-=1;

	/* if there is less than half Lists remaining in the pool, half the pool */
	if (CURR_LIST_POOL_AMOUNT != MIN_LISTS &&
		USED_LISTS < (CURR_LIST_POOL_AMOUNT / 2)) {
		LIST_POOL = HalfLists();
	}

	return;
}

void *ListTrim(List *list) {
	Node *tail, *prevNode;
	void *tailItem;

	if (list == NULL) {
		printf("Error in procedure ListTrim: invalid parameter list\n");
		return NULL;
	}

	tail = list->tail;
	if (tail == NULL) {
		printf("Error: no last item in list\n");
		return NULL;
	}
	prevNode = tail->previous;
	tailItem = tail->item;

	/* if there is more than 1 item in list, set the second last item and
	 *	currNode as the TAIL
	 * otherwise, set TAIL, HEAD, and currNode to NULL
	 *
	 * then, decrease count by 1
	 */
	if (prevNode != NULL) {
		prevNode->next = NULL;
		list->tail = prevNode;
		list->currNode = prevNode;
	} else {
		list->tail = NULL;
		list->head = NULL;
		list->currNode = NULL;
	}
	list->count-=1;

	/* free the tail */
	tail->nextFree = NODE_FREE_HEAD;
	NODE_FREE_HEAD = tail;
	tail->item = NULL;
	tail->next = NULL;
	tail->previous = NULL;
	tail->isUsed = 0;
	USED_NODES-=1;

	/* if there is less than half Nodes remaining in the pool, half the pool */
	if (CURR_NODE_POOL_AMOUNT != MIN_NODES &&
		USED_NODES < (CURR_NODE_POOL_AMOUNT / 2)) {
		NODE_POOL = HalfNodes();
	}

	return tailItem;
}

