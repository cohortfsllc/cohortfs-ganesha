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
 * \file    nfs41_op_layoutcommit.c
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
 * nfs41_op_layoutcommit: The NFS4_OP_LAYOUTCOMMIT operation. 
 *
 * This function implements the NFS4_OP_LAYOUTCOMMIT operation.
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

#define arg_LAYOUTCOMMIT4 op->nfs_argop4_u.oplayoutcommit
#define res_LAYOUTCOMMIT4 resp->nfs_resop4_u.oplayoutcommit

int nfs41_op_layoutcommit(struct nfs_argop4 *op, compound_data_t * data,
                          struct nfs_resop4 *resp)
{
  cache_inode_status_t cache_status;
  fsal_attrib_list_t fsal_attr;

  char __attribute__ ((__unused__)) funcname[] = "nfs41_op_layoutcommit";
  resp->resop = NFS4_OP_LAYOUTCOMMIT;

#if !defined(_USE_PNFS) && !defined(_USE_FSALMDS)
  res_LAYOUTCOMMIT4.locr_status = NFS4ERR_NOTSUPP;
  return res_LAYOUTCOMMIT4.locr_status;
#endif
  /* If there is no FH */
  if(nfs4_Is_Fh_Empty(&(data->currentFH)))
    {
      res_LAYOUTCOMMIT4.locr_status = NFS4ERR_NOFILEHANDLE;
      return res_LAYOUTCOMMIT4.locr_status;
    }

  /* If the filehandle is invalid */
  if(nfs4_Is_Fh_Invalid(&(data->currentFH)))
    {
      res_LAYOUTCOMMIT4.locr_status = NFS4ERR_BADHANDLE;
      return res_LAYOUTCOMMIT4.locr_status;
    }

  /* Tests if the Filehandle is expired (for volatile filehandle) */
  if(nfs4_Is_Fh_Expired(&(data->currentFH)))
    {
      res_LAYOUTCOMMIT4.locr_status = NFS4ERR_FHEXPIRED;
      return res_LAYOUTCOMMIT4.locr_status;
    }

#ifdef _USE_FSALDS
  if(nfs4_Is_Fh_DSHandle(&data->currentFH))
    {
      res_LAYOUTCOMMIT4.locr_status = NFS4ERR_NOTSUPP;
      return res_LAYOUTCOMMIT4.locr_status;
    }
#endif /* _USE_FSALDS */

  /* Commit is done only on a file */
  if(data->current_filetype != REGULAR_FILE)
    {
      /* Type of the entry is not correct */
      switch (data->current_filetype)
        {
        case DIR_BEGINNING:
        case DIR_CONTINUE:
          res_LAYOUTCOMMIT4.locr_status = NFS4ERR_ISDIR;
          break;
        default:
          res_LAYOUTCOMMIT4.locr_status = NFS4ERR_INVAL;
          break;
        }

      return res_LAYOUTCOMMIT4.locr_status;
    }

#ifdef _USE_PNFS
  /* Update the mds */
  if(cache_inode_truncate(data->current_entry,
                          (fsal_size_t) arg_LAYOUTCOMMIT4.loca_length,
                          &fsal_attr,
                          data->ht,
                          data->pclient,
                          data->pcontext, &cache_status) != CACHE_INODE_SUCCESS)
    {
      res_LAYOUTCOMMIT4.locr_status = nfs4_Errno(cache_status);
      return res_LAYOUTCOMMIT4.locr_status;
    }

  /* For the moment, returns no new size */
  res_LAYOUTCOMMIT4.LAYOUTCOMMIT4res_u.locr_resok4.locr_newsize.ns_sizechanged = TRUE;
  res_LAYOUTCOMMIT4.LAYOUTCOMMIT4res_u.locr_resok4.locr_newsize.newsize4_u.ns_size = arg_LAYOUTCOMMIT4.loca_length ;

  res_LAYOUTCOMMIT4.locr_status = NFS4_OK;

  return res_LAYOUTCOMMIT4.locr_status;
#elif defined(_USE_FSALMDS)
  fsal_status_t fsal_status;
  taggedstate state;
  int rc = 0;
  bool_t goodlayout = false;
  fsal_off_t newoff;
  uint64_t cookie;
  bool_t done;
  fsal_time_t newtime;
  layoutsegment segment;
  fsal_attrib_list_t attrs;

  if ((rc = state_retrieve_state(arg_LAYOUTCOMMIT4.loca_stateid,
				 &state)) != ERR_STATE_NO_ERROR)
    {
      res_LAYOUTCOMMIT4.locr_status = staterr2nfs4err(rc);
      return res_LAYOUTCOMMIT4.locr_status;
    }

  /* Check to see if the layout is valid */

  if ((state.tag != STATE_LAYOUT) ||
      (state.u.layout.clientid != data->psession->clientid) ||
      !(FSAL_handlecmp(&(state.u.layout.handle),
		       &(data->current_entry->object.file.handle),
		       &fsal_status)))
    {
      res_LAYOUTCOMMIT4.locr_status = NFS4ERR_BADLAYOUT;
      return res_LAYOUTCOMMIT4.locr_status;
    }
      
  do
    {
      rc = state_iter_layout_entries(arg_LAYOUTCOMMIT4.loca_stateid,
				     &cookie,
				     &done,
				     &segment);
      if (rc != ERR_STATE_NO_ERROR)
	{
	  res_LAYOUTCOMMIT4.locr_status = staterr2nfs4err(rc);
	  return res_LAYOUTCOMMIT4.locr_status;
	}
      if ((segment.iomode == LAYOUTIOMODE4_RW) &&
	  (segment.offset <= arg_LAYOUTCOMMIT4.loca_offset) &&
	  ((segment.offset + segment.length) >=
	   (arg_LAYOUTCOMMIT4.loca_offset +
	    arg_LAYOUTCOMMIT4.loca_length)))
	goodlayout = true;
    } while (!done && !goodlayout);

  if (!goodlayout)
    {
      res_LAYOUTCOMMIT4.locr_status = NFS4ERR_BADLAYOUT;
      return res_LAYOUTCOMMIT4.locr_status;
    }

  if (arg_LAYOUTCOMMIT4.loca_last_write_offset.no_newoffset)
    if ((arg_LAYOUTCOMMIT4.loca_last_write_offset.newoffset4_u.no_offset
	 < arg_LAYOUTCOMMIT4.loca_offset) ||
	(arg_LAYOUTCOMMIT4.loca_last_write_offset.newoffset4_u.no_offset
	 > (arg_LAYOUTCOMMIT4.loca_offset +
	    arg_LAYOUTCOMMIT4.loca_length)))
      {
	res_LAYOUTCOMMIT4.locr_status = NFS4ERR_INVAL;
	return res_LAYOUTCOMMIT4.locr_status;
      }
    else
      newoff =
	arg_LAYOUTCOMMIT4.loca_last_write_offset.newoffset4_u.no_offset;
  
  if (arg_LAYOUTCOMMIT4.loca_time_modify.nt_timechanged)
    {
      newtime.seconds =
	arg_LAYOUTCOMMIT4.loca_time_modify.newtime4_u.nt_time.seconds;
      newtime.nseconds =
	arg_LAYOUTCOMMIT4.loca_time_modify.newtime4_u.nt_time.nseconds;
    }

  if (arg_LAYOUTCOMMIT4.loca_time_modify.nt_timechanged ||
      arg_LAYOUTCOMMIT4.loca_last_write_offset.no_newoffset)
    P_w(&data->current_entry->lock);
    
  fsal_status =
    FSAL_layoutcommit(&(data->current_entry->object.file.handle),
		      arg_LAYOUTCOMMIT4.loca_offset,
		      arg_LAYOUTCOMMIT4.loca_length,
		      (arg_LAYOUTCOMMIT4.loca_last_write_offset.no_newoffset ?
		       &newoff : NULL),
		      (arg_LAYOUTCOMMIT4.loca_time_modify.nt_timechanged ?
		       &newtime : NULL),
		      arg_LAYOUTCOMMIT4.loca_stateid,
		      arg_LAYOUTCOMMIT4.loca_layoutupdate,
		      data->pcontext);

  if ((cache_status = cache_inode_error_convert(fsal_status))
      != CACHE_INODE_SUCCESS)
    {
      if (arg_LAYOUTCOMMIT4.loca_time_modify.nt_timechanged ||
	  arg_LAYOUTCOMMIT4.loca_last_write_offset.no_newoffset)
	V_w(&data->current_entry->lock);
      res_LAYOUTCOMMIT4.locr_status = nfs4_Errno(cache_status);
      return res_LAYOUTCOMMIT4.locr_status;
    }

  if (arg_LAYOUTCOMMIT4.loca_time_modify.nt_timechanged ||
      arg_LAYOUTCOMMIT4.loca_last_write_offset.no_newoffset)
    {
      cache_inode_get_attributes(data->current_entry,
				 &attrs);
      
      if (arg_LAYOUTCOMMIT4.loca_last_write_offset.no_newoffset)
	{
	  attrs.asked_attributes |= FSAL_ATTR_SIZE;
	  attrs.filesize = newoff + 1; /* There is one byte AFTER the
					  offset */
	  res_LAYOUTCOMMIT4.LAYOUTCOMMIT4res_u.locr_resok4.locr_newsize.ns_sizechanged
	    = true;
	  res_LAYOUTCOMMIT4.LAYOUTCOMMIT4res_u.locr_resok4.locr_newsize.newsize4_u.ns_size
	    = attrs.filesize;
	}
      if (arg_LAYOUTCOMMIT4.loca_time_modify.nt_timechanged)
	{
	  attrs.asked_attributes |= (FSAL_ATTR_MTIME |
				     FSAL_ATTR_CHGTIME);
	  
	  attrs.mtime = newtime;
	  attrs.chgtime = newtime;
	}
      cache_inode_set_attributes(data->current_entry,
				 &attrs);
      V_w(&data->current_entry->lock);
    }

    
  res_LAYOUTCOMMIT4.locr_status = NFS4_OK;
  return res_LAYOUTCOMMIT4.locr_status;
#endif                          /* _USE_FSALMDS */
}                               /* nfs41_op_layoutcommit */

/**
 * nfs41_op_layoutcommit_Free: frees what was allocared to handle nfs41_op_layoutcommit.
 * 
 * Frees what was allocared to handle nfs41_op_layoutcommit.
 *
 * @param resp  [INOUT]    Pointer to nfs4_op results
 *
 * @return nothing (void function )
 * 
 */
void nfs41_op_layoutcommit_Free(LOCK4res * resp)
{
  /* Nothing to Mem_Free */
  return;
}                               /* nfs41_op_layoutcommit_Free */
