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
 * \file    nfs4_op_open_confirm.c
 * \author  $Author: deniel $
 * \date    $Date: 2005/11/28 17:02:51 $
 * \version $Revision: 1.8 $
 * \brief   Routines used for managing the NFS4 COMPOUND functions.
 *
 * nfs4_op_open_confirm.c : Routines used for managing the NFS4 COMPOUND functions.
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
#include "nfs_tools.h"
#include "nfs_file_handle.h"

/**
 * nfs4_op_open_confirm: The NFS4_OP_OPEN_CONFIRM
 * 
 * Implements the NFS4_OP_OPEN_CONFIRM
 *
 * @param op    [IN]    pointer to nfs4_op arguments
 * @param data  [INOUT] Pointer to the compound request's data
 * @param resp  [IN]    Pointer to nfs4_op results
 * 
 * @return NFS4_OK if ok, any other value show an error.
 *
 */
#define arg_OPEN_CONFIRM4 op->nfs_argop4_u.opopen_confirm
#define res_OPEN_CONFIRM4 resp->nfs_resop4_u.opopen_confirm

int nfs4_op_open_confirm(struct nfs_argop4 *op,
                         compound_data_t * data, struct nfs_resop4 *resp)
{
  char __attribute__ ((__unused__)) funcname[] = "nfs4_op_open_confirm";
  int rc = 0;
  cache_inode_status_t cache_status;

  resp->resop = NFS4_OP_OPEN_CONFIRM;
  res_OPEN_CONFIRM4.status = NFS4_OK;

#ifdef _USE_FSALDS
  if(nfs4_Is_Fh_DSHandle(&data->currentFH))
    {
      res_OPEN_CONFIRM4.status = NFS4ERR_NOTSUPP;
      return res_OPEN_CONFIRM4.status;
    }
#endif /* _USE_FSALDS */

  /* If there is no FH */
  if(nfs4_Is_Fh_Empty(&(data->currentFH)))
    {
      res_OPEN_CONFIRM4.status = NFS4ERR_NOFILEHANDLE;
      return res_OPEN_CONFIRM4.status;
    }

  /* If the filehandle is invalid */
  if(nfs4_Is_Fh_Invalid(&(data->currentFH)))
    {
      res_OPEN_CONFIRM4.status = NFS4ERR_BADHANDLE;
      return res_OPEN_CONFIRM4.status;
    }

  /* Tests if the Filehandle is expired (for volatile filehandle) */
  if(nfs4_Is_Fh_Expired(&(data->currentFH)))
    {
      res_OPEN_CONFIRM4.status = NFS4ERR_FHEXPIRED;
      return res_OPEN_CONFIRM4.status;
    }

  /* Should not operate on non-file objects */
  if(data->current_entry->internal_md.type != REGULAR_FILE)
    {
      switch (data->current_entry->internal_md.type)
        {
        case DIR_BEGINNING:
        case DIR_CONTINUE:
          res_OPEN_CONFIRM4.status = NFS4ERR_ISDIR;
          return res_OPEN_CONFIRM4.status;
          break;
        default:
          res_OPEN_CONFIRM4.status = NFS4ERR_INVAL;
          return res_OPEN_CONFIRM4.status;
          break;

        }
    }


  return res_OPEN_CONFIRM4.status;
}                               /* nfs4_op_open_confirm */

/**
 * nfs4_op_open_confirm_Free: frees what was allocared to handle nfs4_op_open_confirm.
 * 
 * Frees what was allocared to handle nfs4_op_open_confirm.
 *
 * @param resp  [INOUT]    Pointer to nfs4_op results
 *
 * @return nothing (void function )
 * 
 */
void nfs4_op_open_confirm_Free(OPEN_CONFIRM4res * resp)
{
  /* Nothing to be done */
  return;
}                               /* nfs4_op_open_confirm_Free */
