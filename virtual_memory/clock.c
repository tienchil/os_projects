#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"


extern int memsize;

extern int debug;

extern struct frame *coremap;

// For recording which frame the clock stops
int CLOCK_ARM;

/* Page to evict is chosen using the clock algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */

int clock_evict() {

	int evict = -1;

	while (evict == -1) {

		// Check if the Arm is within the limit
		if (CLOCK_ARM >= memsize) {

			CLOCK_ARM = 0;

		}

		// Find the reference bit
		unsigned int frame = coremap[CLOCK_ARM].pte->frame;
		unsigned int ref = frame & PG_REF;

		if (ref) {
			// The reference bit is on, turn it off
			coremap[CLOCK_ARM].pte->frame = 
			            coremap[CLOCK_ARM].pte->frame & ~PG_REF;

		} else {

			evict = CLOCK_ARM;

		}

		CLOCK_ARM++;

	}

	
	assert(evict != -1);
	
	return evict;
}

/* This function is called on each access to a page to update any information
 * needed by the clock algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void clock_ref(pgtbl_entry_t *p) {

	return;
}

/* Initialize any data structures needed for this replacement
 * algorithm. 
 */
void clock_init() {

	CLOCK_ARM = 0;

}
