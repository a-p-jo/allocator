#include "allocator.h"

/* An allocator that uses statically allocated memory */
static allocator StaticAllocator;
static void *StaticAllocator_alloc(size_t sz)
{
	return allocator_alloc(&StaticAllocator, sz);
}
static void StaticAllocator_free(void *p)
{
	allocator_free(&StaticAllocator, p);
}
static void StaticAllocator_for_blocks(void(*f)(size_t))
{
	allocator_for_blocks(&StaticAllocator, f);
}
static void StaticAllocator_init(void)
{
	static max_align_t heap[4096/sizeof(max_align_t)];
	StaticAllocator = ALLOCATOR_INIT;
	allocator_add(&StaticAllocator, &heap, sizeof(heap));
}

#include <string.h> /* strlen(), strcpy() */
#include <stdio.h>  /* puts(), fputs(), stderr */
#include <assert.h>

static void pblksz(size_t blksz) {printf("%zu ", blksz);}
static void pstatus(const char *msg)
{
	fputs(msg, stdout);
	StaticAllocator_for_blocks(pblksz);
	putchar('\n');
}

int main(int argc, char **argv)
{
	StaticAllocator_init();
	pstatus("Initial blocksize(s) : ");
	
	/* Allocate and copy argv */
	char **args = StaticAllocator_alloc(argc * sizeof(char *));
	assert(args);
	for (int i = 0; i < argc; i++) {
		args[i] = StaticAllocator_alloc(strlen(argv[i]));
		assert(args[i]);
		strcpy(args[i], argv[i]);
	}
	pstatus("Blocksize(s) after allocations : ");

	/* Print the allocated copy and free it */
	while (argc --> 0)
		puts(args[argc]), StaticAllocator_free(args[argc]);
	StaticAllocator_free(args);
	pstatus("Blocksize(s) after freeing : ");
	return 0;
}
