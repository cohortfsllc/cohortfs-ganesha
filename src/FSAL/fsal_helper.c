/*
 * vim:noexpandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright (C) CohrotFS Inc., 2015
 * Author: Daniel Gryniewicz <dang@cohortfs.com>
 *
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 * -------------
 */

/**
 * @addtogroup FSAL
 * @{
 */

/**
 * @file fsal_helper.c
 * @author Daniel Gryniewicz <dang@cohortfs.com>
 * @brief FSAL helper for clients
 */

#include "config.h"

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "log.h"
#include "fsal.h"
#include "nfs_convert.h"
#include "nfs_exports.h"

#define fsal_err_txt(s) msg_fsal_err(s.major)

static bool fsal_is_open(struct fsal_obj_handle *obj)
{
	if ((obj == NULL) || (obj->type != REGULAR_FILE))
		return false;
	return (obj->obj_ops.status(obj) != FSAL_O_CLOSED);
}

static bool fsal_not_in_group_list(gid_t gid)
{
	const struct user_cred *creds = op_ctx->creds;
	int i;
	if (creds->caller_gid == gid) {

		LogDebug(COMPONENT_FSAL,
			    "User %u is has active group %u", creds->caller_uid,
			    gid);
		return false;
	}
	for (i = 0; i < creds->caller_glen; i++) {
		if (creds->caller_garray[i] == gid) {
			LogDebug(COMPONENT_FSAL,
				    "User %u is member of group %u",
				    creds->caller_uid, gid);
			return false;
		}
	}

	LogDebug(COMPONENT_FSAL,
		    "User %u IS NOT member of group %u", creds->caller_uid,
		    gid);
	return true;
}

/**
 * @brief Checks permissions on an entry for setattrs
 *
 * This function checks if the supplied credentials are sufficient to perform
 * the required setattrs.
 *
 * @param[in] obj     The file to be checked
 * @param[in] attr    Attributes to set/result of set
 *
 * @return FSAL status
 */
static fsal_status_t fsal_check_setattr_perms(struct fsal_obj_handle *obj,
					      struct attrlist *attr)
{
	fsal_status_t status = {0, 0};
	fsal_accessflags_t access_check = 0;
	bool not_owner;
	char *note = "";
	const struct user_cred *creds = op_ctx->creds;

	/* Shortcut, if current user is root, then we can just bail out with
	 * success. */
	if (creds->caller_uid == 0) {
		note = " (Ok for root user)";
		goto out;
	}

	not_owner = (creds->caller_uid != obj->attributes.owner);

	/* Only ownership change need to be checked for owner */
	if (FSAL_TEST_MASK(attr->mask, ATTR_OWNER)) {
		/* non-root is only allowed to "take ownership of file" */
		if (attr->owner != creds->caller_uid) {
			status = fsalstat(ERR_FSAL_PERM, 0);
			note = " (new OWNER was not user)";
			goto out;
		}

		/* Owner of file will always be able to "change" the owner to
		 * himself. */
		if (not_owner) {
			access_check |=
			    FSAL_ACE4_MASK_SET(FSAL_ACE_PERM_WRITE_OWNER);
			LogDebug(COMPONENT_FSAL,
				    "Change OWNER requires FSAL_ACE_PERM_WRITE_OWNER");
		}
	}
	if (FSAL_TEST_MASK(attr->mask, ATTR_GROUP)) {
		/* non-root is only allowed to change group_owner to a group
		 * user is a member of. */
		int not_in_group = fsal_not_in_group_list(attr->group);

		if (not_in_group) {
			status = fsalstat(ERR_FSAL_PERM, 0);
			note = " (user is not member of new GROUP)";
			goto out;
		}
		/* Owner is always allowed to change the group_owner of a file
		 * to a group they are a member of.
		 */
		if (not_owner) {
			access_check |=
			    FSAL_ACE4_MASK_SET(FSAL_ACE_PERM_WRITE_OWNER);
			LogDebug(COMPONENT_FSAL,
				    "Change GROUP requires FSAL_ACE_PERM_WRITE_OWNER");
		}
	}

	/* Any attribute after this is always changeable by the owner.
	 * And the above attributes have already been validated as a valid
	 * change for the file owner to make. Note that the owner may be
	 * setting ATTR_OWNER but at this point it MUST be to himself, and
	 * thus is no-op and does not need FSAL_ACE_PERM_WRITE_OWNER.
	 */
	if (!not_owner) {
		note = " (Ok for owner)";
		goto out;
	}

	if (FSAL_TEST_MASK(attr->mask, ATTR_MODE)
	    || FSAL_TEST_MASK(attr->mask, ATTR_ACL)) {
		/* Changing mode or ACL requires ACE4_WRITE_ACL */
		access_check |= FSAL_ACE4_MASK_SET(FSAL_ACE_PERM_WRITE_ACL);
		LogDebug(COMPONENT_FSAL,
			    "Change MODE or ACL requires FSAL_ACE_PERM_WRITE_ACL");
	}

	if (FSAL_TEST_MASK(attr->mask, ATTR_SIZE)) {
		/* Changing size requires owner or write permission */
	  /** @todo: does FSAL_ACE_PERM_APPEND_DATA allow enlarging the file? */
		access_check |= FSAL_ACE4_MASK_SET(FSAL_ACE_PERM_WRITE_DATA);
		LogDebug(COMPONENT_FSAL,
			    "Change SIZE requires FSAL_ACE_PERM_WRITE_DATA");
	}

	/* Check if just setting atime and mtime to "now" */
	if ((FSAL_TEST_MASK(attr->mask, ATTR_MTIME_SERVER)
	     || FSAL_TEST_MASK(attr->mask, ATTR_ATIME_SERVER))
	    && !FSAL_TEST_MASK(attr->mask, ATTR_MTIME)
	    && !FSAL_TEST_MASK(attr->mask, ATTR_ATIME)) {
		/* If either atime and/or mtime are set to "now" then need only
		 * have write permission.
		 *
		 * Technically, client should not send atime updates, but if
		 * they really do, we'll let them to make the perm check a bit
		 * simpler. */
		access_check |= FSAL_ACE4_MASK_SET(FSAL_ACE_PERM_WRITE_DATA);
		LogDebug(COMPONENT_FSAL,
			    "Change ATIME and MTIME to NOW requires FSAL_ACE_PERM_WRITE_DATA");
	} else if (FSAL_TEST_MASK(attr->mask, ATTR_MTIME_SERVER)
		   || FSAL_TEST_MASK(attr->mask, ATTR_ATIME_SERVER)
		   || FSAL_TEST_MASK(attr->mask, ATTR_MTIME)
		   || FSAL_TEST_MASK(attr->mask, ATTR_ATIME)) {
		/* Any other changes to atime or mtime require owner, root, or
		 * ACES4_WRITE_ATTRIBUTES.
		 *
		 * NOTE: we explicity do NOT check for update of atime only to
		 * "now". Section 10.6 of both RFC 3530 and RFC 5661 document
		 * the reasons clients should not do atime updates.
		 */
		access_check |= FSAL_ACE4_MASK_SET(FSAL_ACE_PERM_WRITE_ATTR);
		LogDebug(COMPONENT_FSAL,
			    "Change ATIME and/or MTIME requires FSAL_ACE_PERM_WRITE_ATTR");
	}

	if (isDebug(COMPONENT_FSAL) || isDebug(COMPONENT_NFS_V4_ACL)) {
		char *need_write_owner = "";
		char *need_write_acl = "";
		char *need_write_data = "";
		char *need_write_attr = "";

		if (access_check & FSAL_ACE_PERM_WRITE_OWNER)
			need_write_owner = " WRITE_OWNER";

		if (access_check & FSAL_ACE_PERM_WRITE_ACL)
			need_write_acl = " WRITE_ACL";

		if (access_check & FSAL_ACE_PERM_WRITE_DATA)
			need_write_data = " WRITE_DATA";

		if (access_check & FSAL_ACE_PERM_WRITE_ATTR)
			need_write_attr = " WRITE_ATTR";

		LogDebug(COMPONENT_FSAL,
			    "Requires %s%s%s%s", need_write_owner,
			    need_write_acl, need_write_data, need_write_attr);
	}

	if (obj->attributes.acl) {
		status = obj->obj_ops.test_access(obj, access_check, NULL,
						  NULL);
		note = " (checked ACL)";
		goto out;
	}

	if (access_check != FSAL_ACE4_MASK_SET(FSAL_ACE_PERM_WRITE_DATA)) {
		/* Without an ACL, this user is not allowed some operation */
		status = fsalstat(ERR_FSAL_PERM, 0);
		note = " (no ACL to check)";
		goto out;
	}

	status = obj->obj_ops.test_access(obj, FSAL_W_OK, NULL, NULL);

	note = " (checked mode)";

 out:

	LogDebug(COMPONENT_FSAL,
		    "Access check returned %s%s", fsal_err_txt(status),
		    note);

	return status;
}

/**
 * @brief Checks permissions on an entry for setattrs
 *
 * This function checks if the supplied credentials are sufficient to perform
 * the required setattrs.
 *
 * @param[in] obj     The file to be checked
 * @param[in] attr    Attributes to set/result of set
 *
 * @return FSAL status
 */
fsal_status_t fsal_refresh_attrs(struct fsal_obj_handle *obj)
{
	fsal_status_t status = { ERR_FSAL_NO_ERROR, 0 };

	if (obj->attributes.acl) {
		fsal_acl_status_t acl_status = 0;

		nfs4_acl_release_entry(obj->attributes.acl, &acl_status);
		if (acl_status != NFS_V4_ACL_SUCCESS) {
			LogEvent(COMPONENT_FSAL,
				 "Failed to release old acl, status=%d",
				 acl_status);
		}
		obj->attributes.acl = NULL;
	}

	status = obj->obj_ops.getattrs(obj);
	if (FSAL_IS_ERROR(status)) {
		LogDebug(COMPONENT_FSAL, "Failed on obj %p %s", obj,
			 fsal_err_txt(status));
		return status;
	}

	return status;
}

/**
 * @brief Set attributes on a file
 *
 * @param[in] obj	File to set attributes on
 * @param[in] attr	Attributes to set
 * @return FSAL status
 */
fsal_status_t fsal_setattr(struct fsal_obj_handle *obj, struct attrlist *attr)
{
	fsal_status_t status = { 0, 0 };
	uint64_t before;
	const struct user_cred *creds = op_ctx->creds;
	fsal_acl_t *saved_acl = NULL;
	fsal_acl_status_t acl_status = 0;

	if ((attr->mask & (ATTR_SIZE | ATTR4_SPACE_RESERVED))
	     && (obj->type != REGULAR_FILE)) {
		LogWarn(COMPONENT_FSAL,
			"Attempt to truncate non-regular file: type=%d",
			obj->type);
		return fsalstat(ERR_FSAL_BADTYPE, 0);
	}

	/* Is it allowed to change times ? */
	if (!op_ctx->fsal_export->exp_ops.fs_supports(op_ctx->fsal_export,
						      fso_cansettime) &&
	    (FSAL_TEST_MASK
	     (attr->mask,
	      (ATTR_ATIME | ATTR_CREATION | ATTR_CTIME | ATTR_MTIME))))
		return fsalstat(ERR_FSAL_INVAL, 0);

	/* Refresh attributes for perm checks */
	status = fsal_refresh_attrs(obj);
	if (FSAL_IS_ERROR(status)) {
		LogWarn(COMPONENT_FSAL, "Failed to refresh attributes");
		return status;
	}

	/* Do permission checks */
	status = fsal_check_setattr_perms(obj, attr);
	if (FSAL_IS_ERROR(status))
		return status;

	/* Test for the following condition from chown(2):
	 *
	 *     When the owner or group of an executable file are changed by an
	 *     unprivileged user the S_ISUID and S_ISGID mode bits are cleared.
	 *     POSIX does not specify whether this also should happen when
	 *     root does the chown(); the Linux behavior depends on the kernel
	 *     version.  In case of a non-group-executable file (i.e., one for
	 *     which the S_IXGRP bit is not set) the S_ISGID bit indicates
	 *     mandatory locking, and is not cleared by a chown().
	 *
	 */
	if (creds->caller_uid != 0 &&
	    (FSAL_TEST_MASK(attr->mask, ATTR_OWNER) ||
	     FSAL_TEST_MASK(attr->mask, ATTR_GROUP)) &&
	    ((obj->attributes.mode & (S_IXOTH | S_IXUSR | S_IXGRP)) != 0) &&
	    ((obj->attributes.mode & (S_ISUID | S_ISGID)) != 0)) {
		/* Non-priviledged user changing ownership on an executable
		 * file with S_ISUID or S_ISGID bit set, need to be cleared.
		 */
		if (!FSAL_TEST_MASK(attr->mask, ATTR_MODE)) {
			/* Mode wasn't being set, so set it now, start with
			 * the current attributes.
			 */
			attr->mode = obj->attributes.mode;
			FSAL_SET_MASK(attr->mask, ATTR_MODE);
		}

		/* Don't clear S_ISGID if the file isn't group executable.
		 * In that case, S_ISGID indicates mandatory locking and
		 * is not cleared by chown.
		 */
		if ((obj->attributes.mode & S_IXGRP) != 0)
			attr->mode &= ~S_ISGID;

		/* Clear S_ISUID. */
		attr->mode &= ~S_ISUID;
	}

	/* Test for the following condition from chmod(2):
	 *
	 *     If the calling process is not privileged (Linux: does not have
	 *     the CAP_FSETID capability), and the group of the file does not
	 *     match the effective group ID of the process or one of its
	 *     supplementary group IDs, the S_ISGID bit will be turned off,
	 *     but this will not cause an error to be returned.
	 *
	 * We test the actual mode being set before testing for group
	 * membership since that is a bit more expensive.
	 */
	if (creds->caller_uid != 0 &&
	    FSAL_TEST_MASK(attr->mask, ATTR_MODE) &&
	    (attr->mode & S_ISGID) != 0 &&
	    fsal_not_in_group_list(obj->attributes.group)) {
		/* Clear S_ISGID */
		attr->mode &= ~S_ISGID;
	}

	saved_acl = obj->attributes.acl;
	before = obj->attributes.change;
	status = obj->obj_ops.setattrs(obj, attr);
	if (FSAL_IS_ERROR(status)) {
		if (status.major == ERR_FSAL_STALE) {
			LogEvent(COMPONENT_FSAL,
				 "FSAL returned STALE from setattrs");
		}
		return status;
	}
	status = obj->obj_ops.getattrs(obj);
	*attr = obj->attributes;
	if (FSAL_IS_ERROR(status)) {
		if (status.major == ERR_FSAL_STALE) {
			LogEvent(COMPONENT_FSAL,
				 "FSAL returned STALE from getattrs");
		}
		return status;
	}
	if (before == obj->attributes.change)
		obj->attributes.change++;
	/* Decrement refcount on saved ACL */
	nfs4_acl_release_entry(saved_acl, &acl_status);
	if (acl_status != NFS_V4_ACL_SUCCESS)
		LogCrit(COMPONENT_FSAL,
			"Failed to release old acl, status=%d", acl_status);

	/* Copy the complete set of new attributes out. */
	*attr = obj->attributes;

	return fsalstat(ERR_FSAL_NO_ERROR, 0);
}

/**
 *
 * @brief Checks the permissions on an object
 *
 * This function returns success if the supplied credentials possess
 * permission required to meet the specified access.
 *
 * @param[in]  obj         The object to be checked
 * @param[in]  access_type The kind of access to be checked
 *
 * @return FSAL status
 *
 */
fsal_status_t fsal_access(struct fsal_obj_handle *obj,
			  fsal_accessflags_t access_type,
			  fsal_accessflags_t *allowed,
			  fsal_accessflags_t *denied)
{
	fsal_status_t status = { 0, 0 };

	status = fsal_refresh_attrs(obj);
	if (FSAL_IS_ERROR(status)) {
		LogWarn(COMPONENT_FSAL, "Failed to refresh attributes");
		return status;
	}

	return obj->obj_ops.test_access(obj, access_type, allowed, denied);
}

/**
 * @brief Gets the cached attributes for a file
 *
 * Attributes should have been refreshed before this call (usually by calling
 * fsal_access() )
 *
 * @param[in]     obj     File to be managed.
 * @param[in,out] opaque  Opaque pointer passed to callback
 * @param[in]     cb      User supplied callback
 *
 * @return cache inode status (for historical reasons)
 *
 */
cache_inode_status_t fsal_getattr(struct fsal_obj_handle *obj,
				  void *opaque,
				  fsal_getattr_cb_t cb,
				  enum cb_state cb_state)
{
	cache_inode_status_t status;
	uint64_t mounted_on_fileid;

	mounted_on_fileid = obj->attributes.fileid;

	status = cb(opaque,
		    obj,
		    &obj->attributes,
		    mounted_on_fileid,
		    0,
		    cb_state);

	/* XXX dang - ignoring junctions for now */

	return status;
}

/**
 *
 * @brief Links a new name to a file
 *
 * This function hard links a new name to an existing file.
 *
 * @param[in]  obj      The file to which to add the new name.  Must
 *                      not be a directory.
 * @param[in]  dest_dir The directory in which to create the new name
 * @param[in]  name     The new name to add to the file
 *
 * @return FSAL status
 *                                  in destination.
 */
fsal_status_t fsal_link(struct fsal_obj_handle *obj,
			struct fsal_obj_handle *dest_dir,
			const char *name)
{
	fsal_status_t status = { 0, 0 };

	/* The file to be hardlinked can't be a DIRECTORY */
	if (obj->type == DIRECTORY) {
		return fsalstat(ERR_FSAL_BADTYPE, 0);
	}

	/* Is the destination a directory? */
	if (dest_dir->type != DIRECTORY) {
		return fsalstat(ERR_FSAL_NOTDIR, 0);
	}

	if (!op_ctx->fsal_export->exp_ops.fs_supports(
			op_ctx->fsal_export,
			fso_link_supports_permission_checks)) {
		status = fsal_access(dest_dir,
			FSAL_MODE_MASK_SET(FSAL_W_OK) |
			FSAL_MODE_MASK_SET(FSAL_X_OK) |
			FSAL_ACE4_MASK_SET(FSAL_ACE_PERM_EXECUTE) |
			FSAL_ACE4_MASK_SET(FSAL_ACE_PERM_ADD_FILE),
			NULL, NULL);

		if (FSAL_IS_ERROR(status))
			return status;
	}

	/* Rather than performing a lookup first, just try to make the
	   link and return the FSAL's error if it fails. */
	status = obj->obj_ops.link(obj, dest_dir, name);
	if (FSAL_IS_ERROR(status))
		return status;

	return fsal_refresh_attrs(dest_dir);
}

/**
 * @brief Look up a name in a directory
 *
 * @param[in]  parent  Handle for the parent directory to be managed.
 * @param[in]  name    Name of the file that we are looking up.
 * @param[out] obj     Found file
 *
 * @return FSAL status
 */

fsal_status_t fsal_lookup(struct fsal_obj_handle *parent,
			  const char *name,
			  struct fsal_obj_handle **obj)
{
	fsal_status_t fsal_status = { 0, 0 };
	fsal_accessflags_t access_mask =
	    (FSAL_MODE_MASK_SET(FSAL_X_OK) |
	     FSAL_ACE4_MASK_SET(FSAL_ACE_PERM_EXECUTE));

	*obj = NULL;

	fsal_status = fsal_access(parent, access_mask, NULL, NULL);
	if (FSAL_IS_ERROR(fsal_status))
		return fsal_status;

	if (strcmp(name, ".") == 0) {
		*obj = parent;
		return fsalstat(ERR_FSAL_NO_ERROR, 0);
	} else if (strcmp(name, "..") == 0)
		return fsal_lookupp(parent, obj);


	return parent->obj_ops.lookup(parent, name, obj);
}

/**
 * @brief Look up a directory's parent
 *
 * @param[in]  obj     File whose parent is to be obtained.
 * @param[out] parent  Parent directory
 *
 * @return FSAL status
 */
fsal_status_t fsal_lookupp(struct fsal_obj_handle *obj,
			   struct fsal_obj_handle **parent)
{
	*parent = NULL;

	/* Never even think of calling FSAL_lookup on root/.. */

	if (obj->type == DIRECTORY) {
		fsal_status_t status = {0, 0};
		struct fsal_obj_handle *root_obj = NULL;

		status = op_ctx->export->fsal_export->exp_ops.lookup_path(
					op_ctx->export->fsal_export,
					op_ctx->export->fullpath,
					&root_obj);
		if (FSAL_IS_ERROR(status))
			return status;

		if (obj == root_obj) {
			/* This entry is the root of the current export, so if
			 * we get this far, return itself. Note that NFS v4
			 * LOOKUPP will not come here, it catches the root entry
			 * earlier.
			 */
			*parent = obj;
			return fsalstat(ERR_FSAL_NO_ERROR, 0);
		}
	}

	return obj->obj_ops.lookup(obj, "..", parent);
}

/**
 * @brief Creates an object in a directory
 *
 * This function creates an entry in the FSAL.
 *
 * @param[in]  parent     Parent directory
 * @param[in]  name       Name of the object to create
 * @param[in]  type       Type of the object to create
 * @param[in]  mode       Mode to be used at file creation
 * @param[in]  create_arg Additional argument for object creation
 * @param[out] obj        Created file
 *
 * @return FSAL status
 */

fsal_status_t fsal_create(struct fsal_obj_handle *parent,
			  const char *name,
			  object_file_type_t type, uint32_t mode,
			  cache_inode_create_arg_t *create_arg,
			  struct fsal_obj_handle **obj)
{
	fsal_status_t status = { 0, 0 };
	struct attrlist object_attributes;
	cache_inode_create_arg_t zero_create_arg;

	memset(&zero_create_arg, 0, sizeof(zero_create_arg));
	memset(&object_attributes, 0, sizeof(object_attributes));

	if (create_arg == NULL)
		create_arg = &zero_create_arg;

	if ((type != REGULAR_FILE) && (type != DIRECTORY)
	    && (type != SYMBOLIC_LINK) && (type != SOCKET_FILE)
	    && (type != FIFO_FILE) && (type != CHARACTER_FILE)
	    && (type != BLOCK_FILE)) {
		status = fsalstat(ERR_FSAL_BADTYPE, 0);

		LogFullDebug(COMPONENT_FSAL,
			     "create failed because of bad type");
		*obj = NULL;
		goto out;
	}

	/* Permission checking will be done by the FSAL operation. */

	/* Try to create it first */

	/* we pass in attributes to the create.  We will get them back below */
	FSAL_SET_MASK(object_attributes.mask,
		      ATTR_MODE | ATTR_OWNER | ATTR_GROUP);
	object_attributes.owner = op_ctx->creds->caller_uid;
	object_attributes.group = op_ctx->creds->caller_gid; /* be more
							       * selective? */
	object_attributes.mode = mode;

	switch (type) {
	case REGULAR_FILE:
		status = parent->obj_ops.create(parent, name,
						&object_attributes, obj);
		break;

	case DIRECTORY:
		status = parent->obj_ops.mkdir(parent, name,
					       &object_attributes, obj);
		break;

	case SYMBOLIC_LINK:
		status = parent->obj_ops.symlink(parent, name,
						 create_arg->link_content,
						 &object_attributes, obj);
		break;

	case SOCKET_FILE:
	case FIFO_FILE:
		status = parent->obj_ops.mknode(parent, name, type,
						NULL, /* dev_t !needed */
						&object_attributes, obj);
		break;

	case BLOCK_FILE:
	case CHARACTER_FILE:
		status = parent->obj_ops.mknode(parent, name, type,
						&create_arg->dev_spec,
						&object_attributes, obj);
		break;

	case NO_FILE_TYPE:
	case EXTENDED_ATTR:
		/* we should never go there */
		status = fsalstat(ERR_FSAL_BADTYPE, 0);
		*obj = NULL;
		LogFullDebug(COMPONENT_FSAL,
			     "create failed because inconsistent entry");
		goto out;
		break;
	}

	/* Refresh the parent's attributes */
	fsal_refresh_attrs(parent);

	/* Check for the result */
	if (FSAL_IS_ERROR(status)) {
		if (status.major == ERR_FSAL_STALE) {
			LogEvent(COMPONENT_FSAL,
				 "FSAL returned STALE on create type %d", type);
		} else if (status.major == ERR_FSAL_EXIST) {
			/* Already exists. Check if type if correct */
			status = fsal_lookup(parent, name, obj);
			if (*obj != NULL) {
				status = fsalstat(ERR_FSAL_EXIST, 0);
				LogFullDebug(COMPONENT_FSAL,
					     "create failed because it already "
					     "exists");
				if ((*obj)->type != type) {
					/* Incompatible types, returns NULL */
					*obj = NULL;
				}
				goto out;
			}
		}
		*obj = NULL;
		goto out;
	}

 out:
	LogFullDebug(COMPONENT_FSAL,
		     "Returning obj=%p status=%s for %s FSAL=%s", *obj,
		     fsal_err_txt(status), name, parent->fsal->name);

	return status;
}

/**
 * @brief Return true if create verifier matches
 *
 * This function returns true if the create verifier matches
 *
 * @param[in] obj     File to be managed.
 * @param[in] verf_hi High long of verifier
 * @param[in] verf_lo Low long of verifier
 *
 * @return true if verified, false otherwise
 *
 */
bool fsal_create_verify(struct fsal_obj_handle *obj, uint32_t verf_hi,
			uint32_t verf_lo)
{
	/* True if the verifier matches */
	bool verified = false;

	fsal_refresh_attrs(obj);
	if (FSAL_TEST_MASK(obj->attributes.mask, ATTR_ATIME)
	    && FSAL_TEST_MASK(obj->attributes.mask, ATTR_MTIME)
	    && obj->attributes.atime.tv_sec == verf_hi
	    && obj->attributes.mtime.tv_sec == verf_lo)
		verified = true;

	return verified;
}

/**
 * @brief Read/Write
 *
 * @param[in]     obj          File to be read or written
 * @param[in]     io_direction Whether this is a read or a write
 * @param[in]     offset       Absolute file position for I/O
 * @param[in]     io_size      Amount of data to be read or written
 * @param[out]    bytes_moved  The length of data successfuly read or written
 * @param[in,out] buffer       Where in memory to read or write data
 * @param[out]    eof          Whether a READ encountered the end of file.  May
 *                             be NULL for writes.
 * @param[in]     sync         Whether the write is synchronous or not
 *
 * @return CACHE_INODE_SUCCESS or various errors
 */

fsal_status_t fsal_rdwr_plus(struct fsal_obj_handle *obj,
		      cache_inode_io_direction_t io_direction,
		      uint64_t offset, size_t io_size,
		      size_t *bytes_moved, void *buffer,
		      bool *eof,
		      bool *sync, struct io_info *info)
{
	/* Error return from FSAL calls */
	fsal_status_t fsal_status = { 0, 0 };
	/* Required open mode to successfully read or write */
	fsal_openflags_t openflags = FSAL_O_CLOSED;
	fsal_openflags_t loflags;
	/* TRUE if we opened a previously closed FD */
	bool opened = false;

	/* Set flags for a read or write, as appropriate */
	if (io_direction == CACHE_INODE_READ ||
	    io_direction == CACHE_INODE_READ_PLUS) {
		openflags = FSAL_O_READ;
	} else {
		struct export_perms *perms;

		/* Pretent that the caller requested sync (stable write)
		 * if the export has COMMIT option. Note that
		 * FSAL_O_SYNC is not always honored, so just setting
		 * FSAL_O_SYNC has no guaranty that this write will be
		 * a stable write.
		 */
		perms = &op_ctx->export->export_perms;
		if (perms->options & EXPORT_OPTION_COMMIT)
			*sync = true;
		openflags = FSAL_O_WRITE;
		if (*sync)
			openflags |= FSAL_O_SYNC;
	}

	assert(obj != NULL);

	/* IO is done only on REGULAR_FILEs */
	if (obj->type != REGULAR_FILE) {
		fsal_status = fsalstat(
		    obj->type ==
		    DIRECTORY ? ERR_FSAL_ISDIR :
		    ERR_FSAL_BADTYPE, 0);
		goto out;
	}

	loflags = obj->obj_ops.status(obj);
	while ((!fsal_is_open(obj))
	       || (loflags && loflags != FSAL_O_RDWR && loflags != openflags)) {
		loflags = obj->obj_ops.status(obj);
		if ((!fsal_is_open(obj))
		    || (loflags && loflags != FSAL_O_RDWR
			&& loflags != openflags)) {
			fsal_status = obj->obj_ops.open(obj, openflags);
			if (FSAL_IS_ERROR(fsal_status))
				goto out;
			opened = true;
		}
		loflags = obj->obj_ops.status(obj);
	}

	/* Call FSAL_read or FSAL_write */
	if (io_direction == CACHE_INODE_READ) {
		fsal_status = obj->obj_ops.read(obj, offset, io_size, buffer,
						bytes_moved, eof);
	} else if (io_direction == CACHE_INODE_READ_PLUS) {
		fsal_status = obj->obj_ops.read_plus(obj, offset, io_size,
						     buffer, bytes_moved, eof,
						     info);
	} else {
		bool fsal_sync = *sync;
		if (io_direction == CACHE_INODE_WRITE)
			fsal_status = obj->obj_ops.write(obj, offset, io_size,
							 buffer, bytes_moved,
							 &fsal_sync);
		else
			fsal_status = obj->obj_ops.write_plus(obj, offset,
							      io_size, buffer,
							      bytes_moved,
							      &fsal_sync, info);
		/* Alright, the unstable write is complete. Now if it was
		   supposed to be a stable write we can sync to the hard
		   drive. */

		if (*sync && !(obj->obj_ops.status(obj) & FSAL_O_SYNC)
		    && !fsal_sync && !FSAL_IS_ERROR(fsal_status)) {
			fsal_status = obj->obj_ops.commit(obj, offset, io_size);
		} else {
			*sync = fsal_sync;
		}
	}

	LogFullDebug(COMPONENT_FSAL,
		     "fsal_rdrw_plus: FSAL IO operation returned "
		     "%s, asked_size=%zu, effective_size=%zu",
		     fsal_err_txt(fsal_status), io_size, *bytes_moved);

	if (FSAL_IS_ERROR(fsal_status)) {
		if (fsal_status.major == ERR_FSAL_DELAY) {
			LogEvent(COMPONENT_FSAL,
				 "fsal_rdrw_plus: FSAL_write "
				 " returned EBUSY");
		} else {
			LogDebug(COMPONENT_FSAL,
				 "fsal_rdrw_plus: fsal_status = %s",
				 fsal_err_txt(fsal_status));
		}

		*bytes_moved = 0;

		if (fsal_status.major == ERR_FSAL_STALE) {
			goto out;
		}

		if ((fsal_status.major != ERR_FSAL_NOT_OPENED)
		    && (obj->obj_ops.status(obj) != FSAL_O_CLOSED)) {
			LogFullDebug(COMPONENT_FSAL,
				     "fsal_rdrw_plus: CLOSING file %p",
				     obj);

			fsal_status = obj->obj_ops.close(obj);
			if (FSAL_IS_ERROR(fsal_status)) {
				LogCrit(COMPONENT_FSAL,
					"Error closing file in fsal_rdrw_plus: %s.",
					fsal_err_txt(fsal_status));
			}
		}

		goto out;
	}

	LogFullDebug(COMPONENT_FSAL,
		     "fsal_rdrw_plus: inode/direct: io_size=%zu, "
		     "bytes_moved=%zu, offset=%" PRIu64, io_size, *bytes_moved,
		     offset);

	if (opened) {
		fsal_status = obj->obj_ops.close(obj);
		if (FSAL_IS_ERROR(fsal_status)) {
			LogEvent(COMPONENT_FSAL,
				 "fsal_rdrw_plus: close = %s",
				 fsal_err_txt(fsal_status));
			goto out;
		}
	}

	if (io_direction == CACHE_INODE_WRITE ||
	    io_direction == CACHE_INODE_WRITE_PLUS) {
		fsal_status = fsal_refresh_attrs(obj);
		if (FSAL_IS_ERROR(fsal_status))
			goto out;
	}

	fsal_status = fsalstat(0, 0);

 out:

	return fsal_status;
}

struct fsal_populate_cb_state {
	struct fsal_obj_handle *directory;
	fsal_status_t *status;
	fsal_getattr_cb_t cb;
	void *opaque;
};

static bool
populate_dirent(const char *name, void *dir_state,
		fsal_cookie_t cookie)
{
	struct fsal_populate_cb_state *state =
	    (struct fsal_populate_cb_state *)dir_state;
	struct fsal_obj_handle *obj = state->directory;
	fsal_status_t fsal_status = { 0, 0 };
	cache_inode_status_t ci_status = 0;
	struct cache_inode_readdir_cb_parms cb_parms = { state->opaque, name,
							 true, 0, true };

	fsal_status = obj->obj_ops.lookup(obj, name, &obj);
	if (FSAL_IS_ERROR(fsal_status)) {
		*state->status = fsal_status;
		if (fsal_status.major == ERR_FSAL_XDEV) {
			LogInfo(COMPONENT_NFS_READDIR,
				"Ignoring XDEV entry %s",
				name);
			*state->status = fsalstat(0, 0);
			return true;
		}
		LogInfo(COMPONENT_FSAL,
			"Lookup failed on %s in dir %p with %s",
			name, obj, fsal_err_txt(fsal_status));
		return false;
	}

	fsal_status = fsal_refresh_attrs(obj);
	if (FSAL_IS_ERROR(fsal_status)) {
		LogInfo(COMPONENT_FSAL,
			"attr refresh failed on %s in dir %p with %s",
			name, obj, fsal_err_txt(fsal_status));
		return false;
	}

	ci_status = state->cb(&cb_parms, obj, &obj->attributes,
		       obj->attributes.fileid, cookie, CB_ORIGINAL);
	if (ci_status == CACHE_INODE_CROSS_JUNCTION) {
		/* XXX dang junction */
		return false;
	}

	return true;
}

/**
 * @brief Reads a directory
 *
 * This function iterates over the directory entries  and invokes a supplied
 * callback function for each one.
 *
 * @param[in]  directory The directory to be read
 * @param[in]  cookie    Starting cookie for the readdir operation
 * @param[out] nbfound   Number of entries returned.
 * @param[out] eod_met   Whether the end of directory was met
 * @param[in]  attrmask  Attributes requested, used for permission checking
 *                       really all that matters is ATTR_ACL and any attrs
 *                       at all, specifics never actually matter.
 * @param[in]  cb        The callback function to receive entries
 * @param[in]  opaque    A pointer passed to be passed in
 *                       cache_inode_readdir_cb_parms
 *
 * @return FSAL status
 */

fsal_status_t fsal_readdir(struct fsal_obj_handle *directory,
		    uint64_t cookie, unsigned int *nbfound,
		    bool *eod_met,
		    attrmask_t attrmask,
		    fsal_getattr_cb_t cb,
		    void *opaque)
{
	fsal_status_t fsal_status = {0, 0};
	fsal_status_t attr_status = {0, 0};
	fsal_status_t cb_status = {0, 0};
	struct fsal_populate_cb_state state;
	bool eod = false;

	/* The access mask corresponding to permission to list directory
	   entries */
	fsal_accessflags_t access_mask =
	    (FSAL_MODE_MASK_SET(FSAL_R_OK) |
	     FSAL_ACE4_MASK_SET(FSAL_ACE_PERM_LIST_DIR));
	fsal_accessflags_t access_mask_attr =
	    (FSAL_MODE_MASK_SET(FSAL_R_OK) | FSAL_MODE_MASK_SET(FSAL_X_OK) |
	     FSAL_ACE4_MASK_SET(FSAL_ACE_PERM_LIST_DIR) |
	     FSAL_ACE4_MASK_SET(FSAL_ACE_PERM_EXECUTE));

	/* readdir can be done only with a directory */
	if (directory->type != DIRECTORY) {
		LogFullDebug(COMPONENT_NFS_READDIR,
			     "Not a directory");
		return fsalstat(ERR_FSAL_NOTDIR, 0);
	}

	fsal_status = fsal_refresh_attrs(directory);
	if (FSAL_IS_ERROR(fsal_status)) {
		LogDebug(COMPONENT_NFS_READDIR,
			 "fsal_refresh_attrs status=%s",
			 fsal_err_txt(fsal_status));
		return fsal_status;
	}

	/* Adjust access mask if ACL is asked for.
	 * NOTE: We intentionally do NOT check ACE4_READ_ATTR.
	 */
	if ((attrmask & ATTR_ACL) != 0) {
		access_mask |= FSAL_ACE4_MASK_SET(FSAL_ACE_PERM_READ_ACL);
		access_mask_attr |= FSAL_ACE4_MASK_SET(FSAL_ACE_PERM_READ_ACL);
	}

	fsal_status = fsal_access(directory, access_mask, NULL, NULL);
	if (FSAL_IS_ERROR(fsal_status)) {
		LogFullDebug(COMPONENT_NFS_READDIR,
			     "permission check for directory status=%s",
			     fsal_err_txt(fsal_status));
		return fsal_status;
	}
	if (attrmask != 0) {
		/* Check for access permission to get attributes */
		attr_status = fsal_access(directory, access_mask_attr, NULL,
					  NULL);
		if (FSAL_IS_ERROR(attr_status))
			LogFullDebug(COMPONENT_NFS_READDIR,
				     "permission check for attributes "
				     "status=%s",
				     fsal_err_txt(fsal_status));
	}

	state.directory = directory;
	state.status = &cb_status;
	state.cb = cb;
	state.opaque = opaque;

	fsal_status = directory->obj_ops.readdir(directory, &cookie,
						 (void *)&state,
						 populate_dirent,
						 &eod);

	return fsal_status;
}

/**
 *
 * @brief Remove a name from a directory.
 *
 * @param[in] obj     Handle for the parent directory to be managed
 * @param[in] name    Name to be removed
 *
 * @retval CACHE_INODE_SUCCESS if operation is a success
 */

fsal_status_t
fsal_remove(struct fsal_obj_handle *parent, const char *name)
{
	struct fsal_obj_handle *to_remove_obj = NULL;
	fsal_status_t fsal_status = { 0, 0 };

	if (parent->type != DIRECTORY) {
		fsal_status = fsalstat(ERR_FSAL_NOTDIR, 0);
		goto out;
	}

	/* Factor this somewhat.  In the case where the directory hasn't
	   been populated, the entry may not exist in the cache and we'd
	   be bringing it in just to dispose of it. */

	/* Looks up for the entry to remove */
	fsal_status = fsal_lookup(parent, name, &to_remove_obj);

	if (to_remove_obj == NULL) {
		LogFullDebug(COMPONENT_FSAL, "lookup %s failure %s",
			     name, fsal_err_txt(fsal_status));
		goto out;
	}

	/* Do not remove a junction node or an export root. */
	if (to_remove_obj->type == DIRECTORY) {
		/* XXX dang don't remove junction */
	}

	LogDebug(COMPONENT_FSAL, "%s", name);

	if (fsal_is_open(to_remove_obj)) {
		/* obj is not locked and seems to be open for fd caching
		 * purpose.  candidate for closing since unlink of an open file
		 * results in 'silly rename' on certain platforms */
		fsal_status = to_remove_obj->obj_ops.close(to_remove_obj);
		if (FSAL_IS_ERROR(fsal_status)) {
			/* non-fatal error. log the warning and move on */
			LogCrit(COMPONENT_FSAL,
				"Error closing %s before unlink: %s.", name,
				fsal_err_txt(fsal_status));
		}
	}

	fsal_status = parent->obj_ops.unlink(parent, name);

	if (FSAL_IS_ERROR(fsal_status)) {
		LogFullDebug(COMPONENT_FSAL, "unlink %s failure %s",
			     name, fsal_err_txt(fsal_status));
		goto out;
	}

	fsal_status = fsal_refresh_attrs(parent);

	if (FSAL_IS_ERROR(fsal_status)) {
		LogFullDebug(COMPONENT_FSAL,
			     "not sure this code makes sense %s failure %s",
			     name, fsal_err_txt(fsal_status));
		goto out;
	}

out:
	LogFullDebug(COMPONENT_FSAL, "remove %s: status=%s", name,
		     fsal_err_txt(fsal_status));

	return fsal_status;
}

/**
 * @brief Renames a file
 *
 * @param[in] dir_src  The source directory
 * @param[in] oldname  The current name of the file
 * @param[in] dir_dest The destination directory
 * @param[in] newname  The name to be assigned to the object
 *
 * @return NFS4 error code
 */
nfsstat4 fsal_rename(struct fsal_obj_handle *dir_src,
			  const char *oldname,
			  struct fsal_obj_handle *dir_dest,
			  const char *newname)
{
	nfsstat4 status = NFS4_OK;
	fsal_status_t fsal_status = { 0, 0 };
	struct fsal_obj_handle *lookup_src = NULL;
	struct fsal_obj_handle *lookup_dst = NULL;

	if ((dir_src->type != DIRECTORY) || (dir_dest->type != DIRECTORY)) {
		status = NFS4ERR_NOTDIR;
		goto out;
	}

	/* Check for . and .. on oldname and newname. */
	if (!strcmp(oldname, ".") || !strcmp(oldname, "..")
	    || !strcmp(newname, ".") || !strcmp(newname, "..")) {
		status = NFS4ERR_BADNAME;
		goto out;
	}

	/* Check for object existence in source directory */
	fsal_status = fsal_lookup(dir_src, oldname, &lookup_src);

	if (FSAL_IS_ERROR(fsal_status)) {
		status = fsal_error_convert(fsal_status);

		LogDebug(COMPONENT_FSAL,
			 "Rename (%p,%s)->(%p,%s) : source doesn't exist",
			 dir_src, oldname, dir_dest, newname);
		goto out;
	}

	/* Do not rename a junction node or an export root. */
	if (lookup_src->type == DIRECTORY) {
		/* XXX dang check for junction */
	}

	/* Check if an object with the new name exists in the destination
	   directory */
	fsal_status = fsal_lookup(dir_dest, newname, &lookup_dst);
	if (!FSAL_IS_ERROR(fsal_status)) {
		LogDebug(COMPONENT_FSAL,
			 "Rename (%p,%s)->(%p,%s) : destination already exists",
			 dir_src, oldname, dir_dest, newname);
		if (lookup_src == lookup_dst) {
			/* Nothing to do according to POSIX and NFS3/4 If from
			 * and to both refer to the same file (they might be
			 * hard links of each other), then RENAME should perform
			 * no action and return success */
			LogDebug(COMPONENT_FSAL,
				 "Rename (%p,%s)->(%p,%s) : same file so skipping out",
				 dir_src, oldname, dir_dest, newname);
			goto out;
		}
	}

	/* Perform the rename operation in FSAL before doing anything in the
	 * cache. Indeed, if the FSAL_rename fails unexpectly, the cache would
	 * be inconsistent!
	 *
	 * We do almost no checking before making call because we want to return
	 * error based on the files actually present in the directories, not
	 * what we have in our cache.
	 */
	LogFullDebug(COMPONENT_FSAL, "about to call FSAL rename");

	fsal_status = dir_src->obj_ops.rename(lookup_src, dir_src, oldname,
					      dir_dest, newname);

	LogFullDebug(COMPONENT_FSAL, "returned from FSAL rename");

	if (FSAL_IS_ERROR(fsal_status)) {
		status = fsal_error_convert(fsal_status);

		LogFullDebug(COMPONENT_FSAL,
			     "FSAL rename failed with %s",
			     nfsstat4_to_str(status));

		goto out;
	}

out:
	return status;
}

fsal_status_t fsal_statfs(struct fsal_obj_handle *obj,
			  fsal_dynamicfsinfo_t *dynamicinfo)
{
	fsal_status_t fsal_status;
	struct fsal_export *export;

	export = op_ctx->export->fsal_export;
	/* Get FSAL to get dynamic info */
	fsal_status =
	    export->exp_ops.get_fs_dynamic_info(export, obj, dynamicinfo);
	LogFullDebug(COMPONENT_FSAL,
		     "fsal_statfs: dynamicinfo: {total_bytes = %" PRIu64
		     ", " "free_bytes = %" PRIu64 ", avail_bytes = %" PRIu64
		     ", total_files = %" PRIu64 ", free_files = %" PRIu64
		     ", avail_files = %" PRIu64 "}", dynamicinfo->total_bytes,
		     dynamicinfo->free_bytes, dynamicinfo->avail_bytes,
		     dynamicinfo->total_files, dynamicinfo->free_files,
		     dynamicinfo->avail_files);
	return fsal_status;
}

/**
 * @brief Commit a section of a file to storage
 *
 * @param[in] obj	File to commit
 * @param[in] offset	Offset for start of commit
 * @param[in] len	Length of commit
 * @return FSAL status
 */
fsal_status_t fsal_commit(struct fsal_obj_handle *obj, off_t offset,
			 size_t len)
{
	/* Error return from FSAL calls */
	fsal_status_t fsal_status = { 0, 0 };
	bool opened = false;

	if (!fsal_is_open(obj)) {
		LogFullDebug(COMPONENT_FSAL, "need to open");
		fsal_status = obj->obj_ops.open(obj, FSAL_O_WRITE);
		if (FSAL_IS_ERROR(fsal_status))
			return fsal_status;
		opened = true;
	}

	fsal_status = obj->obj_ops.commit(obj, offset, len);

	if (opened) {
		obj->obj_ops.close(obj);
	}

	return fsal_status;
}

/**
 * @brief Converts an FSAL error to the corresponding cache_inode error
 *
 * This function converts an FSAL error to the corresponding
 * cache_inode error.
 *
 * @param[in] fsal_status FSAL error to be converted
 *
 * @return the result of the conversion.
 *
 */
cache_inode_status_t
cache_inode_error_convert(fsal_status_t fsal_status)
{
	switch (fsal_status.major) {
	case ERR_FSAL_NO_ERROR:
		return CACHE_INODE_SUCCESS;

	case ERR_FSAL_NOENT:
		return CACHE_INODE_NOT_FOUND;

	case ERR_FSAL_EXIST:
		return CACHE_INODE_ENTRY_EXISTS;

	case ERR_FSAL_ACCESS:
		return CACHE_INODE_FSAL_EACCESS;

	case ERR_FSAL_PERM:
		return CACHE_INODE_FSAL_EPERM;

	case ERR_FSAL_NOSPC:
		return CACHE_INODE_NO_SPACE_LEFT;

	case ERR_FSAL_NOTEMPTY:
		return CACHE_INODE_DIR_NOT_EMPTY;

	case ERR_FSAL_ROFS:
		return CACHE_INODE_READ_ONLY_FS;

	case ERR_FSAL_NOTDIR:
		return CACHE_INODE_NOT_A_DIRECTORY;

	case ERR_FSAL_IO:
	case ERR_FSAL_NXIO:
		return CACHE_INODE_IO_ERROR;

	case ERR_FSAL_STALE:
	case ERR_FSAL_FHEXPIRED:
		return CACHE_INODE_ESTALE;

	case ERR_FSAL_INVAL:
	case ERR_FSAL_OVERFLOW:
		return CACHE_INODE_INVALID_ARGUMENT;

	case ERR_FSAL_DQUOT:
	case ERR_FSAL_NO_QUOTA:
		return CACHE_INODE_QUOTA_EXCEEDED;

	case ERR_FSAL_SEC:
		return CACHE_INODE_FSAL_ERR_SEC;

	case ERR_FSAL_NOTSUPP:
	case ERR_FSAL_ATTRNOTSUPP:
		return CACHE_INODE_NOT_SUPPORTED;

	case ERR_FSAL_UNION_NOTSUPP:
		return CACHE_INODE_UNION_NOTSUPP;

	case ERR_FSAL_DELAY:
		return CACHE_INODE_DELAY;

	case ERR_FSAL_NAMETOOLONG:
		return CACHE_INODE_NAME_TOO_LONG;

	case ERR_FSAL_NOMEM:
		return CACHE_INODE_MALLOC_ERROR;

	case ERR_FSAL_BADCOOKIE:
		return CACHE_INODE_BAD_COOKIE;

	case ERR_FSAL_FILE_OPEN:
		return CACHE_INODE_FILE_OPEN;

	case ERR_FSAL_NOT_OPENED:
		LogDebug(COMPONENT_CACHE_INODE,
			 "Conversion of ERR_FSAL_NOT_OPENED to "
			 "CACHE_INODE_FSAL_ERROR");
		return CACHE_INODE_FSAL_ERROR;

	case ERR_FSAL_ISDIR:
		return CACHE_INODE_IS_A_DIRECTORY;

	case ERR_FSAL_SYMLINK:
	case ERR_FSAL_BADTYPE:
		return CACHE_INODE_BAD_TYPE;

	case ERR_FSAL_FBIG:
		return CACHE_INODE_FILE_BIG;

	case ERR_FSAL_XDEV:
		return CACHE_INODE_FSAL_XDEV;

	case ERR_FSAL_MLINK:
		return CACHE_INODE_FSAL_MLINK;

	case ERR_FSAL_FAULT:
	case ERR_FSAL_SERVERFAULT:
	case ERR_FSAL_DEADLOCK:
		return CACHE_INODE_SERVERFAULT;

	case ERR_FSAL_TOOSMALL:
		return CACHE_INODE_TOOSMALL;

	case ERR_FSAL_SHARE_DENIED:
		return CACHE_INODE_FSAL_SHARE_DENIED;

	case ERR_FSAL_IN_GRACE:
		return CACHE_INODE_IN_GRACE;

	case ERR_FSAL_BADHANDLE:
		return CACHE_INODE_BADHANDLE;

	case ERR_FSAL_BLOCKED:
	case ERR_FSAL_INTERRUPT:
	case ERR_FSAL_NOT_INIT:
	case ERR_FSAL_ALREADY_INIT:
	case ERR_FSAL_BAD_INIT:
	case ERR_FSAL_TIMEOUT:
		/* These errors should be handled inside Cache Inode (or
		 * should never be seen by Cache Inode) */
		LogDebug(COMPONENT_CACHE_INODE,
			 "Conversion of FSAL error %d,%d to "
			 "CACHE_INODE_FSAL_ERROR",
			 fsal_status.major, fsal_status.minor);
		return CACHE_INODE_FSAL_ERROR;
	}

	/* We should never reach this line, this may produce a warning with
	 * certain compiler */
	LogCrit(COMPONENT_CACHE_INODE,
		"cache_inode_error_convert: default conversion to "
		"CACHE_INODE_FSAL_ERROR for error %d, line %u should never be "
		"reached",
		fsal_status.major, __LINE__);
	return CACHE_INODE_FSAL_ERROR;
}

/** @} */
