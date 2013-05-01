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
	qlane->size = 0;
}

/**
 *  Package init method
 */
void
ds_cache_pkginit(void)
{
	int ix;
	for (ix = 0; ix < RSV_N_Q_LANES; ++ix) {
		struct rsv_q_lane *qlane = &ds_cache.lru[ix];
		lru_init_queue(qlane);
	}

	ds_cache.max_entries = 16384; /* TODO:  conf */
	ds_cache.n_entries = 0;
	ds_cache.rcache.cachesz = 4096; /* 28K slot table */

	(void) avlx_init(&ds_cache.rcache, rsv_cmpf, 7, AVL_X_FLAG_CACHE_RT);
}

struct ds_rsv *
ds_cache_ref(struct ds *ds, uint64_t osd)
{

	return (NULL);
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
