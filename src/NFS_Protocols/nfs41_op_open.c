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

extern nfs_parameter_t nfs_param;

int open_fh41(struct nfs_argop4 *op, compound_data_t * data,
	      struct nfs_resop4* resp, uid_t uid);
int open_name41(struct nfs_argop4* op, compound_data_t* data,
		struct nfs_resop4* resp, uid_t uid,
		cache_entry_t* pentry_parent, fsal_name_t* filename);
int create_name41(struct nfs_argop4* op, compound_data_t* data,
		  struct nfs_resop4* resp, uid_t uid,
		  cache_entry_t* pentry_parent, fsal_name_t* filename,
		  fattr4* createattrs, verifier4* verf, bool_t exclusive);

/**
 * nfs41_op_open: NFS4_OP_OPEN, opens and eventually creates a regular file.
 * 
 * NFS4_OP_OPEN, opens and eventually creates a regular file.
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

int nfs41_op_open(struct nfs_argop4 *op, compound_data_t * data, struct nfs_resop4 *resp)
{
  cache_entry_t *pentry_parent = NULL;
  fsal_name_t filename;
  nfs_worker_data_t *pworker = NULL;
  char __attribute__ ((__unused__)) funcname[] = "nfs4_op_open";
  uid_t uid = 0;
  int rc;
  fsal_attrib_list_t attr;
  cache_inode_status_t status;

  resp->resop = NFS4_OP_OPEN;
  res_OPEN4.status = NFS4_OK;

  int pnfs_status;
  cache_inode_create_arg_t create_arg;

  pworker = (nfs_worker_data_t *) data->pclient->pworker;
  res_OPEN4.status = NFS4_OK;

  /* If there is no FH */
  if(nfs4_Is_Fh_Empty(&(data->currentFH)))
    {
      res_OPEN4.status = NFS4ERR_NOFILEHANDLE;
      return res_OPEN4.status;
    }

  /* If the filehandle is invalid */
  if(nfs4_Is_Fh_Invalid(&(data->currentFH)))
    {
      res_OPEN4.status = NFS4ERR_BADHANDLE;
      return res_OPEN4.status;
    }

  /* Tests if the Filehandle is expired (for volatile filehandle) */
  if(nfs4_Is_Fh_Expired(&(data->currentFH)))
    {
      res_OPEN4.status = NFS4ERR_FHEXPIRED;
      return res_OPEN4.status;
    }

  /* This can't be done on the pseudofs */
  if(nfs4_Is_Fh_Pseudo(&(data->currentFH)))
    {
      res_OPEN4.status = NFS4ERR_ROFS;
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
                                                   &attr,
                                                   data->pcontext,
                                                   data->pclient,
                                                   data->ht, &rc)) == NULL)
        {
          res_OPEN4.status = NFS4ERR_SERVERFAULT;
          return res_OPEN4.status;
        }
    }

  if (!nfs_finduid(data, &uid))
    {
      res_OPEN4.status = NFS4ERR_SERVERFAULT;
      return res_OPEN4.status;
    }

  /* First switch is based upon claim type */
  switch (arg_OPEN4.claim.claim)
    {
    case CLAIM_DELEGATE_CUR:
    case CLAIM_DELEGATE_PREV:
      /* Check for name length */
      if(arg_OPEN4.claim.open_claim4_u.file.utf8string_len > FSAL_MAX_NAME_LEN)
        {
          res_OPEN4.status = NFS4ERR_NAMETOOLONG;
          return res_OPEN4.status;
        }

      /* get the filename from the argument, it should not be empty */
      if(arg_OPEN4.claim.open_claim4_u.file.utf8string_len == 0)
        {
          res_OPEN4.status = NFS4ERR_INVAL;
          return res_OPEN4.status;
        }

      res_OPEN4.status = NFS4ERR_NOTSUPP;
      return res_OPEN4.status;
      break;

    case CLAIM_PREVIOUS:
      res_OPEN4.status = NFS4ERR_NO_GRACE;
      return res_OPEN4.status;
      break;

    case CLAIM_FH:
      /* We can't create without a filename */
      if (arg_OPEN4.openhow.opentype == OPEN4_CREATE)
	{
	  res_OPEN4.status = NFS4ERR_INVAL;
	  return res_OPEN4.status;
	}
      return open_fh41(op, data, resp, uid);
      break;

    case CLAIM_NULL:
      /* Set parent */
      pentry_parent = data->current_entry;

      /* Parent must be a directory */
      if((pentry_parent->internal_md.type != DIR_BEGINNING) &&
         (pentry_parent->internal_md.type != DIR_CONTINUE))
        {
          /* Parent object is not a directory... */
          if(pentry_parent->internal_md.type == SYMBOLIC_LINK)
            res_OPEN4.status = NFS4ERR_SYMLINK;
          else
            res_OPEN4.status = NFS4ERR_NOTDIR;

          return res_OPEN4.status;
        }

      /* Check for name length */
      if(arg_OPEN4.claim.open_claim4_u.file.utf8string_len > FSAL_MAX_NAME_LEN)
        {
          res_OPEN4.status = NFS4ERR_NAMETOOLONG;
          return res_OPEN4.status;
        }

      /* get the filename from the argument, it should not be empty */
      if(arg_OPEN4.claim.open_claim4_u.file.utf8string_len == 0)
        {
          res_OPEN4.status = NFS4ERR_INVAL;
          return res_OPEN4.status;
        }

      /* Check if filename is correct */
      if((status =
          cache_inode_error_convert(FSAL_buffdesc2name
                                    ((fsal_buffdesc_t *) & arg_OPEN4.claim.open_claim4_u.
                                     file, &filename))) != CACHE_INODE_SUCCESS)
        {
          res_OPEN4.status = nfs4_Errno(status);
          return res_OPEN4.status;
        }


      /* Second switch is based upon "openhow" */
      switch (arg_OPEN4.openhow.opentype)
        {
        case OPEN4_CREATE:
	  switch (arg_OPEN4.openhow.openflag4_u.how.mode)
	    {
	      bool_t exclusive = false;
	    case GUARDED4:
		exclusive = true;
	    case UNCHECKED4:
	      return create_name41(op, data, resp, uid, pentry_parent,
				   &filename,
				   &arg_OPEN4.openhow.openflag4_u.how.createhow4_u.createattrs,
				   NULL,
				   exclusive);
	      break;

	    case EXCLUSIVE4:
	      return create_name41(op, data, resp, uid, pentry_parent,
				   &filename,
				   NULL,
				   &arg_OPEN4.openhow.openflag4_u.how.createhow4_u.createverf,
				   exclusive);
	    case EXCLUSIVE4_1:
	      return create_name41(op, data, resp, uid, pentry_parent,
				   &filename,
				   &arg_OPEN4.openhow.openflag4_u.how.createhow4_u.ch_createboth.cva_attrs,
				   &arg_OPEN4.openhow.openflag4_u.how.createhow4_u.ch_createboth.cva_verf,
				   exclusive);
	      break;

	    default:
	      res_OPEN4.status = NFS4ERR_INVAL;
	      return res_OPEN4.status;
	      break;
	    }
          break;

        case OPEN4_NOCREATE:
	  open_name41(op, data, resp, uid, pentry_parent, &filename);
	  break;

        default:
          res_OPEN4.status = NFS4ERR_INVAL;
          return res_OPEN4.status;
          break;
        }                       /* switch( arg_OPEN4.openhow.opentype ) */

      break;

    default:

      res_OPEN4.status = NFS4ERR_INVAL;
      return res_OPEN4.status;
      break;
    }
  
  /* regular exit */
  return res_OPEN4.status;
}                               /* nfs41_op_open */

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
void nfs41_op_open_Free(OPEN4res * resp)
{
  if (resp->OPEN4res_u.resok4.attrset.bitmap4_len)
    {
      Mem_Free((char *)resp->OPEN4res_u.resok4.attrset.bitmap4_val);
      resp->OPEN4res_u.resok4.attrset.bitmap4_len = 0;
    }
  
  return;
}                               /* nfs41_op_open_Free */

int open_fh41(struct nfs_argop4 *op, compound_data_t * data,
	      struct nfs_resop4* resp, uid_t uid)
{
  cache_inode_status_t status;
  fsal_attrib_list_t attrs;

  /* OPEN4 is to be done on a file */
  if(data->current_entry->internal_md.type != REGULAR_FILE)
    {
      if(data->current_entry->internal_md.type == DIR_BEGINNING
	 || data->current_entry->internal_md.type == DIR_CONTINUE)
	{
	  res_OPEN4.status = NFS4ERR_ISDIR;
	  return res_OPEN4.status;
	}
      else if(data->current_entry->internal_md.type == SYMBOLIC_LINK)
	{
	  res_OPEN4.status = NFS4ERR_SYMLINK;
	  return res_OPEN4.status;
	}
      else
	{
	  res_OPEN4.status = NFS4ERR_INVAL;
	  return res_OPEN4.status;
	}
    }

  cache_inode_getattr(data->current_entry,
		      &attrs,
		      data->ht,
		      data->pclient,
		      data->pcontext,
		      &status);
  
  res_OPEN4.OPEN4res_u.resok4.cinfo.before =
    (changeid4) data->current_entry->internal_md.mod_time;

  if (cache_inode_open(data->current_entry,
		       data->pclient,
		       arg_OPEN4.share_access & OPEN4_SHARE_ACCESS_BOTH,
		       arg_OPEN4.share_deny & OPEN4_SHARE_DENY_BOTH,
		       data->psession->clientid,
		       arg_OPEN4.owner,
		       &res_OPEN4.OPEN4res_u.resok4.stateid,
		       data->pcontext,
		       uid,
		       &status) != CACHE_INODE_SUCCESS)
    {
      res_OPEN4.status = nfs4_Errno(status);
      return res_OPEN4.status;
    }
  
  res_OPEN4.OPEN4res_u.resok4.cinfo.after =
    (changeid4) data->current_entry->internal_md.mod_time;
  res_OPEN4.OPEN4res_u.resok4.cinfo.atomic = true;

  res_OPEN4.OPEN4res_u.resok4.delegation.delegation_type = OPEN_DELEGATE_NONE;
  res_OPEN4.OPEN4res_u.resok4.rflags = 0;

  data->currentstate = res_OPEN4.OPEN4res_u.resok4.stateid;

  res_OPEN4.status = NFS4_OK;
  return res_OPEN4.status;
}

int open_name41(struct nfs_argop4* op, compound_data_t* data,
		struct nfs_resop4* resp, uid_t uid,
		cache_entry_t* pentry_parent, fsal_name_t* filename)
{
  fsal_attrib_list_t attr;
  cache_inode_status_t status;
  cache_entry_t* pentry;
  fsal_handle_t* pnewfsal_handle = NULL;
  nfs_fh4 newfh4;
  int rc;

  if ((status = cache_inode_getattr(pentry_parent,
				    &attr,
				    data->ht,
				    data->pclient,
				    data->pcontext,
				    &status)) != CACHE_INODE_SUCCESS)
    {
      res_OPEN4.status = nfs4_Errno(status);
      return res_OPEN4.status;
    }

  res_OPEN4.OPEN4res_u.resok4.cinfo.before =
    (changeid4) data->current_entry->internal_md.mod_time;

  if((pentry = cache_inode_lookup(pentry_parent,
				  filename,
				  &attr,
				  data->ht,
				  data->pclient,
				  data->pcontext,
				  &status)) == NULL)
    {
      res_OPEN4.status = nfs4_Errno(status);
      return res_OPEN4.status;
    }

  if(pentry->internal_md.type != REGULAR_FILE)
    {
      if(pentry->internal_md.type == DIR_BEGINNING
	 || pentry->internal_md.type == DIR_CONTINUE)
	{
	  res_OPEN4.status = NFS4ERR_ISDIR;
	  return;
	}
      else if(pentry->internal_md.type == SYMBOLIC_LINK)
	{
	  res_OPEN4.status = NFS4ERR_SYMLINK;
	  return;
	}
      else
	{
	  res_OPEN4.status = NFS4ERR_INVAL;
	  return res_OPEN4.status;
	}
    }

  if (cache_inode_open(pentry,
		       data->pclient,
		       arg_OPEN4.share_access & OPEN4_SHARE_ACCESS_BOTH,
		       arg_OPEN4.share_deny & OPEN4_SHARE_DENY_BOTH,
		       data->psession->clientid,
		       arg_OPEN4.owner,
		       &res_OPEN4.OPEN4res_u.resok4.stateid,
		       data->pcontext,
		       uid,
		       &status) != CACHE_INODE_SUCCESS)
    {
      res_OPEN4.status = nfs4_Errno(status);
      return res_OPEN4.status;
    }

  if ((status = cache_inode_getattr(pentry_parent,
				    &attr,
				    data->ht,
				    data->pclient,
				    data->pcontext,
				    &status)) != CACHE_INODE_SUCCESS)
    {
      res_OPEN4.status = nfs4_Errno(status);
      return res_OPEN4.status;
    }

  res_OPEN4.OPEN4res_u.resok4.cinfo.after =
    (changeid4) data->current_entry->internal_md.mod_time;

  res_OPEN4.OPEN4res_u.resok4.cinfo.atomic = false;

  /* Now produce the filehandle to this file */
  if((pnewfsal_handle =
      cache_inode_get_fsal_handle(pentry, &status)) == NULL)
    res_OPEN4.status = nfs4_Errno(status);
  
  /* Allocation of a new file handle */
  if((rc = nfs4_AllocateFH(&newfh4)) != NFS4_OK)
    {
      res_OPEN4.status = rc;
      return res_OPEN4.status;
    }
  
  /* Building a new fh */
  if(!nfs4_FSALToFhandle(&newfh4, pnewfsal_handle, data))
    {
      res_OPEN4.status = NFS4ERR_SERVERFAULT;
      return res_OPEN4.status;
    }
  
  /* This new fh replaces the current FH */
  data->currentFH.nfs_fh4_len = newfh4.nfs_fh4_len;
  memcpy(data->currentFH.nfs_fh4_val, newfh4.nfs_fh4_val, newfh4.nfs_fh4_len);
  
  data->current_entry = pentry;
  data->current_filetype = REGULAR_FILE;
  
  /* No do not need newfh any more */
  Mem_Free((char *)newfh4.nfs_fh4_val);

  data->currentstate = res_OPEN4.OPEN4res_u.resok4.stateid;

  res_OPEN4.OPEN4res_u.resok4.delegation.delegation_type = OPEN_DELEGATE_NONE;
  res_OPEN4.OPEN4res_u.resok4.rflags = 0;

  res_OPEN4.status = NFS4_OK;
  return res_OPEN4.status;
}


int create_name41(struct nfs_argop4* op, compound_data_t* data,
		  struct nfs_resop4* resp, uid_t uid,
		  cache_entry_t* pentry_parent, fsal_name_t* filename,
		  fattr4* createattrs, verifier4* verf, bool_t exclusive)
{
  fsal_attrib_list_t sattr, attr;
  cache_entry_t *pentry = NULL;
  int convrc = 0;
  bool_t created = false;
  bool_t truncated = false;
  cache_inode_status_t status;
  fsal_handle_t* pnewfsal_handle;
  nfs_fh4 newfh4;
  int rc;

  /* CLient may have provided fattr4 to set attributes at creation time */

  memset(&sattr, 0, sizeof(fsal_attrib_list_t));

  if (createattrs)
    {
      if(!nfs4_Fattr_Supported(createattrs))
	{
	  res_OPEN4.status = NFS4ERR_ATTRNOTSUPP;
	  return res_OPEN4.status;
	}
      
      /* Do not use READ attr, use WRITE attr */
      if(!nfs4_Fattr_Check_Access(createattrs, FATTR4_ATTR_WRITE))
	{
	  res_OPEN4.status = NFS4ERR_ACCESS;
	  return res_OPEN4.status;
	}
      
      /* Convert fattr4 so nfs4_sattr */
      convrc = nfs4_Fattr_To_FSAL_attr(&sattr, createattrs);

      if(convrc == 0)
	{
	  res_OPEN4.status = NFS4ERR_ATTRNOTSUPP;
	  return res_OPEN4.status;
	}
      if(convrc == -1)
	{
	  res_OPEN4.status = NFS4ERR_BADXDR;
	  return res_OPEN4.status;
	}
    }

  /* We must provide a valid mode */
      
  if (!(sattr.asked_attributes & FSAL_ATTR_MODE))
    {
      sattr.asked_attributes |= FSAL_ATTR_MODE;
      sattr.mode = FSAL_MODE_RUSR | FSAL_MODE_WUSR;
    }

  if (verf && (sattr.asked_attributes & (FSAL_ATTR_ATIME |
					 FSAL_ATTR_MTIME)))
    {
      res_OPEN4.status = NFS4ERR_ATTRNOTSUPP;
      return res_OPEN4.status;
    }

  if ((status = cache_inode_getattr(pentry_parent,
				    &attr,
				    data->ht,
				    data->pclient,
				    data->pcontext,
				    &status)) != CACHE_INODE_SUCCESS)
    {
      res_OPEN4.status = nfs4_Errno(status);
      return res_OPEN4.status;
    }

  res_OPEN4.OPEN4res_u.resok4.cinfo.before =
    (changeid4) data->current_entry->internal_md.mod_time;

  if ((status =
       cache_inode_open_create_name(pentry_parent,
				    filename,
				    &pentry,
				    (arg_OPEN4.share_access &
				     OPEN4_SHARE_ACCESS_BOTH),
				    (arg_OPEN4.share_deny &
				     OPEN4_SHARE_DENY_BOTH),
				    exclusive,
				    &sattr,
				    verf,
				    data->psession->clientid,
				    arg_OPEN4.owner,
				    &res_OPEN4.OPEN4res_u.resok4.stateid,
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
      return res_OPEN4.status;
    }

  if ((status = cache_inode_getattr(pentry_parent,
				    &attr,
				    data->ht,
				    data->pclient,
				    data->pcontext,
				    &status)) != CACHE_INODE_SUCCESS)
    {
      res_OPEN4.status = nfs4_Errno(status);
      return res_OPEN4.status;
    }

  res_OPEN4.OPEN4res_u.resok4.cinfo.after =
    (changeid4) data->current_entry->internal_md.mod_time;

  res_OPEN4.OPEN4res_u.resok4.cinfo.atomic = true;
  
  if (created)
    {
      res_OPEN4.OPEN4res_u.resok4.attrset.bitmap4_len =
	createattrs->attrmask.bitmap4_len;
      if((res_OPEN4.OPEN4res_u.resok4.attrset.bitmap4_val =
	  (uint32_t *) Mem_Alloc(4 * sizeof(uint32_t))) == NULL)
	res_OPEN4.OPEN4res_u.resok4.attrset.bitmap4_len = 0;
      else
	{
	  memset((char *)res_OPEN4.OPEN4res_u.resok4.attrset.bitmap4_val, 0,
		 res_OPEN4.OPEN4res_u.resok4.attrset.bitmap4_len * sizeof(u_int));
	  if (createattrs && createattrs->attrmask.bitmap4_val)
	    memcpy(res_OPEN4.OPEN4res_u.resok4.attrset.bitmap4_val,
		   createattrs->attrmask.bitmap4_val,
		   res_OPEN4.OPEN4res_u.resok4.attrset.bitmap4_len * sizeof(u_int));
	  /* We always set the mode on create */
	  res_OPEN4.OPEN4res_u.resok4.attrset.bitmap4_val[1] |= (1 << 2); 
	  if (verf)
	      {
		res_OPEN4.OPEN4res_u.resok4.attrset.bitmap4_val[1] |=
		  (1 << 17); 
		res_OPEN4.OPEN4res_u.resok4.attrset.bitmap4_val[1] |=
		  (1 << 23); 
	      }
	  }
    }
  else if (truncated)
    {
      res_OPEN4.OPEN4res_u.resok4.attrset.bitmap4_len = 2;
      if((res_OPEN4.OPEN4res_u.resok4.attrset.bitmap4_val =
	  (uint32_t *) Mem_Alloc(res_OPEN4.OPEN4res_u.resok4.attrset.bitmap4_len *
				 sizeof(uint32_t))) == NULL)
	{
	  res_OPEN4.OPEN4res_u.resok4.attrset.bitmap4_len = 0;
	}
      else
	{
	  memset((char *)res_OPEN4.OPEN4res_u.resok4.attrset.bitmap4_val, 0,
		 res_OPEN4.OPEN4res_u.resok4.attrset.bitmap4_len * sizeof(u_int));
	  res_OPEN4.OPEN4res_u.resok4.attrset.bitmap4_val[0] |= 1 << 4;
	}
    }
  else
    res_OPEN4.OPEN4res_u.resok4.attrset.bitmap4_len = 0;
    
  /* Now produce the filehandle to this file */
  if((pnewfsal_handle =
      cache_inode_get_fsal_handle(pentry, &status)) == NULL)
    res_OPEN4.status = nfs4_Errno(status);
  
  /* Allocation of a new file handle */
  if((rc = nfs4_AllocateFH(&newfh4)) != NFS4_OK)
    {
      res_OPEN4.status = rc;
      return res_OPEN4.status;
    }
  
  /* Building a new fh */
  if(!nfs4_FSALToFhandle(&newfh4, pnewfsal_handle, data))
    {
      res_OPEN4.status = NFS4ERR_SERVERFAULT;
      return res_OPEN4.status;
    }
  
  /* This new fh replaces the current FH */
  data->currentFH.nfs_fh4_len = newfh4.nfs_fh4_len;
  memcpy(data->currentFH.nfs_fh4_val, newfh4.nfs_fh4_val, newfh4.nfs_fh4_len);
  
  data->current_entry = pentry;
  data->current_filetype = REGULAR_FILE;
  
  /* No do not need newfh any more */
  Mem_Free((char *)newfh4.nfs_fh4_val);

  data->currentstate = res_OPEN4.OPEN4res_u.resok4.stateid;

  res_OPEN4.OPEN4res_u.resok4.delegation.delegation_type = OPEN_DELEGATE_NONE;
  res_OPEN4.OPEN4res_u.resok4.rflags = 0;

  res_OPEN4.status = NFS4_OK;
  return res_OPEN4.status;
}
