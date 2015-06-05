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

/* handle.c
 */

#include "config.h"

#include "fsal.h"
#include <libgen.h>		/* used for 'dirname' */
#include <pthread.h>
#include <string.h>
#include <sys/types.h>
#include "gsh_list.h"
#include "fsal_convert.h"
#include "FSAL/fsal_commonlib.h"
#include "mdcache_methods.h"
#include "nfs4_acls.h"
#include <os/subr.h>

static void mdcache_handle_ops_init(struct fsal_obj_ops *ops);

/* helpers
 */

/**
 * Release the acl of an attrlist, with error checking and logging.
 *
 * This function is a wrapper of nfs4_acl_release_entry, checking its return
 * value and logging any error.
 *
 * The argument must be not null. Its acl field can be null : in this case this
 * function does nothing.
 *
 * If the function is successful, attrs->acl is set to NULL.
 *
 * @param[in,out] attrs The attrlist which acl field will be released.
 */
static void mdcache_attrlist_acl_release(struct attrlist *attrs)
{
	fsal_acl_status_t status;
	/* checking if the pointer is null is done in nfs4_acl_release_entry. */
	nfs4_acl_release_entry(attrs->acl, &status);

	if (status != NFS_V4_ACL_SUCCESS)
		LogCrit(COMPONENT_FSAL, "Release of acl failed (status = %i)",
			status);
	else
		attrs->acl = NULL;
}

void mdcache_copy_attrlist(struct attrlist *dest, struct attrlist *source)
{
	if (dest == NULL || source == NULL)
		return;

	if (source->mask != 0LL) {
		/* A full copy of the structure is done. It can obviously be
		 * optimized.
		 *
		 * Testing each bit of the mask to copy fields one by one
		 * doesn't seems efficient either. I think the best way would be
		 * to check and copy the fields that are likely to be changed in
		 * each fsal function, and to call a general purpose function
		 * like this only if unexpected bits remains in the source mask.
		 */

		/* We must handle attrlist.acl field separately, because it has
		 * a refcounter.
		 */
		if (dest->acl != source->acl) {
			if (source->acl != NULL)
				nfs4_acl_entry_inc_ref(source->acl);

			mdcache_attrlist_acl_release(dest);
		}

		memcpy(dest, source, sizeof(*dest));
		FSAL_CLEAR_MASK(source->mask);
	}
}


/* handle methods
 */

/**
 * Allocate and initialize a new mdcache handle.
 *
 * This function doesn't free the sub_handle if the allocation fails. It must
 * be done in the calling function.
 *
 * @param[in] export The mdcache export used by the handle.
 * @param[in] sub_handle The handle used by the subfsal.
 * @param[in] fs The filesystem of the new handle.
 *
 * @return The new handle, or NULL if the allocation failed.
 */
static struct mdcache_fsal_obj_handle *mdcache_alloc_handle(
		struct mdcache_fsal_export *export,
		struct fsal_obj_handle *sub_handle,
		struct fsal_filesystem *fs)
{
	struct mdcache_fsal_obj_handle *result =
		gsh_calloc(1, sizeof(struct mdcache_fsal_obj_handle));
	if (result) {
		/* default handlers */
		fsal_obj_handle_init(&result->obj_handle, &export->export,
				     sub_handle->type);
		/* mdcache handlers */
		mdcache_handle_ops_init(&result->obj_handle.obj_ops);
		result->sub_handle = sub_handle;
		result->obj_handle.type = sub_handle->type;
		result->obj_handle.fs = fs;
		/* attributes */
		mdcache_copy_attrlist(&result->obj_handle.attributes,
			&sub_handle->attributes);
	}

	return result;
}

/**
 * Attempts to create a new mdcache handle, or cleanup memory if it fails.
 *
 * This function is a wrapper of mdcache_alloc_handle. It adds error checking
 * and logging. It also cleans objects allocated in the subfsal if it fails.
 *
 * @param[in] export The mdcache export used by the handle.
 * @param[in,out] sub_handle The handle used by the subfsal.
 * @param[in] fs The filesystem of the new handle.
 * @param[in] new_handle Address where the new allocated pointer should be
 * written.
 * @param[in] subfsal_status Result of the allocation of the subfsal handle.
 *
 * @return An error code for the function.
 */
static fsal_status_t mdcache_alloc_and_check_handle(
		struct mdcache_fsal_export *export,
		struct fsal_obj_handle *sub_handle,
		struct fsal_filesystem *fs,
		struct fsal_obj_handle **new_handle,
		fsal_status_t subfsal_status)
{
	/** Result status of the operation. */
	fsal_status_t status = subfsal_status;

	if (!FSAL_IS_ERROR(subfsal_status)) {
		struct mdcache_fsal_obj_handle *null_handle =
			mdcache_alloc_handle(export, sub_handle, fs);
		if (null_handle == NULL) {
			status = fsalstat(ERR_FSAL_NOMEM, ENOMEM);
			LogCrit(COMPONENT_FSAL, "Out of memory");

			sub_handle->obj_ops.release(sub_handle);
		} else {
			*new_handle = &null_handle->obj_handle;
		}
	}
	return status;
}

/* lookup
 * deprecated NULL parent && NULL path implies root handle
 */

static fsal_status_t lookup(struct fsal_obj_handle *parent,
			    const char *path, struct fsal_obj_handle **handle)
{
	/** Parent as mdcache handle.*/
	struct mdcache_fsal_obj_handle *null_parent =
		container_of(parent, struct mdcache_fsal_obj_handle, obj_handle);

	/** Handle given by the subfsal. */
	struct fsal_obj_handle *sub_handle = NULL;

	*handle = NULL;

	/* call to subfsal lookup with the good context. */
	fsal_status_t status;
	/** Current mdcache export. */
	struct mdcache_fsal_export *export =
		container_of(op_ctx->fsal_export, struct mdcache_fsal_export,
			     export);
	op_ctx->fsal_export = export->sub_export;
	status = null_parent->sub_handle->obj_ops.lookup(
			null_parent->sub_handle, path, &sub_handle);
	op_ctx->fsal_export = &export->export;

	/* wraping the subfsal handle in a mdcache handle. */
	return mdcache_alloc_and_check_handle(export, sub_handle, parent->fs,
					     handle, status);
}

static fsal_status_t create(struct fsal_obj_handle *dir_hdl,
			    const char *name, struct attrlist *attrib,
			    struct fsal_obj_handle **handle)
{
	/** Parent directory mdcache handle. */
	struct mdcache_fsal_obj_handle *mdcache_dir =
		container_of(dir_hdl, struct mdcache_fsal_obj_handle,
			     obj_handle);
	/** Current mdcache export. */
	struct mdcache_fsal_export *export =
		container_of(op_ctx->fsal_export, struct mdcache_fsal_export,
			     export);

	/** Subfsal handle of the new file.*/
	struct fsal_obj_handle *sub_handle;

	*handle = NULL;

	/* creating the file with a subfsal handle. */
	op_ctx->fsal_export = export->sub_export;
	fsal_status_t status = mdcache_dir->sub_handle->obj_ops.create(
		mdcache_dir->sub_handle, name, attrib, &sub_handle);
	op_ctx->fsal_export = &export->export;

	/* wraping the subfsal handle in a mdcache handle. */
	return mdcache_alloc_and_check_handle(export, sub_handle, dir_hdl->fs,
					     handle, status);
}

static fsal_status_t makedir(struct fsal_obj_handle *dir_hdl,
			     const char *name, struct attrlist *attrib,
			     struct fsal_obj_handle **handle)
{
	*handle = NULL;
	/** Parent directory mdcache handle. */
	struct mdcache_fsal_obj_handle *parent_hdl =
		container_of(dir_hdl, struct mdcache_fsal_obj_handle,
			     obj_handle);
	/** Current mdcache export. */
	struct mdcache_fsal_export *export =
		container_of(op_ctx->fsal_export, struct mdcache_fsal_export,
			     export);

	/** Subfsal handle of the new directory.*/
	struct fsal_obj_handle *sub_handle;

	/* Creating the directory with a subfsal handle. */
	op_ctx->fsal_export = export->sub_export;
	fsal_status_t status = parent_hdl->sub_handle->obj_ops.mkdir(
		parent_hdl->sub_handle, name, attrib, &sub_handle);
	op_ctx->fsal_export = &export->export;

	/* wraping the subfsal handle in a mdcache handle. */
	return mdcache_alloc_and_check_handle(export, sub_handle, dir_hdl->fs,
					     handle, status);
}

static fsal_status_t makenode(struct fsal_obj_handle *dir_hdl,
			      const char *name, object_file_type_t nodetype,
			      fsal_dev_t *dev,	/* IN */
			      struct attrlist *attrib,
			      struct fsal_obj_handle **handle)
{
	/** Parent directory mdcache handle. */
	struct mdcache_fsal_obj_handle *mdcache_dir =
		container_of(dir_hdl, struct mdcache_fsal_obj_handle,
			     obj_handle);
	/** Current mdcache export. */
	struct mdcache_fsal_export *export =
		container_of(op_ctx->fsal_export, struct mdcache_fsal_export,
			     export);

	/** Subfsal handle of the new node.*/
	struct fsal_obj_handle *sub_handle;

	*handle = NULL;

	/* Creating the node with a subfsal handle. */
	op_ctx->fsal_export = export->sub_export;
	fsal_status_t status = mdcache_dir->sub_handle->obj_ops.mknode(
		mdcache_dir->sub_handle, name, nodetype, dev, attrib,
		&sub_handle);
	op_ctx->fsal_export = &export->export;

	/* wraping the subfsal handle in a mdcache handle. */
	return mdcache_alloc_and_check_handle(export, sub_handle, dir_hdl->fs,
					     handle, status);
}

/** makesymlink
 *  Note that we do not set mode bits on symlinks for Linux/POSIX
 *  They are not really settable in the kernel and are not checked
 *  anyway (default is 0777) because open uses that target's mode
 */

static fsal_status_t makesymlink(struct fsal_obj_handle *dir_hdl,
				 const char *name, const char *link_path,
				 struct attrlist *attrib,
				 struct fsal_obj_handle **handle)
{
	/** Parent directory mdcache handle. */
	struct mdcache_fsal_obj_handle *mdcache_dir =
		container_of(dir_hdl, struct mdcache_fsal_obj_handle,
			     obj_handle);
	/** Current mdcache export. */
	struct mdcache_fsal_export *export =
		container_of(op_ctx->fsal_export, struct mdcache_fsal_export,
			     export);

	/** Subfsal handle of the new link.*/
	struct fsal_obj_handle *sub_handle;

	*handle = NULL;

	/* creating the file with a subfsal handle. */
	op_ctx->fsal_export = export->sub_export;
	fsal_status_t status = mdcache_dir->sub_handle->obj_ops.symlink(
		mdcache_dir->sub_handle, name, link_path, attrib, &sub_handle);
	op_ctx->fsal_export = &export->export;

	/* wraping the subfsal handle in a mdcache handle. */
	return mdcache_alloc_and_check_handle(export, sub_handle, dir_hdl->fs,
					     handle, status);
}

static fsal_status_t readsymlink(struct fsal_obj_handle *obj_hdl,
				 struct gsh_buffdesc *link_content,
				 bool refresh)
{
	struct mdcache_fsal_obj_handle *handle =
		(struct mdcache_fsal_obj_handle *) obj_hdl;
	struct mdcache_fsal_export *export =
		container_of(op_ctx->fsal_export, struct mdcache_fsal_export,
			     export);

	/* attributes : upper layer to subfsal */
	mdcache_copy_attrlist(&handle->sub_handle->attributes,
			     &handle->obj_handle.attributes);
	/* calling subfsal method */
	op_ctx->fsal_export = export->sub_export;
	fsal_status_t status =
		handle->sub_handle->obj_ops.readlink(handle->sub_handle,
						     link_content, refresh);
	op_ctx->fsal_export = &export->export;
	/* attributes : subfsal to upper layer */
	mdcache_copy_attrlist(&handle->obj_handle.attributes,
			     &handle->sub_handle->attributes);

	return status;
}

static fsal_status_t linkfile(struct fsal_obj_handle *obj_hdl,
			      struct fsal_obj_handle *destdir_hdl,
			      const char *name)
{
	struct mdcache_fsal_obj_handle *handle =
		(struct mdcache_fsal_obj_handle *) obj_hdl;
	struct mdcache_fsal_obj_handle *mdcache_dir =
		(struct mdcache_fsal_obj_handle *) destdir_hdl;
	struct mdcache_fsal_export *export =
		container_of(op_ctx->fsal_export, struct mdcache_fsal_export,
			     export);

	/* attributes : upper layer to subfsal */
	mdcache_copy_attrlist(&handle->sub_handle->attributes,
			     &handle->obj_handle.attributes);
	mdcache_copy_attrlist(&mdcache_dir->sub_handle->attributes,
			     &mdcache_dir->obj_handle.attributes);
	/* calling subfsal method */
	op_ctx->fsal_export = export->sub_export;
	fsal_status_t status = handle->sub_handle->obj_ops.link(
		handle->sub_handle, mdcache_dir->sub_handle, name);
	op_ctx->fsal_export = &export->export;
	/* attributes : subfsal to upper layer */
	mdcache_copy_attrlist(&mdcache_dir->obj_handle.attributes,
			     &mdcache_dir->sub_handle->attributes);
	mdcache_copy_attrlist(&handle->obj_handle.attributes,
			     &handle->sub_handle->attributes);

	return status;
}

/**
 * Callback function for read_dirents.
 *
 * See fsal_readdir_cb type for more details.
 *
 * This function restores the context for the upper stacked fsal or inode.
 *
 * @param name Directly passed to upper layer.
 * @param dir_state A mdcache_readdir_state struct.
 * @param cookie Directly passed to upper layer.
 *
 * @return Result coming from the upper layer.
 */
static bool mdcache_readdir_cb(const char *name, void *dir_state,
			       fsal_cookie_t cookie)
{
	struct mdcache_readdir_state *state =
		(struct mdcache_readdir_state *) dir_state;

	op_ctx->fsal_export = &state->exp->export;
	bool result = state->cb(name, state->dir_state, cookie);

	op_ctx->fsal_export = state->exp->sub_export;

	return result;
}

/**
 * read_dirents
 * read the directory and call through the callback function for
 * each entry.
 * @param dir_hdl [IN] the directory to read
 * @param whence [IN] where to start (next)
 * @param dir_state [IN] pass thru of state to callback
 * @param cb [IN] callback function
 * @param eof [OUT] eof marker true == end of dir
 */

static fsal_status_t read_dirents(struct fsal_obj_handle *dir_hdl,
				  fsal_cookie_t *whence, void *dir_state,
				  fsal_readdir_cb cb, bool *eof)
{
	struct mdcache_fsal_obj_handle *handle =
		container_of(dir_hdl, struct mdcache_fsal_obj_handle,
			     obj_handle);

	struct mdcache_fsal_export *export =
		container_of(op_ctx->fsal_export, struct mdcache_fsal_export,
			     export);

	struct mdcache_readdir_state cb_state = {
		.cb = cb,
		.dir_state = dir_state,
		.exp = export
	};

	/* attributes : upper layer to subfsal */
	mdcache_copy_attrlist(&handle->sub_handle->attributes,
		&handle->obj_handle.attributes);
	/* calling subfsal method */
	op_ctx->fsal_export = export->sub_export;
	fsal_status_t status =
		handle->sub_handle->obj_ops.readdir(handle->sub_handle,
		whence, &cb_state, mdcache_readdir_cb, eof);
	op_ctx->fsal_export = &export->export;
	/* attributes : subfsal to upper layer */
	mdcache_copy_attrlist(&handle->obj_handle.attributes,
		&handle->sub_handle->attributes);

	return status;
}

static fsal_status_t renamefile(struct fsal_obj_handle *obj_hdl,
				struct fsal_obj_handle *olddir_hdl,
				const char *old_name,
				struct fsal_obj_handle *newdir_hdl,
				const char *new_name)
{
	struct mdcache_fsal_obj_handle *mdcache_olddir =
		container_of(olddir_hdl, struct mdcache_fsal_obj_handle,
			     obj_handle);
	struct mdcache_fsal_obj_handle *mdcache_newdir =
		container_of(newdir_hdl, struct mdcache_fsal_obj_handle,
			     obj_handle);
	struct mdcache_fsal_obj_handle *mdcache_obj =
		container_of(obj_hdl, struct mdcache_fsal_obj_handle,
			     obj_handle);

	struct mdcache_fsal_export *export =
		container_of(op_ctx->fsal_export, struct mdcache_fsal_export,
			     export);

	/* attributes : upper layer to subfsal */
	mdcache_copy_attrlist(&mdcache_olddir->sub_handle->attributes,
			     &mdcache_olddir->obj_handle.attributes);
	mdcache_copy_attrlist(&mdcache_newdir->sub_handle->attributes,
			     &mdcache_newdir->obj_handle.attributes);
	mdcache_copy_attrlist(&mdcache_obj->sub_handle->attributes,
			     &mdcache_obj->obj_handle.attributes);
	/* calling subfsal method */
	op_ctx->fsal_export = export->sub_export;
	fsal_status_t status = mdcache_olddir->sub_handle->obj_ops.rename(
		mdcache_obj->sub_handle, mdcache_olddir->sub_handle,
		old_name, mdcache_newdir->sub_handle, new_name);
	op_ctx->fsal_export = &export->export;
	/* attributes : subfsal to upper layer */
	mdcache_copy_attrlist(&mdcache_newdir->obj_handle.attributes,
			     &mdcache_newdir->sub_handle->attributes);
	mdcache_copy_attrlist(&mdcache_olddir->obj_handle.attributes,
			     &mdcache_olddir->sub_handle->attributes);
	mdcache_copy_attrlist(&mdcache_obj->obj_handle.attributes,
			     &mdcache_obj->sub_handle->attributes);
	return status;
}

static fsal_status_t getattrs(struct fsal_obj_handle *obj_hdl)
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
		handle->sub_handle->obj_ops.getattrs(handle->sub_handle);
	op_ctx->fsal_export = &export->export;
	/* attributes : subfsal to upper layer */
	mdcache_copy_attrlist(&handle->obj_handle.attributes,
			     &handle->sub_handle->attributes);

	return status;
}

/*
 * NOTE: this is done under protection of the
 * attributes rwlock in the cache entry.
 */

static fsal_status_t setattrs(struct fsal_obj_handle *obj_hdl,
			      struct attrlist *attrs)
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
	fsal_status_t status = handle->sub_handle->obj_ops.setattrs(
		handle->sub_handle, attrs);
	op_ctx->fsal_export = &export->export;
	/* attributes : subfsal to upper layer */
	mdcache_copy_attrlist(&handle->obj_handle.attributes,
			     &handle->sub_handle->attributes);

	return status;
}

/* file_unlink
 * unlink the named file in the directory
 */

static fsal_status_t file_unlink(struct fsal_obj_handle *dir_hdl,
				 const char *name)
{
	struct mdcache_fsal_obj_handle *mdcache_dir =
		container_of(dir_hdl, struct mdcache_fsal_obj_handle,
			     obj_handle);
	struct mdcache_fsal_export *export =
		container_of(op_ctx->fsal_export, struct mdcache_fsal_export,
			     export);

	/* attributes : upper layer to subfsal */
	mdcache_copy_attrlist(&mdcache_dir->sub_handle->attributes,
			     &mdcache_dir->obj_handle.attributes);
	/* calling subfsal method */
	op_ctx->fsal_export = export->sub_export;
	fsal_status_t status = mdcache_dir->sub_handle->obj_ops.unlink(
		mdcache_dir->sub_handle, name);
	op_ctx->fsal_export = &export->export;
	/* attributes : subfsal to upper layer */
	mdcache_copy_attrlist(&mdcache_dir->obj_handle.attributes,
			     &mdcache_dir->sub_handle->attributes);

	return status;
}

/* handle_digest
 * fill in the opaque f/s file handle part.
 * we zero the buffer to length first.  This MAY already be done above
 * at which point, remove memset here because the caller is zeroing
 * the whole struct.
 */

static fsal_status_t handle_digest(const struct fsal_obj_handle *obj_hdl,
				   fsal_digesttype_t output_type,
				   struct gsh_buffdesc *fh_desc)
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
	fsal_status_t status = handle->sub_handle->obj_ops.handle_digest(
		handle->sub_handle, output_type, fh_desc);
	op_ctx->fsal_export = &export->export;
	/* attributes : subfsal to upper layer */
	mdcache_copy_attrlist(&handle->obj_handle.attributes,
			     &handle->sub_handle->attributes);

	return status;
}

/**
 * handle_to_key
 * return a handle descriptor into the handle in this object handle
 * @TODO reminder.  make sure things like hash keys don't point here
 * after the handle is released.
 */

static void handle_to_key(struct fsal_obj_handle *obj_hdl,
			  struct gsh_buffdesc *fh_desc)
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
	handle->sub_handle->obj_ops.handle_to_key(handle->sub_handle, fh_desc);
	op_ctx->fsal_export = &export->export;
	/* attributes : subfsal to upper layer */
	mdcache_copy_attrlist(&handle->obj_handle.attributes,
			     &handle->sub_handle->attributes);
}

/*
 * release
 * release our export first so they know we are gone
 */

static void release(struct fsal_obj_handle *obj_hdl)
{
	struct mdcache_fsal_obj_handle *hdl =
		container_of(obj_hdl, struct mdcache_fsal_obj_handle,
			     obj_handle);

	struct mdcache_fsal_export *export =
		container_of(op_ctx->fsal_export, struct mdcache_fsal_export,
			     export);

	mdcache_attrlist_acl_release(&hdl->sub_handle->attributes);

	/* calling subfsal method */
	op_ctx->fsal_export = export->sub_export;
	hdl->sub_handle->obj_ops.release(hdl->sub_handle);
	op_ctx->fsal_export = &export->export;

	mdcache_attrlist_acl_release(&hdl->obj_handle.attributes);

	/* cleaning data allocated by mdcache */
	fsal_obj_handle_fini(&hdl->obj_handle);
	gsh_free(hdl);
}

static void mdcache_handle_ops_init(struct fsal_obj_ops *ops)
{
	ops->release = release;
	ops->lookup = lookup;
	ops->readdir = read_dirents;
	ops->create = create;
	ops->mkdir = makedir;
	ops->mknode = makenode;
	ops->symlink = makesymlink;
	ops->readlink = readsymlink;
	ops->test_access = fsal_test_access;
	ops->getattrs = getattrs;
	ops->setattrs = setattrs;
	ops->link = linkfile;
	ops->rename = renamefile;
	ops->unlink = file_unlink;
	ops->open = mdcache_open;
	ops->status = mdcache_status;
	ops->read = mdcache_read;
	ops->write = mdcache_write;
	ops->commit = mdcache_commit;
	ops->lock_op = mdcache_lock_op;
	ops->close = mdcache_close;
	ops->lru_cleanup = mdcache_lru_cleanup;
	ops->handle_digest = handle_digest;
	ops->handle_to_key = handle_to_key;

	/* xattr related functions */
	ops->list_ext_attrs = mdcache_list_ext_attrs;
	ops->getextattr_id_by_name = mdcache_getextattr_id_by_name;
	ops->getextattr_value_by_name = mdcache_getextattr_value_by_name;
	ops->getextattr_value_by_id = mdcache_getextattr_value_by_id;
	ops->setextattr_value = mdcache_setextattr_value;
	ops->setextattr_value_by_id = mdcache_setextattr_value_by_id;
	ops->getextattr_attrs = mdcache_getextattr_attrs;
	ops->remove_extattr_by_id = mdcache_remove_extattr_by_id;
	ops->remove_extattr_by_name = mdcache_remove_extattr_by_name;

}

/* export methods that create object handles
 */

/* lookup_path
 * modeled on old api except we don't stuff attributes.
 * KISS
 */

fsal_status_t mdcache_lookup_path(struct fsal_export *exp_hdl,
				 const char *path,
				 struct fsal_obj_handle **handle)
{
	/** Handle given by the subfsal. */
	struct fsal_obj_handle *sub_handle = NULL;
	*handle = NULL;

	/* call underlying FSAL ops with underlying FSAL handle */
	struct mdcache_fsal_export *exp =
		container_of(exp_hdl, struct mdcache_fsal_export, export);

	/* call to subfsal lookup with the good context. */
	fsal_status_t status;

	op_ctx->fsal_export = exp->sub_export;
	status = exp->sub_export->exp_ops.lookup_path(exp->sub_export, path,
						      &sub_handle);
	op_ctx->fsal_export = &exp->export;

	/* wraping the subfsal handle in a mdcache handle. */
	/* Note : mdcache filesystem = subfsal filesystem or NULL ? */
	return mdcache_alloc_and_check_handle(exp, sub_handle, NULL, handle,
					     status);
}

/* create_handle
 * Does what original FSAL_ExpandHandle did (sort of)
 * returns a ref counted handle to be later used in cache_inode etc.
 * NOTE! you must release this thing when done with it!
 * BEWARE! Thanks to some holes in the *AT syscalls implementation,
 * we cannot get an fd on an AF_UNIX socket, nor reliably on block or
 * character special devices.  Sorry, it just doesn't...
 * we could if we had the handle of the dir it is in, but this method
 * is for getting handles off the wire for cache entries that have LRU'd.
 * Ideas and/or clever hacks are welcome...
 */

fsal_status_t mdcache_create_handle(struct fsal_export *exp_hdl,
				   struct gsh_buffdesc *hdl_desc,
				   struct fsal_obj_handle **handle)
{
	/** Current mdcache export. */
	struct mdcache_fsal_export *export =
		container_of(exp_hdl, struct mdcache_fsal_export, export);

	struct fsal_obj_handle *sub_handle; /*< New subfsal handle.*/
	*handle = NULL;

	/* call to subfsal lookup with the good context. */
	fsal_status_t status;

	op_ctx->fsal_export = export->sub_export;

	status = export->sub_export->exp_ops.create_handle(export->sub_export,
		hdl_desc, &sub_handle);
	op_ctx->fsal_export = &export->export;

	/* wraping the subfsal handle in a mdcache handle. */
	/* Note : mdcache filesystem = subfsal filesystem or NULL ? */
	return mdcache_alloc_and_check_handle(export, sub_handle, NULL, handle,
					     status);
}
