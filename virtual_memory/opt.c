#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"

extern int memsize;

extern int debug;

extern struct frame *coremap;

extern char *tracefile;

const int MAXLINE = 256;

// The counter for each instruction coming in
int TIME = 1;

// A base struct for building a tree and a list
typedef struct node_t {

	int data; // To store indices or time difference
	struct node_t *next;  // For linked list use
	struct node_t *prev;  // For linked list use
	struct node_t *left;  // For BST use
	struct node_t *right; // For BST use
	struct node_t *secondary;  // Point to the root of secondary tree
	
} node;

// The root
struct node_t *master_BST;

/* Helper Function for linked list operation */
// Search if the list contains the data
// Return the node if successful, else return NULL
node *list_search(node *head, int data) {

	node *curr = head;

	while (curr != NULL) {

		if (curr->data == data) {

			return curr;

		}

		curr = curr->next;

	}

	return NULL;

}



void list_remove(node *head, int data) {

	// Check if the list contains the data
	node *curr = list_search(head, data);
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
		free(curr);

	}

	return;

}

void list_insert(node *head, int data) {

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

// Print the linked list
void print_list(node *head) {

	if (head == NULL) {

		printf("TAIL\n");

		return;

	}

	printf("%d-> ", head->data);
	print_list(head->next);

}


/* Helper Function for BST operation */
// Helper function for insertion
node *new_node(int data) { 

	node *new = (node *) malloc(sizeof(node));
	new->data = data; 
	new->left = NULL; 
	new->right = NULL;
	new->prev = NULL; 
	new->next = NULL;
	new->secondary = NULL;

	return new; 

} 


// Insert a node into the tree
node *insert(node *node, int data) { 

// The tree is empty
	if (node == NULL) { 

		return new_node(data); 

	} else { 
		// the tree is not empty
		if (data <= node->data) {

			node->left = insert(node->left, data); 

		} else {

			node->right = insert(node->right, data);

		}

	} 

	return node;

} 


// For searching the node
node *lookup(node* node, int target) { 
  
	// Emprty tree
	if (node == NULL) { 

		return NULL; 

	} 

	else { 
		// the target is at the current node
		if (target == node->data) {

			return node;

		} else { 
			// Go down the tree 
			if (target < node->data) {

				return lookup(node->left, target); 

			} else {

				return lookup(node->right, target); 

			}
		} 
	} 
}

// Print the tree
void print_tree(node *root) {

	if (root == NULL) {

		return;

	}

	print_tree(root->left);
	printf("{%d}\n", root->data);

	

	// Also print the secondary tree if any
	if (root->secondary != NULL) {

		printf("--Secondary Tree--\n");

		print_tree(root->secondary);

		

		printf("--End--\n");

	}

	// Print the list, if any
	if (root->next != NULL) {

		printf("List: ");
		print_list(root);

	}

	
	print_tree(root->right);

}


/* Page to evict is chosen using the optimal (aka MIN) algorithm. 
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int opt_evict() {
	
	int i;
	int evict;
	int best = 0;

	for (i = 0; i < memsize; i++) {

		

		if (coremap[i].diff > best) {

			best = coremap[i].diff;
			evict = i;

		}

		coremap[i].diff++;

		printf("Frame %d: %d\n", i, coremap[i].diff);

	}

	printf("EVICT: %d\n", evict);
	
	return evict;
}

/* This function is called on each access to a page to update any information
 * needed by the opt algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void opt_ref(pgtbl_entry_t *p) {

	node *pgdir;
	node *pgtbl;
	node *head;

	int frame_num = p->frame >> PAGE_SHIFT;
	int pgdir_idx = coremap[frame_num].pgdir_idx;
	int pgtbl_idx = coremap[frame_num].pgtbl_idx;

	pgdir = lookup(master_BST, pgdir_idx);

	// Error Checking
	if (pgdir != NULL) {

		pgtbl = lookup(pgdir->secondary, pgtbl_idx);

	}

	if (pgtbl != NULL) {

		head = pgtbl->next;

	}
	
	if (head == NULL) {

		return;

	}

	// Update the time field for coremap
	if (head->next != NULL) {

		// Calculate the time diff
		int diff = head->next->data - head->data;
		list_remove(head, head->data);
		coremap[frame_num].diff = diff;

	} else {

		coremap[frame_num].diff = 0;
		list_remove(head, head->data);

	}

	return;
}

/* Initializes any data structures needed for this
 * replacement algorithm.
 */
void opt_init() {

	// Read the trace file
	FILE *tfp;
	
	addr_t vaddr = 0;
	char type;
	char buf[MAXLINE];
	unsigned pgdir_idx;
	unsigned pgtbl_idx;

	printf("-----------OPT INIT-----------\n");


	if(tracefile != NULL) {
		if((tfp = fopen(tracefile, "r")) == NULL) {
			perror("Error opening tracefile:");
			exit(1);
		}
	}

	while(fgets(buf, MAXLINE, tfp) != NULL) {

		if(buf[0] != '=') {

			sscanf(buf, "%c %lx", &type, &vaddr);

			// Insert directory index into the master tree
			pgdir_idx = PGDIR_INDEX(vaddr);
			
			// Check if the tree is empty
			if (master_BST == NULL) {

				master_BST = insert(master_BST, pgdir_idx);

			} else {

				// Check if the directory index already exists
				if (lookup(master_BST, pgdir_idx) == NULL) {

					// Insert a new node
					insert(master_BST, pgdir_idx);

				}

			}

			// Insert pagetable into the second-level tree
			pgtbl_idx = PGTBL_INDEX(vaddr);
			node *dir = lookup(master_BST, pgdir_idx);

			if (dir != NULL) {

				if (dir->secondary == NULL) {
					// Empty secondary tree
					node *new_pgtbl = insert(dir->secondary, pgtbl_idx);
					dir->secondary = new_pgtbl;

					list_insert(new_pgtbl, TIME);

				} else {

					// Check if the pagetable already exists
					node *temp = lookup(dir->secondary, pgtbl_idx);

					if (temp != NULL) {

						// Already exists, insert time into the linked list
						list_insert(temp, TIME);

					} else  {
						// Doesn't exist, insert the index into the tree
						insert(dir->secondary, pgtbl_idx);
						node *new_pgtbl = lookup(dir->secondary, pgtbl_idx);

						list_insert(new_pgtbl, TIME);
						

					}

				}
			}
		
		} else {

			continue;

		}

		TIME++;

	}

	fclose(tfp);

	// Initalize fields in frame
	int i;

	for (i = 0; i < memsize; i++) {

		coremap[i].pgtbl_idx = 0;
		coremap[i].pgdir_idx = 0;
		coremap[i].diff = 0;

	}
	
	printf("-----------OPT INIT END-----------\n");
// print_tree(master_BST);
}

