#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"


extern int memsize;

extern int debug;

extern struct frame *coremap;


// A base struct for building a linked list (also a stack)
typedef struct node_t {
	int data;
	struct node_t *next;
	struct node_t *prev;

} node;

// The head
node *head;

// Helper Function for linked list operation
// Check if the list contains the data
// Return the node if successful, else return NULL
node *check(int data) {

	node *curr = head;

	while (curr != NULL) {

		if (curr->data == data) {

			return curr;

		}

		curr = curr->next;

	}

	return NULL;

}



void delete(int data) {

	// Check if the list contains the data
	node *curr = check(data);
	node *prev_node;

	if (curr == NULL) {

		printf("Deleting non-existent data");

		return;

	}

	// Three cases on deletion
	if ((curr->prev != NULL) && (curr->next == NULL)) {

		// Last node of the list
		prev_node = curr->prev;
		prev_node->next = NULL;
		curr->prev = NULL;

		printf("Deleting Case 1\n");

		free(curr);

	} else if ((curr->prev != NULL) && (curr->next != NULL)) {

		// The node in the middle
		node *next_node = curr->next;
		prev_node = curr->prev;

		prev_node->next = curr->next;
		next_node->prev = curr->prev;
		curr->next = NULL;
		curr->prev = NULL;

		free(curr);

	} else if ((curr->prev == NULL) && (curr->next != NULL)) {

		// The head
		node *next_node = curr->next;

		
		next_node->prev = NULL;
		curr->next = NULL;
		head = next_node;
	

		free(curr);

	} else if ((curr->prev == NULL) && (curr->next == NULL)) {

		// A single node
		head->data = -1;


	}

	return;

}

void add(int data) {

	node *new = (node *) malloc(sizeof(node));
	new->data = data;

	node *curr = head;

	// Find the last node
	while (curr->next != NULL) {

		curr = curr->next;

	}

	// Append the new node into the list
	curr->next = new;
	new->prev = curr;
	new->next = NULL;

	return;

}

/* Page to evict is chosen using the accurate LRU algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */

int lru_evict() {

	// Delete the head
	int evict = head->data;

	assert(evict != -1);

	delete(evict);
	
	return evict;
}

/* This function is called on each access to a page to update any information
 * needed by the lru algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void lru_ref(pgtbl_entry_t *p) {

	// Find the frame number
	int frame_num = p->frame >> PAGE_SHIFT;

	// Non-empty list
	if (head->data != -1) {

		// Check if the frame is in the list
		// If so, remove it and add it to the end.
		if (check(frame_num)) {

			delete(frame_num);

		}

		add(frame_num);

	}

	// Empty List
	if (head->data == -1) {

		head->data = frame_num;

	}


	return;
}


/* Initialize any data structures needed for this 
 * replacement algorithm 
 */
void lru_init() {

	head = (node *) malloc(sizeof(node));
	head->data = -1;
	head->next = NULL;
	head->prev = NULL;

}


