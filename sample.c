#include "allocator.h"
#include <stddef.h>
#include <string.h>   /* strlen(), strcpy()      */
#include <inttypes.h> /* PRIXPTR                 */
#include <stdio.h>    /* puts(), fputs(), stderr */
#include <assert.h>   /* assert()                */

/* Counter - track how many blocks we've been called with */
static size_t pblksz_cnt = 0;
/* callback function for allocator_for_blocks() */
static void pblksz(uintptr_t blkaddr, size_t blksz)
{
	/* A 0th node makes no sense, so pre-increment */
	printf(
		"Block #%zu: 0x%"PRIXPTR", %zu bytes\n",
		++pblksz_cnt, blkaddr, blksz
	);
}
/* print msg and show the nodes in the freelist for a */
static void show_freelist(allocator *a, const char *restrict msg)
{
	puts(msg);
	allocator_for_blocks(a, pblksz);
	pblksz_cnt = 0;           /* reset count            */
	putchar('\n'); /* like a paragraph break */
}

/* 4KiB heap composed of 4 equally-sized blocks */
#define HEAP_SIZE 4096
#define NUMBER_OF_BLOCKS 4

/* A type that is HEAP_SIZE/NUMBER_OF_BLOCKS bytes large and
 * aligned to the largest alignment supported.
 */
typedef allocator_align_t heap_block[
	(HEAP_SIZE/NUMBER_OF_BLOCKS) / sizeof(allocator_align_t)
];

/* Extra block non-adjacent with the others (not in the array) */
static heap_block extra_block;

int main(int argc, char **argv)
{
	allocator a = ALLOCATOR_INIT;
	heap_block heap[NUMBER_OF_BLOCKS];

	/* Test allocator_add() */
	allocator_add(&a, &extra_block, sizeof(extra_block));

	/* Test coalescing - blocks should be merged if they are adjacent */
	for (size_t i = 0; i < NUMBER_OF_BLOCKS; i++)
		allocator_add(&a, heap+i, sizeof(heap[0]));

	show_freelist(&a, "Initial freelist :");
	
	/* Test allocator_alloc() - deepcopy argv */
	char **argv_copy = allocator_alloc(&a, argc*sizeof(char *));
	assert(argv_copy);

	for (int i = 0; i < argc; i++) {
		argv_copy[i] = allocator_alloc(&a, strlen(argv[i])+1);
		assert(argv_copy[i]);
		strcpy(argv_copy[i], argv[i]);
	}

	show_freelist(&a, "Freelist after cloning argv :");

	/* Test allocator_allocsz & allocator_free */
	puts("Allocated:");
	/* First thing we allocated was argv_copy itself! */
	printf(
		"Block #0: @0x%"PRIXPTR", %zu bytes used of %zu\n",
		(uintmax_t)argv_copy, sizeof(argv_copy[0])*argc,
		allocator_allocsz(&a, argv_copy)
	);
	/* Then we allocated & copied each arg inside it */
	for (int i = 0; i < argc; i++) {
		char *cur = argv_copy[i];
		printf(
			"Block #%d: \"%s\", %zu bytes used of %zu\n",
			i+1, cur, strlen(cur)+1, allocator_allocsz(&a, cur)
		);
		allocator_free(&a, cur);
	}
	allocator_free(&a, argv_copy);
	putchar('\n');

	show_freelist(&a, "Freelist after freeing :");
}
