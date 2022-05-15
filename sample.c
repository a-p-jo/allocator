#include "allocator.h"

static allocator StaticAllocator;

static inline void StaticAllocator_add(void *p, size_t sz)
{
	allocator_add(&StaticAllocator, p, sz);
}
static inline void *StaticAllocator_alloc(size_t sz)
{
	return allocator_alloc(&StaticAllocator, sz);
}
static inline void StaticAllocator_free(void *p)
{
	allocator_free(&StaticAllocator, p);
}
static inline void *StaticAllocator_realloc(void *p, size_t newsz)
{
	return allocator_realloc(&StaticAllocator, p, newsz);
}
static size_t (*StaticAllocator_size)(const void *) = allocator_size;

static inline void StaticAllocator_init(void)
{
	enum {STATIC_ALLOCATOR_CAP = 4096};
	static max_align_t heap[STATIC_ALLOCATOR_CAP/sizeof(max_align_t)];

	StaticAllocator = ALLOCATOR_INIT;
	StaticAllocator_add(&heap, sizeof(heap));
}

#include <string.h> /* strlen, strcpy */
#include <stdio.h>  /* puts, fputs, stderr */

int main(int argc, char **argv)
{
	StaticAllocator_init();
	char *progname = StaticAllocator_alloc(strlen(argv[0]));
	if (progname)
		strcpy(progname, argv[0]), puts(progname), StaticAllocator_free(progname);
	else
		fputs("Couldn't allocate.\n", stderr);
	return 0;
}
