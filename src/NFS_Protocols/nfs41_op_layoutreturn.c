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
#include "sal.h"

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
  bool_t nomore = FALSE;
  fsal_status_t fsal_status;
  cache_inode_status_t cache_status;
  stateid4 stateid;
  taggedstate state;
  int rc;
  uint64_t statecookie = 0;
  fsal_attrib_list_t attrs;
  fsal_fsid_t fsid;
  cache_entry_t* pentry;
  bool_t finished = FALSE;
  char __attribute__ ((__unused__)) funcname[] = "nfs41_op_layoutreturn";

  resp->resop = NFS4_OP_LAYOUTGET;
#ifdef _USE_FSALMDS
  switch (arg_LAYOUTRETURN4.lora_layoutreturn.lr_returntype)
    {
    case LAYOUTRETURN4_FILE:
      stateid =
	arg_LAYOUTRETURN4.lora_layoutreturn.layoutreturn4_u.lr_layout.lrf_stateid;
      if (stateid.seqid == 0)
	{
	  res_LAYOUTRETURN4.lorr_status = NFS4ERR_BAD_STATEID;
	  return res_LAYOUTRETURN4.lorr_status;
	}

      /* The client specifies a range, this may encompass several
	 granted layout segments or sub-segments */
      state_lock_filehandle(&(data->current_entry->object.file.handle),
			    writelock);
      fsal_status =
	FSAL_layoutreturn(&(data->current_entry->object.file.handle),
			  arg_LAYOUTRETURN4.lora_layout_type,
			  arg_LAYOUTRETURN4.lora_iomode,
			  arg_LAYOUTRETURN4.lora_layoutreturn.layoutreturn4_u.lr_layout.lrf_offset,
			  arg_LAYOUTRETURN4.lora_layoutreturn.layoutreturn4_u.lr_layout.lrf_length,
			  data->pcontext,
			  &nomore,
			  &stateid);
      state_unlock_filehandle(&(data->current_entry->object.file.handle));

      if (FSAL_IS_ERROR(fsal_status))
	{
	  res_LAYOUTRETURN4.lorr_status = fsal_status.major;
	  return res_LAYOUTRETURN4.lorr_status;
	}

      if (nomore)
	{
	  state_delete_layout_state(stateid);
	  data->currentstate = state_anonymous_stateid;
	  res_LAYOUTRETURN4.LAYOUTRETURN4res_u.lorr_stateid.lrs_present
	    = 0;
	}
      else
	{
	  data->currentstate = stateid;
	  res_LAYOUTRETURN4.LAYOUTRETURN4res_u.lorr_stateid.lrs_present
	    = 1;
	  res_LAYOUTRETURN4.LAYOUTRETURN4res_u.lorr_stateid.layoutreturn_stateid_u.lrs_stateid
	    = stateid;
	}
      res_LAYOUTRETURN4.lorr_status = NFS4_OK;
      return res_LAYOUTRETURN4.lorr_status;
      break;

    case LAYOUTRETURN4_FSID:
      memset(&attrs, 0, sizeof(fsal_attrib_list_t));
      attrs.asked_attributes |= FSAL_ATTR_FSID;
      cache_status = cache_inode_getattr(data->current_entry,
					 &attrs,
					 data->ht,
					 data->pclient,
					 data->pcontext,
					 &cache_status);
      if (cache_status != CACHE_INODE_SUCCESS)
	{
	  res_LAYOUTRETURN4.lorr_status = nfs4_Errno(cache_status);
	  return res_LAYOUTRETURN4.lorr_status;
	}
      fsid = attrs.fsid;
    case LAYOUTRETURN4_ALL:
      do
	{
	  rc = state_iterate_by_clientid(data->psession->clientid,
					 STATE_LAYOUT, &statecookie,
					 &finished, &state);
	  if (rc == ERR_STATE_NOENT)
	    break;
	  else if (rc != ERR_STATE_NO_ERROR)
	    {
	      res_LAYOUTRETURN4.lorr_status = staterr2nfs4err(rc);
	      return res_LAYOUTRETURN4.lorr_status;
	    }
	  
	  if (state.u.layout.type
	      != arg_LAYOUTRETURN4.lora_layout_type)
	    continue;

	  if (arg_LAYOUTRETURN4.lora_layoutreturn.lr_returntype
		   == LAYOUTRETURN4_FSID)
	    {
	      cache_inode_fsal_data_t fsdata;

	      fsdata.handle = state.u.layout.handle;
	      fsdata.cookie = 0;
	      memset(&attrs, 0, sizeof(fsal_attrib_list_t));
	      attrs.asked_attributes |= FSAL_ATTR_FSID;

	      pentry = cache_inode_get(&fsdata, &attrs,
					     data->ht, data->pclient,
					     data->pcontext,
					     &cache_status);
	      if (cache_status != CACHE_INODE_SUCCESS)
		{
		  res_LAYOUTRETURN4.lorr_status =
		    nfs4_Errno(cache_status);
		  return res_LAYOUTRETURN4.lorr_status;
		}
	      if (memcmp(&fsid, &(attrs.fsid), sizeof(fsal_fsid_t)))
		continue;
	    }

	  stateid = state.u.layout.stateid;
	  state_lock_filehandle(&(state.u.layout.handle),
				writelock);
	  fsal_status =
	    FSAL_layoutreturn(&(state.u.layout.handle),
			      arg_LAYOUTRETURN4.lora_layout_type,
			      arg_LAYOUTRETURN4.lora_iomode,
			      0,
			      NFS4_UINT64_MAX,
			      data->pcontext,
			      &nomore,
			      &stateid);
	  if (FSAL_IS_ERROR(fsal_status))
	    {
	      res_LAYOUTRETURN4.lorr_status = fsal_status.major;
	      return res_LAYOUTRETURN4.lorr_status;
	    }
	  state_delete_layout_state(stateid);
	  state_unlock_filehandle(&(data->current_entry->object.file.handle));
	} while (!finished);
      data->currentstate = state_anonymous_stateid;
      res_LAYOUTRETURN4.LAYOUTRETURN4res_u.lorr_stateid.lrs_present = 0;
      break;

    default:
      res_LAYOUTRETURN4.lorr_status = NFS4ERR_INVAL;
      return res_LAYOUTRETURN4.lorr_status;
    }
#endif

  res_LAYOUTRETURN4.lorr_status = NFS4_OK;
  return res_LAYOUTRETURN4.lorr_status;
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

