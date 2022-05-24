#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <stddef.h>    /* size_t */
#include <stdatomic.h> /* atomic_bool, ATOMIC_BOOL_LOCK_FREE, atomic_flag */

/* Opaque type for nodes in the freelist */
typedef struct FreeNode FreeNode;

/* Encapsulates allocator's state.
 * Treat all members as private.
 */
typedef struct allocator {
        FreeNode *p;
        #if ATOMIC_BOOL_LOCK_FREE == 2
        atomic_bool lock;
        #else
        atomic_flag lock;
        #endif
} allocator;

/* Constant expression to defualt-initialize an allocator */
#if ATOMIC_BOOL_LOCK_FREE == 2
#define ALLOCATOR_INIT (allocator){0}
#else
#define ALLOCATOR_INIT (allocator){.lock = ATOMIC_FLAG_INIT}
#endif

/* Add memory region of n bytes refrenced by p to allocator's freelist. */
void allocator_add(allocator *, void *restrict p, size_t n);

void *allocator_alloc(allocator *, size_t);
void allocator_free(allocator *, void *restrict);
void *allocator_realloc(allocator *, void *restrict, size_t);

/* Obtain actual size, in bytes, of allocation refrenced by p */
size_t allocator_size(const void *restrict p);

#endif
