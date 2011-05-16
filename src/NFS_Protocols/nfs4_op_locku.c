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
 * ---------------------------------------
 */

/**
 * \file    nfs4_op_locku.c
 * \author  $Author: deniel $
 * \date    $Date: 2005/11/28 17:02:50 $
 * \version $Revision: 1.8 $
 * \brief   Routines used for managing the NFS4 COMPOUND functions.
 *
 * nfs4_op_locku.c : Routines used for managing the NFS4 COMPOUND functions.
 *
 *
 */
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
#ifdef _USE_GSSRPC
#include <gssrpc/types.h>
#include <gssrpc/rpc.h>
#include <gssrpc/auth.h>
#include <gssrpc/pmap_clnt.h>
#else
#include <rpc/types.h>
#include <rpc/rpc.h>
#include <rpc/auth.h>
#include <rpc/pmap_clnt.h>
#endif

#include "log_macros.h"
#include "stuff_alloc.h"
#include "nfs23.h"
#include "nfs4.h"
#include "mount.h"
#include "nfs_core.h"
#include "cache_inode.h"
#include "cache_content.h"
#include "nfs_exports.h"
#include "nfs_creds.h"
#include "nfs_proto_functions.h"
#include "nfs_file_handle.h"
#include "nfs_tools.h"
#include "sal.h"

/**
 * 
 * nfs4_op_locku: The NFS4_OP_LOCKU operation. 
 *
 * This function implements the NFS4_OP_LOCKU operation.
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

#define arg_LOCKU4 op->nfs_argop4_u.oplocku
#define res_LOCKU4 resp->nfs_resop4_u.oplocku

int nfs4_op_locku(struct nfs_argop4 *op, compound_data_t * data, struct nfs_resop4 *resp)
{
  char __attribute__ ((__unused__)) funcname[] = "nfs4_op_locku";
  cache_inode_status_t cache_status;
  unsigned int rc = 0;
  state_lock_trans_t* transaction;

  /* Lock are not supported */
  resp->resop = NFS4_OP_LOCKU;

  /* If there is no FH */
  if(nfs4_Is_Fh_Empty(&(data->currentFH)))
    {
      res_LOCKU4.status = NFS4ERR_NOFILEHANDLE;
      return res_LOCKU4.status;
    }

  /* If the filehandle is invalid */
  if(nfs4_Is_Fh_Invalid(&(data->currentFH)))
    {
      res_LOCKU4.status = NFS4ERR_BADHANDLE;
      return res_LOCKU4.status;
    }

  /* Tests if the Filehandle is expired (for volatile filehandle) */
  if(nfs4_Is_Fh_Expired(&(data->currentFH)))
    {
      res_LOCKU4.status = NFS4ERR_FHEXPIRED;
      return res_LOCKU4.status;
    }

  /* Commit is done only on a file */
  if(data->current_filetype != REGULAR_FILE)
    {
      /* Type of the entry is not correct */
      switch (data->current_filetype)
        {
        case DIR_BEGINNING:
        case DIR_CONTINUE:
          res_LOCKU4.status = NFS4ERR_ISDIR;
          break;
        default:
          res_LOCKU4.status = NFS4ERR_INVAL;
          break;
        }
    }

  /* Lock length should not be 0 */
  if(arg_LOCKU4.length == 0LL)
    {
      res_LOCKU4.status = NFS4ERR_INVAL;
      return res_LOCKU4.status;
    }

  /* Check for range overflow 
   * Remember that a length with all bits set to 1 means "lock until the end of file" (RFC3530, page 157) */
  if(arg_LOCKU4.length != 0xffffffffffffffffLL)
    {
      /* Comparing beyond 2^64 is not possible int 64 bits precision, but off+len > 2^64 is equivalent to len > 2^64 - off */
      if(arg_LOCKU4.length > (0xffffffffffffffffLL - arg_LOCKU4.offset))
        {
          res_LOCKU4.status = NFS4ERR_INVAL;
          return res_LOCKU4.status;
        }
    }

  if (data->minorversion == 0)
    {
      res_LOCKU4.status = NFS4ERR_NOTSUPP;
      return res_LOCKU4.status;
    }

  if ((rc = state_exist_lock_owner_begin41(&(data->current_entry->
					     object.file.handle),
					   data->psession->clientid,
					   (arg_LOCKU4.lock_stateid),
					   &transaction))
      != ERR_STATE_NO_ERROR)
    {
      res_LOCKU4.status = staterr2nfs4err(rc);
      return res_LOCKU4.status;
    }

  state_unlock(transaction, arg_LOCKU4.offset, arg_LOCKU4.length);

  if ((rc = state_lock_commit(transaction)) != ERR_STATE_NO_ERROR)
    {
      state_lock_get_nfs4err(transaction, &res_LOCKU4.status);
    }
  else
    {
      state_lock_get_stateid(transaction, &(res_LOCKU4.LOCKU4res_u.
					    lock_stateid));
      res_LOCKU4.status = NFS4_OK;
    }

  state_lock_dispose_transaction(transaction);
  return res_LOCKU4.status;
}                               /* nfs4_op_locku */

/**
 * nfs4_op_locku_Free: frees what was allocared to handle nfs4_op_locku.
 * 
 * Frees what was allocared to handle nfs4_op_locku.
 *
 * @param resp  [INOUT]    Pointer to nfs4_op results
 *
 * @return nothing (void function )
 * 
 */
void nfs4_op_locku_Free(LOCKU4res * resp)
{
  /* Nothing to Mem_Free */
  return;
}                               /* nfs4_op_locku_Free */
