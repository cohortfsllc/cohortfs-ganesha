/*
 * vim:noexpandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright (C) CohortFS LLC, 2015
 * Author: Daniel Gryniewicz <dang@cohortfs.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/* file.c
 * File I/O methods for NULL module
 */

#include "config.h"

#include <assert.h>
#include "fsal.h"
#include "FSAL/access_check.h"
#include "fsal_convert.h"
#include <unistd.h>
#include <fcntl.h>
#include "FSAL/fsal_commonlib.h"
#include "mdcache_methods.h"


/** mdcache_open
 * called with appropriate locks taken at the cache inode level
 */

fsal_status_t mdcache_open(struct fsal_obj_handle *obj_hdl,
			  fsal_openflags_t openflags)
{
	struct mdcache_fsal_obj_handle *handle =
		container_of(obj_hdl, struct mdcache_fsal_obj_handle,
			     obj_handle);

	struct mdcache_fsal_export *export =
		container_of(op_ctx->fsal_export, struct mdcache_fsal_export,
			     export);

	/* attributes : upper layer to subfsal */
	mdcache_copy_attrlist(&handle->sub_handle->attributes,
			     &handle->obj_handle.attributes);
	/* calling subfsal method */
	op_ctx->fsal_export = export->sub_export;
	fsal_status_t status =
		handle->sub_handle->obj_ops.open(handle->sub_handle, openflags);
	op_ctx->fsal_export = &export->export;
	/* attributes : subfsal to upper layer */
	mdcache_copy_attrlist(&handle->obj_handle.attributes,
			     &handle->sub_handle->attributes);

	return status;
}

/* mdcache_status
 * Let the caller peek into the file's open/close state.
 */

fsal_openflags_t mdcache_status(struct fsal_obj_handle *obj_hdl)
{
	struct mdcache_fsal_obj_handle *handle =
		container_of(obj_hdl, struct mdcache_fsal_obj_handle,
			     obj_handle);

	struct mdcache_fsal_export *export =
		container_of(op_ctx->fsal_export, struct mdcache_fsal_export,
			     export);

	/* attributes : upper layer to subfsal */
	mdcache_copy_attrlist(&handle->sub_handle->attributes,
			     &handle->obj_handle.attributes);
	/* calling subfsal method */
	op_ctx->fsal_export = export->sub_export;
	fsal_openflags_t status =
		handle->sub_handle->obj_ops.status(handle->sub_handle);
	op_ctx->fsal_export = &export->export;
	/* attributes : subfsal to upper layer */
	mdcache_copy_attrlist(&handle->obj_handle.attributes,
			     &handle->sub_handle->attributes);

	return status;
}

/* mdcache_read
 * concurrency (locks) is managed in cache_inode_*
 */

fsal_status_t mdcache_read(struct fsal_obj_handle *obj_hdl,
			  uint64_t offset,
			  size_t buffer_size, void *buffer,
			  size_t *read_amount,
			  bool *end_of_file)
{
	struct mdcache_fsal_obj_handle *handle =
		container_of(obj_hdl, struct mdcache_fsal_obj_handle,
			     obj_handle);

	struct mdcache_fsal_export *export =
		container_of(op_ctx->fsal_export, struct mdcache_fsal_export,
			     export);

	/* attributes : upper layer to subfsal */
	mdcache_copy_attrlist(&handle->sub_handle->attributes,
			     &handle->obj_handle.attributes);
	/* calling subfsal method */
	op_ctx->fsal_export = export->sub_export;
	fsal_status_t status =
		handle->sub_handle->obj_ops.read(handle->sub_handle, offset,
						 buffer_size, buffer,
						 read_amount, end_of_file);
	op_ctx->fsal_export = &export->export;
	/* attributes : subfsal to upper layer */
	mdcache_copy_attrlist(&handle->obj_handle.attributes,
			     &handle->sub_handle->attributes);

	return status;
}

/* mdcache_write
 * concurrency (locks) is managed in cache_inode_*
 */

fsal_status_t mdcache_write(struct fsal_obj_handle *obj_hdl,
			   uint64_t offset,
			   size_t buffer_size, void *buffer,
			   size_t *write_amount, bool *fsal_stable)
{
	struct mdcache_fsal_obj_handle *handle =
		container_of(obj_hdl, struct mdcache_fsal_obj_handle,
			     obj_handle);

	struct mdcache_fsal_export *export =
		container_of(op_ctx->fsal_export, struct mdcache_fsal_export,
			     export);

	/* attributes : upper layer to subfsal */
	mdcache_copy_attrlist(&handle->sub_handle->attributes,
			     &handle->obj_handle.attributes);
	/* calling subfsal method */
	op_ctx->fsal_export = export->sub_export;
	fsal_status_t status =
		handle->sub_handle->obj_ops.write(handle->sub_handle,
						  offset,
						  buffer_size,
						  buffer,
						  write_amount,
						  fsal_stable);
	op_ctx->fsal_export = &export->export;
	/* attributes : subfsal to upper layer */
	mdcache_copy_attrlist(&handle->obj_handle.attributes,
			     &handle->sub_handle->attributes);

	return status;
}

/* mdcache_commit
 * Commit a file range to storage.
 * for right now, fsync will have to do.
 */

fsal_status_t mdcache_commit(struct fsal_obj_handle *obj_hdl,	/* sync */
			    off_t offset, size_t len)
{
	struct mdcache_fsal_obj_handle *handle =
		container_of(obj_hdl, struct mdcache_fsal_obj_handle,
			     obj_handle);

	struct mdcache_fsal_export *export =
		container_of(op_ctx->fsal_export, struct mdcache_fsal_export,
			     export);

	/* attributes : upper layer to subfsal */
	mdcache_copy_attrlist(&handle->sub_handle->attributes,
			     &handle->obj_handle.attributes);
	/* calling subfsal method */
	op_ctx->fsal_export = export->sub_export;
	fsal_status_t status =
		handle->sub_handle->obj_ops.commit(handle->sub_handle,
						   offset, len);
	op_ctx->fsal_export = &export->export;
	/* attributes : subfsal to upper layer */
	mdcache_copy_attrlist(&handle->obj_handle.attributes,
			     &handle->sub_handle->attributes);

	return status;
}

/* mdcache_lock_op
 * lock a region of the file
 * throw an error if the fd is not open.  The old fsal didn't
 * check this.
 */

fsal_status_t mdcache_lock_op(struct fsal_obj_handle *obj_hdl,
			     void *p_owner,
			     fsal_lock_op_t lock_op,
			     fsal_lock_param_t *request_lock,
			     fsal_lock_param_t *conflicting_lock)
{
	struct mdcache_fsal_obj_handle *handle =
		container_of(obj_hdl, struct mdcache_fsal_obj_handle,
			     obj_handle);

	struct mdcache_fsal_export *export =
		container_of(op_ctx->fsal_export, struct mdcache_fsal_export,
			     export);

	/* attributes : upper layer to subfsal */
	mdcache_copy_attrlist(&handle->sub_handle->attributes,
			     &handle->obj_handle.attributes);
	/* calling subfsal method */
	op_ctx->fsal_export = export->sub_export;
	fsal_status_t status =
		handle->sub_handle->obj_ops.lock_op(handle->sub_handle,
						    p_owner,
						    lock_op,
						    request_lock,
						    conflicting_lock);
	op_ctx->fsal_export = &export->export;
	/* attributes : subfsal to upper layer */
	mdcache_copy_attrlist(&handle->obj_handle.attributes,
			     &handle->sub_handle->attributes);

	return status;
}

/* mdcache_close
 * Close the file if it is still open.
 * Yes, we ignor lock status.  Closing a file in POSIX
 * releases all locks but that is state and cache inode's problem.
 */

fsal_status_t mdcache_close(struct fsal_obj_handle *obj_hdl)
{
	struct mdcache_fsal_obj_handle *handle =
		container_of(obj_hdl, struct mdcache_fsal_obj_handle,
			     obj_handle);

	struct mdcache_fsal_export *export =
		container_of(op_ctx->fsal_export, struct mdcache_fsal_export,
			     export);

	/* attributes : upper layer to subfsal */
	mdcache_copy_attrlist(&handle->sub_handle->attributes,
			     &handle->obj_handle.attributes);
	/* calling subfsal method */
	op_ctx->fsal_export = export->sub_export;
	fsal_status_t status =
		handle->sub_handle->obj_ops.close(handle->sub_handle);
	op_ctx->fsal_export = &export->export;
	/* attributes : subfsal to upper layer */
	mdcache_copy_attrlist(&handle->obj_handle.attributes,
			     &handle->sub_handle->attributes);

	return status;
}

/* mdcache_lru_cleanup
 * free non-essential resources at the request of cache inode's
 * LRU processing identifying this handle as stale enough for resource
 * trimming.
 */

fsal_status_t mdcache_lru_cleanup(struct fsal_obj_handle *obj_hdl,
				 lru_actions_t requests)
{
	struct mdcache_fsal_obj_handle *handle =
		container_of(obj_hdl, struct mdcache_fsal_obj_handle,
			     obj_handle);

	struct mdcache_fsal_export *export =
		container_of(op_ctx->fsal_export, struct mdcache_fsal_export,
			     export);

	/* attributes : upper layer to subfsal */
	mdcache_copy_attrlist(&handle->sub_handle->attributes,
			     &handle->obj_handle.attributes);
	/* calling subfsal method */
	op_ctx->fsal_export = export->sub_export;
	fsal_status_t status =
		handle->sub_handle->obj_ops.lru_cleanup(handle->sub_handle,
							requests);
	op_ctx->fsal_export = &export->export;
	/* attributes : subfsal to upper layer */
	mdcache_copy_attrlist(&handle->obj_handle.attributes,
		&handle->sub_handle->attributes);

	return status;
}
