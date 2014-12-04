/*
 * vim:noexpandtab:shiftwidth=8:tabstop=8:tw=80:
 *
 * Copyright © 2012-2014, CohortFS, LLC.
 * Author: Adam C. Emerson <aemerson@linuxbox.com>
 *
 * contributeur : William Allen Simpson <bill@cohortfs.com>
 *		  Marcus Watts <mdw@cohortfs.com>
 *		  Daniel Gryniewicz <dang@cohortfs.com>
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
 * @file   ds.c
 * @author Adam C. Emerson <aemerson@linuxbox.com>
 * @author William Allen Simpson <bill@cohortfs.com>
 * @author Marcus Watts <mdw@cohortfs.com>
 * @author Daniel Gryniewicz <dang@cohortfs.com>
 * @date Wed Oct 22 13:24:33 2014
 *
 * @brief pNFS DS operations for Cohort
 *
 * This file implements the read, write, commit, and dispose
 * operations for Cohort data-server handles.  The functionality to
 * create a data server handle is in the export.c file, as it is part
 * of the export object's interface.
 */

#include "config.h"

#include <cephfs/libcephfs.h>
#include <fcntl.h>
#include "fsal.h"
#include "fsal_api.h"
#include "FSAL/fsal_commonlib.h"
#include "fsal_up.h"
#include "internal.h"
#include "pnfs_utils.h"

/**
 * @brief Release a DS object
 *
 * @param[in] obj_pub The object to release
 *
 * @return NFS Status codes.
 */

static void release(struct fsal_ds_handle *const ds_pub)
{
	/* The private 'full' DS handle */
	struct cohort_ds *ds = container_of(ds_pub, struct cohort_ds, ds);
	fsal_ds_handle_uninit(&ds->ds);
	gsh_free(ds);
}

/**
 * @brief Read from a data-server handle.
 *
 * NFSv4.1 data server handles are disjount from normal
 * filehandles (in Ganesha, there is a ds_flag in the filehandle_v4_t
 * structure) and do not get loaded into cache_inode or processed the
 * normal way.
 *
 * @param[in]  ds_pub           FSAL DS handle
 * @param[in]  req_ctx          Credentials
 * @param[in]  stateid          The stateid supplied with the READ operation,
 *                              for validation
 * @param[in]  offset           The offset at which to read
 * @param[in]  requested_length Length of read requested (and size of buffer)
 * @param[out] buffer           The buffer to which to store read data
 * @param[out] supplied_length  Length of data read
 * @param[out] eof              True on end of file
 *
 * @return An NFSv4.1 status code.
 */
static nfsstat4 ds_read(struct fsal_ds_handle *const ds_pub,
			struct req_op_context *const req_ctx,
			const stateid4 *stateid, const offset4 offset,
			const count4 requested_length, void *const buffer,
			count4 * const supplied_length,
			bool * const end_of_file)
{
	/* The private 'full' DS handle */
	struct cohort_ds *ds = container_of(ds_pub, struct cohort_ds, ds);
	/* The amount actually read */
	int amount_read;

	amount_read =
		libosd_read(CohortFSM.osd, ds->wire.object_key, ds->wire.volume,
			offset, requested_length, (char *)buffer,
			LIBOSD_READ_FLAGS_NONE, NULL, NULL);
	if (amount_read < 0)
		return posix2nfs4_error(-amount_read);

	LogDebug(COMPONENT_FSAL, "amount read: %d", amount_read);

	*supplied_length = amount_read;

	*end_of_file = false;

	return NFS4_OK;
}

/**
 *
 * @brief Write to a data-server handle.
 *
 * This performs a DS write not going through the data server unless
 * FILE_SYNC4 is specified, in which case it connects the filehandle
 * and performs an MDS write.
 *
 * @param[in]  ds_pub           FSAL DS handle
 * @param[in]  req_ctx          Credentials
 * @param[in]  stateid          The stateid supplied with the READ operation,
 *                              for validation
 * @param[in]  offset           The offset at which to read
 * @param[in]  write_length     Length of write requested (and size of buffer)
 * @param[out] buffer           The buffer to which to store read data
 * @param[in]  stability wanted Stability of write
 * @param[out] written_length   Length of data written
 * @param[out] writeverf        Write verifier
 * @param[out] stability_got    Stability used for write (must be as
 *                              or more stable than request)
 *
 * @return An NFSv4.1 status code.
 */
static nfsstat4 ds_write(struct fsal_ds_handle *const ds_pub,
			 struct req_op_context *const req_ctx,
			 const stateid4 *stateid, const offset4 offset,
			 const count4 write_length, const void *buffer,
			 const stable_how4 stability_wanted,
			 count4 * const written_length,
			 verifier4 * const writeverf,
			 stable_how4 * const stability_got)
{
	/* The private 'full' DS handle */
	struct cohort_ds *ds = container_of(ds_pub, struct cohort_ds, ds);
	/* The amount actually written */
	int amount_written;

	amount_written =
		libosd_write(CohortFSM.osd, ds->wire.object_key,
			ds->wire.volume, offset, write_length,
			(char *)buffer, (stability_wanted >= DATA_SYNC4) ?
			LIBOSD_WRITE_CB_STABLE : LIBOSD_WRITE_CB_UNSTABLE,
			NULL, NULL);

	if (amount_written < 0) {
		return posix2nfs4_error(-amount_written);
	}

	LogDebug(COMPONENT_FSAL, "write_length: %d, amount written: %d",
	    write_length, amount_written);

	*stability_got = stability_wanted;

	/* libosd cannot do file sync, just data sync */
	if (stability_wanted >= DATA_SYNC4) {
		*stability_got = DATA_SYNC4;
	}

	*written_length = amount_written;

	memset(*writeverf, 0, NFS4_VERIFIER_SIZE);
	return NFS4_OK;
}

/**
 * @brief Commit a byte range to a DS handle.
 *
 * NFSv4.1 data server filehandles are disjount from normal
 * filehandles (in Ganesha, there is a ds_flag in the filehandle_v4_t
 * structure) and do not get loaded into cache_inode or processed the
 * normal way.
 *
 * @param[in]  ds_pub    FSAL DS handle
 * @param[in]  req_ctx   Credentials
 * @param[in]  offset    Start of commit window
 * @param[in]  count     Length of commit window
 * @param[out] writeverf Write verifier
 *
 * @return An NFSv4.1 status code.
 */
static nfsstat4 ds_commit(struct fsal_ds_handle *const ds_pub,
			  struct req_op_context *const req_ctx,
			  const offset4 offset, const count4 count,
			  verifier4 * const writeverf)
{
	/* Currently no commit for libosd */
#if 0
	/* The private 'full' export */
	struct cohort_export *export =
	    container_of(req_ctx->fsal_export, struct cohort_export, export);
	/* The private 'full' DS handle */
	struct cohort_ds *ds = container_of(ds_pub, struct cohort_ds, ds);
	/* Error return from Cohort */
	int rc = 0;
	Inode *i;

	/* Find out what stripe we're writing to and where within the
	   stripe. */

	i = ceph_ll_get_inode(export->cmount, ds->wire.vi);
	if (!i)
		return posix2nfs4_error(EINVAL);
	rc = ceph_ll_commit_blocks(export->cmount, i, offset,
				   (count == 0) ? UINT64_MAX : count);
	if (rc < 0)
		return posix2nfs4_error(rc);


#endif
	memset(*writeverf, 0, NFS4_VERIFIER_SIZE);
	return NFS4_OK;
}

void ds_ops_init(struct fsal_ds_ops *ops)
{
	ops->release = release;
	ops->read = ds_read;
	ops->write = ds_write;
	ops->commit = ds_commit;
};
