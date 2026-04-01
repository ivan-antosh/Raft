#ifndef LIST_H
#define LIST_H

/* The MIN amount of Lists in the List library */
#define MIN_LISTS 30
/* The MIN amount of Nodes in the List library */
#define MIN_NODES 100

/* a node structure used in the List structure to hold an item */
typedef struct Node {
	/* a generic pointer to an item in the list */
	void *item;
	/* the next node it is linked to */
	struct Node *next;
	/* the previous node it is linked to */
	struct Node *previous;
	/* the next free Node. It is used when a Node gets used from the
	 * NODE_FREE_HEAD to set it's next value
	 */
	struct Node *nextFree;
	int isUsed;
} Node;

/* a doubly-linked list structure */
typedef struct List {
	/* the amount of items in the list */
	int count;
	/* the head node in the list */
	Node *head;
	/* the tail node in the list */
	Node *tail;
	/* the current node in the list. It can be manipulated through the API
	 * procedures
	 */
	Node *currNode;
	/* the next free List. It is used when a List gets used from the
	 * LIST_FREE_HEAD to set it's next value
	 */
	struct List *nextFree;
	int isUsed;
} List;

/* a pointer to the first free Node */
extern Node *NODE_FREE_HEAD;
/* a pointer to the first free List */
extern List *LIST_FREE_HEAD;

/* a pointer to the node pool */
extern Node *NODE_POOL;
/* a pointer to the list pool */
extern List *LIST_POOL;

/* amount of total nodes in the node pool */
extern int CURR_NODE_POOL_AMOUNT;
/* amount of total lists in the list pool */
extern int CURR_LIST_POOL_AMOUNT;

/* amount of nodes in use */
extern int USED_NODES;
/* amount of lists in use */
extern int USED_LISTS;


/* frees an item
 * Input:
 *	- itemToBeFreed: a void pointer to the item that is being freed
 * Post: the item passed in is freed
 */
typedef void (*ItemFree)(void *itemToBeFreed);

/* Compares an item to a comparisonArg
 * Input:
 *	- item: a void pointer to an item to be compared against the comparisonArg
 *	- comaprisonArg: a void pointer to a comparison argument to be compared
 *		against the item
 * Output: 1 if the item matches the comparisonArg, 0 if it doesn't
 */
typedef int (*Comparator)(void *item, void *comparisonArg);


/* Creates a new List
 * Output: pointer to a new List
 * Post: if the list/node pools haven't been initialized yet, then it
 *	initializes them
 */
List *ListCreate();

/* Gets the amount of items in a list
 * Input:
 *	- list: pointer to a List to get the amount of items from
 * Output: -1 if failed, 0 if successful
 */
int ListCount(List *list);

/* Lists the first item in a list
 * Input:
 *	- list: pointer to a list to get the first item from
 * Output: void pointer to the first item, or NULL if there is none
 * Post: the list's current item will be the first item
 */
void *ListFirst(List *list);

/* Lists the last item in a list
 * Input:
 *	- list: pointer to a list to get the last item from
 * Output: void pointer to the last item, or NULL if there is none
 * Post: the list's current item will be the last item
 */
void *ListLast(List *list);

/* Lists the next item in a list
 * Input:
 *	- list: pointer to a list to get the next item from
 * Output: void pointer to the next item based on the list's current item,
 *	or NULL if there is none
 * Post: advance the list's current item to the next item,
 *	or not if there is none
 */
void *ListNext(List *list);

/* Lists the previous item in a list
 * Input:
 *	- list: pointer to a list to get the previous item from
 * Output: void pointer to the previous item based on the list's current item,
 *	or NULL if there is none
 * Post: retreat the list's current item to the previous item,
 *	or not if there is none
 */
void *ListPrev(List *list);

/* Lists the current item in a list
 * Input:
 *	- list: pointer to a list to get the current item from
 * Output: void pointer to the current item in a list, or NULL if there is none
 */
void *ListCurr(List *list);

/* Add an item after the current item in a list
 * Input:
 *	- list: pointer to a list to add the item to
 *	- item: void pointer to an item to add to the list
 * Output: -1 if failed, 0 if successful
 * Post: set the current item in the list to the new item
 */
int ListAdd(List *list, void *item);

/* Insert an item before the current item in a list
 * Input:
 *	- list: pointer to a list to insert the item to
 *	- item: void pointer to an item to insert into the list
 * Output: -1 if failed, 0 if successful
 * Post: set the current item in the list to the new item
 */
int ListInsert(List *list, void *item);

/* Append an item to the end of a list
 * Input:
 *	- list: pointer to a list to append the item to
 *	- item: void pointer to an item to append to the list
 * Output: -1 if failed, 0 if successful
 * Post: set the current item in the list to the new item
 */
int ListAppend(List *list, void *item);

/* Prepend an item to the beginning of a list
 * Input:
 *	- list: pointer to a list to prepend the item to
 *	- item: void pointer to an item to prepend to the list
 * Output: -1 if failed, 0 if successful
 * Post: set the current item in the list to the new item
 */
int ListPrepend(List *list, void *item);

/* Remove the current item from a list
 * Input:
 *	- list: pointer to a list to remove an item from
 * Output: void pointer to the item that was removed from the list
 * Post: frees the node holding the item to be reused. Will set the list's
 *	current item to the next item, or to the previous one if there is none
 */
void *ListRemove(List *list);

/* Concatenate list2 to the end of list1
 * Input:
 *	- list1: pointer to a list to be the list that is concatenated onto
 *	- list2: pointer to a list to be the list that is concatenated with
 * Post: list1 will be the result of the concatenation, where list2 is the end
 *	of list1. Frees list2 so it can be reused, and sets it to NULL
 */
void ListConcat(List *list1, List *list2);

/* Frees a list and all it's items
 * Input:
 *	- list: pointer to a list to be freed
 *	- itemFree: a pointer to an ItemFree function that frees an item in a list
 * Post: all nodes and the list are freed to be reused. all items are freed.
 */
void ListFree(List *list, ItemFree itemFree);

/* Trims off the last item from a list
 * Input:
 *	- a pointer to a list to trim the last item from
 * Output: a void pointer to the item that was removed from the list
 * Post: frees the node holding the item so it can be reused. the list's
 *	current item will be the new last item
 */
void *ListTrim(List *list);

/* Searches a list for an item based on a comparator and comparisonArg
 * Input:
 *	- list: a pointer to a list to be searched
 *	- comparator: a pointer to a Comparator function that compares an item in
 *		the list to the comparisonArg
 *	- comparisonArg: a void pointer to a value that is comapred against to the
 *		item in the comparator.
 * Output: a void pointer to the item that was found based on the comparator,
 *	or NULL if none were found
 * Post: the list's current item is set to the item that was found, or to the
 *	last item in the list if none were found
 */
void *ListSearch(List *list, Comparator comparator, void *comparisonArg);

#endif

