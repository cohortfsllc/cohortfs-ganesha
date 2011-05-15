/**
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
 *
 * \file    cache_inode_open_close.c
 * \author  $Author: deniel $
 * \date    $Date: 2010/10/18 14:32:27 $
 * \version $Revision: 1.20 $
 *
 * cache_inode_open_close.c : handles open and close operations on a
 * regular file
 *
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _SOLARIS
#include "solaris_port.h"
#endif                          /* _SOLARIS */

#include "fsal.h"

#include "LRU_List.h"
#include "log_macros.h"
#include "HashData.h"
#include "HashTable.h"
#include "cache_inode.h"
#include "cache_content.h"
#include "stuff_alloc.h"

#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <time.h>
#include <pthread.h>
#include <strings.h>
#include "sal.h"

/**
 *
 * cache_inode_open_create_name: opens and possibly creates a named
 * file in a directory
 *
 * @param pentry_parent [IN]  parent entry for the file
 * @param pname         [IN]  name of the file to be opened in the parent directory
 * @param new_entry     [OUT] file entry opened
 * @param pclient       [IN]  ressource allocated by the client for the nfs management.
 * @param share_access  [IN]  Access wanted
 * @param share_deny    [IN]  Access denied others
 * @param exclusive     [IN]  Create only, return an error if the file
 *                            already exists (Not to be confused with
 *                            EXCLUSIVE4 or EXCLUSIVE4_1)
 * @param attrs         [IN]  Attributes to be set on the new file.
 * @param transaction   [IN]  The state transaction pointer
 * @param created       [OUT] True if the file was created, false if
 *                            it already existed and was opened.
 * @param truncated     [OUT] True if a pre-existing file was
 *                            truncated
 * @param ht            [IN]  Cache_Inode hashtable (for filehandle
 *                            lookups)
 * @param pcontext      [IN]  Client context
 * @param uid           [IN]  User ID (for open file descriptor
 *                            refcount)
 * @param pstatus       [OUT] Status of operation.
 *
 * @return CACHE_CONTENT_SUCCESS is successful.
 *
 */

cache_inode_status_t cache_inode_open_create_name(cache_entry_t* pentry_parent,
						  fsal_name_t* pname,
						  cache_entry_t** new_entry,
						  uint32_t share_access,
						  uint32_t share_deny,
						  bool_t exclusive,
						  fsal_attrib_list_t* attrs,
						  verifier4* verf,
						  struct state_share_trans__* transaction,
						  bool_t* created,
						  bool_t* truncated,
						  hash_table_t* ht,
						  fsal_op_context_t*  pcontext,
						  cache_inode_client_t* pclient,
						  uid_t uid,
						  cache_inode_status_t*  pstatus)
{
  fsal_status_t fsal_status;
  fsal_handle_t new_handle;
  fsal_handle_t parent_handle;
  fsal_attrib_list_t found_attrs;
  cache_inode_status_t privstatus;
  cache_inode_fsal_data_t fsal_data;
  struct cache_inode_dir_begin__ *dir_begin;
  uint32_t* verfpieces = (uint32_t*) verf;
  int rc = 0;

  if((pentry_parent == NULL) || (pname == NULL) || (new_entry == NULL) ||
     (pclient == NULL) || (pcontext == NULL) || (pstatus == NULL) ||
     (ht == NULL) || (attrs == NULL))
    return CACHE_INODE_INVALID_ARGUMENT;
      /* If proxy if used, we should keep the name of the file to do FSAL_rcp if needed */
  if((pentry_parent->internal_md.type != DIR_BEGINNING)
     && (pentry_parent->internal_md.type != DIR_CONTINUE))
    {
      *pstatus = CACHE_INODE_BAD_TYPE;
      return *pstatus;
    }

  /* Based on cache_inode_create.  Locking the whole directory is bad,
     but let's get it Correct first and then make it efficient later */

  /* Get the lock for the parent */
  P_w(&pentry_parent->lock);
  
  if(pentry_parent->internal_md.type == DIR_BEGINNING)
    parent_handle = pentry_parent->object.dir_begin.handle;
  
  if(pentry_parent->internal_md.type == DIR_CONTINUE)
    {
      P_r(&pentry_parent->object.dir_cont.pdir_begin->lock);
      parent_handle = pentry_parent->object.dir_cont.pdir_begin->object.dir_begin.handle;
      V_r(&pentry_parent->object.dir_cont.pdir_begin->lock);
    }

  found_attrs.asked_attributes = pclient->attrmask;

  *new_entry = cache_inode_lookup_sw(pentry_parent,
				     pname, &found_attrs,
				     ht, pclient, pcontext, pstatus,
				     FALSE);
  if (*new_entry != NULL)
    {
      *created = FALSE;
      if (exclusive)
	{
	  if (!verf) /* GUARDEF4 */
	    {
	      V_w(&pentry_parent->lock);
	      *pstatus = CACHE_INODE_ENTRY_EXISTS;
	      return *pstatus;
	    }
	  else
	    {
	      if (!((found_attrs.atime.seconds == verfpieces[0]) &&
		    (found_attrs.mtime.seconds == verfpieces[1])))
		{
		  V_w(&pentry_parent->lock);
		  *pstatus = CACHE_INODE_ENTRY_EXISTS;
		  return *pstatus;
		}
	    }
	}

      /* UNCHECKED4 or EXCLUSIVE4/EXCLUSIVE4_1 with matching verifier */
      *truncated = FALSE;
      if ((rc = state_share_open(transaction,
				 &((*new_entry)->object.file.handle),
				 pcontext,
				 share_access,
				 share_deny,
				 uid))
	  != 0)
	{
	  V_w(&pentry_parent->lock);
	  return (*pstatus = CACHE_INODE_STATE_ERROR);
	}
      
      /* If the filesize is set to 0 for UNCHECKED4, the file should
	 be truncated, (unless it's locked, we don't have write
	 access, or someone has a SHARE_DENY) */
      
      if (!verf &&
	  (attrs->asked_attributes & FSAL_ATTR_SIZE) &&
	  (attrs->filesize == 0))
	{
	  memset(attrs, 0, sizeof(fsal_attrib_list_t));
	  attrs->asked_attributes |= FSAL_ATTR_SIZE;
	  if ((privstatus = cache_inode_setattr(*new_entry,
						attrs,
						ht,
						pclient,
						pcontext,
						state_anonymous_stateid,
						&privstatus))
	      == CACHE_INODE_SUCCESS)
	    *truncated = TRUE;
	}
      
      V_w(&pentry_parent->lock);
      *pstatus = CACHE_INODE_SUCCESS;
      return *pstatus;
    }

  memset(&found_attrs, 0, sizeof(found_attrs));

  found_attrs.asked_attributes = pclient->attrmask;

  fsal_status = FSAL_create(&parent_handle,
			    pname, pcontext, attrs->mode,
			    &new_handle, &found_attrs);

  if(FSAL_IS_ERROR(fsal_status) && (fsal_status.major != ERR_FSAL_NOT_OPENED))
    {
      *pstatus = cache_inode_error_convert(fsal_status);
      V_w(&pentry_parent->lock);
      return *pstatus;
    }
  
  *created = 1;
  
  fsal_data.handle = new_handle;
  fsal_data.cookie = DIR_START;
  *new_entry = cache_inode_new_entry(&fsal_data, &found_attrs,
				    REGULAR_FILE, NULL, NULL,
				    ht, pclient, pcontext,
				    TRUE, /* This is a creation and not a population */
				    pstatus);
  if (new_entry == NULL)
    {
      *pstatus = CACHE_INODE_INSERT_ERROR;
      V_w(&pentry_parent->lock);
      return *pstatus;
    }

  /* Add this entry to the directory */
  *pstatus = cache_inode_add_cached_dirent(pentry_parent,
					   pname, *new_entry,
					   NULL, ht,
					   pclient, pcontext,
					   pstatus);
  if (*pstatus != CACHE_INODE_SUCCESS)
    {
      V_w(&pentry_parent->lock);
      return *pstatus;
    }

  /* Update the parent cached attributes */
  if(pentry_parent->internal_md.type == DIR_BEGINNING)
    dir_begin = &pentry_parent->object.dir_begin;
  else
    dir_begin = &pentry_parent->object.dir_cont.pdir_begin->object.dir_begin;
  
  dir_begin->attributes.mtime.seconds = time(NULL);
  dir_begin->attributes.mtime.nseconds = 0;
  dir_begin->attributes.ctime = dir_begin->attributes.mtime;
  
  /* valid the parent */
  *pstatus = cache_inode_valid(pentry_parent,
			       CACHE_INODE_OP_SET,
			       pclient);

  if ((rc = state_share_open(transaction,
			     &((*new_entry)->object.file.handle),
			     pcontext,
			     share_access,
			     share_deny,
			     uid))
      != 0)
    {
      V_w(&pentry_parent->lock);
      return (*pstatus = CACHE_INODE_STATE_ERROR);
    }

  if (verf)
    {
      attrs->asked_attributes |= (FSAL_ATTR_ATIME | FSAL_ATTR_MTIME);
      attrs->atime.seconds = verfpieces[0];
      attrs->mtime.seconds = verfpieces[1];
    }
  
  cache_inode_setattr(*new_entry, attrs, ht, pclient,
		      pcontext, state_anonymous_stateid, &privstatus);

  /* release the lock for the parent */
  V_w(&pentry_parent->lock);
  
  *pstatus = CACHE_INODE_SUCCESS;
  return *pstatus;
}                               /* cache_inode_open_by_name */
