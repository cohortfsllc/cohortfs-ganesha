/*
 * Copyright (c) 2013 CohortFS, LLC.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AVL_X_H
#define AVL_X_H

#include <stdint.h>
#include <pthread.h>
#include "log.h"
#include "abstract_mem.h"
#include "abstract_atomic.h"
#include "gsh_intrinsic.h"
#include "avltree.h"

struct avl_x_part
{
	pthread_rwlock_t lock;
	pthread_mutex_t mtx;
	void *u1;
	void *u2;
	struct avltree t;
	struct avltree_node **cache;
	struct {
		char *func;
		uint32_t line;
	} locktrace;
	CACHE_PAD(0);
};

struct avl_x
{
	CACHE_PAD(0);
	struct avl_x_part *tree;
	uint32_t npart;
	uint32_t flags;
	int32_t cachesz;
};

#define AVL_X_FLAG_NONE      0x0000
#define AVL_X_FLAG_ALLOC     0x0001

/* Cache strategies.
 *
 * In read-through strategy, entries are always inserted in the
 * tree, but lookups may be O(1) when an entry is shadowed in cache.
 *
 * In the write through strategy, t->cache and t->tree partition
 * t, and t->cache is always consulted first.
 */

#define AVL_X_FLAG_CACHE_RT   0x0002
#define AVL_X_FLAG_CACHE_WT   0x0004

extern int avlx_init(struct avl_x *xt, avltree_cmp_fn_t cmpf,
		     uint32_t npart, uint32_t flags);

#define avlx_idx_of_scalar(xt,k) ((k)%((xt)->npart))
#define avlx_partition_of_ix(xt,ix) ((xt)->tree+(ix))
#define avlx_partition_of_scalar(xt,k) \
	(avlx_partition_of_ix((xt),avlx_idx_of_scalar((xt),(k))))

static inline uint32_t
avlx_cache_offsetof(struct avl_x *xt, uint64_t k)
{
    return (k % xt->cachesz);
}

static inline struct avltree_node *
avl_x_cached_lookup(struct avl_x *xt, struct avl_x_part *t,
		    struct avltree_node *nk, uint64_t hk)
{
	struct avltree_node *nv_cached, *nv = NULL;
	uint32_t cache_offset;
	void **cache_slot;

	if (! t)
		t = avlx_partition_of_scalar(xt, hk);

	cache_offset = avlx_cache_offsetof(xt, hk);
	cache_slot = (void **) &(t->cache[cache_offset]);
        nv_cached = (struct avltree_node *)
		atomic_fetch_voidptr(cache_slot);
	if (nv_cached) {
		if (t->t.cmp_fn(nv_cached, nk) == 0) {
			nv = nv_cached;
			goto out;
		}
	}

	nv = avltree_lookup(nk, &t->t);
	if (nv && (xt->flags & AVL_X_FLAG_CACHE_RT)) {
		/* update cache */
		atomic_store_voidptr(cache_slot, nv);
	}

	LogFullDebug(COMPONENT_AVL_CACHE,
		"avl_x_cached_lookup: t %p nk %p nv %p"
		"(%s hk %"PRIx64" slot/offset %d)",
		t, nk, nv, (nv_cached) ? "CACHED" : "",
		hk, cache_offset);

out:
	return (nv);
}

static inline struct avltree_node *
avl_x_cached_insert(struct avl_x *xt, struct avl_x_part *t,
		    struct avltree_node *nk, uint64_t hk)
{
	struct avltree_node *v_cached, *nv = NULL;
	uint32_t cache_offset;
	void **cache_slot;
	int cix;

	cix = avlx_idx_of_scalar(xt, hk); /* diag */
	if (! t)
		t = avlx_partition_of_ix(xt, cix);

	cache_offset = avlx_cache_offsetof(xt, hk);
	cache_slot = (void **) &(t->cache[cache_offset]);
        v_cached = (struct avltree_node *)
		atomic_fetch_voidptr(cache_slot);

	LogFullDebug(COMPONENT_AVL_CACHE,
		     "avl_x_cached_insert: cix %d t %p inserting %p "
		     "(%s hk %"PRIx64" slot/offset %d) flags %d",
		     cix, t, nk, (v_cached) ? "rbt" : "cache",
		     hk, cache_offset, xt->flags);

	if (xt->flags & AVL_X_FLAG_CACHE_WT) {
		if (! v_cached) {
			nv = nk;
			/* update cache */
			atomic_store_voidptr(cache_slot, nv);
		} else {

			nv = avltree_insert(nk, &t->t);
			if (! nv)
				nv = nk;
		}
	} else {
		/* AVL_X_FLAG_CACHE_RT */
		nv = nk;
		/* update cache */
		atomic_store_voidptr(cache_slot, nv);
		(void) avltree_insert(nv, &t->t);
	}

	return (nv);
}

static inline void
avl_x_cached_remove(struct avl_x *xt, struct avl_x_part *t,
		    struct avltree_node *nk, uint64_t hk)
{
	struct avltree_node *v_cached;
	uint32_t cache_offset;
	void **cache_slot;
	int cix;

	cix = avlx_idx_of_scalar(xt, hk); /* diag */
	if (! t)
		t = avlx_partition_of_ix(xt, cix);

	cache_offset = avlx_cache_offsetof(xt, hk);
	cache_slot = (void **) &(t->cache[cache_offset]);
        v_cached = (struct avltree_node *)
		atomic_fetch_voidptr(cache_slot);

	LogFullDebug(COMPONENT_AVL_CACHE,
		"avl_x_cached_remove: cix %d t %p removing %p "
		"(%s hk %"PRIx64" slot/offset %d) flags %d",
		cix, t, nk, (v_cached) ? "cache" : "rbt",
		hk, cache_offset, xt->flags);

	if (xt->flags & AVL_X_FLAG_CACHE_WT) {
		if (v_cached && (t->t.cmp_fn(nk, v_cached) == 0)) {
			atomic_store_voidptr(cache_slot, NULL);
		}
		else {
			avltree_remove(nk, &t->t);
			goto out;
		}
	} else {
		/* AVL_X_FLAG_CACHE_RT */
		if (v_cached && (t->t.cmp_fn(nk, v_cached) == 0))
			atomic_store_voidptr(cache_slot, NULL);
		avltree_remove(nk, &t->t);
		goto out;
	}
out:
	return;
}

#endif /* AVL_X_H */
