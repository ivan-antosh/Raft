#include <stdio.h>
#include <stdlib.h>
#include <list.h>

/* Doubles the nodes in the Node Pool
 * Ouput: pointer to new Node Pool after realloc
 * Post: NODE_POOL may be moved to different memory. The size of NODE_POOL is
 *	doubled. FREE_NODE_HEAD and CURR_NODE_POOL_AMOUNT are updated. The new
 *	space if filled with empty Nodes
 */
Node *DoubleNodes() {
	int i;
	Node *newNodePool;
	size_t nodeSize = sizeof(Node);
	int newNodePoolAmount = CURR_NODE_POOL_AMOUNT * 2;

	/* double the pool size */
	newNodePool = realloc(NODE_POOL, nodeSize * newNodePoolAmount);
	if (newNodePool == NULL) {
		printf("Error: couldn't double the Node size\n");
		return NODE_POOL;
	}

	/* initialize all new Nodes in pool */
	for (i = CURR_NODE_POOL_AMOUNT; i < newNodePoolAmount; i++) {
		Node *currNode = &newNodePool[i];
		currNode->item = NULL;
		currNode->next = NULL;
		currNode->previous = NULL;
		currNode->isUsed = 0;
		if (i+1 < newNodePoolAmount) {
			currNode->nextFree = &newNodePool[i+1];
		} else {
			currNode->nextFree = NULL;
		}
	}

	/* set the free head and pool amount */
	NODE_FREE_HEAD = &newNodePool[CURR_NODE_POOL_AMOUNT];
	CURR_NODE_POOL_AMOUNT = newNodePoolAmount;

	return newNodePool;
}

int ListAdd(List *list, void *item) {
	Node *newNode;
	Node *currNode;
	Node *nextNode;

	if (list == NULL) {
		printf("Error in procedure ListAdd: invalid parameter list\n");
		return -1;
	} else if (item == NULL) {
		printf("Error in procedure ListAdd: invalid parameter item\n");
		return -1;
	}

	/* get a free node. if there is none, double the size */
	newNode = NODE_FREE_HEAD;
	if (newNode == NULL) {
		NODE_POOL = DoubleNodes();
		newNode = NODE_FREE_HEAD;
	}
	NODE_FREE_HEAD = newNode->nextFree;
	newNode->item = item;
	newNode->nextFree = NULL;
	newNode->isUsed = 1;
	USED_NODES+=1;
	
	/* if there are no nodes in the list,
	 * then we can just set count/head/tail/currNode and exit
	 */
	if (list->count == 0) {
		list->count+=1;
		list->head = newNode;
		list->tail = newNode;
		list->currNode = newNode;
		return 0;
	}

	/* set curr node and next node if there is one */
	currNode = list->currNode;
	nextNode = NULL;
	if (currNode != list->tail) {
		nextNode = currNode->next;
	}

	currNode->next = newNode;
	newNode->previous = currNode;
	/* if there is a next node, set a link to it, otherwise set current node
	 * as tail. then set current node to new node
	 */
	if (nextNode != NULL) {
		newNode->next = nextNode;
		nextNode->previous = newNode;
	} else {
		list->tail = newNode;
	}
	list->currNode = newNode;
	list->count+=1;

	return 0;
}

int ListInsert(List *list, void *item) {
	Node *newNode;
	Node *currNode;
	Node *prevNode;

	if (list == NULL) {
		printf("Error in procedure ListInsert: invalid parameter list\n");
		return -1;
	} else if (item == NULL) {
		printf("Error in procedure ListInsert: invalid parameter item\n");
		return -1;
	}
	
	/* get a free node. if there is none, double the size */
	newNode = NODE_FREE_HEAD;
	if (newNode == NULL) {
		NODE_POOL = DoubleNodes();
		newNode = NODE_FREE_HEAD;
	}
	NODE_FREE_HEAD = newNode->nextFree;
	newNode->item = item;
	newNode->nextFree = NULL;
	newNode->isUsed = 1;
	USED_NODES+=1;
	
	/* if there are no nodes in the list,
	 * then we can just set count/head/tail/currNode and exit
	 */
	if (list->count == 0) {
		list->count+=1;
		list->head = newNode;
		list->tail = newNode;
		list->currNode = newNode;
		return 0;
	}

	/* set current node and prev node if there is one */
	currNode = list->currNode;
	prevNode = NULL;
	if (currNode != list->head) {
		prevNode = currNode->previous;
	}

	currNode->previous = newNode;
	newNode->next = currNode;
	/* if there is a previous node, set the links to it, otherwise set new
	 * node as head
	 */
	if (prevNode != NULL) {
		newNode->previous = prevNode;
		prevNode->next = newNode;
	} else {
		list->head = newNode;
	}
	list->currNode = newNode;
	list->count+=1;

	return 0;
}

int ListAppend(List *list, void *item) {
	Node *newNode;
	Node *tail;
	
	if (list == NULL) {
		printf("Error in procedure ListAppend: invalid parameter list\n");
		return -1;
	} else if (item == NULL) {
		printf("Error in procedure ListAppend: invalid parameter item\n");
		return -1;
	}

	/* get a free node. if there is none, double the size */
	newNode = NODE_FREE_HEAD;
	if (newNode == NULL) {
		NODE_POOL = DoubleNodes();
		newNode = NODE_FREE_HEAD;
	}
	NODE_FREE_HEAD = newNode->nextFree;
	newNode->item = item;
	newNode->nextFree = NULL;
	newNode->isUsed = 1;
	USED_NODES+=1;

	/* check if there is a tail. if there is, then link it to the new node,
	 * otherwise set head to new node (since it means its the only node)
	 * then, set it as tail and current node
	 */
	tail = list->tail;
	if (tail != NULL) {
		tail->next = newNode;
		newNode->previous = tail;
	} else {
		list->head = newNode;
	}
	list->tail = newNode;
	list->currNode = newNode;
	list->count+=1;

	return 0;
}

int ListPrepend(List *list, void *item) {
	Node *newNode;
	Node *head;

	if (list == NULL) {
		printf("Error in procedure ListPrepend: invalid parameter list\n");
		return -1;
	} else if (item == NULL) {
		printf("Error in procedure ListPrepend: invalid parameter item\n");
		return -1;
	}

	/* get a free node. if there is none, double the size */
	newNode = NODE_FREE_HEAD;
	if (newNode == NULL) {
		NODE_POOL = DoubleNodes();
		newNode = NODE_FREE_HEAD;
	}
	NODE_FREE_HEAD = newNode->nextFree;
	newNode->item = item;
	newNode->nextFree = NULL;
	newNode->isUsed = 1;
	USED_NODES+=1;

	/* check if there is a head. if there is, then link it to the new node,
	 * otherwise set tail to new node (since it means its the only node)
	 * then, set it as head and current node
	 */
	head = list->head;
	if (head != NULL) {
		head->previous = newNode;
		newNode->next = head;
	} else {
		list->tail = newNode;
	}
	list->head = newNode;
	list->currNode = newNode;
	list->count+=1;
	
	return 0;
}

