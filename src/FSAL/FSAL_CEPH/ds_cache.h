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

#ifndef FSAL_CEPH_DS_CACHE_H
#define FSAL_CEPH_DS_CACHE_H

#include <cephfs/libcephfs.h>
#include "avl_x.h"
#include "internal.h"
#include "nlm_list.h"
#include "abstract_mem.h"
#include "wait_queue.h"

/**
 * Expanded reservation (local cache)
 */

#define DS_RSV_FLAG_NONE         0x0000
#define DS_RSV_FLAG_FENCED       0x0001
#define DS_RSV_FLAG_NOMATCH      0x0002
#define DS_RSV_FLAG_FETCHING     0x0004

struct ds_rsv {
	uint64_t hk;
	uint32_t flags;
	int32_t refcnt;
	struct glist_head q;
	struct avltree_node node_k; /*< AVL node in tree */
	struct ceph_reservation rsv;
	uint64_t ino;
	wait_entry_t we;
	uint32_t waiters;
};

/**
 * Reservation support.
 */

/* Cache-line padding macro from MCAS */
#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64 /* XXX arch-specific define */
#endif
#define CACHE_PAD(_n) char __pad ## _n [CACHE_LINE_SIZE]
#define ALIGNED_ALLOC(_s)					\
     ((void *)(((unsigned long)malloc((_s)+CACHE_LINE_SIZE*2) + \
		CACHE_LINE_SIZE - 1) & ~(CACHE_LINE_SIZE-1)))

#define RSV_N_Q_LANES 7

struct rsv_q_lane
{
	pthread_mutex_t mtx;
	struct glist_head q; /* LRU is at HEAD, MRU at tail */
	struct avl_x_part *t;
	CACHE_PAD(0);
};

struct ds_rsv_cache
{
	uint32_t max_entries; /* TODO:  move to config file */
	uint32_t n_entries;

	struct avl_x xt;
	struct rsv_q_lane lru[RSV_N_Q_LANES];
};

extern struct ds_rsv_cache ds_cache;

struct ds_rsv *ds_cache_ref(struct export *export, struct ds *ds, uint64_t osd);
void ds_cache_unref(struct ds_rsv *rsv);

void ds_cache_pkginit(void);
void ds_cache_pkgshutdown(void);

#endif /* FSAL_CEPH_DS_CACHE_H */
