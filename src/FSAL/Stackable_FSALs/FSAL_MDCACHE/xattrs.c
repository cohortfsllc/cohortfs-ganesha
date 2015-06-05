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
 * 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

/* xattrs.c
 * NULL object (file|dir) handle object extended attributes
 */

#include "config.h"

#include "fsal.h"
#include <libgen.h>		/* used for 'dirname' */
#include <pthread.h>
#include <string.h>
#include <sys/types.h>
#include <os/xattr.h>
#include <ctype.h>
#include "gsh_list.h"
#include "fsal_convert.h"
#include "FSAL/fsal_commonlib.h"
#include "mdcache_methods.h"

fsal_status_t mdcache_list_ext_attrs(struct fsal_obj_handle *obj_hdl,
				    unsigned int argcookie,
				    fsal_xattrent_t *xattrs_tab,
				    unsigned int xattrs_tabsize,
				    unsigned int *p_nb_returned,
				    int *end_of_list)
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
	fsal_status_t status = handle->sub_handle->obj_ops.list_ext_attrs(
		handle->sub_handle, argcookie,
		xattrs_tab, xattrs_tabsize,
		p_nb_returned, end_of_list);
	op_ctx->fsal_export = &export->export;
	/* attributes : subfsal to upper layer */
	mdcache_copy_attrlist(&handle->obj_handle.attributes,
			     &handle->sub_handle->attributes);

	return status;
}

fsal_status_t mdcache_getextattr_id_by_name(struct fsal_obj_handle *obj_hdl,
					   const char *xattr_name,
					   unsigned int *pxattr_id)
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
		handle->sub_handle->obj_ops.getextattr_id_by_name(
				handle->sub_handle, xattr_name, pxattr_id);
	op_ctx->fsal_export = &export->export;
	/* attributes : subfsal to upper layer */
	mdcache_copy_attrlist(&handle->obj_handle.attributes,
			     &handle->sub_handle->attributes);

	return status;
}

fsal_status_t mdcache_getextattr_value_by_id(struct fsal_obj_handle *obj_hdl,
					    unsigned int xattr_id,
					    caddr_t buffer_addr,
					    size_t buffer_size,
					    size_t *p_output_size)
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
	handle->sub_handle->obj_ops.getextattr_value_by_id(
				handle->sub_handle,
				xattr_id, buffer_addr,
				buffer_size,
				p_output_size);
	op_ctx->fsal_export = &export->export;
	/* attributes : subfsal to upper layer */
	mdcache_copy_attrlist(&handle->obj_handle.attributes,
			     &handle->sub_handle->attributes);

	return status;
}

fsal_status_t mdcache_getextattr_value_by_name(struct fsal_obj_handle *obj_hdl,
					      const char *xattr_name,
					      caddr_t buffer_addr,
					      size_t buffer_size,
					      size_t *p_output_size)
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
		handle->sub_handle->obj_ops.getextattr_value_by_name(
				handle->sub_handle,
				xattr_name,
				buffer_addr,
				buffer_size,
				p_output_size);
	op_ctx->fsal_export = &export->export;
	/* attributes : subfsal to upper layer */
	mdcache_copy_attrlist(&handle->obj_handle.attributes,
			     &handle->sub_handle->attributes);

	return status;
}

fsal_status_t mdcache_setextattr_value(struct fsal_obj_handle *obj_hdl,
				      const char *xattr_name,
				      caddr_t buffer_addr, size_t buffer_size,
				      int create)
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
	fsal_status_t status = handle->sub_handle->obj_ops.setextattr_value(
		handle->sub_handle, xattr_name,
		buffer_addr, buffer_size,
		create);
	op_ctx->fsal_export = &export->export;
	/* attributes : subfsal to upper layer */
	mdcache_copy_attrlist(&handle->obj_handle.attributes,
			     &handle->sub_handle->attributes);

	return status;
}

fsal_status_t mdcache_setextattr_value_by_id(struct fsal_obj_handle *obj_hdl,
					    unsigned int xattr_id,
					    caddr_t buffer_addr,
					    size_t buffer_size)
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
		handle->sub_handle->obj_ops.setextattr_value_by_id(
				handle->sub_handle,
				xattr_id, buffer_addr,
				buffer_size);
	op_ctx->fsal_export = &export->export;
	/* attributes : subfsal to upper layer */
	mdcache_copy_attrlist(&handle->obj_handle.attributes,
			     &handle->sub_handle->attributes);

	return status;
}

fsal_status_t mdcache_getextattr_attrs(struct fsal_obj_handle *obj_hdl,
				      unsigned int xattr_id,
				      struct attrlist *p_attrs)
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
	fsal_status_t status = handle->sub_handle->obj_ops.getextattr_attrs(
		handle->sub_handle, xattr_id,
		p_attrs);
	op_ctx->fsal_export = &export->export;
	/* attributes : subfsal to upper layer */
	mdcache_copy_attrlist(&handle->obj_handle.attributes,
			     &handle->sub_handle->attributes);

	return status;
}

fsal_status_t mdcache_remove_extattr_by_id(struct fsal_obj_handle *obj_hdl,
					  unsigned int xattr_id)
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
	fsal_status_t status = handle->sub_handle->obj_ops.remove_extattr_by_id(
		handle->sub_handle, xattr_id);
	op_ctx->fsal_export = &export->export;
	/* attributes : subfsal to upper layer */
	mdcache_copy_attrlist(&handle->obj_handle.attributes,
		&handle->sub_handle->attributes);

	return status;
}

fsal_status_t mdcache_remove_extattr_by_name(struct fsal_obj_handle *obj_hdl,
					    const char *xattr_name)
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
		handle->sub_handle->obj_ops.remove_extattr_by_name(
				handle->sub_handle, xattr_name);
	op_ctx->fsal_export = &export->export;
	/* attributes : subfsal to upper layer */
	mdcache_copy_attrlist(&handle->obj_handle.attributes,
			     &handle->sub_handle->attributes);

	return status;
}
