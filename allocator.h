#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <stddef.h>    /* size_t, max_align_t                             */
#include <stdatomic.h> /* atomic_bool, ATOMIC_BOOL_LOCK_FREE, atomic_flag */
#include <stdint.h>    /* uintptr_t                                       */

/* The type against which all allocations are aligned to by default.
 * MSVC does not define a max_align_t in its stddef.h, so we roll our own.
 * See also: 
 * https://developercommunity.visualstudio.com/t/stdc11-should-add-max-align-t-to-stddefh/1386891
 * https://patchwork.ffmpeg.org/project/ffmpeg/patch/20240205195802.14522-1-anton@khirnov.net/
 * https://github.com/llvm/llvm-project/blob/main/clang/lib/Headers/__stddef_max_align_t.h
 */
#ifdef _MSC_VER
typedef union {
        long double a;
        long long b;
        void (*c)(void);
} allocator_align_t;
#else
typedef max_align_t allocator_align_t;
#endif

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

/* Obtain actual usable space, in bytes, in allocation refrenced by p */
size_t allocator_allocsz(allocator *a, const void *restrict p);

/* Callsback with address & size (in bytes) of each block in freelist.
 * Don't call any allocator_* function inside callback, it will deadlock.
 */
void allocator_for_blocks(allocator *, void(*)(uintptr_t, size_t));

#endif
