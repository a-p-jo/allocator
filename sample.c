#include "allocator.h"

/* An allocator that uses statically allocated memory */
static allocator sta;

#define sta_alloc(sz)     allocator_alloc(&sta, (sz))
#define sta_free(p)       allocator_free(&sta, (p))
#define sta_for_blocks(f) allocator_for_blocks(&sta, (f))
static void sta_init(void)
{
	static max_align_t heap[4096/sizeof(max_align_t)];
	sta = ALLOCATOR_INIT;
	allocator_add(&sta, heap, sizeof(heap));
}

#include <string.h> /* strlen(), memcpy() */
#include <stdio.h>  /* puts(), fputs(), stderr */
#include <assert.h> /* assert() */

static void pblksz(size_t blksz) {printf("%zu ", blksz);}
static void pstatus(const char *msg)
{
	fputs(msg, stdout);
	sta_for_blocks(pblksz);
	putchar('\n');
}

int main(int argc, char **argv)
{
	sta_init();
	pstatus("Initial blocksize(s) : ");
	
	/* Allocate and deepcopy argv */
	char **args = sta_alloc(argc * sizeof(char *));
	assert(args);

	for (int i = 0; i < argc; i++) {
		size_t len = strlen(argv[i]);
		assert( (args[i] = sta_alloc(len)) );
		memcpy(args[i], argv[i], len);
	}
	pstatus("Blocksize(s) after allocations : ");

	/* Print the allocated copy backwards and free it */
	while (argc --> 0)
		puts(args[argc]), sta_free(args[argc]);
	
	sta_free(args);
	pstatus("Blocksize(s) after freeing : ");
	return 0;
}
