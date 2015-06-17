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

/* export.c
 * NULL FSAL export object
 */

#include "config.h"

#include "fsal.h"
#include <libgen.h>		/* used for 'dirname' */
#include <pthread.h>
#include <string.h>
#include <sys/types.h>
#include <os/mntent.h>
#include <os/quota.h>
#include <dlfcn.h>
#include "gsh_list.h"
#include "config_parsing.h"
#include "fsal_convert.h"
#include "FSAL/fsal_commonlib.h"
#include "FSAL/fsal_config.h"
#include "mdcache_methods.h"
#include "nfs_exports.h"
#include "export_mgr.h"

/* helpers to/from other NULL objects
 */

struct fsal_staticfsinfo_t *mdcache_staticinfo(struct fsal_module *hdl);

/* export object methods
 */

/**
 * @brief Return the name of the sub-FSAL
 *
 * For MDCACHE, we want to return the name of the sub-FSAL, not ourselves.
 *
 * @param[in] exp_hdl	Our export handle
 * @return Name of sub-FSAL
 */
static char *mdcache_get_name(struct fsal_export *exp_hdl)
{
	struct mdcache_fsal_export *myself =
		container_of(exp_hdl, struct mdcache_fsal_export, export);
	return myself->sub_export->fsal->name;
}

static void mdcache_release(struct fsal_export *exp_hdl)
{
	struct mdcache_fsal_export *myself;
	struct fsal_module *sub_fsal;

	myself = container_of(exp_hdl, struct mdcache_fsal_export, export);
	sub_fsal = myself->sub_export->fsal;

	/* Release the sub_export */
	myself->sub_export->exp_ops.release(myself->sub_export);
	fsal_put(sub_fsal);

	fsal_detach_export(exp_hdl->fsal, &exp_hdl->exports);
	free_export_ops(exp_hdl);

	gsh_free(myself);	/* elvis has left the building */
}

static fsal_status_t get_dynamic_info(struct fsal_export *exp_hdl,
				      struct fsal_obj_handle *obj_hdl,
				      fsal_dynamicfsinfo_t *infop)
{
	struct mdcache_fsal_export *exp =
		container_of(exp_hdl, struct mdcache_fsal_export, export);

	struct mdcache_fsal_obj_handle *handle =
		container_of(obj_hdl, struct mdcache_fsal_obj_handle,
			     obj_handle);

	/* attributes : upper layer to subfsal */
	mdcache_copy_attrlist(&handle->sub_handle->attributes,
		&handle->obj_handle.attributes);
	/* calling subfsal method */
	op_ctx->fsal_export = exp->sub_export;
	fsal_status_t status = exp->sub_export->exp_ops.get_fs_dynamic_info(
		exp->sub_export, handle->sub_handle, infop);
	op_ctx->fsal_export = &exp->export;
	/* attributes : subfsal to upper layer */
	mdcache_copy_attrlist(&handle->obj_handle.attributes,
		&handle->sub_handle->attributes);

	return status;
}

static bool fs_supports(struct fsal_export *exp_hdl,
			fsal_fsinfo_options_t option)
{
	struct mdcache_fsal_export *exp =
		container_of(exp_hdl, struct mdcache_fsal_export, export);

	op_ctx->fsal_export = exp->sub_export;
	bool result =
		exp->sub_export->exp_ops.fs_supports(exp->sub_export, option);

	op_ctx->fsal_export = &exp->export;

	return result;
}

static uint64_t fs_maxfilesize(struct fsal_export *exp_hdl)
{
	struct mdcache_fsal_export *exp =
		container_of(exp_hdl, struct mdcache_fsal_export, export);

	op_ctx->fsal_export = exp->sub_export;
	uint64_t result =
		exp->sub_export->exp_ops.fs_maxfilesize(exp->sub_export);
	op_ctx->fsal_export = &exp->export;

	return result;
}

static uint32_t fs_maxread(struct fsal_export *exp_hdl)
{
	struct mdcache_fsal_export *exp =
		container_of(exp_hdl, struct mdcache_fsal_export, export);

	op_ctx->fsal_export = exp->sub_export;
	uint32_t result = exp->sub_export->exp_ops.fs_maxread(exp->sub_export);

	op_ctx->fsal_export = &exp->export;

	return result;
}

static uint32_t fs_maxwrite(struct fsal_export *exp_hdl)
{
	struct mdcache_fsal_export *exp =
		container_of(exp_hdl, struct mdcache_fsal_export, export);

	op_ctx->fsal_export = exp->sub_export;
	uint32_t result = exp->sub_export->exp_ops.fs_maxwrite(exp->sub_export);

	op_ctx->fsal_export = &exp->export;

	return result;
}

static uint32_t fs_maxlink(struct fsal_export *exp_hdl)
{
	struct mdcache_fsal_export *exp =
		container_of(exp_hdl, struct mdcache_fsal_export, export);

	op_ctx->fsal_export = exp->sub_export;
	uint32_t result = exp->sub_export->exp_ops.fs_maxlink(exp->sub_export);

	op_ctx->fsal_export = &exp->export;

	return result;
}

static uint32_t fs_maxnamelen(struct fsal_export *exp_hdl)
{
	struct mdcache_fsal_export *exp =
		container_of(exp_hdl, struct mdcache_fsal_export, export);

	op_ctx->fsal_export = exp->sub_export;
	uint32_t result =
		exp->sub_export->exp_ops.fs_maxnamelen(exp->sub_export);
	op_ctx->fsal_export = &exp->export;

	return result;
}

static uint32_t fs_maxpathlen(struct fsal_export *exp_hdl)
{
	struct mdcache_fsal_export *exp =
		container_of(exp_hdl, struct mdcache_fsal_export, export);

	op_ctx->fsal_export = exp->sub_export;
	uint32_t result =
		exp->sub_export->exp_ops.fs_maxpathlen(exp->sub_export);
	op_ctx->fsal_export = &exp->export;

	return result;
}

static struct timespec fs_lease_time(struct fsal_export *exp_hdl)
{
	struct mdcache_fsal_export *exp =
		container_of(exp_hdl, struct mdcache_fsal_export, export);

	op_ctx->fsal_export = exp->sub_export;
	struct timespec result = exp->sub_export->exp_ops.fs_lease_time(
		exp->sub_export);
	op_ctx->fsal_export = &exp->export;

	return result;
}

static fsal_aclsupp_t fs_acl_support(struct fsal_export *exp_hdl)
{
	struct mdcache_fsal_export *exp =
		container_of(exp_hdl, struct mdcache_fsal_export, export);

	op_ctx->fsal_export = exp->sub_export;
	fsal_aclsupp_t result = exp->sub_export->exp_ops.fs_acl_support(
		exp->sub_export);
	op_ctx->fsal_export = &exp->export;

	return result;
}

static attrmask_t fs_supported_attrs(struct fsal_export *exp_hdl)
{
	struct mdcache_fsal_export *exp =
		container_of(exp_hdl, struct mdcache_fsal_export, export);

	op_ctx->fsal_export = exp->sub_export;
	attrmask_t result =
		exp->sub_export->exp_ops.fs_supported_attrs(
		exp->sub_export);
	op_ctx->fsal_export = &exp->export;

	return result;
}

static uint32_t fs_umask(struct fsal_export *exp_hdl)
{
	struct mdcache_fsal_export *exp =
		container_of(exp_hdl, struct mdcache_fsal_export, export);

	op_ctx->fsal_export = exp->sub_export;
	uint32_t result = exp->sub_export->exp_ops.fs_umask(exp->sub_export);

	op_ctx->fsal_export = &exp->export;

	return result;
}

static uint32_t fs_xattr_access_rights(struct fsal_export *exp_hdl)
{
	struct mdcache_fsal_export *exp =
		container_of(exp_hdl, struct mdcache_fsal_export, export);

	op_ctx->fsal_export = exp->sub_export;
	uint32_t result =
		exp->sub_export->exp_ops.fs_xattr_access_rights(exp_hdl);
	op_ctx->fsal_export = &exp->export;

	return result;
}

/* get_quota
 * return quotas for this export.
 * path could cross a lower mount boundary which could
 * mask lower mount values with those of the export root
 * if this is a real issue, we can scan each time with setmntent()
 * better yet, compare st_dev of the file with st_dev of root_fd.
 * on linux, can map st_dev -> /proc/partitions name -> /dev/<name>
 */

static fsal_status_t get_quota(struct fsal_export *exp_hdl,
			       const char *filepath, int quota_type,
			       fsal_quota_t *pquota)
{
	struct mdcache_fsal_export *exp =
		container_of(exp_hdl, struct mdcache_fsal_export, export);

	op_ctx->fsal_export = exp->sub_export;
	fsal_status_t result =
		exp->sub_export->exp_ops.get_quota(exp->sub_export, filepath,
						   quota_type, pquota);
	op_ctx->fsal_export = &exp->export;

	return result;
}

/* set_quota
 * same lower mount restriction applies
 */

static fsal_status_t set_quota(struct fsal_export *exp_hdl,
			       const char *filepath, int quota_type,
			       fsal_quota_t *pquota, fsal_quota_t *presquota)
{
	struct mdcache_fsal_export *exp =
		container_of(exp_hdl, struct mdcache_fsal_export, export);

	op_ctx->fsal_export = exp->sub_export;
	fsal_status_t result =
		exp->sub_export->exp_ops.set_quota(exp->sub_export, filepath,
						   quota_type, pquota,
						   presquota);
	op_ctx->fsal_export = &exp->export;

	return result;
}

/* extract a file handle from a buffer.
 * do verification checks and flag any and all suspicious bits.
 * Return an updated fh_desc into whatever was passed.  The most
 * common behavior, done here is to just reset the length.  There
 * is the option to also adjust the start pointer.
 */

static fsal_status_t extract_handle(struct fsal_export *exp_hdl,
				    fsal_digesttype_t in_type,
				    struct gsh_buffdesc *fh_desc,
				    int flags)
{
	struct mdcache_fsal_export *exp =
		container_of(exp_hdl, struct mdcache_fsal_export, export);

	op_ctx->fsal_export = exp->sub_export;
	fsal_status_t result =
		exp->sub_export->exp_ops.extract_handle(exp->sub_export,
							in_type, fh_desc,
							flags);
	op_ctx->fsal_export = &exp->export;

	return result;
}

/* mdcache_export_ops_init
 * overwrite vector entries with the methods that we support
 */

void mdcache_export_ops_init(struct export_ops *ops)
{
	ops->get_name = mdcache_get_name;
	ops->release = mdcache_release;
	ops->lookup_path = mdcache_lookup_path;
	ops->extract_handle = extract_handle;
	ops->create_handle = mdcache_create_handle;
	ops->get_fs_dynamic_info = get_dynamic_info;
	ops->fs_supports = fs_supports;
	ops->fs_maxfilesize = fs_maxfilesize;
	ops->fs_maxread = fs_maxread;
	ops->fs_maxwrite = fs_maxwrite;
	ops->fs_maxlink = fs_maxlink;
	ops->fs_maxnamelen = fs_maxnamelen;
	ops->fs_maxpathlen = fs_maxpathlen;
	ops->fs_lease_time = fs_lease_time;
	ops->fs_acl_support = fs_acl_support;
	ops->fs_supported_attrs = fs_supported_attrs;
	ops->fs_umask = fs_umask;
	ops->fs_xattr_access_rights = fs_xattr_access_rights;
	ops->get_quota = get_quota;
	ops->set_quota = set_quota;
}

#if 0
struct mdcache_fsal_args {
	struct subfsal_args subfsal;
};

static struct config_item sub_fsal_params[] = {
	CONF_ITEM_STR("name", 1, 10, NULL,
		      subfsal_args, name),
	CONFIG_EOL
};

static struct config_item export_params[] = {
	CONF_ITEM_NOOP("name"),
	CONF_RELAX_BLOCK("FSAL", sub_fsal_params,
			 noop_conf_init, subfsal_commit,
			 mdcache_fsal_args, subfsal),
	CONFIG_EOL
};

static struct config_block export_param = {
	.dbus_interface_name = "org.ganesha.nfsd.config.fsal.mdcache-export%d",
	.blk_desc.name = "FSAL",
	.blk_desc.type = CONFIG_BLOCK,
	.blk_desc.u.blk.init = noop_conf_init,
	.blk_desc.u.blk.params = export_params,
	.blk_desc.u.blk.commit = noop_conf_commit
};
#endif

/**
 * @brief Create an export for MDCACHE
 *
 * Create the stacked export for MDCACHE to allow metadata caching on another
 * export.  Unlike other Stackable FSALs, this one is created @b after the FSAL
 * underneath.  It assumes the sub-FSAL's export is already created and
 * available via the @e fsal_export member of @link op_ctx @endlink, the same
 * way that this export is returned.
 *
 * There is currently no config; FSALs that want caching should call @ref
 * mdcache_export_init
 *
 * @param[in] fsal_hdl		FSAL module handle
 * @param[in] parse_node	Config node for export
 * @param[out] err_type		Parse errors
 * @param[in] up_ops		Upcall ops for export
 * @return FSAL status
 */

fsal_status_t mdcache_create_export(struct fsal_module *fsal_hdl,
				   void *parse_node,
				   struct config_error_type *err_type,
				   const struct fsal_up_vector *up_ops)
{
	struct mdcache_fsal_export *myself;
	int retval;

#if 0
	fsal_status_t status = {0, 0};
	struct fsal_module *fsal_stack;
	struct mdcache_fsal_args mdcache_fsal;
	/* process our FSAL block to get the name of the fsal
	 * underneath us.
	 */
	retval = load_config_from_node(parse_node,
				       &export_param,
				       &mdcache_fsal,
				       true,
				       err_type);
	if (retval != 0)
		return fsalstat(ERR_FSAL_INVAL, 0);
	fsal_stack = lookup_fsal(mdcache_fsal.subfsal.name);
	if (fsal_stack == NULL) {
		LogMajor(COMPONENT_FSAL, "failed to lookup for FSAL %s",
			 mdcache_fsal.subfsal.name);
		return fsalstat(ERR_FSAL_INVAL, EINVAL);
	}

	status = fsal_stack->m_ops.create_export(fsal_stack,
						 mdcache_fsal.subfsal.fsal_node,
						 err_type,
						 up_ops);
	fsal_put(fsal_stack);
	if (FSAL_IS_ERROR(status)) {
		LogMajor(COMPONENT_FSAL,
			 "Failed to call create_export on underlying FSAL %s",
			 mdcache_fsal.subfsal.name);
		gsh_free(myself);
		return status;
	}

#endif
	myself = gsh_calloc(1, sizeof(struct mdcache_fsal_export));
	if (myself == NULL) {
		LogMajor(COMPONENT_FSAL,
			 "Could not allocate memory for export %s",
			 op_ctx->export->fullpath);
		return fsalstat(ERR_FSAL_NOMEM, ENOMEM);
	}

	myself->sub_export = op_ctx->fsal_export;
	fsal_get(myself->sub_export->fsal);

	retval = fsal_export_init(&myself->export);
	if (retval) {
		gsh_free(myself);
		return fsalstat(posix2fsal_error(retval), retval);
	}
	mdcache_export_ops_init(&myself->export.exp_ops);
	myself->export.up_ops = up_ops;
	myself->export.fsal = fsal_hdl;

	op_ctx->fsal_export = &myself->export;
	op_ctx->fsal_module = fsal_hdl;
	return fsalstat(ERR_FSAL_NO_ERROR, 0);
}
