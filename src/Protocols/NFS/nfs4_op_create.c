 /*
  * vim:noexpandtab:shiftwidth=8:tabstop=8:
  *
  * Copyright CEA/DAM/DIF  (2008)
  * contributeur : Philippe DENIEL   philippe.deniel@cea.fr
  *                Thomas LEIBOVICI  thomas.leibovici@cea.fr
  *
  *
  * This program is free software; you can redistribute it and/or
  * modify it under the terms of the GNU Lesser General Public License
  * as published by the Free Software Foundation; either version 3 of
  * the License, or (at your option) any later version.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  * Lesser General Public License for more details.
  *
  * You should have received a copy of the GNU Lesser General Public
  * License along with this library; if not, write to the Free
  * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  * Boston, MA 02110-1301 USA
  *
  * ---------------------------------------
  */

/**
 * @file    nfs4_op_create.c
 * @brief   Routines used for managing the NFS4 COMPOUND functions.
 *
 * Routines used for managing the NFS4 COMPOUND functions.
 *
 */
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/file.h>
#include "log.h"
#include "fsal.h"
#include "nfs_core.h"
#include "nfs_exports.h"
#include "nfs_creds.h"
#include "nfs_proto_functions.h"
#include "nfs_proto_tools.h"
#include "nfs_convert.h"
#include "nfs_file_handle.h"
#include "export_mgr.h"

/**
 * @brief NFS4_OP_CREATE, creates a non-regular entry
 *
 * This function implements the NFS4_OP_CREATE operation, which
 * creates a non-regular entry.
 *
 * @param[in]     op   Arguments for nfs4_op
 * @param[in,out] data Compound request's data
 * @param[out]    resp Results for nfs4_op
 *
 * @return per RFC5661, p. 363
 */

int nfs4_op_create(struct nfs_argop4 *op, compound_data_t *data,
		   struct nfs_resop4 *resp)
{
	CREATE4args * const arg_CREATE4 = &op->nfs_argop4_u.opcreate;
	CREATE4res * const res_CREATE4 = &resp->nfs_resop4_u.opcreate;

	struct fsal_obj_handle *obj_parent = NULL;
	struct fsal_obj_handle *obj_new = NULL;
	struct attrlist sattr;
	nfs_fh4 newfh4;
	int convrc = 0;
	char *name = NULL;
	char *link_content = NULL;
	struct fsal_export *exp_hdl;
	fsal_status_t fsal_status;
	struct attrlist object_attributes;
	fsal_dev_t dev_spec;

	memset(&object_attributes, 0, sizeof(object_attributes));

	resp->resop = NFS4_OP_CREATE;
	res_CREATE4->status = NFS4_OK;

	/* Do basic checks on a filehandle */
	res_CREATE4->status = nfs4_sanity_check_FH(data, DIRECTORY, false);
	if (res_CREATE4->status != NFS4_OK)
		goto out;

	/* if quota support is active, then we should check is the FSAL allows
	 * inode creation or not */
	exp_hdl = op_ctx->fsal_export;

	fsal_status = exp_hdl->exp_ops.check_quota(exp_hdl,
						op_ctx->export->fullpath,
						FSAL_QUOTA_INODES);

	if (FSAL_IS_ERROR(fsal_status)) {
		res_CREATE4->status = NFS4ERR_DQUOT;
		goto out;
	}

	/* Ask only for supported attributes */
	if (!nfs4_Fattr_Supported(&arg_CREATE4->createattrs)) {
		res_CREATE4->status = NFS4ERR_ATTRNOTSUPP;
		goto out;
	}

	/* Do not use READ attr, use WRITE attr */
	if (!nfs4_Fattr_Check_Access
	    (&arg_CREATE4->createattrs, FATTR4_ATTR_WRITE)) {
		res_CREATE4->status = NFS4ERR_INVAL;
		goto out;
	}

	/* This operation is used to create a non-regular file,
	 * this means: - a symbolic link
	 *             - a block device file
	 *             - a character device file
	 *             - a socket file
	 *             - a fifo
	 *             - a directory
	 *
	 * You can't use this operation to create a regular file,
	 * you have to use NFS4_OP_OPEN for this
	 */

	/* Validate and convert the UFT8 objname to a regular string */
	res_CREATE4->status = nfs4_utf8string2dynamic(&arg_CREATE4->objname,
						      UTF8_SCAN_ALL,
						      &name);

	if (res_CREATE4->status != NFS4_OK)
		goto out;

	/* Convert current FH into a obj, the current_obj
	   (assocated with the current FH will be used for this */
	obj_parent = data->current_obj;

	/* The currentFH must point to a directory
	 * (objects are always created within a directory)
	 */
	if (data->current_filetype != DIRECTORY) {
		res_CREATE4->status = NFS4ERR_NOTDIR;
		goto out;
	}

	res_CREATE4->CREATE4res_u.resok4.cinfo.before =
		fsal_get_changeid4(obj_parent);

	FSAL_SET_MASK(object_attributes.mask,
		      ATTR_MODE | ATTR_OWNER | ATTR_GROUP);
	object_attributes.owner = op_ctx->creds->caller_uid;
	object_attributes.group = op_ctx->creds->caller_gid; /* be more
							       * selective? */
	/* Convert the incoming fattr4 to a vattr structure,
	 * if such arguments are supplied
	 */
	if (arg_CREATE4->createattrs.attrmask.bitmap4_len != 0) {
		/* Arguments were supplied, extract them */
		convrc = nfs4_Fattr_To_FSAL_attr(&sattr,
						 &arg_CREATE4->createattrs,
						 data);

		if (convrc != NFS4_OK) {
			res_CREATE4->status = convrc;
			goto out;
		}

		if (sattr.mask & ATTR_MODE)
			object_attributes.mode = sattr.mode;
	}

	/* Create either a symbolic link or a directory */
	switch (arg_CREATE4->objtype.type) {
	case NF4LNK:
		/* Convert the name to link from into a regular string */
		res_CREATE4->status = nfs4_utf8string2dynamic(
				&arg_CREATE4->objtype.createtype4_u.linkdata,
				UTF8_SCAN_SYMLINK,
				&link_content);

		if (res_CREATE4->status != NFS4_OK)
			goto out;

		/* do the symlink operation */
		fsal_status = obj_parent->obj_ops.symlink(obj_parent, name,
					     link_content,
					     &object_attributes,
					     &obj_new);
		break;

	case NF4DIR:
		/* Create a new directory */
		fsal_status = obj_parent->obj_ops.mkdir(obj_parent, name,
					      &object_attributes, &obj_new);
		break;

	case NF4SOCK:
		/* Create a new socket file */
		fsal_status = obj_parent->obj_ops.mknode(obj_parent,
						      name, SOCKET_FILE,
						      NULL, /* dev_t !needed */
						      &object_attributes,
						      &obj_new);
		break;

	case NF4FIFO:
		/* Create a new socket file */
		fsal_status = obj_parent->obj_ops.mknode(obj_parent,
						      name, FIFO_FILE,
						      NULL, /* dev_t !needed */
						      &object_attributes,
						      &obj_new);
		break;

	case NF4CHR:
		dev_spec.major =
		    arg_CREATE4->objtype.createtype4_u.devdata.specdata1;
		dev_spec.minor =
		    arg_CREATE4->objtype.createtype4_u.devdata.specdata2;

		/* Create a new socket file */
		fsal_status =
		    obj_parent->obj_ops.mknode(obj_parent, name, CHARACTER_FILE,
					       &dev_spec, &object_attributes,
					       &obj_new);
		break;

	case NF4BLK:
		dev_spec.major =
		    arg_CREATE4->objtype.createtype4_u.devdata.specdata1;
		dev_spec.minor =
		    arg_CREATE4->objtype.createtype4_u.devdata.specdata2;

		/* Create a new socket file */
		fsal_status =
		    obj_parent->obj_ops.mknode(obj_parent, name, BLOCK_FILE,
					       &dev_spec, &object_attributes,
					       &obj_new);
		break;

	default:
		/* Should never happen, but return NFS4ERR_BADTYPE
		 *in this case
		 */
		res_CREATE4->status = NFS4ERR_BADTYPE;
		goto out;
	}			/* switch( arg_CREATE4.objtype.type ) */


	if (FSAL_IS_ERROR(fsal_status)) {
		if (fsal_status.major == ERR_FSAL_EXIST) {
			res_CREATE4->status = NFS4ERR_EXIST;
			goto out;
		}

		res_CREATE4->status = fsal_error_convert(fsal_status);
		goto out;
	}

	/* Allocation of a new file handle */
	if (nfs4_AllocateFH(&newfh4) != NFS4_OK) {
		res_CREATE4->status = NFS4ERR_SERVERFAULT;
		goto out;
	}

	/* Building the new file handle */
	if (!nfs4_FSALToFhandle(&newfh4, obj_new, op_ctx->export)) {
		res_CREATE4->status = NFS4ERR_SERVERFAULT;
		goto out;
	}

	/* This new fh replaces the current FH */
	data->currentFH.nfs_fh4_len = newfh4.nfs_fh4_len;

	memcpy(data->currentFH.nfs_fh4_val,
	       newfh4.nfs_fh4_val,
	       newfh4.nfs_fh4_len);

	/* Mark current_stateid as invalid */
	data->current_stateid_valid = false;

	/* No do not need newfh any more */
	gsh_free(newfh4.nfs_fh4_val);

	/* Set the mode if requested */
	/* Use the same fattr mask for reply, if one attribute was not
	   settable, NFS4ERR_ATTRNOTSUPP was replyied */
	res_CREATE4->CREATE4res_u.resok4.attrset.bitmap4_len =
	    arg_CREATE4->createattrs.attrmask.bitmap4_len;

	if (arg_CREATE4->createattrs.attrmask.bitmap4_len != 0) {
		/* If owner or owner_group are set, and the credential was
		 * squashed, then we must squash the set owner and owner_group.
		 */
		squash_setattr(&sattr);

		/* Skip setting attributes if all asked attributes
		 * are handled by create
		 */
		if ((sattr.mask & CREATE_MASK_NON_REG_NFS4)
		    || ((sattr.mask & ATTR_OWNER)
			&& (op_ctx->creds->caller_uid != sattr.owner))
		    || ((sattr.mask & ATTR_GROUP)
			&& (op_ctx->creds->caller_gid != sattr.group))) {

			/* mask off flags handled by create */
			 sattr.mask &= (CREATE_MASK_NON_REG_NFS4 |
					ATTRS_CREDS);

			fsal_status = fsal_setattr(obj_new, &sattr);
			if (FSAL_IS_ERROR(fsal_status)) {
				res_CREATE4->status =
					fsal_error_convert(fsal_status);
				goto out;
			}
		}

		/* copy over bitmap */
		res_CREATE4->CREATE4res_u.resok4.attrset =
		    arg_CREATE4->createattrs.attrmask;
	}

	memset(&res_CREATE4->CREATE4res_u.resok4.cinfo.after,
	       0,
	       sizeof(changeid4));

	res_CREATE4->CREATE4res_u.resok4.cinfo.after =
		fsal_get_changeid4(obj_parent);

	/* Operation is supposed to be atomic .... */
	res_CREATE4->CREATE4res_u.resok4.cinfo.atomic = FALSE;

	LogFullDebug(COMPONENT_NFS_V4,
		     "CREATE CINFO before = %" PRIu64 "  after = %" PRIu64
		     "  atomic = %d",
		     res_CREATE4->CREATE4res_u.resok4.cinfo.before,
		     res_CREATE4->CREATE4res_u.resok4.cinfo.after,
		     res_CREATE4->CREATE4res_u.resok4.cinfo.atomic);

	/* @todo : BUGAZOMEU: fair ele free dans cette fonction */

	/* Keep the vnode entry for the file in the compound data */
	set_current_entry(data, obj_new);

	/* If you reach this point, then no error occured */
	res_CREATE4->status = NFS4_OK;

 out:

	if (name != NULL)
		gsh_free(name);

	if (link_content != NULL)
		gsh_free(link_content);

	return res_CREATE4->status;
}				/* nfs4_op_create */

/**
 * @brief Free memory allocated for CREATE result
 *
 * This function frees any memory allocated for the result of the
 * NFS4_OP_CREATE operation.
 *
 * @param[in,out] resp nfs4_op results
 */
void nfs4_op_create_Free(nfs_resop4 *resp)
{
	/* Nothing to be done */
}
