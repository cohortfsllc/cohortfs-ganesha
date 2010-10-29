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

hash_table_t* openref_ht = NULL;
cache_inode_openref_t* openref_pool = NULL;

/**
 * openref_init: initialise open reference counting
 */

int openref_init(cache_inode_openref_params_t params)
{
  openref_ht = HashTable_Init(params.hparam);
  if (!openref_ht)
    return 1;
  STUFF_PREALLOC(openref_pool, params.nb_openref_prealloc,
		 cache_inode_openref_t, next_alloc);
  if (!openref_pool)
    return 1;

  return 0;
}

unsigned long cache_inode_openref_hash_func(p_hash_parameter_t param,
					    hash_buffer_t* key)
{
  cache_inode_openref_key_t* okey = (cache_inode_openref_key_t*) key->pdata;

  return FSAL_Handle_to_HashIndex(&(okey->handle), okey->uid,
				  param->alphabet_length,
				  param->index_size);
}

unsigned long cache_inode_openref_rbt_func(p_hash_parameter_t param,
					   hash_buffer_t* key)
{
  cache_inode_openref_key_t* okey = (cache_inode_openref_key_t*) key->pdata;

  return FSAL_Handle_to_RBTIndex(&(okey->handle), okey->uid);
}

int cache_inode_display_openref(hash_buffer_t* key, char* str)
{
  return 0;
}

int cache_inode_compare_key_openref(hash_buffer_t* key1, hash_buffer_t* key2)
{
  cache_inode_openref_key_t* okey1 = (cache_inode_openref_key_t*)
    key1->pdata;
  cache_inode_openref_key_t* okey2 = (cache_inode_openref_key_t*)
    key2->pdata;
  fsal_status_t status;

  if (okey1->uid == okey2->uid)
    return FSAL_handlecmp(&(okey1->handle), &(okey2->handle),
			  &status);
  else
    return 1;
}

cache_inode_status_t cache_inode_get_openref(fsal_handle_t* handle,
					     uint32_t share_access,
					     uid_t uid,
					     fsal_op_context_t*  pcontext,
					     cache_inode_openref_t** openref)
{
  cache_inode_openref_key_t okey;
  hash_buffer_t key, val;
  int rc;
  int currentmode = 0;
  bool_t tostore = true;
  fsal_status_t fsal_status;

  *openref = NULL;
  
  okey.handle = *handle;
  okey.uid = uid;

  key.pdata = (caddr_t) &okey;
  key.len = sizeof(cache_inode_openref_key_t);

  rc = HashTable_Get(openref_ht, &key, &val);
  if (rc = HASHTABLE_SUCCESS)
    {
      currentmode = (*openref)->openflags;
      *openref = (cache_inode_openref_t*) val.pdata;
      if ((currentmode == FSAL_O_RDWR) ||
	  ((currentmode == FSAL_O_RDONLY) &&
	   (share_access == OPEN4_SHARE_ACCESS_READ)) ||
	  ((currentmode == FSAL_O_WRONLY) &&
	   (share_access == OPEN4_SHARE_ACCESS_WRITE)))
	return CACHE_INODE_SUCCESS;
      else
	{
	  tostore = false;
	  fsal_status = FSAL_close(&((*openref)->descriptor));
	  if(FSAL_IS_ERROR(fsal_status))
	    return cache_inode_error_convert(fsal_status);
	}
    }
  else if (rc != HASHTABLE_ERROR_NO_SUCH_KEY)
    return CACHE_INODE_HASH_TABLE_ERROR;

  if (tostore)
    {
      GET_PREALLOC((*openref), openref_pool, 1, cache_inode_openref_t,
		   next_alloc);
      if (!(*openref))
	return CACHE_INODE_MALLOC_ERROR;
      (*openref)->refcount = 0;
    }

  if (currentmode != FSAL_O_RDWR)
    {
      if (!currentmode)
	{
	  if (share_access == OPEN4_SHARE_ACCESS_READ)
	    currentmode = FSAL_O_RDONLY;
	  else if (share_access == OPEN4_SHARE_ACCESS_WRITE)
	    currentmode = FSAL_O_WRONLY;
	  else
	    currentmode = FSAL_O_RDWR;
	}
      else if (((currentmode == FSAL_O_RDONLY) &&
		(share_access | OPEN4_SHARE_ACCESS_WRITE)) ||
	       ((currentmode == FSAL_O_WRONLY) &&
		(share_access | OPEN4_SHARE_ACCESS_READ)))
	currentmode = FSAL_O_RDWR;
    }
  
  fsal_status = FSAL_open(handle, pcontext,
			  currentmode,
			  &((*openref)->descriptor),
			  NULL);

  if(FSAL_IS_ERROR(fsal_status))
    return cache_inode_error_convert(fsal_status);

  (*openref)->openflags = currentmode;

  if (tostore)
    {
      (*openref)->key = okey;
      key.pdata = (caddr_t) &((*openref)->key);
      rc = HashTable_Test_And_Set(openref_ht, &key, &val,
				  HASHTABLE_SET_HOW_SET_NO_OVERWRITE);
      if (rc != HASHTABLE_SUCCESS)
	{
	  FSAL_close(&((*openref)->descriptor));
	  RELEASE_PREALLOC((*openref), openref_pool, next_alloc);
	}
    }
  return CACHE_INODE_SUCCESS;
}
  
cache_inode_status_t cache_inode_kill_openref(cache_inode_openref_t* openref)
{
  hash_buffer_t key;
  cache_inode_status_t status = CACHE_INODE_SUCCESS;
  fsal_status_t fsal_status;
  
  if (openref->refcount)
    return CACHE_INODE_SUCCESS;

  key.pdata = (caddr_t) &(openref->key);
  key.len = sizeof(cache_inode_openref_key_t);

  if (HashTable_Del(openref_ht, &key, NULL, NULL) !=
      HASHTABLE_SUCCESS)
    status = CACHE_INODE_HASH_TABLE_ERROR;

  fsal_status = FSAL_close(&(openref->descriptor));
  if(FSAL_IS_ERROR(fsal_status))
    status = cache_inode_error_convert(fsal_status);

  RELEASE_PREALLOC(openref, openref_pool, next_alloc);
  
  return status;
}

/**
 *
 * cache_inode_open: opens the local fd on the cache.
 *
 * Opens the fd on  the FSAL
 *
 * @param pentry       [IN]  entry in file content layer whose content is to be accessed.
 * @param pclient      [IN]  ressource allocated by the client for the nfs management.
 * @param share_access [IN]  access requested by the client
 * @param share_deny   [IN]  access client wants to deny
 * @param clientid     [IN]  clientid
 * @param open_owner   [IN]  open_owner
 * @param stateid      [OUT] stateid
 * @param pcontext     [IN]  request context
 * @param uid          [IN]  mapped ID of requesting user
 * @param pstatus      [OUT] status of operation
 *
 * @return CACHE_INODE_SUCCESS is successful .
 *
 */

cache_inode_status_t cache_inode_open(cache_entry_t* pentry,
                                      cache_inode_client_t* pclient,
				      uint32_t share_access,
				      uint32_t share_deny,
				      clientid4 clientid,
				      open_owner4 open_owner,
				      stateid4* stateid,
                                      fsal_op_context_t*  pcontext,
				      uid_t uid,
                                      cache_inode_status_t*  pstatus)
{
  fsal_status_t fsal_status;
  int rc;
  sharestate existingstate;
  fsal_handle_t handle = pentry->object.file.handle;
  bool_t upgrade = false;
  cache_inode_openref_t* openref = NULL;

  if((pentry == NULL) || (pclient == NULL) || (pcontext == NULL) ||
     (pstatus == NULL) || !share_access ||
     (share_access & OPEN4_SHARE_ACCESS_BOTH) ||
     (share_deny & OPEN4_SHARE_DENY_BOTH))
    return CACHE_INODE_INVALID_ARGUMENT;

  if(pentry->internal_md.type != REGULAR_FILE)
    {
      *pstatus = CACHE_INODE_BAD_TYPE;
      return *pstatus;
    }

  rc = state_check_share(handle, share_access, share_deny);
  if (rc == ERR_STATE_CONFLICT)
    {
      rc = state_query_share(&handle, clientid, open_owner,
			     &existingstate);
      if (rc == ERR_STATE_NOENT)
	{
	  *pstatus = CACHE_INODE_STATE_CONFLICT;
	  state_unlock_filehandle(&handle);
	  return *pstatus;
	}
      else if (rc == ERR_STATE_NO_ERROR)
	{
	  rc = state_query_share(&handle, clientid, open_owner,
				 &existingstate);
	  if (rc != ERR_STATE_NO_ERROR)
	    {
	      *pstatus = CACHE_INODE_STATE_ERROR;
	      state_unlock_filehandle(&handle);
	      return *pstatus;
	    }
	  upgrade = true;
	}
    }
  else if (rc != ERR_STATE_NO_ERROR)
    {
      state_unlock_filehandle(&handle);
      *pstatus = CACHE_INODE_STATE_ERROR;
      return *pstatus;
    }

  *pstatus = cache_inode_get_openref(&handle, share_access, uid,
				     pcontext, &openref);

  if (*pstatus != CACHE_INODE_SUCCESS)
    {
      state_unlock_filehandle(&handle);
      return *pstatus;
    }

  if (!upgrade)
    {
      rc == state_create_share(&(pentry->object.file.handle), open_owner, clientid,
			       share_access, share_deny, openref, stateid);
      if (rc == ERR_STATE_PREEXISTS)
	upgrade = true;
      else if (rc == ERR_STATE_NO_ERROR)
	{
	  openref->refcount++;
	  *pstatus = CACHE_INODE_SUCCESS;
	}
      else
	{
	  if (openref->refcount == 0)
	    cache_inode_kill_openref(openref);
	  *pstatus = CACHE_INODE_STATE_ERROR;
	}
    }

  if (upgrade)
      if (!((share_access & ~existingstate.share_access) ||
	    (share_deny & ~existingstate.share_deny)))
	{
	  rc = state_upgrade_share(share_access, share_deny,
				   stateid);
	  if (rc == ERR_STATE_CONFLICT)
	    *pstatus = CACHE_INODE_STATE_CONFLICT;
	  else if (rc == ERR_STATE_NO_ERROR)
	    *pstatus = CACHE_INODE_SUCCESS;
	  else
	    *pstatus = CACHE_INODE_STATE_ERROR;
	}

  state_unlock_filehandle(&handle);
  return *pstatus;
}                               /* cache_inode_open */

/**
 *
 * cache_content_open: opens the local fd on  the cache.
 *
 * Opens the fd on  the FSAL
 *
 * @param pentry_dir  [IN]  parent entry for the file
 * @param pname       [IN]  name of the file to be opened in the parent directory
 * @param pentry_file [IN]  file entry to be opened
 * @param pclient     [IN]  ressource allocated by the client for the nfs management.
 * @param openflags   [IN]  flags to be used to open the file 
 * @param pcontent    [IN]  FSAL operation context
 * @pstatus           [OUT] returned status.
 *
 * @return CACHE_CONTENT_SUCCESS is successful .
 *
 */

cache_inode_status_t cache_inode_open_create_name(cache_entry_t* pentry_parent,
						  fsal_name_t* pname,
						  cache_entry_t** new_entry,
						  uint32_t share_access,
						  uint32_t share_deny,
						  bool_t exclusive,
						  fsal_attrib_list_t* attrs,
						  clientid4 clientid,
						  open_owner4 open_owner,
						  stateid4* stateid,
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
  cache_inode_create_arg_t create_arg;

  memset(&create_arg, 0, sizeof(cache_inode_create_arg_t));

  if((pentry_parent == NULL) || (pname == NULL) || (new_entry == NULL) ||
     (pclient == NULL) || (pcontext == NULL) || (pstatus == NULL) ||
     (ht == NULL) || (stateid == NULL) || (attrs == NULL))
    return CACHE_INODE_INVALID_ARGUMENT;

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

  *new_entry = cache_inode_lookup(pentry_parent,
				  pname, &found_attrs,
				  ht, pclient, pcontext, pstatus);
  if (new_entry != NULL)
    {
      *created = false;
      if (exclusive) /* GUARDEF4 */
	{
	  V_w(&pentry_parent->lock);
	  *pstatus = CACHE_INODE_ENTRY_EXISTS;
	  return *pstatus;
	}

      /* UNCHECKED4 */
      *truncated = false;
      if ((*pstatus = cache_inode_open(*new_entry, pclient,
				       share_access,
				       share_deny,
				       clientid,
				       open_owner,
				       stateid,
				       pcontext,
				       uid,
				       pstatus)) !=
	  CACHE_INODE_SUCCESS)
	{
	  V_w(&pentry_parent->lock);
	  return *pstatus;
	}
      

      /* If the filesize is set to 0, the file should be truncated,
	 (unless it's locked, we don't have write access, or someone
	 has a SHARE_DENY) */

      if ((attrs->asked_attributes & FSAL_ATTR_SIZE) &&
	  (attrs->filesize == 0))
	{
	  memset(attrs, 0, sizeof(fsal_attrib_list_t));
	  attrs->asked_attributes |= FSAL_ATTR_SIZE;
	  if ((privstatus = cache_inode_setattr(*new_entry,
						attrs,
						ht,
						pclient,
						pcontext,
						*stateid,
						&privstatus))
	      == CACHE_INODE_SUCCESS)
	    *truncated = true;
	}

      V_w(&pentry_parent->lock);
      *pstatus = CACHE_INODE_SUCCESS;
      return *pstatus;
    }

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
				    REGULAR_FILE, &create_arg, NULL,
				    ht, pclient, pcontext,
				    true, /* This is a creation and not a population */
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

  if ((*pstatus = cache_inode_open(*new_entry, pclient,
				   share_access,
				   share_deny,
				   clientid,
				   open_owner,
				   stateid,
				   pcontext,
				   uid,
				   pstatus)) !=
      CACHE_INODE_SUCCESS)
    {
      V_w(&pentry_parent->lock);
      return *pstatus;
    }

  cache_inode_setattr(*new_entry, attrs, ht, pclient,
	      pcontext, *stateid, &privstatus);

  /* release the lock for the parent */
  V_w(&pentry_parent->lock);
  
  *pstatus = CACHE_INODE_SUCCESS;
  return *pstatus;
}                               /* cache_inode_open_by_name */

/**
 *
 * cache_inode_close: closes the local fd in the FSAL.
 *
 * Closes the local fd in the FSAL.
 *
 * No lock management is done in this layer: the related pentry in the cache inode layer is
 * locked and will prevent from concurent accesses.
 *
 * @param pentry  [IN] entry in file content layer whose content is to be accessed.
 * @param pclient [IN]  ressource allocated by the client for the nfs management.
 * @pstatus       [OUT] returned status.
 *
 * @return CACHE_CONTENT_SUCCESS is successful .
 *
 */
cache_inode_status_t cache_inode_close(cache_entry_t * pentry,
                                       cache_inode_client_t * pclient,
                                       cache_inode_status_t * pstatus,
				       stateid4* stateid)
{
  fsal_status_t fsal_status;
  taggedstate state;
  int rc;

  if((pentry == NULL) || (pclient == NULL) || (pstatus == NULL) ||
     (stateid == NULL))
    return CACHE_CONTENT_INVALID_ARGUMENT;

  if(pentry->internal_md.type != REGULAR_FILE)
    {
      *pstatus = CACHE_INODE_BAD_TYPE;
      return *pstatus;
    }

  rc = state_retrieve_state(*stateid, &state);

  if (rc != ERR_STATE_NO_ERROR)
    {
      *pstatus = CACHE_INODE_STATE_ERROR;
      return *pstatus;
    }

  if (state_delete_share(state.u.share.stateid) != ERR_STATE_NO_ERROR)
    {
      *pstatus = CACHE_INODE_STATE_ERROR;
      return *pstatus;
    }

  state.u.share.openref->refcount--;

  if (state.u.share.openref->refcount == 0)
    cache_inode_kill_openref(state.u.share.openref);

  memset(stateid->other, 12, 0);
  stateid->seqid = NFS4_UINT32_MAX;
  
  *pstatus = CACHE_CONTENT_SUCCESS;
  return *pstatus;
}                               /* cache_content_close */

cache_inode_status_t cache_inode_downgrade(cache_entry_t * pentry,
					   cache_inode_client_t * pclient,
					   cache_inode_status_t * pstatus,
					   uint32_t share_access,
					   uint32_t share_deny,
					   stateid4* stateid)
{
  int rc;
  taggedstate state;

  rc = state_retrieve_state(*stateid, &state);
  if (rc != ERR_STATE_NO_ERROR)
    {
      *pstatus = CACHE_INODE_STATE_ERROR;
      return *pstatus;
    }

  if ((state.u.share.share_access == share_access) &&
      (state.u.share.share_deny == share_deny))
    {
      *pstatus = CACHE_INODE_SUCCESS;
      return *pstatus;
    }

  if (state_downgrade_share(share_access, share_deny, stateid))
    *pstatus = CACHE_INODE_STATE_ERROR;
  else
    *pstatus = CACHE_INODE_SUCCESS;

  return *pstatus;
}
