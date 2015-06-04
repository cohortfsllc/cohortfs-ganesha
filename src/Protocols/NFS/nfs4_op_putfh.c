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
 * ---------------------------------------
 */

/**
 * @file    nfs4_op_putfh.c
 * @brief   Routines used for managing the NFS4_OP_PUTFH operation.
 *
 * Routines used for managing the NFS4_OP_PUTFH operation.
 *
 */
#include "config.h"
#include "hashtable.h"
#include "log.h"
#include "fsal.h"
#include "nfs_core.h"
#include "cache_inode.h"
#include "nfs_exports.h"
#include "nfs_creds.h"
#include "nfs_proto_functions.h"
#include "nfs_convert.h"
#include "export_mgr.h"
#include "client_mgr.h"
#include "fsal_convert.h"
#include "nfs_file_handle.h"
#include "pnfs_utils.h"

static int nfs4_ds_putfh(compound_data_t *data)
{
	struct file_handle_v4 *v4_handle =
		(struct file_handle_v4 *)data->currentFH.nfs_fh4_val;
	struct fsal_pnfs_ds *pds;
	struct gsh_buffdesc fh_desc;
	bool changed = true;

	LogFullDebug(COMPONENT_FILEHANDLE, "NFS4 Handle 0x%X export id %d",
		v4_handle->fhflags1, v4_handle->id.exports);

	/* Find any existing server by the "id" from the handle,
	 * before releasing the old DS (to prevent thrashing).
	 */
	pds = pnfs_ds_get(v4_handle->id.servers);
	if (pds == NULL) {
		LogInfoAlt(COMPONENT_DISPATCH, COMPONENT_EXPORT,
			   "NFS4 Request from client (%s) has invalid server identifier %d",
			   op_ctx->client ?
			   op_ctx->client->hostaddr_str : "unknown",
			   v4_handle->id.servers);

		return NFS4ERR_STALE;
	}

	/* If old CurrentFH had a related server, release reference. */
	if (op_ctx->fsal_pnfs_ds != NULL) {
		changed = v4_handle->id.servers
			!= op_ctx->fsal_pnfs_ds->id_servers;
		pnfs_ds_put(op_ctx->fsal_pnfs_ds);
	}

	/* If old CurrentFH had a related export, release reference. */
	if (op_ctx->export != NULL) {
		changed = op_ctx->export != pds->mds_export;
		put_gsh_export(op_ctx->export);
	}

	if (pds->mds_export == NULL) {
		/* most likely */
		op_ctx->export = NULL;
		op_ctx->fsal_export = NULL;
	} else if (pds->pnfs_ds_status == PNFS_DS_READY) {
		/* special case: avoid lookup of related export.
		 * get_gsh_export_ref() was bumped in pnfs_ds_get()
		 */
		op_ctx->export = pds->mds_export;
		op_ctx->fsal_export = op_ctx->export->fsal_export;
	} else {
		/* export reference has been dropped. */
		put_gsh_export(pds->mds_export);
		op_ctx->export = NULL;
		op_ctx->fsal_export = NULL;
		return NFS4ERR_STALE;
	}

	/* Clear out current entry for now */
	set_current_entry(data, NULL);

	/* update _ctx fields */
	op_ctx->fsal_pnfs_ds = pds;

	if (changed) {
		int status;
		/* permissions may have changed */
		status = pds->s_ops.permissions(pds, data->req);
		if (status != NFS4_OK)
			return status;
	}

	fh_desc.len = v4_handle->fs_len;
	fh_desc.addr = &v4_handle->fsopaque;

	/* Leave the current_entry as NULL, but indicate a
	 * regular file.
	 */
	data->current_filetype = REGULAR_FILE;

	return pds->s_ops.make_ds_handle(pds, &fh_desc, &data->current_ds,
					 v4_handle->fhflags1);
}

static int nfs4_mds_putfh(compound_data_t *data)
{
	struct file_handle_v4 *v4_handle =
		(struct file_handle_v4 *)data->currentFH.nfs_fh4_val;
	struct gsh_export *exporting;
	cache_inode_fsal_data_t fsal_data;
	struct fsal_obj_handle *new_hdl;
	fsal_status_t fsal_status = { 0, 0 };
	bool changed = true;

	LogFullDebug(COMPONENT_FILEHANDLE, "NFS4 Handle 0x%X export id %d",
		v4_handle->fhflags1, v4_handle->id.exports);

	/* Find any existing export by the "id" from the handle,
	 * before releasing the old export (to prevent thrashing).
	 */
	exporting = get_gsh_export(v4_handle->id.exports);
	if (exporting == NULL) {
		LogInfoAlt(COMPONENT_DISPATCH, COMPONENT_EXPORT,
			   "NFS4 Request from client (%s) has invalid export identifier %d",
			   op_ctx->client ?
			   op_ctx->client->hostaddr_str : "unknown",
			   v4_handle->id.exports);

		return NFS4ERR_STALE;
	}

	/* If old CurrentFH had a related export, release reference. */
	if (op_ctx->export != NULL) {
		changed = v4_handle->id.exports != op_ctx->export->export_id;
		put_gsh_export(op_ctx->export);
	}

	/* If old CurrentFH had a related server, release reference. */
	if (op_ctx->fsal_pnfs_ds != NULL) {
		pnfs_ds_put(op_ctx->fsal_pnfs_ds);
		op_ctx->fsal_pnfs_ds = NULL;
	}

	/* Clear out current entry for now */
	set_current_entry(data, NULL);

	/* update _ctx fields needed by nfs4_export_check_access */
	op_ctx->export = exporting;

	if (changed) {
		int status;
		status = nfs4_export_check_access(data->req);
		if (status != NFS4_OK)
			return status;
	}

	op_ctx->fsal_export = fsal_data.export = exporting->fsal_export;
	fsal_data.fh_desc.len = v4_handle->fs_len;
	fsal_data.fh_desc.addr = &v4_handle->fsopaque;

	/* adjust the handle opaque into a cache key */
	fsal_status = fsal_data.export->exp_ops.extract_handle(fsal_data.export,
			       FSAL_DIGEST_NFSV4, &fsal_data.fh_desc,
			       v4_handle->fhflags1);
	if (FSAL_IS_ERROR(fsal_status))
		return fsal_error_convert(fsal_status);

	fsal_status =
		fsal_data.export->exp_ops.create_handle(fsal_data.export,
							 &fsal_data.fh_desc,
							 &new_hdl);
	if (FSAL_IS_ERROR(fsal_status)) {
		LogDebug(COMPONENT_FILEHANDLE,
			 "could not get create_handle object");
		return fsal_error_convert(fsal_status);
	}

	/* Set the current entry using the ref from get */
	set_current_entry(data, new_hdl);

	LogFullDebug(COMPONENT_FILEHANDLE,
		     "File handle is of type %s(%d)",
		     object_file_type_to_str(data->current_filetype),
		     data->current_filetype);

	return NFS4_OK;
}

/**
 * @brief The NFS4_OP_PUTFH operation
 *
 * Sets the current FH with the value given in argument.
 *
 * @param[in]     op   Arguments for nfs4_op
 * @param[in,out] data Compound request's data
 * @param[out]    resp Results for nfs4_op
 *
 * @return per RFC5661, p. 371
 *
 * @see nfs4_Compound
 *
 */

int nfs4_op_putfh(struct nfs_argop4 *op, compound_data_t *data,
		  struct nfs_resop4 *resp)
{
	/* Convenience alias for args */
	PUTFH4args * const arg_PUTFH4 = &op->nfs_argop4_u.opputfh;
	/* Convenience alias for resopnse */
	PUTFH4res * const res_PUTFH4 = &resp->nfs_resop4_u.opputfh;

	resp->resop = NFS4_OP_PUTFH;

	/* First check the handle.  If it is rubbish, we go no further
	 */
	res_PUTFH4->status = nfs4_Is_Fh_Invalid(&arg_PUTFH4->object);
	if (res_PUTFH4->status != NFS4_OK)
		return res_PUTFH4->status;

	/* If no currentFH were set, allocate one */
	if (data->currentFH.nfs_fh4_val == NULL) {
		res_PUTFH4->status = nfs4_AllocateFH(&(data->currentFH));
		if (res_PUTFH4->status != NFS4_OK)
			return res_PUTFH4->status;
	}

	/* Copy the filehandle from the arg structure */
	data->currentFH.nfs_fh4_len = arg_PUTFH4->object.nfs_fh4_len;
	memcpy(data->currentFH.nfs_fh4_val, arg_PUTFH4->object.nfs_fh4_val,
	       arg_PUTFH4->object.nfs_fh4_len);

	/* The export and fsalid should be updated, but DS handles
	 * don't support metadata operations.  Thus, we can't call into
	 * cache_inode to populate the metadata cache.
	 */
	if (nfs4_Is_Fh_DSHandle(&data->currentFH))
		res_PUTFH4->status = nfs4_ds_putfh(data);
	else
		res_PUTFH4->status = nfs4_mds_putfh(data);

	return res_PUTFH4->status;
}				/* nfs4_op_putfh */

/**
 * @brief Free memory allocated for PUTFH result
 *
 * This function frees any memory allocated for the result of the
 * NFS4_OP_PUTFH operation.
 *
 * @param[in,out] resp nfs4_op results
 */
void nfs4_op_putfh_Free(nfs_resop4 *resp)
{
	/* Nothing to be done */
}
