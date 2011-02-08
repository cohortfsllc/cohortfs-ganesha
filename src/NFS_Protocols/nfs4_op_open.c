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
 * \file    nfs41_op_open.c
 * \author  $Author: deniel $
 * \date    $Date: 2005/11/28 17:02:51 $
 * \version $Revision: 1.18 $
 * \brief   Routines used for managing the NFS4 COMPOUND functions.
 *
 * nfs41_op_open.c : Routines used for managing the NFS4 COMPOUND functions.
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
#include "nfs_proto_tools.h"
#include "nfs_tools.h"
#include "nfs_file_handle.h"
#include "sal.h"

extern nfs_parameter_t nfs_param;

/**
 * nfs4_op_open: NFS4_OP_OPEN, opens and sometimes creates a regular file.
 * 
 * NFS4_OP_OPEN, opens and sometimes creates a regular file.
 *
 * @param op    [IN]    pointer to nfs4_op arguments
 * @param data  [INOUT] Pointer to the compound request's data
 * @param resp  [IN]    Pointer to nfs4_op results
 *
 * @return NFS4_OK if successfull, other values show an error.  
 * 
 */

extern time_t ServerBootTime;

#define arg_OPEN4 op->nfs_argop4_u.opopen
#define res_OPEN4 resp->nfs_resop4_u.opopen

int nfs4_op_open(struct nfs_argop4 *op, compound_data_t * data, struct nfs_resop4 *resp)
{
  char __attribute__ ((__unused__)) funcname[] = "nfs4_op_open";
  cache_inode_status_t status; /* Status of FSAL calls */
  int rc = 0; /* Integer return, all purpose save */
  uid_t uid = 0; /* User ID associated with file open */
  fsal_attrib_list_t comp_attrs; /* Attributes of parent (or file, in
				    CLAIM_FH) */
  cache_entry_t *comp_entry = NULL; /* cache_entry_t to be compared
				       before/after open call */
  cache_entry_t *parent_entry = NULL; /* cache_entry_t of directory
					 containing file to be opened
					 (for CLAIM_NULL) */
  cache_entry_t* new_entry; /* cache_entry_t of newly opened/created
			       file */
  fsal_name_t filename; /* Name of file to be opened (for CLAIM_NULL) */
  fsal_handle_t* new_fsal_handle = NULL; /* fsal_handle_t of newly
					    opened/created file */
  nfs_fh4 newfh4 = /* Filehandle for newly opened/created file */
      {               
	  .nfs_fh4_val = NULL,
	  .nfs_fh4_len = 0
      };
  bool_t new = FALSE; /* If this is a new ownerid (for NFSv4.0) */
  nfs_resop4* saved = NULL; /* If this operation has been saved (for
			       NFSv4.0) */
  clientid4 clientid; /* Retrieved from session for NFSv4.1, from
		         owner for 4.0 */

  resp->resop = NFS4_OP_OPEN;

  if (data->minorversion == 0)
      {
	  rc = state_lock_state_owner(arg_OPEN4.owner, FALSE, arg_OPEN4.seqid,
				      &new, &saved);
	  
	  if (rc == ERR_STATE_BADSEQ)
	      {
		  res_OPEN4.status = NFS4ERR_BAD_SEQID;
		  return res_OPEN4.status;
	      }
	  else if (rc != ERR_STATE_NO_ERROR)
	      {
		  res_OPEN4.status = NFS4ERR_SERVERFAULT;
		  return res_OPEN4.status;
	      }
	  else if (saved)
	      {
		  memcpy(resp, saved, sizeof(nfs_resop4));
		  state_unlock_state_owner(arg_OPEN4.owner, FALSE);
		  return res_OPEN4.status;
	      }
	  clientid = arg_OPEN4.owner.clientid;
      }
#ifdef _USE_NFS4_1
  else
      {
	  clientid = data->psession->clientid;
      }
#endif /* _USE_NFS4_1 */
  
  /* If there is no FH */
  if(nfs4_Is_Fh_Empty(&(data->currentFH)))
    {
      res_OPEN4.status = NFS4ERR_NOFILEHANDLE;
      if (data->minorversion == 0)
	  {
	      state_save_response(arg_OPEN4.owner, FALSE, resp);
	      state_unlock_state_owner(arg_OPEN4.owner, FALSE);
	  }
      return res_OPEN4.status;
    }

  /* If the filehandle is invalid */
  if(nfs4_Is_Fh_Invalid(&(data->currentFH)))
    {
      res_OPEN4.status = NFS4ERR_BADHANDLE;
      if (data->minorversion == 0)
	  {
	      state_save_response(arg_OPEN4.owner, FALSE, resp);
	      state_unlock_state_owner(arg_OPEN4.owner, FALSE);
	  }
      return res_OPEN4.status;
    }

  /* Tests if the Filehandle is expired (for volatile filehandle) */
  if(nfs4_Is_Fh_Expired(&(data->currentFH)))
    {
      res_OPEN4.status = NFS4ERR_FHEXPIRED;
      if (data->minorversion == 0)
	  {
	      state_save_response(arg_OPEN4.owner, FALSE, resp);
	      state_unlock_state_owner(arg_OPEN4.owner, FALSE);
	  }
      return res_OPEN4.status;
    }

  /* This can't be done on the pseudofs */
  if(nfs4_Is_Fh_Pseudo(&(data->currentFH)))
    {
      res_OPEN4.status = NFS4ERR_ROFS;
      if (data->minorversion == 0)
	  {
	      state_save_response(arg_OPEN4.owner, FALSE, resp);
	      state_unlock_state_owner(arg_OPEN4.owner, FALSE);
	  }
      return res_OPEN4.status;
    }

#ifdef _USE_FSALDS
  if(nfs4_Is_Fh_DSHandle(&data->currentFH))
    {
      res_OPEN4.status = NFS4ERR_NOTSUPP;
      return res_OPEN4.status;
    }
#endif /* _USE_FSALDS */

  /* If Filehandle points to a xattr object, manage it via the xattrs specific functions */
  if(nfs4_Is_Fh_Xattr(&(data->currentFH)))
    return nfs4_op_open_xattr(op, data, resp);

  res_OPEN4.OPEN4res_u.resok4.attrset.bitmap4_len = 0;

  /* If data->current_entry is empty, repopulate it */
  if(data->current_entry == NULL)
    {
      if((data->current_entry = nfs_FhandleToCache(NFS_V4,
                                                   NULL,
                                                   NULL,
                                                   &(data->currentFH),
                                                   NULL,
                                                   NULL,
                                                   &(res_OPEN4.status),
                                                   &comp_attrs,
                                                   data->pcontext,
                                                   data->pclient,
                                                   data->ht, &rc)) == NULL)
        {
          res_OPEN4.status = NFS4ERR_SERVERFAULT;
	  if (data->minorversion == 0)
	      {
		  state_save_response(arg_OPEN4.owner, FALSE, resp);
		  state_unlock_state_owner(arg_OPEN4.owner, FALSE);
	      }
          return res_OPEN4.status;
        }
    }

  if (!nfs_finduid(data, &uid))
    {
      res_OPEN4.status = NFS4ERR_SERVERFAULT;
      if (data->minorversion == 0)
	  {
	      state_save_response(arg_OPEN4.owner, FALSE, resp);
	      state_unlock_state_owner(arg_OPEN4.owner, FALSE);
	  }
      return res_OPEN4.status;
    }

  /* Switch to filter out errors */
  switch (arg_OPEN4.claim.claim)
      {
      case CLAIM_DELEGATE_CUR:
      case CLAIM_DELEGATE_PREV:
	  /* Check for name length */
	  if(arg_OPEN4.claim.open_claim4_u.file.utf8string_len >
	     FSAL_MAX_NAME_LEN)
	      {
		  res_OPEN4.status = NFS4ERR_NAMETOOLONG;
		  if (data->minorversion == 0)
		      {
			  state_save_response(arg_OPEN4.owner, FALSE, resp);
			  state_unlock_state_owner(arg_OPEN4.owner, FALSE);
		      }
		  return res_OPEN4.status;
	      }
	  
	  /* get the filename from the argument, it should not be empty */
	  if(arg_OPEN4.claim.open_claim4_u.file.utf8string_len == 0)
	      {
		  res_OPEN4.status = NFS4ERR_INVAL;
		  if (data->minorversion == 0)
		      {
			  state_save_response(arg_OPEN4.owner, FALSE, resp);
			  state_unlock_state_owner(arg_OPEN4.owner, FALSE);
		      }
		  return res_OPEN4.status;
	      }
	  
	  res_OPEN4.status = NFS4ERR_NOTSUPP;
	  if (data->minorversion == 0)
	      {
		  state_save_response(arg_OPEN4.owner, FALSE, resp);
		  state_unlock_state_owner(arg_OPEN4.owner, FALSE);
	      }
	  return res_OPEN4.status;
	  break;
	  
      case CLAIM_PREVIOUS:
	  res_OPEN4.status = NFS4ERR_NO_GRACE;
	  if (data->minorversion == 0)
	      {
		  state_save_response(arg_OPEN4.owner, FALSE, resp);
		  state_unlock_state_owner(arg_OPEN4.owner, FALSE);
	      }
	  return res_OPEN4.status;
	  break;
	  
#ifdef _USE_NFS4_1
      case CLAIM_FH:
	  /* We can't create without a filename */
	  if ((arg_OPEN4.openhow.opentype == OPEN4_CREATE) ||
	      (data->minorversion == 0))
	      {
		  res_OPEN4.status = NFS4ERR_INVAL;
		  if (data->minorversion == 0)
		      {
			  state_save_response(arg_OPEN4.owner, FALSE, resp);
			  state_unlock_state_owner(arg_OPEN4.owner, FALSE);
		      }
		  return res_OPEN4.status;
	      }
	  if (data->minorversion == 0)
	      {
		  state_save_response(arg_OPEN4.owner, FALSE, resp);
		  state_unlock_state_owner(arg_OPEN4.owner, FALSE);
	      }
	  return res_OPEN4.status;
	  comp_entry = data->current_entry;
	  break;
#endif /* _USE_NFS4_1 */
	  
      case CLAIM_NULL:
	  /* Set parent */
	  parent_entry = data->current_entry;
	  comp_entry = data->current_entry;
	  
	  /* Parent must be a directory */
	  if((parent_entry->internal_md.type != DIR_BEGINNING) &&
	     (parent_entry->internal_md.type != DIR_CONTINUE))
	      {
		  /* Parent object is not a directory... */
		  if(parent_entry->internal_md.type == SYMBOLIC_LINK)
		      res_OPEN4.status = NFS4ERR_SYMLINK;
		  else
		      res_OPEN4.status = NFS4ERR_NOTDIR;
		  
		  if (data->minorversion == 0)
		      {
			  state_save_response(arg_OPEN4.owner, FALSE, resp);
			  state_unlock_state_owner(arg_OPEN4.owner, FALSE);
		      }
		  return res_OPEN4.status;
	      }
	  
	  /* Check for name length */
	  if(arg_OPEN4.claim.open_claim4_u.file.utf8string_len > FSAL_MAX_NAME_LEN)
	      {
		  res_OPEN4.status = NFS4ERR_NAMETOOLONG;
		  if (data->minorversion == 0)
		      {
			  state_save_response(arg_OPEN4.owner, FALSE, resp);
			  state_unlock_state_owner(arg_OPEN4.owner, FALSE);
		      }
		  return res_OPEN4.status;
	      }
	  
	  /* get the filename from the argument, it should not be empty */
	  if(arg_OPEN4.claim.open_claim4_u.file.utf8string_len == 0)
	      {
		  res_OPEN4.status = NFS4ERR_INVAL;
		  if (data->minorversion == 0)
		      {
			  state_save_response(arg_OPEN4.owner, FALSE, resp);
			  state_unlock_state_owner(arg_OPEN4.owner, FALSE);
		      }
		  return res_OPEN4.status;
	      }
	  
	  /* Check if filename is correct */
	  if((status =
	      cache_inode_error_convert(FSAL_buffdesc2name
					((fsal_buffdesc_t *) & arg_OPEN4.claim.open_claim4_u.
					 file, &filename))) != CACHE_INODE_SUCCESS)
	      {
		  res_OPEN4.status = nfs4_Errno(status);
		  if (data->minorversion == 0)
		      {
			  state_save_response(arg_OPEN4.owner, FALSE, resp);
			  state_unlock_state_owner(arg_OPEN4.owner, FALSE);
		      }
		  return res_OPEN4.status;
	      }
	  break;
	  
      default:
	  res_OPEN4.status = NFS4ERR_INVAL;
	  if (data->minorversion == 0)
	      {
		  state_save_response(arg_OPEN4.owner, FALSE, resp);
		  state_unlock_state_owner(arg_OPEN4.owner, FALSE);
	      }
	  return res_OPEN4.status;
      }

  /* Call cache_inode_getattr to force metadata refresh */

  if ((status = cache_inode_getattr(comp_entry,
				    &comp_attrs,
				    data->ht,
				    data->pclient,
				    data->pcontext,
				    &status)) != CACHE_INODE_SUCCESS)
      {
	  res_OPEN4.status = nfs4_Errno(status);
	  if (data->minorversion == 0)
	      {
		  state_save_response(arg_OPEN4.owner, FALSE, resp);
		  state_unlock_state_owner(arg_OPEN4.owner, FALSE);
	      }
	  return res_OPEN4.status;
      }

  res_OPEN4.OPEN4res_u.resok4.cinfo.before =
      (changeid4) comp_entry->internal_md.mod_time;

  
  /* Second switch to actually open */
  switch (arg_OPEN4.openhow.opentype)
      {
      case OPEN4_CREATE:
      {
	  bool_t exclusive = FALSE; /* Is it an error if the file
				       already exists? */
	  fattr4* createattrs = NULL; /* With which the fiel is to be
					 created */
	  verifier4* verf = NULL; /* Create verifier (for EXCLUSIVE4
				     and EXCLUSIVE4_1) */
	  fsal_attrib_list_t fsal_crattrs; /* Create attributes
					      converted to
					      fsal_attrib_list_t */
	  bool_t created = FALSE; /* File was created */
	  bool_t truncated = FALSE; /* Existing file was truncated */

	  memset(&fsal_crattrs, 0, sizeof(fsal_attrib_list_t));

	  /* Set parameters based on create mode */
	  
	  if (arg_OPEN4.openhow.openflag4_u.how.mode == UNCHECKED4)
	      {
		  exclusive = FALSE;
	      }
	  else
	      {
		  exclusive = TRUE;
	      }
	  if ((arg_OPEN4.openhow.openflag4_u.how.mode
	       == UNCHECKED4) ||
	      (arg_OPEN4.openhow.openflag4_u.how.mode
	       == UNCHECKED4))
	      {
		  createattrs = (&arg_OPEN4.openhow.openflag4_u
				 .how.createhow4_u.createattrs);
		  verf = NULL;
	      }
	  else if (arg_OPEN4.openhow.openflag4_u.how.mode
		   == EXCLUSIVE4)
	      {
		  createattrs = NULL;
		  verf = (&arg_OPEN4.openhow.openflag4_u.how
			  .createhow4_u.createverf);
	      }
#ifdef _USE_NFS4_1
	  else if (arg_OPEN4.openhow.openflag4_u.how.mode
		   == EXCLUSIVE4_1)
	      {
		  if (data->minorversion == 0)
		      {
			  res_OPEN4.status = NFS4ERR_INVAL;
			  state_save_response(arg_OPEN4.owner, FALSE, resp);
			  state_unlock_state_owner(arg_OPEN4.owner, FALSE);
			  return res_OPEN4.status;
		      }

		  createattrs = (&arg_OPEN4.openhow.openflag4_u.how
				 .createhow4_u.ch_createboth.cva_attrs);
		  verf = (&arg_OPEN4.openhow.openflag4_u.how
			  .createhow4_u.ch_createboth.cva_verf);
	      }
#endif /* _USE_NFS4_1 */

	  /* Check and convert attributes if supplied */
	  
	  if (createattrs)
	      {
		  if(!nfs4_Fattr_Supported(createattrs))
		      {
			  res_OPEN4.status = NFS4ERR_ATTRNOTSUPP;
			  if (data->minorversion == 0)
			      {
				  state_save_response(arg_OPEN4.owner, FALSE,
						      resp);
				  state_unlock_state_owner(arg_OPEN4.owner,
							   FALSE);
			      }
			  return res_OPEN4.status;
		      }
		  
		  /* Do not use READ attr, use WRITE attr */
		  if(!nfs4_Fattr_Check_Access(createattrs, FATTR4_ATTR_WRITE))
		      {
			  res_OPEN4.status = NFS4ERR_ACCESS;
			  if (data->minorversion == 0)
			      {
				  state_save_response(arg_OPEN4.owner, FALSE,
						      resp);
				  state_unlock_state_owner(arg_OPEN4.owner,
							   FALSE);
			      }
			  return res_OPEN4.status;
		      }
		  
		  rc = nfs4_Fattr_To_FSAL_attr(&fsal_crattrs, createattrs);
		  
		  if(rc == 0)
		      {
			  res_OPEN4.status = NFS4ERR_ATTRNOTSUPP;
			  if (data->minorversion == 0)
			      {
				  state_save_response(arg_OPEN4.owner, FALSE,
						      resp);
				  state_unlock_state_owner(arg_OPEN4.owner,
							   FALSE);
			      }
			  return res_OPEN4.status;
		      }
		  if(rc == -1)
		      {
			  res_OPEN4.status = NFS4ERR_BADXDR;
			  if (data->minorversion == 0)
			      {
				  state_save_response(arg_OPEN4.owner, FALSE,
						      resp);
				  state_unlock_state_owner(arg_OPEN4.owner,
							   FALSE);
			      }
			  return res_OPEN4.status;
		      }
	      }
	  
	  /* If no mode is provided, default to 0600 */
	  
	  if (!(fsal_crattrs.asked_attributes & FSAL_ATTR_MODE))
	      {
		  fsal_crattrs.asked_attributes |= FSAL_ATTR_MODE;
		  fsal_crattrs.mode = FSAL_MODE_RUSR | FSAL_MODE_WUSR;
	      }

	  /* Currently we store the verifier in ATIME and MTIME.  This
	     may change later for FSALs that support extended
	     attributes. Therefore, we cannot set ATIME or MTIME with
	     EXCLUSIVE4_1. */

	  if (verf && (fsal_crattrs.asked_attributes &
		       (FSAL_ATTR_ATIME | FSAL_ATTR_MTIME)))
	      {
		  res_OPEN4.status = NFS4ERR_ATTRNOTSUPP;
		  if (data->minorversion == 0)
		      {
			  state_save_response(arg_OPEN4.owner, FALSE, resp);
			  state_unlock_state_owner(arg_OPEN4.owner, FALSE);
		      }
		  return res_OPEN4.status;
	      }

	  /* Call into cache_inode to attempt creation */

	  if ((status =
	       cache_inode_open_create_name(parent_entry,
					    &filename,
					    &new_entry,
					    (arg_OPEN4.share_access &
					     OPEN4_SHARE_ACCESS_BOTH),
					    (arg_OPEN4.share_deny &
					     OPEN4_SHARE_DENY_BOTH),
					    exclusive,
					    &fsal_crattrs,
					    verf,
					    clientid,
					    arg_OPEN4.owner,
					    (&res_OPEN4.OPEN4res_u
					     .resok4.stateid),
					    &created,
					    &truncated,
					    data->ht,
					    data->pcontext,
					    data->pclient,
					    uid,
					    &status)) !=
	      CACHE_INODE_SUCCESS)
	      {
		  res_OPEN4.status = nfs4_Errno(status);
		  if (data->minorversion == 0)
		      {
			  state_save_response(arg_OPEN4.owner, FALSE, resp);
			  state_unlock_state_owner(arg_OPEN4.owner, FALSE);
		      }
		  return res_OPEN4.status;
	      }
	  
	  res_OPEN4.OPEN4res_u.resok4.attrset.bitmap4_len = 4;
	  if ((res_OPEN4.OPEN4res_u.resok4.attrset.bitmap4_val =
	       (uint32_t *) Mem_Alloc(4 * sizeof(uint32_t)))
	      == NULL)
	      res_OPEN4.OPEN4res_u.resok4.attrset.bitmap4_len = 0;
	  else
	      {
		  memset((char *)
			 res_OPEN4.OPEN4res_u.resok4.attrset.bitmap4_val,
			 0,
			 res_OPEN4.OPEN4res_u.resok4.attrset.bitmap4_len
			 * sizeof(u_int));
		  if (created)
		      {
			  /* If attributes were supplied we either set
			     them all or error */

			  if (createattrs &&
			      createattrs->attrmask.bitmap4_val)
			      {
				  int i;
				  for (i = 0;
				       i < (createattrs->attrmask
					    .bitmap4_len);
				       i++)
				      (res_OPEN4.OPEN4res_u
				       .resok4.attrset.bitmap4_val)[i]
					  = (createattrs->attrmask
					     .bitmap4_val)[i];
			      }
			  
			  /* We always set the mode on create */

			  res_OPEN4.OPEN4res_u.resok4.attrset.bitmap4_val[1]
			      |= (1 << 2); 

			  /* If a verifier was supplied, then we set
			     the ATIME and MTIME to store it */

			  if (verf)
			      {
				  (res_OPEN4.OPEN4res_u.resok4.attrset
				   .bitmap4_val[1]) |= (1 << 17);
				  (res_OPEN4.OPEN4res_u.resok4.attrset
				   .bitmap4_val[1]) |=
				      (1 << 23);
			      }
		      }
		  else if (truncated)
		      {
			  res_OPEN4.OPEN4res_u.resok4.attrset.bitmap4_val[0]
			      |= 1 << 4;
		      }
	      }
	  break;
      }
	  
      case OPEN4_NOCREATE:
      {
	  new_entry = data->current_entry;
	  switch (arg_OPEN4.claim.claim)
	      {
	      case CLAIM_NULL:
		  if((new_entry = cache_inode_lookup(parent_entry,
						     &filename,
						     &comp_attrs,
						     data->ht,
						     data->pclient,
						     data->pcontext,
						     &status)) == NULL)
		      {
			  res_OPEN4.status = nfs4_Errno(status);
			  if (data->minorversion == 0)
			      {
				  state_save_response(arg_OPEN4.owner, FALSE, resp);
				  state_unlock_state_owner(arg_OPEN4.owner, FALSE);
			      }
			  return res_OPEN4.status;
		      }

#ifdef _USE_NFS4_1
	      case CLAIM_FH:
#endif /* USE_NFS4_1 */
		  /* OPEN4 is to be done on a file */
		  if(new_entry->internal_md.type != REGULAR_FILE)
		      {
			  if((new_entry->internal_md.type
			      == DIR_BEGINNING) ||
			     (new_entry->internal_md.type
			      == DIR_CONTINUE))
			      {
				  res_OPEN4.status = NFS4ERR_ISDIR;
				  if (data->minorversion == 0)
				      {
					  state_save_response(arg_OPEN4.owner, FALSE, resp);
					  state_unlock_state_owner(arg_OPEN4.owner, FALSE);
				      }
				  return res_OPEN4.status;
			      }
			  else if(new_entry->internal_md.type
				  == SYMBOLIC_LINK)
			      {
				  res_OPEN4.status = NFS4ERR_SYMLINK;
				  if (data->minorversion == 0)
				      {
					  state_save_response(arg_OPEN4.owner, FALSE, resp);
					  state_unlock_state_owner(arg_OPEN4.owner, FALSE);
				      }
				  return res_OPEN4.status;
			      }
			  else
			      {
#ifdef _USE_NFS4_1
				  res_OPEN4.status = NFS4ERR_WRONG_TYPE;
#else
				  res_OPEN4.status = NFS4ERR_INVAL;
#endif
				  if (data->minorversion == 0)
				      {
					  res_OPEN4.status = NFS4ERR_INVAL;
					  state_save_response(arg_OPEN4.owner, FALSE, resp);
					  state_unlock_state_owner(arg_OPEN4.owner, FALSE);
				      }
				  return res_OPEN4.status;
			      }
		      }

		  if (cache_inode_open(new_entry,
				       data->pclient,
				       (arg_OPEN4.share_access &
					OPEN4_SHARE_ACCESS_BOTH),
				       (arg_OPEN4.share_deny &
					OPEN4_SHARE_DENY_BOTH),
				       clientid,
				       arg_OPEN4.owner,
				       &res_OPEN4.OPEN4res_u.resok4.stateid,
				       data->pcontext,
				       uid,
				       &status) != CACHE_INODE_SUCCESS)
		      {
			  res_OPEN4.status = nfs4_Errno(status);
			  if (data->minorversion == 0)
			      {
				  state_save_response(arg_OPEN4.owner, FALSE, resp);
				  state_unlock_state_owner(arg_OPEN4.owner, FALSE);
			      }
			  return res_OPEN4.status;
		      }
	      }
      }
      break;
	  
      default:
          res_OPEN4.status = NFS4ERR_INVAL;
	  if (data->minorversion == 0)
	      {
		  state_save_response(arg_OPEN4.owner, FALSE, resp);
		  state_unlock_state_owner(arg_OPEN4.owner, FALSE);
	      }
          return res_OPEN4.status;
          break;
      }                       /* switch( arg_OPEN4.openhow.opentype ) */

  /* Get timestamp after open/create */

  if ((status = cache_inode_getattr(comp_entry,
				    &comp_attrs,
				    data->ht,
				    data->pclient,
				    data->pcontext,
				    &status)) != CACHE_INODE_SUCCESS)
      {
	  res_OPEN4.status = nfs4_Errno(status);
	  if (data->minorversion == 0)
	      {
		  state_save_response(arg_OPEN4.owner, FALSE, resp);
		  state_unlock_state_owner(arg_OPEN4.owner, FALSE);
	      }
	  return res_OPEN4.status;
      }

  res_OPEN4.OPEN4res_u.resok4.cinfo.after =
      (changeid4) data->current_entry->internal_md.mod_time;
  
  res_OPEN4.OPEN4res_u.resok4.cinfo.atomic = FALSE;

  /* Produce the filehandle to this file, if required */

#ifdef _USE_NFS4_1
  if (arg_OPEN4.claim.claim != CLAIM_FH)
#endif /* _USE_NFS4_1 */
      {
	  if ((new_fsal_handle =
	       cache_inode_get_fsal_handle(new_entry, &status)) == NULL)
	      {
		  res_OPEN4.status = nfs4_Errno(status);
		  if (data->minorversion == 0)
		      {
			  state_save_response(arg_OPEN4.owner, FALSE, resp);
			  state_unlock_state_owner(arg_OPEN4.owner, FALSE);
		      }
		  return res_OPEN4.status;
	      }
	  
	  /* Allocation of a new file handle */
	  if((rc = nfs4_AllocateFH(&newfh4)) != NFS4_OK)
	      {
		  res_OPEN4.status = rc;
		  if (data->minorversion == 0)
		      {
			  state_save_response(arg_OPEN4.owner, FALSE, resp);
			  state_unlock_state_owner(arg_OPEN4.owner, FALSE);
		      }
		  return res_OPEN4.status;
	      }
	  
	  /* Building a new fh */
	  if(!nfs4_FSALToFhandle(&newfh4, new_fsal_handle, data))
	      {
		  res_OPEN4.status = NFS4ERR_SERVERFAULT;
		  if (data->minorversion == 0)
		      {
			  state_save_response(arg_OPEN4.owner, FALSE, resp);
			  state_unlock_state_owner(arg_OPEN4.owner, FALSE);
		      }
		  return res_OPEN4.status;
	      }
	  
	  /* This new fh replaces the current FH */
	  data->currentFH.nfs_fh4_len = newfh4.nfs_fh4_len;
	  memcpy(data->currentFH.nfs_fh4_val, newfh4.nfs_fh4_val, newfh4.nfs_fh4_len);
	  
	  data->current_filetype = REGULAR_FILE;
	  
	  /* No do not need newfh any more */
	  Mem_Free((char *)newfh4.nfs_fh4_val);

	  data->current_entry = new_entry;
      }

  res_OPEN4.OPEN4res_u.resok4.delegation.delegation_type = OPEN_DELEGATE_NONE;
  res_OPEN4.OPEN4res_u.resok4.rflags = 0;

#ifdef _USE_NFS4_1
  data->currentstate = res_OPEN4.OPEN4res_u.resok4.stateid;
#endif /* _USE_NFS4_1 */

  res_OPEN4.status = NFS4_OK;
  if (data->minorversion == 0)
      {
	  state_save_response(arg_OPEN4.owner, FALSE, resp);
	  state_unlock_state_owner(arg_OPEN4.owner, FALSE);
      }
  
  return res_OPEN4.status;
}                               /* nfs4_op_open */

/**
 * nfs4_op_open_Free: frees what was allocared to handle nfs4_op_open.
 * 
 * Frees what was allocared to handle nfs4_op_open.
 *
 * @param resp  [INOUT]    Pointer to nfs4_op results
 *
 * @return nothing (void function )
 * 
 */
void nfs4_op_open_Free(OPEN4res * resp)
{
  if (resp->OPEN4res_u.resok4.attrset.bitmap4_len)
    {
      Mem_Free((char *)resp->OPEN4res_u.resok4.attrset.bitmap4_val);
      resp->OPEN4res_u.resok4.attrset.bitmap4_len = 0;
    }
  
  return;
}                               /* nfs41_op_open_Free */


