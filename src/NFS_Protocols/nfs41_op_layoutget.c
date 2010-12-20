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
 * \file    nfs41_op_layoutget.c
 * \author  $Author: deniel $
 * \date    $Date: 2005/11/28 17:02:50 $
 * \version $Revision: 1.8 $
 * \brief   Routines used for managing the NFS4 COMPOUND functions.
 *
 * nfs41_op_lock.c : Routines used for managing the NFS4 COMPOUND functions.
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
#ifdef _USE_FSALMDS
#include "fsal.h"
#include "sal.h"
#include "layouttypes/layouts.h"
#endif                                          /* _USE_FSALMDS */


/**
 * 
 * nfs41_op_layoutget: The NFS4_OP_LAYOUTGET operation. 
 *
 * This function implements the NFS4_OP_LAYOUTGET operation.
 *
 * @param op    [IN]    pointer to nfs4_op arguments
 * @param data  [INOUT] Pointer to the compound request's data
 * @param resp  [IN]    Pointer to nfs4_op results
 * 
 * @return NFS4_OK if successfull, other values show an error. 
 *
 * @see all the nfs41_op_<*> function
 * @see nfs4_Compound
 *
 */

#define arg_LAYOUTGET4 op->nfs_argop4_u.oplayoutget
#define res_LAYOUTGET4 resp->nfs_resop4_u.oplayoutget

int nfs41_op_layoutget(struct nfs_argop4 *op, compound_data_t * data,
                       struct nfs_resop4 *resp)
{
  int rc;

  char __attribute__ ((__unused__)) funcname[] = "nfs41_op_layoutget";

#if !defined(_USE_PNFS) && !defined(_USE_FSALMDS)
  resp->resop = NFS4_OP_LAYOUTGET;
  res_LAYOUTGET4.logr_status = NFS4ERR_NOTSUPP;
  return res_LAYOUTGET4.logr_status;
#else
  cache_inode_status_t cache_status;

  /* Lock are not supported */
  resp->resop = NFS4_OP_LAYOUTGET;

  /* If there is no FH */
  if(nfs4_Is_Fh_Empty(&(data->currentFH)))
    {
      res_LAYOUTGET4.logr_status = NFS4ERR_NOFILEHANDLE;
      return res_LAYOUTGET4.logr_status;
    }

  /* If the filehandle is invalid */
  if(nfs4_Is_Fh_Invalid(&(data->currentFH)))
    {
      res_LAYOUTGET4.logr_status = NFS4ERR_BADHANDLE;
      return res_LAYOUTGET4.logr_status;
    }

  /* Tests if the Filehandle is expired (for volatile filehandle) */
  if(nfs4_Is_Fh_Expired(&(data->currentFH)))
    {
      res_LAYOUTGET4.logr_status = NFS4ERR_FHEXPIRED;
      return res_LAYOUTGET4.logr_status;
    }

#ifdef _USE_FSALDS
  if(nfs4_Is_Fh_DSHandle(&data->currentFH))
    {
      res_LAYOUTGET4.logr_status = NFS4ERR_NOTSUPP;
      return res_LAYOUTGET4.logr_status;
    }
#endif /* _USE_FSALDS */

  /* Layouts are only granted on files */
  if(data->current_filetype != REGULAR_FILE)
    {
      /* Type of the entry is not correct */
      switch (data->current_filetype)
        {
        case DIR_BEGINNING:
        case DIR_CONTINUE:
          res_LAYOUTGET4.logr_status = NFS4ERR_ISDIR;
          break;
        default:
          res_LAYOUTGET4.logr_status = NFS4ERR_INVAL;
          break;
        }

      return res_LAYOUTGET4.logr_status;
    }

  /* Parameters's consistency */
  if(arg_LAYOUTGET4.loga_length < arg_LAYOUTGET4.loga_minlength)
    {
      res_LAYOUTGET4.logr_status = NFS4ERR_INVAL;
      return res_LAYOUTGET4.logr_status;
    

    }
#if defined(_USE_PNFS)
  char *buff = NULL;
  unsigned int lenbuff = 0;

  if((buff = Mem_Alloc(1024)) == NULL)
    {
      res_LAYOUTGET4.logr_status = NFS4ERR_SERVERFAULT;
      return res_LAYOUTGET4.logr_status;
    }

  /* Add a pstate */
  candidate_type = CACHE_INODE_STATE_LAYOUT;
  candidate_data.layout.layout_type = arg_LAYOUTGET4.loga_layout_type;
  candidate_data.layout.iomode = arg_LAYOUTGET4.loga_iomode;
  candidate_data.layout.offset = arg_LAYOUTGET4.loga_offset;
  candidate_data.layout.length = arg_LAYOUTGET4.loga_length;
  candidate_data.layout.minlength = arg_LAYOUTGET4.loga_minlength;

  /* Add the lock state to the lock table */
  if(cache_inode_add_state(data->current_entry,
                           candidate_type,
                           &candidate_data,
                           pstate_exists->powner,
                           data->pclient,
                           data->pcontext,
                           &file_state, &cache_status) != CACHE_INODE_SUCCESS)
    {
      res_LAYOUTGET4.logr_status = NFS4ERR_STALE_STATEID;
      return res_LAYOUTGET4.logr_status;
    }

  /* set the returned status */

  /* No return on close for the moment */
  res_LAYOUTGET4.LAYOUTGET4res_u.logr_resok4.logr_return_on_close = FALSE;

  /* Manages the stateid */
  res_LAYOUTGET4.LAYOUTGET4res_u.logr_resok4.logr_stateid.seqid = 1;
  memcpy(res_LAYOUTGET4.LAYOUTGET4res_u.logr_resok4.logr_stateid.other,
         arg_LAYOUTGET4.loga_stateid.other, 12);
  //file_state->stateid_other, 12);

  /* Now the layout specific information */
  res_LAYOUTGET4.LAYOUTGET4res_u.logr_resok4.logr_layout.logr_layout_len = 1;  /** @todo manages more than one segment */
  res_LAYOUTGET4.LAYOUTGET4res_u.logr_resok4.logr_layout.logr_layout_val =
      (layout4 *) Mem_Alloc(sizeof(layout4));

  res_LAYOUTGET4.LAYOUTGET4res_u.logr_resok4.logr_layout.logr_layout_val[0].lo_offset =
      arg_LAYOUTGET4.loga_offset;
  res_LAYOUTGET4.LAYOUTGET4res_u.logr_resok4.logr_layout.logr_layout_val[0].lo_length = 0xFFFFFFFFFFFFFFFFLL;   /* Whole file */
  res_LAYOUTGET4.LAYOUTGET4res_u.logr_resok4.logr_layout.logr_layout_val[0].lo_iomode =
      arg_LAYOUTGET4.loga_iomode;
  res_LAYOUTGET4.LAYOUTGET4res_u.logr_resok4.logr_layout.logr_layout_val[0].
      lo_content.loc_type = LAYOUT4_NFSV4_1_FILES;

  pnfs_encode_layoutget( &data->current_entry->object.file.pnfs_file,
                         buff,
                         &lenbuff);

  res_LAYOUTGET4.LAYOUTGET4res_u.logr_resok4.logr_layout.logr_layout_val[0].
      lo_content.loc_body.loc_body_len =
      lenbuff,
      res_LAYOUTGET4.LAYOUTGET4res_u.logr_resok4.logr_layout.logr_layout_val[0].
      lo_content.loc_body.loc_body_val = buff;

  res_LAYOUTGET4.logr_status = NFS4_OK;
  return res_LAYOUTGET4.logr_status;
#elif defined(_USE_FSALMDS)
  /* set the returned status */

  fsal_boolean_t return_on_close;
  fsal_status_t status;
  fsal_layout_t *layouts;
  int numlayouts;
  layoutiomode4 iomode = arg_LAYOUTGET4.loga_iomode;
  offset4 offset = arg_LAYOUTGET4.loga_offset;
  length4 length = arg_LAYOUTGET4.loga_length;
  fsal_handle_t* fsalh;
  int i;
  stateid4 lstateid;
  stateid4* ostateid;

  fsalh = &(data->current_entry->object.file.handle);

  rc = state_create_layout_state(fsalh,
				 arg_LAYOUTGET4.loga_stateid,
				 data->psession->clientid,
				 arg_LAYOUTGET4.loga_layout_type,
				 &lstateid);

  if (rc != ERR_STATE_NO_ERROR)
    {
      res_LAYOUTGET4.logr_status = staterr2nfs4err(rc);
      return res_LAYOUTGET4.logr_status;
    }

  ostateid = (!memcmp(&arg_LAYOUTGET4.loga_stateid, &lstateid,
		      sizeof(stateid4))) ?
    NULL :
    &arg_LAYOUTGET4.loga_stateid;
    

  status = FSAL_layoutget(fsalh,
			  arg_LAYOUTGET4.loga_layout_type,
			  iomode,
			  offset,
			  length,
			  arg_LAYOUTGET4.loga_minlength,
			  &layouts,
			  &numlayouts,
			  &return_on_close,
			  data->pcontext,
			  &lstateid,
			  ostateid,
			  (void*) data);

  if (FSAL_IS_ERROR(status))
    {
      /* if a new layout state was created, delete it */
      if (lstateid.seqid == 0)
	state_delete_layout_state(lstateid);

      res_LAYOUTGET4.logr_status = status.major;
      return res_LAYOUTGET4.logr_status;
    }

  res_LAYOUTGET4.LAYOUTGET4res_u.logr_resok4.logr_return_on_close =
    return_on_close;

  res_LAYOUTGET4.LAYOUTGET4res_u.logr_resok4.logr_stateid = lstateid;

  /* Now the layout specific information */
  res_LAYOUTGET4.LAYOUTGET4res_u.logr_resok4.logr_layout.logr_layout_len
    = numlayouts;
  res_LAYOUTGET4.LAYOUTGET4res_u.logr_resok4.logr_layout.logr_layout_val
    = layouts;

  res_LAYOUTGET4.logr_status = NFS4_OK;
  return res_LAYOUTGET4.logr_status;
#endif                          /* _USE_FSALMDS */
#endif
}                               /* nfs41_op_layoutget */

/**
 * nfs41_op_layoutget_Free: frees what was allocared to handle nfs41_op_layoutget.
 * 
 * Frees what was allocared to handle nfs41_op_layoutget.
 *
 * @param resp  [INOUT]    Pointer to nfs4_op results
 *
 * @return nothing (void function )
 * 
 */
void nfs41_op_layoutget_Free(LAYOUTGET4res * resp)
{
#ifdef _USE_PNFS 
  if(resp->logr_status == NFS4_OK)
    {
      if(resp->LAYOUTGET4res_u.logr_resok4.logr_layout.logr_layout_val[0].lo_content.
         loc_body.loc_body_val != NULL)
        Mem_Free((char *)resp->LAYOUTGET4res_u.logr_resok4.logr_layout.
                 logr_layout_val[0].lo_content.loc_body.loc_body_val);
    }

  return;
#elif defined(_USE_FSALMDS)
  if (resp->logr_status == NFS4_OK)
    Mem_Free(resp->LAYOUTGET4res_u.logr_resok4.logr_layout.logr_layout_val);
#endif                          /* _USE_FSALMDS */
}                               /* nfs41_op_layoutget_Free */

#ifdef _USE_FSALMDS

#endif                          /* _USE_FSALMDS */
