#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"


extern int memsize;

extern int debug;

extern struct frame *coremap;

// Global Counter for FIFO
int FIFO_COUNT;

/* Page to evict is chosen using the fifo algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int fifo_evict() {

	int evict = FIFO_COUNT;

	FIFO_COUNT++;

	if (FIFO_COUNT >= memsize) {
		// Over the limit, Restart from the beginning
		FIFO_COUNT = 0;

	}

	return evict;

}

/* This function is called on each access to a page to update any information
 * needed by the fifo algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void fifo_ref(pgtbl_entry_t *p) {

	return;

}

/* Initialize any data structures needed for this 
 * replacement algorithm 
 */
void fifo_init() {

	FIFO_COUNT = 0;

}
