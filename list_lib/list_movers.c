#include <stdio.h>
#include <list.h>

void *ListFirst(List *list) {
	if (list == NULL) {
		printf("Error in procedure ListFirst: invalid parameter list\n");
		return NULL;
	}

	/* the first item in the list is the head. set current node to it aswell */
	list->currNode = list->head;
	if (list->currNode == NULL) {
		return NULL;
	}
	return list->currNode->item;
}

void *ListLast(List *list) {
	if (list == NULL) {
		printf("Error in procedure ListLast: invalid parameter list\n");
		return NULL;
	}

	/* the last item in the list is the tail. set current node to it aswell */
	list->currNode = list->tail;
	if (list->currNode == NULL) {
		return NULL;
	}
	return list->currNode->item;
}

void *ListNext(List *list) {
	if (list == NULL) {
		printf("Error in procedure ListNext: invalid parameter list\n");
		return NULL;
	}

	/* if there is no current node or a node after current node, then there
	 * is no next item and we can return NULL
	 */
	if (list->currNode == NULL || list->currNode->next == NULL) {
		return NULL;
	}
	
	/* get the item after the current node and set the current node to it */
	list->currNode = list->currNode->next;
	return list->currNode->item;
}

void *ListPrev(List *list) {
	if (list == NULL) {
		printf("Error in procedure ListPrev: invalid parameter list\n");
		return NULL;
	}

	/* if there is no current node or a node before current node, then there
	 * is no prev item and we can return NULL
	 */
	if (list->currNode == NULL || list->currNode->previous == NULL) {
		return NULL;
	}

	/* get the item before the current node and set the current node to it */
	list->currNode = list->currNode->previous;
	return list->currNode->item;
}

void *ListCurr(List *list) {
	if (list == NULL) {
		printf("Error in procedure ListCurr: invalid parameter list\n");
		return NULL;
	}

	/* just get the current node item if it exists. Dont need to change
	 * anything in list
	 */
	if (list->currNode == NULL) {
		return NULL;
	}
	return list->currNode->item;
}

