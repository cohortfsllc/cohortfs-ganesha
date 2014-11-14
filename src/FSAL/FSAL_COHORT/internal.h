/*
 * Copyright © 2012-2014, CohortFS, LLC.
 * Author: Adam C. Emerson <aemerson@linuxbox.com>
 *
 * contributeur : William Allen Simpson <bill@cohortfs.com>
 *		  Marcus Watts <mdw@cohortfs.com>
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
 * @file   internal.h
 * @author Adam C. Emerson <aemerson@linuxbox.com>
 * @author William Allen Simpson <bill@cohortfs.com>
 * @author Marcus Watts <mdw@cohortfs.com>
 * @date Wed Oct 22 13:24:33 2014
 *
 * @brief Internal declarations for the Cohort FSAL
 *
 * This file includes declarations of data types, functions,
 * variables, and constants for the Cohort FSAL.
 */

#ifndef FSAL_COHORT_INTERNAL_INTERNAL__
#define FSAL_COHORT_INTERNAL_INTERNAL__

#include <cephfs/libcephfs.h>
#include <ceph_osd.h>
#include "fsal.h"
#include "fsal_types.h"
#include "fsal_api.h"
#include "fsal_convert.h"
#include <stdbool.h>
#include <uuid/uuid.h>

/**
 * Cohort Main (global) module object
 */

struct cohort_fsal_module {
	struct fsal_module fsal;
	struct fsal_staticfsinfo_t fs_info;
	struct libosd *osd;
	char *where;
	bool start_osd;
};
extern struct cohort_fsal_module CohortFSM;

/**
 * Cohort private export object
 */

struct cohort_export {
	struct fsal_export export;	/*< The public export object */
	struct ceph_mount_info *cmount;	/*< The mount object used to
					   access all Cohort methods on
					   this export. */
	struct cohort_handle *root;	/*< The root handle */
};

/**
 * The 'private' Cohort FSAL handle
 */

struct cohort_handle {
	struct fsal_obj_handle handle;	/*< The public handle */
	Fh *fd;
	struct Inode *i;	/*< The Cohort inode */
	const struct fsal_up_vector *up_ops;	/*< Upcall operations */
	struct cohort_export *export;	/*< The first export listed */
	vinodeno_t vi;		/*< The object identifier */
	fsal_openflags_t openflags;
#ifdef COHORT_PNFS
	uint64_t rd_issued;
	uint64_t rd_serial;
	uint64_t rw_issued;
	uint64_t rw_serial;
	uint64_t rw_max_len;
#endif				/* COHORT_PNFS */
};

/**
 * The full, 'private' DS (data server) handle
 */

struct cohort_ds {
	struct fsal_ds_handle ds;	/*< Public DS handle */
	uuid_t volume;
	char object_key[114];		/* MUST be same length as
					struct alloc_file_handle_v4
					pad[122] minus sizeof(uuid_t) */
};

#ifndef COHORT_INTERNAL_C
/* Keep internal.c from clashing with itself */
extern attrmask_t supported_attributes;
extern attrmask_t settable_attributes;
#endif				/* !COHORT_INTERNAL_C */

/**
 * Linux supports a stripe pattern with no more than 4096 stripes, but
 * for now we stick to 1024 to keep them da_addrs from being too
 * gigantic.
 */

static const size_t BIGGEST_PATTERN = 1024;

/* private helper for export object */

static inline fsal_staticfsinfo_t *cohort_staticinfo(struct fsal_module *hdl)
{
	struct cohort_fsal_module *myself =
	    container_of(hdl, struct cohort_fsal_module, fsal);
	return &myself->fs_info;
}

/* Prototypes */

int construct_handle(const struct stat *st, struct Inode *i,
		     struct cohort_export *export, struct cohort_handle **obj);
void deconstruct_handle(struct cohort_handle *obj);

/**
 * @brief FSAL status from Cohort error
 *
 * This function returns a fsal_status_t with the FSAL error as the
 * major, and the posix error as minor. (Cohort's error codes are just
 * negative signed versions of POSIX error codes.)
 *
 * @param[in] cohort_errorcode Cohort error (negative Posix)
 *
 * @return FSAL status.
 */
static inline fsal_status_t cohort2fsal_error(const int cohort_errorcode)
{
	return fsalstat(posix2fsal_error(-cohort_errorcode), -cohort_errorcode);
}
void cohort2fsal_attributes(const struct stat *buffstat,
			    struct attrlist *fsalattr);

struct fsal_staticfsinfo_t *cohort_staticinfo(struct fsal_module *hdl);

void export_ops_init(struct export_ops *ops);
void handle_ops_init(struct fsal_obj_ops *ops);
void ds_ops_init(struct fsal_ds_ops *ops);
#ifdef COHORT_PNFS
void export_ops_pnfs(struct export_ops *ops);
void handle_ops_pnfs(struct fsal_obj_ops *ops);
#endif				/* COHORT_PNFS */

#endif				/* !FSAL_COHORT_INTERNAL_INTERNAL__ */
