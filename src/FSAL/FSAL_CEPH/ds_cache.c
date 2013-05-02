/*
 * Copyright © 2013, CohortFS, LLC.
 * Author: Matt Benjamin <matt@linuxbox.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 * -------------
 */

/**
 * @file   ds_cache.h
 * @author Matt Benjamin <matt@linuxbox.com>
 * @date   Wed May  1 08:29:47 PDT 2013
 *
 * @brief Ceph FSAL DS reservation cache (decls)
 *
 * Ceph FSAL DS reservation cache (decls).
 */

#include "config.h"

#include "ds_cache.h"
#include "fsal.h"
#include "fsal_api.h"
#include "FSAL/fsal_commonlib.h"
#include "internal.h"

struct ds_rsv_cache ds_cache;
static pool_t *ds_rsv_pool;

#define QLOCK(qlane) \
	do { \
	        pthread_mutex_lock(&(qlane)->mtx); \
	} while(0)

#define QUNLOCK(qlane) \
	pthread_mutex_unlock(&(qlane)->mtx)

#define COND_QLOCK(qlane) \
	do { \
		if (! locked) \
			pthread_mutex_lock(&(qlane)->mtx); \
	} while(0)

#define COND_QUNLOCK(qlane) \
	if (! locked) \
		pthread_mutex_unlock(&(qlane)->mtx)

#define qlane_of_ix(ix) \
	(&ds_cache.lru[(ix)])

#define qlane_of_tpart(t) \
	((struct rsv_q_lane *) (t)->u1)

/**
 * Expanded reservation comparaison function.
 *
 * Orders by inode, then rsv.id.
 */
static int
rsv_cmpf(const struct avltree_node *lhs,
	 const struct avltree_node *rhs)
{
	struct ds_rsv *lk, *rk;

	lk = avltree_container_of(lhs, struct ds_rsv, node_k);
	rk = avltree_container_of(rhs, struct ds_rsv, node_k);

	if (lk->ino < rk->ino)
		return (-1);

	if (lk->ino == rk->ino) {
		if (lk->rsv.id < rk->rsv.id)
			return (-1);

		if (lk->rsv.id == rk->rsv.id)
			return (0);
	}

	return (1);
}

/**
 * @brief Initialize a queue.
 *
 * Initialize a queue.
 */
static inline void
lru_init_queue(struct rsv_q_lane *qlane)
{
	pthread_mutex_init(&qlane->mtx, NULL);
	init_glist(&qlane->q);
}

/**
 *  Package init method
 */
void
ds_cache_pkginit(void)
{
	int ix;
	struct avl_x_part *t;
	struct rsv_q_lane *qlane;

	ds_rsv_pool =
		pool_init("ds_rsv_pool", sizeof(struct ds_rsv),
			  pool_basic_substrate,
			  NULL, NULL, NULL);

	ds_cache.max_entries = 16384; /* TODO:  conf */
	ds_cache.n_entries = 0;
	ds_cache.xt.cachesz = 4096; /* 28K slot table */

	(void) avlx_init(&ds_cache.xt, rsv_cmpf, RSV_N_Q_LANES,
			 AVL_X_FLAG_CACHE_RT);

	/* for simplicity, unify partitions and lanes */
	for (ix = 0; ix < RSV_N_Q_LANES; ++ix) {
		t = avlx_partition_of_ix(&ds_cache.xt, ix);
		qlane = qlane_of_ix(ix);
		lru_init_queue(qlane);
		t->u1 = qlane;
		qlane->t = t;
	}

}

#define SENTINEL_REFCOUNT 1

#define LRU_NEXT(n) \
	(atomic_inc_uint32_t(&(n)) % RSV_N_Q_LANES)

static inline struct ds_rsv *
new_rsv(void)
{
	struct ds_rsv *rsv;

	rsv = pool_alloc(ds_rsv_pool, NULL);
	if (rsv) {
		init_wait_entry(&rsv->we);
		rsv->refcnt = 2;
		rsv->waiters = 0;
	}

	return (rsv);
}

static inline struct ds_rsv *
try_reclaim(struct rsv_q_lane *qlane, struct ds_rsv *rsv)
{
	struct avl_x_part *t;
	int32_t refcnt;

	/* QLOCKED */
	refcnt = atomic_inc_int32_t(&rsv->refcnt);
	if (refcnt != SENTINEL_REFCOUNT+1) {
		(void) atomic_dec_int32_t(&rsv->refcnt);
		return (NULL);
	}
	t = qlane->t;

	/* rsv is almost always moving, due to new hk */
	avl_x_cached_remove(&ds_cache.xt, t, &rsv->node_k, rsv->hk);
	glist_del(&rsv->q);	

	return (rsv);
}

static inline struct ds_rsv *
try_reap_rsv(int locked_ix)
{
	static uint32_t reap_lane = 0;
	struct ds_rsv *lru, *rsv = NULL;
	struct rsv_q_lane *qlane;
	struct glist_head *glist;
	struct glist_head *glistn;
	uint32_t lane, n_entries;
	int ix;

	n_entries = atomic_fetch_uint32_t(&ds_cache.n_entries);
	if (n_entries >= ds_cache.n_entries) {
		/* try to reclaim */		
		for (ix = 0; ix < RSV_N_Q_LANES; ++ix,
			     lane = LRU_NEXT(reap_lane)) {
			qlane = qlane_of_ix(lane);
			bool locked = (locked_ix == lane);
			COND_QLOCK(qlane);
			glist_for_each_safe(glist, glistn, &qlane->q) {
				lru = glist_entry(glist, struct ds_rsv, q);
				if (lru) {
					rsv = try_reclaim(qlane, lru);
					if (rsv) {
						COND_QUNLOCK(qlane);
						(void) atomic_dec_uint32_t(
							&ds_cache.n_entries);
						return (rsv);
					}
				}
			}
			COND_QUNLOCK(qlane);
		}
	}

	rsv = new_rsv();

	return (rsv);
}

struct ds_rsv *
ds_cache_ref(struct ds *ds, uint64_t osd)
{
	struct avltree_node *node;
	struct ds_rsv rk, *rsv;
	struct rsv_q_lane *qlane;
	struct avl_x_part *t;
	struct timespec ts;
	int ix, r;

	rk.rsv.id = ds->wire.rsv.id;
	rk.hk = ds->wire.rsv.hk;
	rk.ino = ds->wire.wire.vi.ino.val;

	ix = avlx_idx_of_scalar(&ds_cache.xt, rk.hk);
	t = avlx_partition_of_ix(&ds_cache.xt, ix);
	qlane = qlane_of_tpart(t);

	QLOCK(qlane);
	node = avl_x_cached_lookup(&ds_cache.xt, t, &rk.node_k, rk.hk);
	if (node) {
		rsv = avltree_container_of(node, struct ds_rsv, node_k);
		(void) atomic_inc_int32_t(&rsv->refcnt);
		if (rsv->flags & DS_RSV_FLAG_FETCHING) {
			++rsv->waiters;
			while (rsv->flags & DS_RSV_FLAG_FETCHING) {
				clock_gettime(CLOCK_MONOTONIC_FAST, &ts);
				/* XXX improve wrt lease time and try to use
				 * NFS4ERR_DELAY */
				ts.tv_sec += 120;
				ts.tv_nsec = 0;
				r = pthread_cond_timedwait(
					&rsv->we.cv, &qlane->mtx, &ts);
				if (r == ETIMEDOUT) {
					/* TODO: log crit this */
					--(rsv->waiters);
					goto unlock;
				}
			}
			--(rsv->waiters);
		}
	} else {
		/* ! node */
		rsv = try_reap_rsv(-1); /* allocates if nothing available */
		/* ref+1 */
		rsv->flags = DS_RSV_FLAG_FETCHING; /* deal with races */
		rsv->rsv.id = rk.rsv.id;
		rsv->hk = rk.hk;
		rsv->ino = rk.ino;
		/* pins the tree at this position */
		(void) avl_x_cached_insert(&ds_cache.xt, t, &rsv->node_k,
					   rsv->hk);
		/* try_reap_rsv() can't find this */
		QUNLOCK(qlane);
		r = ceph_ll_verify_reservation(rsv, osd);
		QLOCK(qlane);
		rsv->flags &= ~DS_RSV_FLAG_FETCHING;
		if (r != 0) { 
			/* fenced */
			rsv->flags |= DS_RSV_FLAG_FENCED;
		}
		/* add rsv to MRU */
		glist_add(&qlane->q, &rsv->q);
		if (rsv->waiters > 0) {
			/* wake them all */
			pthread_cond_broadcast(&rsv->we.cv);
		}
	}

unlock:
	QUNLOCK(qlane); /* ! QLOCKED */

	return (rsv);
}

void
ds_cache_unref(struct ds_rsv *rsv)
{
}

/**
 * Package shutdown
 */
void
ds_cache_pkgshutdown(void)
{
	/* TODO:  discard all cached */
}
