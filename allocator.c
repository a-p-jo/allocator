#include "allocator.h"
#include <stdbool.h>   /* bool, true, false       */
#include <stdint.h>    /* uintptr_t, uint_fast8_t */
#include <stdalign.h>  /* alignof                 */
#include <string.h>    /* memcpy()                */

/* A FreeNode header prefixes blocks in the freelist,
 * putting them in a circular singly-linked list and
 * storing size information.
 */
typedef struct FreeNode {
	/* block size in terms of UNITSZ */
	size_t nunits;
	struct FreeNode *nxt;
	/* aligning header to strictest type also aligns blocks */ 
	allocator_align_t _align[];
} FreeNode;

enum {UNITSZ = sizeof(FreeNode)};

/* Returns least value to add to base to align it to aln */
static inline uint_fast8_t aln_offset(uintptr_t base, uint_fast8_t aln)
{
	return base%aln? aln - base%aln : 0;
}
 
#if ATOMIC_BOOL_LOCK_FREE == 2
/* Test and test-and-set */
static inline void spinlock(atomic_bool *lock)
{
	retry :
	if (atomic_exchange_explicit(lock, true, memory_order_acquire)) {
		/* As it loops on a load, this performs better 
		 * on many processors where atomic loads are cheaper
		 * than atomic exchanges.
		 * This is why atomic_bool is preferred for the lock if
		 * it is lock-free, as atomic_flag cannot be loaded.
		 */
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
/* Test-and-set */
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

/* K&R style next fit allocator */
void *allocator_alloc(allocator *a, size_t nbytes)
{
	/* If a is NULL
	 * or 0 size allocation is requested
	 * or rounding up the requested size would overflow,
	 * or allocator's freelist pointer is NULL (no freelist)
	 * return NULL.
	 */

	uint_fast8_t inc = aln_offset(nbytes, UNITSZ);
	if (!a || !nbytes || SIZE_MAX-inc < nbytes)
		return NULL;
	
	void *res = NULL;
	spinlock(&a->lock);
	if (a->p) {
		/* Round up nbytes to number of units, +1 unit for header    */
		size_t nunits = (nbytes+inc)/UNITSZ + 1;

		for (FreeNode *prv = a->p, *cur = prv->nxt ;; prv = cur, cur = cur->nxt) {
			if (cur->nunits >= nunits) {         /* match found  */
				if (cur->nunits == nunits) { /* unlink block */
					if (prv->nxt != cur->nxt)
						prv->nxt = cur->nxt;
					else /* freelist is singleton       */
						prv = NULL; /* No freelist! */
				} else /* adjust size & allocate from tail  */
					(cur += (cur->nunits -= nunits))->nunits = nunits;
				a->p = prv;   /* Aids freelist consistency  */
				res  = cur+1; /* Usable region after header */
				break;
			} else if (cur == a->p) /* wrapped around, no match */
				break;
		}
	}
	spinunlock(&a->lock);
	return res;
}

/* Return ptr to a's freelist */
void allocator_free(allocator *a, void *restrict ptr)
{
	FreeNode *p = ptr;
	if (a && p--) { /* No-op if either a or p is NULL                   */
		spinlock(&a->lock);
		if (a->p) {
			/* Freelist is in ascending order of addresses,
			 * traverse to reach insertion point.
			 */
			FreeNode *cur;
			for (cur = a->p; !(p > cur && p < cur->nxt); cur = cur->nxt) {
				if(cur >= cur->nxt && (p > cur || p < cur->nxt))
					break;
			}

			if (p + p->nunits == cur->nxt) { /* Coalesce to nxt  */
				p->nunits += cur->nxt->nunits;
				p->nxt = cur->nxt->nxt;
			} else /* Insert p after cur */
				p->nxt = cur->nxt;
			if (cur + cur->nunits == p) {   /* Coalesce to prv  */
				cur->nunits += p->nunits;
				cur->nxt = p->nxt;
			} else                          /* Insert after cur */
				cur->nxt = p;
			a->p = cur;
		} else /* If no freelist, create singleton with p           */
			a->p = p, p->nxt = p;
		spinunlock(&a->lock);
	}
}

void allocator_add(allocator *a, void *restrict p, size_t nbytes)
{
	uintptr_t addr = (uintptr_t)p;
	uint_fast8_t inc = aln_offset(addr, alignof(allocator_align_t));
	size_t nunits = (nbytes - inc)/UNITSZ; /* Round down */

	/* Ensure a is not NULL, aligned pointer doesn't exceed bounds,
	 * and given size is of at least one unit.
	 */
	if (a && nbytes > inc+UNITSZ && nunits) {
		FreeNode *new = (FreeNode *)(addr+inc); /* Create header */
		new->nunits = nunits;
		allocator_free(a, new+1);
	}
}

static inline size_t node_usable_space(const FreeNode *n)
{
	return (n->nunits-1) * UNITSZ;
}

size_t allocator_allocsz(allocator *a, const void *restrict p)
{
	size_t retval = 0;
	if (a && p) {
		spinlock(&a->lock);
		retval = node_usable_space((FreeNode *)p-1);
		spinunlock(&a->lock);
	}
	return retval;
}

void allocator_for_blocks(allocator *a, void(*f)(uintptr_t, size_t))
{
	if (a && f) {
		spinlock(&a->lock);
		if (a->p) {
			FreeNode *cur = a->p;
			do {
				f((uintptr_t)(cur+1), node_usable_space(cur));
				cur = cur->nxt;
			} while (cur != a->p);
		}
		spinunlock(&a->lock);
	}
}

void *allocator_realloc(allocator *a, void *restrict p, size_t nbytes) 
{
	if (!a)
		return NULL;
	else if (!p)
		return allocator_alloc(a, nbytes);
	else if (!nbytes) {
		allocator_free(a, p);
		return NULL;
	} else {
		void *res = p;
		size_t p_usable_space = allocator_allocsz(a, p);
		if (
			p_usable_space < nbytes 
			&& (res = allocator_alloc(a, nbytes))
		) {
			memcpy(res, p, p_usable_space);
			allocator_free(a, p);
		}
		return res;
	}
}
