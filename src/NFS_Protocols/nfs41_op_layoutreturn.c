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
 * \file    nfs41_op_layoutreturn.c
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

/**
 * 
 * nfs41_op_layoutreturn: The NFS4_OP_LAYOUTRETURN operation. 
 *
 * This function implements the NFS4_OP_LAYOUTRETURN operation.
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

#define arg_LAYOUTRETURN4 op->nfs_argop4_u.oplayoutreturn
#define res_LAYOUTRETURN4 resp->nfs_resop4_u.oplayoutreturn

int nfs41_op_layoutreturn(struct nfs_argop4 *op, compound_data_t * data,
                          struct nfs_resop4 *resp)
{
  char __attribute__ ((__unused__)) funcname[] = "nfs41_op_layoutreturn";
  cache_inode_state_t *pstate_exists = NULL;
  fsal_status_t status;
  fsal_fsid_t basefsid;
  fsal_attrib_list_t attr;
  cache_inode_status_t cache_status;
  int rc;
  int i;

#if !defined(_USE_PNFS) && !defined(_USE_FSALMDS)
  res_LAYOUTRETURN4.lorr_status = NFS4ERR_NOTSUPP;
  return res_LAYOUTRETURN4.lorr_status;
#elsif defined(_USE_PNFS)
  res_LAYOUTRETURN4.lorr_status = NFS4_OK;
  return res_LAYOUTRETURN4.lorr_status;
#elsif defined(_USE_FSALMDS)
#if 0
  fsal_handle_t fsalh;
  struct lg_cbc cookie;
  /* If there is no FH */
  if(nfs4_Is_Fh_Empty(&(data->currentFH)))
    {
      res_LAYOUTRETURN4.lorr_status = NFS4ERR_NOFILEHANDLE;
      return res_LAYOUTRETURN4.lorr_status;
    }

  /* If the filehandle is invalid */
  if(nfs4_Is_Fh_Invalid(&(data->currentFH)))
    {
      res_LAYOUTRETURN4.lorr_status = NFS4ERR_BADHANDLE;
      return res_LAYOUTRETURN4.lorr_status;
    }

  /* Tests if the Filehandle is expired (for volatile filehandle) */
  if(nfs4_Is_Fh_Expired(&(data->currentFH)))
    {
      res_LAYOUTRETURN4.lorr_status = NFS4ERR_FHEXPIRED;
      return res_LAYOUTRETURN4.lorr_status;
    }

#ifdef _USE_FSALDS
  if(nfs4_Is_Fh_DSHandle(data->currentFH))
    {
      res_LAYOUTRETURN4.status = NFS4ERR_NOTSUPP;
      return res_LAYOUTRETURN4.status;
    }
#endif /* _USE_FSALDS */

  if(data->current_filetype != REGULAR_FILE)
    {
      /* Type of the entry is not correct */
      switch (data->current_filetype)
        {
        case DIR_BEGINNING:
        case DIR_CONTINUE:
          res_LAYOUTRETURN4.lorr_status = NFS4ERR_ISDIR;
          break;
        default:
          res_LAYOUTRETURN4.lorr_status = NFS4ERR_INVAL;
          break;
        }

      return res_LAYOUTRETURN4.lorr_status;
    }
  resp->resop = NFS4_OP_LAYOUTRETURN;
  /* For now, we let Ganesha be primarily responsible for managing
     state.  Rather than passing the arguments through directly, we
     look through the pstates and find the right ones then call the
     function. */

  nfs4_FhandletoFSAL(data->currentFH, &fsalh, data->pcontext);
  switch (arg_LAYOUTRETURN4.lora_layout_type)
    {
    case LAYOUTRETURN4_FILE:
      /* For now, assume a restart clears out the FSALs. */
      if (arg_LAYOUTRETURN4.lora_reclaim)
	{
	  res_LAYOUTRETURN4.lorr_status = NFS4_OK;
	  return res_LAYOUTRETURN4.lorr_status;
	}

      if((rc = nfs4_Check_Stateid(&(arg_LAYOUTRETURN4.lora_layoutreturn
				    .layoutreturn4_u.lr_layout.lrf_stateid),
				  data->current_entry,
				  data->psession->clientid)) != NFS4_OK)
	{
	  res_LAYOUTRETURN4.lorr_status = rc;
	  return res_LAYOUTRETURN4.lorr_status;
	}

      if(cache_inode_get_state((arg_LAYOUTRETURN4.lora_layoutreturn
				.layoutreturn4_u.lr_layout.lrf_stateid.other),
			       &pstate_exists,
			       data->pclient, &cache_status) != CACHE_INODE_SUCCESS)
	{
	  if(cache_status == CACHE_INODE_NOT_FOUND)
	    res_LAYOUTRETURN4.lorr_status = NFS4ERR_STALE_STATEID;
	  else
	    res_LAYOUTRETURN4.lorr_status = NFS4ERR_INVAL;
	  
	  return res_LAYOUTRETURN4.lorr_status;
	}

      /* Currently each file can have only one layout.  So we call the
	 FSAL and let it decide what to do if there's a match */
      
      if (pstate_exists->state_type != CACHE_INODE_STATE_LAYOUT)
	{
	  res_LAYOUTRETURN4.lorr_status = NFS4ERR_INVAL;
	  return res_LAYOUTRETURN4.lorr_status;
	}

      if (!((arg_LAYOUTRETURN4.lora_iomode == LAYOUTIOMODE4_ANY) ||
	    (arg_LAYOUTRETURN4.lora_iomode ==
	     pstate_exists->state_data.layout.iomode)))
	break;

      if (((arg_LAYOUTRETURN4.lora_layoutreturn
	    .layoutreturn4_u.lr_layout.lrf_offset) +
	   (arg_LAYOUTRETURN4.lora_layoutreturn
	    .layoutreturn4_u.lr_layout.lrf_length) <
	   pstate_exists->state_data.layout.offset) ||
	  ((arg_LAYOUTRETURN4.lora_layoutreturn
	    .layoutreturn4_u.lr_layout.lrf_offset) >
	   pstate_exists->state_data.layout.offset +
	   pstate_exists->state_data.layout.length))
	break;

      cookie.current_entry=data->current_entry;
      cookie.powner=pstate_exists->powner;
      cookie.pclient=data->pclient;
      cookie.pcontext=data->pcontext;
      cookie.data=data;
      cookie.passed_state=state_exists;
      
      status=FSAL_layoutreturn(&fsalh,
			       pstate_exists->state_data.layout.layout_type,
			       arg_LAYOUTRETURN4.lora_iomode,
			       (arg_LAYOUTRETURN4.lora_layoutreturn
				.layoutreturn4_u.lr_layout.lrf_offset),
			       (arg_LAYOUTRETURN4.lora_layoutreturn
				.layoutreturn4_u.lr_layout.lrf_length),
			       pstate_exists->state_data.layout.iomode,
			       pstate_exists->state_data.layout.offset,
			       pstate_exists->state_data.layout.length,
			       pstate_exists->state.data.layout.fsaldata,
			       data->pcontext);
      if (FSAL_IS_ERROR(status))
	{
	  res_LAYOUTRETURN4.lorr_status = status.major;
	  return res_LAYOUTRETURN4.lorr_status;
	}
      
      break;
    case LAYOUTRETURN4_FSID:
      /* If there is no FH */
      if(nfs4_Is_Fh_Empty(&(data->currentFH)))
	{
	  res_LAYOUTRETURN4.lorr_status = NFS4ERR_NOFILEHANDLE;
	  return res_LAYOUTRETURN4.lorr_status;
	}
      
      /* If the filehandle is invalid */
      if(nfs4_Is_Fh_Invalid(&(data->currentFH)))
	{
	  res_LAYOUTRETURN4.lorr_status = NFS4ERR_BADHANDLE;
	  return res_LAYOUTRETURN4.lorr_status;
	}
      
      /* Tests if the Filehandle is expired (for volatile filehandle) */
      if(nfs4_Is_Fh_Expired(&(data->currentFH)))
	{
	  res_LAYOUTRETURN4.lorr_status = NFS4ERR_FHEXPIRED;
	  return res_LAYOUTRETURN4.lorr_status;
	}
      
      attr.asked_attributes=FSAL_ATTR_FSID;
      if(cache_inode_getattr(data->current_entry,
			     &attr,
			     data->ht,
			     data->pclient,
			     data->pcontext, &cache_status)
	 != CACHE_INODE_SUCCESS)
	{
	  res_LAYOUTRETURN4.lorr_status = NFS4ERR_SERVERFAULT;
	  return res_LAYOUTRETURN4.lorr_status;
	}

      basefsid=attr.fsid;
      
    case LAYOUTRETURN4_ALL:
      if (arg_LAYOUTRETURN4.lora_reclaim)
	{
	  res_LAYOUTRETURN4.lorr_status = NFS4ERR_INVAL;
	  return res_LAYOUTRETURN4.lorr_status;
	}

      break;
    default:
      res_LAYOUTRETURN4.lorr_status = NFS4ERR_INVAL;
      return res_LAYOUTRETURN4.lorr_status;
    } 

#endif /* 0 */
  res_LAYOUTRETURN4.lorr_status = NFS4_OK;
  return res_LAYOUTRETURN4.lorr_status;
#endif                          /* _USE_FSALMDS */
}                               /* nfs41_op_layoutreturn */

/**
 * nfs41_op_layoutreturn_Free: frees what was allocared to handle nfs41_op_layoutreturn.
 * 
 * Frees what was allocared to handle nfs41_op_layoutreturn.
 *
 * @param resp  [INOUT]    Pointer to nfs4_op results
 *
 * @return nothing (void function )
 * 
 */
void nfs41_op_layoutreturn_Free(LOCK4res * resp)
{
  /* Nothing to Mem_Free */
  return;
}                               /* nfs41_op_layoutreturn_Free */

#ifdef _USE_FSALMDS

#if 0

#endif /* 0 */

/**
 * 
 * FSALBACK_layout_remove_state:
 * Callback from FSAL_layoutreturn to attempt to remove the layout
 * state for which FSAL_layoutreturn was called.
 *
 * @param opaque   [IN]    A pointer, opaque to the FSAL.
 * 
 * @return Zero on success, nonzero on failure.
 *
 *
 */

int FSALBACK_layout_remove_state(void* opaque)
{
  struct lg_cbc* cbc=(struct lg_cbc*) opaque;

  cache_inode_state_t* state = cbc->passed_state;
  cache_inode_status_t status;


  cache_inode_del_state(cbc->passed_state,
			     cbc->pclient,
			     &status);
  
  return (status != CACHE_INODE_SUCCESS);
}
#endif                          /* _USE_FSALMDS */
