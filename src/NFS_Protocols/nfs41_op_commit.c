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
 * \file    nfs4_op_commit.c
 * \author  $Author: deniel $
 * \date    $Date: 2005/11/28 17:02:50 $
 * \version $Revision: 1.10 $
 * \brief   Routines used for managing the NFS4 COMPOUND functions.
 *
 * nfs4_op_commit.c : Routines used for managing the NFS4 COMPOUND functions.
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
 *
 * nfs4_op_commit: Implemtation of NFS4_OP_COMMIT
 * 
 * Implemtation of NFS4_OP_COMMIT. This is usually made for cache validator implementation.
 *
 * @param op    [IN]    pointer to nfs4_op arguments
 * @param data  [INOUT] Pointer to the compound request's data
 * @param resp  [IN]    Pointer to nfs4_op results
 * 
 * @return NFS4_OK 
 * 
 */

extern verifier4 NFS4_write_verifier;   /* NFS V4 write verifier */

#define arg_COMMIT4 op->nfs_argop4_u.opcommit
#define res_COMMIT4 resp->nfs_resop4_u.opcommit

int nfs41_op_commit(struct nfs_argop4 *op, compound_data_t * data, struct nfs_resop4 *resp)
{
  char __attribute__ ((__unused__)) funcname[] = "nfs4_op_commit";

  fsal_attrib_list_t attr;
  cache_inode_status_t cache_status;

#ifdef _USE_FSALMDS
  fsal_status_t status;
  fsal_handle_t fsalh;
#endif /* _USE_FSALMDS */

  /* for the moment, read/write are not done asynchronously, no commit is necessary */
  resp->resop = NFS4_OP_COMMIT;
  res_COMMIT4.status = NFS4_OK;

  /* If there is no FH */
  if(nfs4_Is_Fh_Empty(&(data->currentFH)))
    {
      res_COMMIT4.status = NFS4ERR_NOFILEHANDLE;
      return res_COMMIT4.status;
    }

  /* If the filehandle is invalid */
  if(nfs4_Is_Fh_Invalid(&(data->currentFH)))
    {
      res_COMMIT4.status = NFS4ERR_BADHANDLE;
      return res_COMMIT4.status;
    }

  /* Tests if the Filehandle is expired (for volatile filehandle) */
  if(nfs4_Is_Fh_Expired(&(data->currentFH)))
    {
      res_COMMIT4.status = NFS4ERR_FHEXPIRED;
      return res_COMMIT4.status;
    }

  /* Commit is done only on a file */
  if(data->current_filetype != REGULAR_FILE)
    {
      /* Type of the entry is not correct */
      switch (data->current_filetype)
        {
        case DIR_BEGINNING:
        case DIR_CONTINUE:
          res_COMMIT4.status = NFS4ERR_ISDIR;
          break;
        default:
          res_COMMIT4.status = NFS4ERR_INVAL;
          break;
        }

      /* Exit with an error */
      return res_COMMIT4.status;
    }

#ifdef _USE_FSALMDS

  /* See if we have a layout */
  
#endif /* _USE_FSALMDS */
#ifdef _USE_FSALDS

  if(nfs4_Is_Fh_DSHandle(&data->currentFH))
    {
      return(op_dscommit(op, data, resp));
    }

#endif /* _USE_FSALDS */
  

  if(cache_inode_commit(data->current_entry,
                        arg_COMMIT4.offset,
                        arg_COMMIT4.count,
                        &attr,
                        data->ht,
                        data->pclient,
                        data->pcontext, 
			FSAL_UNSAFE_WRITE_TO_FS_BUFFER, /** @todo I had to add this, just for compiling */
			&cache_status) != CACHE_INODE_SUCCESS)
    {
      res_COMMIT4.status = NFS4ERR_INVAL;
      return res_COMMIT4.status;
    }

  memcpy(res_COMMIT4.COMMIT4res_u.resok4.writeverf, (char *)&NFS4_write_verifier,
         NFS4_VERIFIER_SIZE);

  /* If you reach this point, then an error occured */
  res_COMMIT4.status = NFS4_OK;
  return res_COMMIT4.status;
}                               /* nfs4_op_commit */

/**
 * nfs41_op_commit_Free: frees what was allocared to handle nfs4_op_commit.
 * 
 * Frees what was allocared to handle nfs4_op_commit.
 *
 * @param resp  [INOUT]    Pointer to nfs4_op results
 *
 * @return nothing (void function )
 * 
 */
void nfs41_op_commit_Free(COMMIT4res * resp)
{
  /* Nothing to be done */
  return;
}                               /* nfs4_op_commit_Free */

#ifdef _USE_FSALDS

int op_dscommit(struct nfs_argop4 *op,
		compound_data_t * data,
		struct nfs_resop4 *resp)

{
  fsal_off_t offset;
  fsal_handle_t fsalh;
  fsal_size_t length;
  fsal_status_t status;
  cache_inode_status_t cache_status;

  /* Special stateids are not permitted, nor is any non-zero seqid, by
     RFC 5661, 13.9.1, pp. 329-330 */

  if(data->current_filetype != REGULAR_FILE)
    {
      /* If the source is no file, return EISDIR if it is a directory and EINAVL otherwise */
      if(data->current_filetype == DIR_BEGINNING
         || data->current_filetype == DIR_CONTINUE)
        res_COMMIT4.status = NFS4ERR_ISDIR;
      else
        res_COMMIT4.status = NFS4ERR_INVAL;

      return res_COMMIT4.status;
    }

  /* Get the size and offset of the read operation */
  offset = arg_COMMIT4.offset;
  length = arg_COMMIT4.count;

  /* If size == 0 , no I/O is to be made and everything is alright */
  if(length == 0)
    {
      memcpy(res_COMMIT4.COMMIT4res_u.resok4.writeverf, (char *)&NFS4_write_verifier,
	     NFS4_VERIFIER_SIZE);

      res_COMMIT4.status = NFS4_OK;
    }

  /* Magical nonexistent state management */

  nfs4_FhandleToFSAL(&data->currentFH, &fsalh, data->pcontext);

  /* This is subject to change, once the cache happens */

  status=FSAL_ds_commit(&fsalh, offset, length);

  if (cache_inode_error_convert(status) != CACHE_INODE_SUCCESS)
    {
      res_COMMIT4.status = nfs4_Errno(cache_status);
      return res_COMMIT4.status;
    }

  memcpy(res_COMMIT4.COMMIT4res_u.resok4.writeverf, (char *)&NFS4_write_verifier,
         NFS4_VERIFIER_SIZE);

  /* If you reach this point, then an error occured */
  res_COMMIT4.status = NFS4_OK;
  return res_COMMIT4.status;
}

#endif /* _USE_FSALDS */
