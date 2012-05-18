/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright CEA/DAM/DIF  (2008)
 * contributeur : Philippe DENIEL   philippe.deniel@cea.fr
 *                Thomas LEIBOVICI  thomas.leibovici@cea.fr
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * ---------------------------------------*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef _SOLARIS
#include "solaris_port.h"
#endif
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/file.h>           /* for having FNDELAY */
#include "HashData.h"
#include "HashTable.h"
#include "log.h"
#include "ganesha_rpc.h"
#include "nfs23.h"
#include "nfs4.h"
#include "mount.h"
#include "nfs_core.h"
#include "cache_inode.h"
#include "nfs_exports.h"
#include "nfs_creds.h"
#include "nfs_proto_functions.h"
#include "nfs_proto_tools.h"
#include "nfs_file_handle.h"
#include "nfs_tools.h"

/**
 *
 *	nfs4_op_getfh: The NFS4_OP_GETFH operation
 *
 * Gets the currentFH for the current compound requests.
 * This operation returns the current FH in the reply structure.
 *
 * @param op    [IN]    pointer to nfs4_op arguments
 * @param data  [INOUT] Pointer to the compound request's data
 * @param resp  [IN]    Pointer to nfs4_op results
 *
 * @return NFS4_OK if successfull, other values show an error.
 *
 * @see all the nfs4_op_<*> function
 * @see nfs4_Compound
 *
 */

#define arg_GETFG op->nfs_argop4_u.opgetfh
#define res_GETFH resp->nfs_resop4_u.opgetfh

int nfs4_op_getfh(struct nfs_argop4 *op, compound_data_t * data, struct nfs_resop4 *resp)
{
  char __attribute__ ((__unused__)) funcname[] = "nfs4_op_getfh";

  resp->resop = NFS4_OP_GETFH;
  res_GETFH.status = NFS4_OK;

  LogHandleNFS4("NFS4 GETFH BEFORE: %s", &data->currentFH);

  /* Do basic checks on a filehandle */
  res_GETFH.status = nfs4_sanity_check_FH(data, 0LL);
  if(res_GETFH.status != NFS4_OK)
    return res_GETFH.status;

  /* Test if the filehandle is related to a referral */
  if(nfs4_Is_Fh_Referral(&(data->currentFH)))
    {
      res_GETFH.status = NFS4ERR_MOVED;
      return res_GETFH.status;
    }


  /* Copy the filehandle to the reply structure */
  res_GETFH.status = nfs4_AllocateFH(&(res_GETFH.GETFH4res_u.resok4.object));
  if(res_GETFH.status != NFS4_OK)
    return res_GETFH.status;

  /* Put the data in place */
  res_GETFH.GETFH4res_u.resok4.object.nfs_fh4_len = data->currentFH.nfs_fh4_len;
  memcpy(res_GETFH.GETFH4res_u.resok4.object.nfs_fh4_val,
         data->currentFH.nfs_fh4_val, data->currentFH.nfs_fh4_len);

  LogHandleNFS4("NFS4 GETFH AFTER: %s", &res_GETFH.GETFH4res_u.resok4.object);

  return NFS4_OK;
}                               /* nfs4_op_getfh */

/**
 * nfs4_op_getfh_Free: frees what was allocared to handle nfs4_op_getfh.
 *
 * Frees what was allocared to handle nfs4_op_getfh.
 *
 * @param resp  [INOUT]    Pointer to nfs4_op results
 *
 * @return nothing (void function )
 *
 */
void nfs4_op_getfh_Free(GETFH4res * resp)
{
  if(resp->status == NFS4_OK)
    gsh_free(resp->GETFH4res_u.resok4.object.nfs_fh4_val);
  return;
}                               /* nfs4_op_getfh_Free */
