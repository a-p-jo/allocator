#include "allocator.h"
#include <stdbool.h>   /* bool, true, false       */
#include <stdint.h>    /* uintptr_t, uint_fast8_t */
#include <stdalign.h>  /* max_align_t, alignof    */
#include <string.h>    /* memcpy() */

/* A FreeNode header prefixes blocks in the freelist,
 * linking them as a circular singly-linked list and
 * storing size information.
 */
typedef struct FreeNode {
	/* block size in terms of UINTSZ */
	size_t nunits;
	struct FreeNode *nxt;
	/* aligning header to strictest type also aligns blocks */ 
	max_align_t _align[];
} FreeNode;
enum {UNITSZ = sizeof(FreeNode)};

/* Returns least value to add to base to align it to aln */
static inline uint_fast8_t aln_offset(uintptr_t base, uint_fast8_t aln)
{
	return base%aln? aln - base%aln : 0;
}

#if ATOMIC_BOOL_LOCK_FREE == 2
/* Classic test and test-and-set spinlock */
static inline void spinlock(atomic_bool *lock)
{
	retry :
	if (atomic_exchange_explicit(lock, true, memory_order_acquire)) {
		while (atomic_load_explicit(lock, memory_order_acquire))
			;
		goto retry;
	}
}
static inline void spinunlock(atomic_bool *lock)
{
	atomic_store_explicit(lock, false, memory_order_release);
}
#else
/* Classic test-and-set spinlock */
static inline void spinlock(atomic_flag *lock)
{
	while (atomic_flag_test_and_set_explicit(lock, memory_order_acquire))
		;
}
static inline void spinunlock(atomic_flag *lock)
{
	atomic_flag_clear_explicit(lock, memory_order_release);
}
#endif

/* Classic K&R style next fit allocator */
void *allocator_alloc(allocator *a, size_t nbytes)
{
	void *res = NULL;
	uint_fast8_t inc = aln_offset(nbytes, UNITSZ);

	/* If allocator's pointer to freelist is NULL (no freelist)
	 * or 0 size allocation is requested
	 * or rounding up the requested size would overflow,
	 * return NULL.
	 */
	if (a->p && nbytes && SIZE_MAX-inc >= nbytes) {
		/* Round up nbytes to next number of units, +1 unit for header */
		size_t nunits = (nbytes+inc)/UNITSZ + 1;

		spinlock(&a->lock);
		for (FreeNode *prv = a->p, *cur = prv->nxt ;; prv = cur, cur = cur->nxt) {
			if (cur->nunits >= nunits) { /* unlink block */
				if (cur->nunits == nunits) {
					if (prv->nxt != cur->nxt)
						prv->nxt = cur->nxt;
					else /* freelist is singleton */
						prv = NULL; /* NULL signifies no freelist */
				} else
					/* adjust sizes, allocate from tail end of block */
					(cur += (cur->nunits -= nunits))->nunits = nunits;
				/* Update allocator's ptr for consitency of freelist */
				a->p = prv;
				res  = cur+1;
				break;
			} else if (cur == a->p)
				break; /* wrapped around freelist, no match found */
		}
		spinunlock(&a->lock);
	}
	return res;
}

/* Maintains ascending order (by address) of freelist
 * in insertion and coalesces adjacent blocks.
 */
void allocator_free(allocator *a, void *restrict ptr)
{
	FreeNode *p = ptr;
	if (p--) {
		spinlock(&a->lock);
		if (a->p) {
			/* Freelist is in ascending order of addresses,
			* traverse to reach insertion point for p.
			*/
			FreeNode *cur;
			for (cur = a->p; !(p > cur && p < cur->nxt); cur = cur->nxt) {
				if(cur >= cur->nxt && (p > cur || p < cur->nxt))
					break;
			}

			if (p + p->nunits == cur->nxt) { /* Coalesce  with next block */
				p->nunits += cur->nxt->nunits;
				p->nxt = cur->nxt->nxt;
			} else
				p->nxt = cur->nxt;
			if (cur + cur->nunits == p) {   /* Coalesce with prev block  */
				cur->nunits += p->nunits;
				cur->nxt = p->nxt;
			} else
				cur->nxt = p;

			a->p = cur;
		} else /* If allocator has no freelist, create one with p */
			a->p = p, p->nxt = p;
		spinunlock(&a->lock);
	}
}

void allocator_add(allocator *a, void *restrict p, size_t nbytes)
{
	uintptr_t addr = (uintptr_t)p;
	uint_fast8_t inc = aln_offset(addr, alignof(max_align_t));
	size_t nunits = (nbytes - inc)/UNITSZ;

	/* Ensure aligned pointer doesn't exceed bounds,
	 * and given size is of at least one unit.
	 */
	if (nbytes > inc+UNITSZ && nunits) {
		FreeNode *new = (FreeNode *)(addr+inc);
		new->nunits = nunits;
		allocator_free(a, new+1);
	}
}

size_t allocator_size(const void *restrict p)
{
	return p? (((FreeNode *)p-1)->nunits-1) * UNITSZ : 0;	
}

void *allocator_realloc(allocator *a, void *restrict p, size_t nbytes)
{
	if (!p)
		return allocator_alloc(a, nbytes);
	else if (!nbytes) {
		allocator_free(a, p);
		return NULL;
	} else {
		void *res = p;
		if (allocator_size(p) < nbytes && (res = allocator_alloc(a, nbytes)))
			memcpy(res, p, allocator_size(p)), allocator_free(a, p);
		return res;
	}
}