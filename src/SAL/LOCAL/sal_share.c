/*
 *
 * Copyright (C) 2010, The Linux Box, inc.
 * All Rights Reserved
 *
 * Contributor: Adam C. Emerson
 */

#include "sal.h"
#include "stuff_alloc.h"
#include "nfs_core.h"
#include "log_macros.h"
#include "sal_internal.h"
#include "err_cache_inode.h"

void hash_open_owner(open_owner_key_t* owner, uint32_t* h1,
		     uint32_t* h2);

/************************************************************************
 * File Private Lock Functions
 *
 * This functionality is used directly by exported share calls and
 * nowhere else.
 ***********************************************************************/

static int
acquire_share_state(open_owner_t* owner,
		    perfile_state_t* perfile,
		    bool_t* created,
		    state_t** state)
{
    hash_buffer_t key, val;
    int rc;

    if (created) {
	 *created = 0;
    }

    GetFromPool(*state, &state_pool, state_t);
    if (!(*state)) {
	 return ERR_STATE_FAIL;
    }

    (*state)->state.share.open_owner = owner;
    (*state)->perfile = perfile;
    key.pdata = (caddr_t) *state;
    key.len = sizeof(state_t);
    (*state)->type = STATE_SHARE;
    val.pdata = (caddr_t) *state;
    val.len = sizeof(state_t);
    rc = HashTable_Set_Or_Fetch(share_state_table, &key, &val);
    if (rc == HASHTABLE_ERROR_KEY_ALREADY_EXISTS) {
	 ReleaseToPool(*state, &state_pool);
	 *state = (state_t*) val.pdata;
	 return ERR_STATE_NO_ERROR;
    } else if (rc != HASHTABLE_SUCCESS) {
	 ReleaseToPool(*state, &state_pool);
	 *state = NULL;
	 return ERR_STATE_FAIL;
    }

    (*state)->clientid = owner->key.clientid;
    if (rc = assign_stateid(*state) != 0) {
	 ReleaseToPool(*state, &state_pool);
	 *state = NULL;
	 if (created) {
	      *created = FALSE;
	 }
	 return rc;
    } else {
	 if (created) {
	      *created = TRUE;
	 }
	 return ERR_STATE_NO_ERROR;
    }
}

static void
maybe_kill_open_owner(open_owner_t* owner)
{
     hash_buffer_t key;

     pthread_mutex_lock(&(owner->mutex));
     if (--(owner->refcount) == 0) {
	  key.pdata = (caddr_t) &(owner->key);
	  key.len = sizeof(open_owner_key_t);
	  HashTable_Del(open_owner_table, &key, NULL, NULL);
	  pthread_mutex_unlock(&owner->mutex);
	  ReleaseToPool(owner, &open_owner_pool);
     } else {
	  pthread_mutex_unlock(&owner->mutex);
     }
}

static int
acquire_open_owner(char* name, size_t len,
		   clientid4 clientid,
		   bool_t* created,
		   open_owner_t** owner)
{
     hash_buffer_t key, val;
     int rc;
     
     if (created) {
	  *created = 0;
     }
     
     GetFromPool(*owner, &open_owner_pool, open_owner_t);
     if (!*owner) {
	  return ERR_STATE_FAIL;
     }
     
     (*owner)->key.clientid = clientid;
     (*owner)->key.owner_len = len;
     memcpy((*owner)->key.owner_val, name, len);
     key.pdata = (caddr_t) &((*owner)->key);
     key.len = sizeof(open_owner_key_t);
     (*owner)->seqid = 0;
     (*owner)->refcount = 0; /* Every open_owner gets created with a
				refcount of 1, to prevent races. */
     (*owner)->last_response = NULL;
     pthread_mutex_init(&((*owner)->mutex), NULL);
     val.pdata = (caddr_t) *owner;
     val.len = sizeof(open_owner_t);
     rc = HashTable_Set_Or_Fetch(open_owner_table, &key, &val);
     if (rc == HASHTABLE_SUCCESS) {
	  if (created) {
	       *created = TRUE;
	       return ERR_STATE_NO_ERROR;
	  }
     } else if (rc == HASHTABLE_ERROR_KEY_ALREADY_EXISTS) {
	  ReleaseToPool(*owner, &open_owner_pool);
	  *owner = (open_owner_t*) val.pdata;
	  return ERR_STATE_NO_ERROR;
     } else {
	  ReleaseToPool(*owner, &open_owner_pool);
	  *owner = NULL;
	  return ERR_STATE_FAIL;
     }
}

static int
share_state_cmp_func(hash_buffer_t* key1,
		     hash_buffer_t* key2)
{
     fsal_status_t status;
     state_t* state1 = (state_t*) key1->pdata;
     state_t* state2 = (state_t*) key2->pdata;
     fsal_handle_t* handle1 = &state1->perfile->handle;
     fsal_handle_t* handle2 = &state2->perfile->handle;
     open_owner_key_t* open_owner1 =
	  &(state1->state.share.open_owner->key);
     open_owner_key_t* open_owner2 =
	  &(state2->state.share.open_owner->key);

     if (FSAL_handlecmp(handle1, handle2,
			&status)) {
	  return !(open_owners_equal(open_owner1, open_owner2));
     } else {
	  return 0;
     }
}

static unsigned int 
share_state_hash_func(hash_parameter_t* hashparm,
		     hash_buffer_t* keybuff,
		     uint32_t* hashval,
		     uint32_t* rbtval)
{
     uint32_t h1 = 0;
     uint32_t h2 = 0;
     state_t* state = (state_t*) keybuff;
     fsal_handle_t* handle = &state->perfile->handle;
     open_owner_key_t* open_owner =
	  &(state->state.share.open_owner->key); 

     unsigned int rc = 1;

     h1 = (FSAL_Handle_to_HashIndex(handle, 0,
				    hashparm->alphabet_length,
				    hashparm->index_size));
     h2 = (FSAL_Handle_to_RBTIndex(handle, 0));
     hash_open_owner(open_owner, &h1, &h2);

     h1 %= hashparm->index_size;
     *hashval = h1;
     *rbtval = h2;
     return rc;
}

static int
localsalstringnoop(hash_buffer_t * pbuff, char *str)
{
     return 0;
}

static hash_parameter_t
share_state_hash_params = {
     .index_size = 29,
     .alphabet_length = 10,
     .nb_node_prealloc = 1000,
     .hash_func_key = NULL,
     .hash_func_rbt = NULL,
     .hash_func_both = share_state_hash_func,
     .compare_key = share_state_cmp_func,
     .key_to_str = localsalstringnoop,
     .val_to_str = localsalstringnoop
};

static int
share_conflict(perfile_state_t* perfile, uint32_t share_access, uint32_t share_deny)
{
    if (((share_access & OPEN4_SHARE_ACCESS_READ) &&
	 perfile->deny_readers) ||
	((share_access & OPEN4_SHARE_ACCESS_WRITE) &&
	 perfile->deny_writers) ||
	((share_deny & OPEN4_SHARE_DENY_READ) &&
	 (perfile->access_readers ||
	  perfile->anon_readers)) ||
	((share_deny & OPEN4_SHARE_DENY_WRITE) &&
	 (perfile->access_writers ||
	  perfile->anon_writers))) {
	 return ERR_STATE_CONFLICT;
    } else {
	 return ERR_STATE_NO_ERROR;
    }
}

static int
open_owner_cmp_func(hash_buffer_t* key1,
		    hash_buffer_t* key2)
{
     open_owner_key_t* owner1 = (open_owner_key_t*) key1->pdata;
     open_owner_key_t* owner2 = (open_owner_key_t*) key2->pdata;

     return !(open_owners_equal(owner1, owner2));
}

/* We split this out so we can compose it with other hash functions */

void
hash_open_owner(open_owner_key_t* owner, uint32_t* h1, uint32_t* h2)
{
     Lookup3_hash_buff_dual((char*)&(owner->clientid),
			    sizeof(clientid4), 
			    h1, h2);
     Lookup3_hash_buff_dual((char*)(owner->owner_val),
			    owner->owner_len, 
			    h1, h2);
}

static unsigned int 
open_owner_hash_func(hash_parameter_t* hashparm,
		     hash_buffer_t* key,
		     uint32_t* hashval,
		     uint32_t* rbtval)
{
     uint32_t h1 = 0;
     uint32_t h2 = 0;
     open_owner_key_t* owner = (open_owner_key_t*) key->pdata;
     
     hash_open_owner(owner, &h1, &h2);
     h1 %= hashparm->index_size;
     *hashval = h1;
     *rbtval = h2;
     return 1;
}

static hash_parameter_t
open_owner_hash_params = {
     .index_size = 29,
     .alphabet_length = 10,
     .nb_node_prealloc = 1000,
     .hash_func_key = NULL,
     .hash_func_rbt = NULL,
     .hash_func_both = open_owner_hash_func,
     .compare_key = open_owner_cmp_func,
     .key_to_str = localsalstringnoop,
     .val_to_str = localsalstringnoop
};

static unsigned long
openref_hash_func(p_hash_parameter_t param,
		  hash_buffer_t* key)
{
     openref_key_t* okey = (openref_key_t*) key->pdata;

     return FSAL_Open_to_HashIndex(&(okey->handle), okey->uid,
				   param->alphabet_length,
				   param->index_size);
}

static unsigned long
openref_rbt_func(p_hash_parameter_t param,
		 hash_buffer_t* key)
{
     openref_key_t* okey = (openref_key_t*) key->pdata;
     
     return FSAL_Open_to_RBTIndex(&(okey->handle), okey->uid);
}

static int
openref_cmp_func(hash_buffer_t* key1, hash_buffer_t* key2)
{
     openref_key_t* okey1 = (openref_key_t*) key1->pdata;
     openref_key_t* okey2 = (openref_key_t*) key2->pdata;
     fsal_status_t status;

     return FSAL_opencmp(&(okey1->handle), okey1->uid, &(okey2->handle),
			 okey2->uid);
}

static int
acquire_openref(fsal_handle_t* handle,
		uint32_t share_access,
		uid_t uid,
		fsal_op_context_t* context,
		openref_t** outref,
		int* errsource)
{
     openref_key_t okey;
     hash_buffer_t key, val;
     int rc;
     fsal_openflags_t currentmode = 0;
     bool_t tostore = TRUE;
     fsal_status_t fsal_status;
     openref_t* openref = NULL;
  
     okey.handle = *handle;
     okey.uid = uid;
     
     key.pdata = (caddr_t) &okey;
     key.len = sizeof(openref_key_t);

     rc = HashTable_Get(openref_table, &key, &val);
     if (rc == HASHTABLE_SUCCESS) {
	  openref = (openref_t*) val.pdata;
	  currentmode = openref->openflags;
	  if ((currentmode == FSAL_O_RDWR) ||
	      ((currentmode == FSAL_O_RDONLY) &&
	       (share_access == OPEN4_SHARE_ACCESS_READ)) ||
	      ((currentmode == FSAL_O_WRONLY) &&
	       (share_access == OPEN4_SHARE_ACCESS_WRITE))) {
	       *outref = openref;
	       return ERR_STATE_NO_ERROR;
	  } else {
	       tostore = FALSE;
	       fsal_status = FSAL_close(&(openref->descriptor));
	       if(FSAL_IS_ERROR(fsal_status)) {
		    *errsource = ERROR_SOURCE_FSAL;
		    return fsal_status.major;
	       }
	  }
     } else if (rc != HASHTABLE_ERROR_NO_SUCH_KEY) {
	  *errsource = ERROR_SOURCE_HASHTABLE;
	  return rc;
     }
     
     if (tostore) {
	  GetFromPool(openref, &openref_pool, openref_t);
	  if (!openref) {
	       errsource = ERROR_SOURCE_SAL;
	       return ERR_STATE_FAIL;
	  }
	  openref->refcount = 0;
     }
     
     if (currentmode != FSAL_O_RDWR) {
	  if (!currentmode) {
	       if (share_access == OPEN4_SHARE_ACCESS_READ) {
		    currentmode = FSAL_O_RDONLY;
	       } else if (share_access == OPEN4_SHARE_ACCESS_WRITE) {
		    currentmode = FSAL_O_WRONLY;
	       } else {
		    currentmode = FSAL_O_RDWR;
	       }
	  }
	  else if (((currentmode == FSAL_O_RDONLY) &&
		    (share_access | OPEN4_SHARE_ACCESS_WRITE)) ||
		   ((currentmode == FSAL_O_WRONLY) &&
		    (share_access | OPEN4_SHARE_ACCESS_READ)))
	       currentmode = FSAL_O_RDWR;
     }
     
     fsal_status = FSAL_open(handle, context,
			     &currentmode,
			     &(openref->descriptor),
			     NULL);
     
     if(FSAL_IS_ERROR(fsal_status)) {
	  *errsource = ERROR_SOURCE_FSAL;
	  return fsal_status.major;
     }
     
     openref->openflags = currentmode;
     
     if (tostore) {
	  openref->key = okey;
	  key.pdata = (caddr_t) &(openref->key);
	  val.pdata = (caddr_t) openref;
	  val.len = sizeof(openref_t);
	  rc = HashTable_Test_And_Set(openref_table, &key, &val,
				      HASHTABLE_SET_HOW_SET_NO_OVERWRITE);
	  if (rc != HASHTABLE_SUCCESS) {
	       FSAL_close(&(openref->descriptor));
	       ReleaseToPool(openref, &openref_pool);
	       *errsource = ERROR_SOURCE_HASHTABLE;
	       return rc;
	  }
     }
     *outref = openref;
     return ERR_STATE_NO_ERROR;
}

static int
get_openref(fsal_handle_t* handle,
	    uid_t uid,
	    openref_t** outref,
	    int* errsource)
{
     openref_key_t okey;
     hash_buffer_t key, val;
     int rc;
     openref_t* openref = NULL;
  
     okey.handle = *handle;
     okey.uid = uid;
     
     key.pdata = (caddr_t) &okey;
     key.len = sizeof(openref_key_t);

     rc = HashTable_Get(openref_table, &key, &val);
     if (rc == HASHTABLE_SUCCESS) {
	  openref = (openref_t*) val.pdata;
	  *outref = openref;
	  return ERR_STATE_NO_ERROR;
     } else if (rc != HASHTABLE_ERROR_NO_SUCH_KEY) {
	  *errsource = ERROR_SOURCE_HASHTABLE;
	  return rc;
     } else {
	  *errsource = ERROR_SOURCE_SAL;
	  return ERR_STATE_NOENT;
     }
}     


static int
maybe_kill_openref(openref_t* openref,
		   int* errsource)
{
     hash_buffer_t key;
     fsal_status_t fsal_status;
     int rc;
     
     if (openref->refcount) {
	  return 0;
     }
     
     key.pdata = (caddr_t) &(openref->key);
     key.len = sizeof(openref_key_t);
     
     if ((rc = HashTable_Del(openref_table, &key, NULL, NULL))
	 != HASHTABLE_SUCCESS) {
	  *errsource = ERROR_SOURCE_HASHTABLE;
	  return rc;
     }
     
     fsal_status = FSAL_close(&(openref->descriptor));
     if(FSAL_IS_ERROR(fsal_status)) {
	  *errsource = ERROR_SOURCE_FSAL;
	  return fsal_status.major;
     }
     
     ReleaseToPool(openref, &openref_pool);
     return ERR_STATE_NO_ERROR;
}

static hash_parameter_t
openref_hash_params = {
     .index_size = 29,
     .alphabet_length = 10,
     .hash_func_key = openref_hash_func,
     .hash_func_rbt = openref_rbt_func,
     .compare_key = openref_cmp_func,
     .key_to_str = localsalstringnoop,
     .val_to_str = localsalstringnoop
};

int
start_anon(fsal_handle_t *handle,
	   int uid,
	   fsal_file_t** descriptor,
	   fsal_op_context_t* context,
	   uint32_t access)
{
     perfile_state_t* perfile;
     int rc;
     int errsource;
     bool_t perfile_lock_acquired = FALSE;
     openref_t* openref;
     
     /* Retrieve or create header for per-filehandle chain */
     
     if ((rc = acquire_perfile_state(handle, &perfile)) !=
	 ERR_STATE_NO_ERROR) {
	  LogMajor(COMPONENT_STATES,
		   "state_share_open: could not "
		   "find/create per-file state header.");
	  rc = CACHE_INODE_STATE_ERROR;
	  goto out;
     }
     
     if (pthread_rwlock_wrlock(&(perfile->lock)) != 0) {
	  rc = CACHE_INODE_STATE_ERROR;
	  goto out;
     } else {
	  perfile_lock_acquired = TRUE;
     }

     if ((rc = share_conflict(perfile, access,
			      OPEN4_SHARE_DENY_NONE))
	 != 0) {
	  rc = ERR_CACHE_INODE_STATE_CONFLICT;
	  goto out;
     }

     if ((acquire_openref(handle, access, uid, context,
			  &openref, &errsource)) != 0)
     {
	  if (errsource == ERROR_SOURCE_FSAL) {
	       fsal_status_t fsal_status;
	       fsal_status.major = rc;
	       rc = cache_inode_error_convert(fsal_status);
	  } else if (errsource == ERROR_SOURCE_HASHTABLE) {
	       rc = ERR_CACHE_INODE_HASH_TABLE_ERROR;
	  } else {
	       rc = ERR_CACHE_INODE_STATE_ERROR;
	  }
	  goto out;
     }

     openref->refcount++;
     *descriptor = &(openref->descriptor);

     if (access & OPEN4_SHARE_ACCESS_READ) {
	  perfile->anon_readers++;
     }
     if (access & OPEN4_SHARE_ACCESS_WRITE) {
	  perfile->anon_writers++;
     }
     
     rc = CACHE_INODE_SUCCESS;
out:
     if (perfile_lock_acquired) {
	  pthread_rwlock_unlock(&(perfile->lock));
	  perfile_lock_acquired = FALSE;
     }
     return rc;
}

int
end_anon(fsal_handle_t *handle,
	 int uid,
	 uint32_t access)
{
     perfile_state_t* perfile;
     int rc;
     int errsource;
     bool_t perfile_lock_acquired = FALSE;
     openref_t* openref;
     
     /* Retrieve or create header for per-filehandle chain */
     
     if ((rc = acquire_perfile_state(handle, &perfile)) !=
	 ERR_STATE_NO_ERROR) {
	  LogMajor(COMPONENT_STATES,
		   "state_share_open: could not "
		   "find/create per-file state header.");
	  rc = CACHE_INODE_STATE_ERROR;
	  goto out;
     }
     
     if (pthread_rwlock_wrlock(&(perfile->lock)) != 0) {
	  rc = CACHE_INODE_STATE_ERROR;
	  goto out;
     } else {
	  perfile_lock_acquired = TRUE;
     }

     if (access & OPEN4_SHARE_ACCESS_READ) {
	  perfile->anon_readers--;
     }
     if (access & OPEN4_SHARE_ACCESS_WRITE) {
	  perfile->anon_writers--;
     }

     if ((get_openref(handle, uid, &openref, &errsource)) != 0)
     {
	  if (errsource == ERROR_SOURCE_HASHTABLE) {
	       rc = ERR_CACHE_INODE_HASH_TABLE_ERROR;
	  } else {
	       rc = ERR_CACHE_INODE_STATE_ERROR;
	  }
	  goto out;
     }

     openref->refcount--;
     
     rc = CACHE_INODE_SUCCESS;
out:
     if (perfile_lock_acquired) {
	  pthread_rwlock_unlock(&(perfile->lock));
	  perfile_lock_acquired = FALSE;
     }
     return rc;
}


/************************************************************************
 * Unexported Lock Functions
 *
 * Share functionality used by other systems within this SAL
 * realisation, but not exported.
 ***********************************************************************/

int
open_owners_equal(open_owner_key_t* owner1,
		  open_owner_key_t* owner2)
{
     return ((owner1->clientid == owner2->clientid) &&
	     (owner1->owner_len == owner2->owner_len) &&
	     (memcmp(owner1->owner_val,
		     owner2->owner_val,
		     owner1->owner_len) == 0));
}

hash_table_t*
init_open_owner_table(void)
{
     open_owner_table = HashTable_Init(open_owner_hash_params);
     return open_owner_table;
}

hash_table_t*
init_share_state_table(void)
{
     share_state_table = HashTable_Init(share_state_hash_params);
     return share_state_table;
}

hash_table_t*
init_openref_table(void)
{
     openref_table = HashTable_Init(openref_hash_params);
     return openref_table;
}

/************************************************************************
 * Share Functions
 *
 * These functions realise share state functionality.
 ***********************************************************************/

int
localstate_open_owner_begin41(clientid4 clientid,
			      open_owner4 nfs_open_owner,
			      state_share_trans_t** transaction)
{
     int rc = 0;
     perfile_state_t* perfile;
     state_t* state = NULL;
     open_owner_t* owner;
     bool_t owner_created = FALSE;
     
     if ((rc = acquire_open_owner(nfs_open_owner.owner.owner_val,
				  nfs_open_owner.owner.owner_len,
				  clientid,
				  &owner_created,
				  &owner))
	 != ERR_STATE_NO_ERROR) {
	  pthread_rwlock_unlock(&(perfile->lock));
	  return rc;
}
     
     /* Every share state increments the reference count on an open
	owner by 1.  If this is a new owner, the reference count has
	already been set to 1. */

     if (!owner_created) {
	  if ((rc = pthread_mutex_lock(&(owner->mutex))) != 0) {
	       pthread_rwlock_unlock(&(perfile->lock));
	       maybe_kill_open_owner(owner);
	       return ERR_STATE_FAIL;
	  }

	  owner->refcount++;
	  pthread_mutex_unlock(&(owner->mutex));
     }
     
     *transaction = (state_share_trans_t*) Mem_Alloc(sizeof(state_share_trans_t));
     (*transaction)->status = TRANSACT_LIVE;
     (*transaction)->share_state = NULL;
     (*transaction)->owner = owner;

     return ERR_STATE_NO_ERROR;
}

int
localstate_open_stateid_begin41(stateid4 stateid,
				state_share_trans_t** transaction)
{
     int rc = 0;
     state_t* share_state = NULL;
     perfile_state_t* perfile;

     if ((rc = lookup_state(stateid, &share_state))
	 != ERR_STATE_NO_ERROR) {
	  return rc;
     }

     *transaction = (state_share_trans_t*) Mem_Alloc(sizeof(state_share_trans_t));
     (*transaction)->status = TRANSACT_LIVE;
     (*transaction)->share_state = share_state;

     return ERR_STATE_NO_ERROR;
}

int
localstate_share_open(state_share_trans_t* transaction,
		      fsal_handle_t* handle,
		      fsal_op_context_t* context,
		      uint32_t share_access,
		      uint32_t share_deny,
		      uint32_t uid)
{
     int rc = 0;
     perfile_state_t* perfile;
     bool_t perfile_lock_acquired = FALSE;
     state_t* state;
     bool_t reopen = FALSE;
     int errsource = 0;
     openref_t* openref = NULL;

     if (transaction->status != TRANSACT_LIVE) {
	  return ERR_STATE_DEAD_TRANSACTION;
     }
     
     /* For shares, we acquire and lock the per-file state header with
	when an operation is called rather than at the transaction
	start.  This is because share states are the only ones that
	can legitimately be called with the current filehandle not
	being the file-handle on which the state is to be held. */

     if ((rc = acquire_perfile_state(handle, &perfile)) !=
	 ERR_STATE_NO_ERROR) {
	  LogMajor(COMPONENT_STATES,
		   "state_share_open: could not "
		   "find/create per-file state header.");
	  transaction->status = TRANSACT_FAILED;
	  transaction->errsource = ERROR_SOURCE_SAL;
	  rc = transaction->errcode = rc;
	  goto out;
     }
     
     if (pthread_rwlock_wrlock(&(perfile->lock)) != 0) {
	  transaction->status = TRANSACT_FAILED;
	  transaction->errsource = ERROR_SOURCE_SAL;
	  rc = transaction->errcode = ERR_STATE_FAIL;
	  goto out;
     } else {
	  perfile_lock_acquired = TRUE;
     }
	  
     /* If there is no share state, create or find it. */
     
     if (!(transaction->share_state)) {
	  if ((rc = acquire_share_state(transaction->owner,
					perfile,
					NULL,
					&state)) != 0)
	  {
	       transaction->status = TRANSACT_FAILED;
	       transaction->errsource = ERROR_SOURCE_SAL;
	       transaction->errcode = rc;
	       goto out;
	  } else if (!state) {
	       transaction->status = TRANSACT_FAILED;
	       rc = transaction->errcode = ERR_STATE_FAIL;
	       transaction->errsource = ERROR_SOURCE_SAL;
	       goto out;
	  }
	  transaction->share_state = state;
     } else {
	  reopen = TRUE;
	  state = transaction->share_state;
     }

     /* Reject invalid share and deny modes. */

     if ((share_access == 0) ||
	 (share_access & ~OPEN4_SHARE_ACCESS_BOTH) ||
	 (share_deny & ~OPEN4_SHARE_DENY_BOTH)) {
	  transaction->status = TRANSACT_FAILED;
	  rc = transaction->errcode = ERR_STATE_INVAL;
	  transaction->errsource = ERROR_SOURCE_SAL;
	  goto out;
     }

     /* Mask access and deny modes we already possess from the request */
     
     share_access &= ~state->state.share.share_access;
     share_deny &= ~state->state.share.share_deny;

     /* If we have everything we want, declare victory. */

     if ((share_access == 0) &&
	 (share_deny == 0)) {
	  rc = ERR_STATE_NO_ERROR;
	  goto out;
     }

     /* Check that what we want and don't have is available */

     if ((rc = share_conflict(perfile, share_access, share_deny))
	 != 0) {
	  transaction->status = TRANSACT_FAILED;
	  transaction->errcode = rc;
	  transaction->errsource = ERROR_SOURCE_SAL;
	  goto out;
     }

     /* If we are increasing access rights, we need to go through the
	FSAL. */

     if (share_access) {
	  if ((acquire_openref(handle, share_access, uid, context,
			       &openref, &errsource)) != 0)
	  {
	       transaction->status = TRANSACT_FAILED;
	       transaction->errsource = errsource;
	       transaction->errcode = rc;
	       rc = (transaction->errsource == ERROR_SOURCE_SAL) ?
		    (transaction->errcode) :
		    ERR_STATE_FAIL;
	       goto out;
	  } else {
	       transaction->share_state->state.share.openref = openref;
	       state->state.share.share_access |= share_access;
	       if (share_access | OPEN4_SHARE_ACCESS_READ) {
		    perfile->access_readers++;
	       }
	       if (share_access | OPEN4_SHARE_ACCESS_WRITE) {
		    perfile->access_writers++;
	       }
	  }
     }

     /* If we have increased deny rights, we need to record them */

     if (share_deny) {
	  state->state.share.share_deny |= share_deny;
	  if (share_deny | OPEN4_SHARE_DENY_READ) {
	       perfile->deny_readers++;
	  }
	  if (share_deny | OPEN4_SHARE_DENY_WRITE) {
	       perfile->deny_writers++;
	  }
     }

     /* If this is a new open, increase the reference count on the
	file descriptor and the open owner. */

     if (!reopen) {
	  transaction->share_state->state.share.openref->refcount++;
	  transaction->share_state->state.share.open_owner->refcount++;
     }

     rc = ERR_STATE_NO_ERROR;
out:
     if (perfile_lock_acquired) {
	  pthread_rwlock_unlock(&(perfile->lock));
	  perfile_lock_acquired = FALSE;
     }
     return rc;
}

int
localstate_share_close(state_share_trans_t* transaction,
		       fsal_handle_t* handle,
		       fsal_op_context_t* context)
{
     int rc = 0;
     perfile_state_t* perfile;
     bool_t perfile_lock_acquired = FALSE;
     state_t* state;
     int errsource = 0;

     if (transaction->status != TRANSACT_LIVE) {
	  return ERR_STATE_DEAD_TRANSACTION;
     }
		      
     if (!transaction->share_state) {
	  transaction->errsource = ERROR_SOURCE_SAL;
	  transaction->status = TRANSACT_FAILED;
	  rc = transaction->errcode = ERR_STATE_BAD;
	  goto out;
     }
     
     if ((rc = acquire_perfile_state(handle, &perfile)) !=
	 ERR_STATE_NO_ERROR) {
	  LogMajor(COMPONENT_STATES,
		   "state_share_close: could not "
		   "find/create per-file state header.");
	  transaction->errsource = ERROR_SOURCE_SAL;
	  transaction->status = TRANSACT_FAILED;
	  rc = transaction->errcode = ERR_STATE_FAIL;
     } 
     
     if (pthread_rwlock_wrlock(&(perfile->lock)) != 0) {
	  transaction->status = TRANSACT_FAILED;
	  transaction->errsource = ERROR_SOURCE_SAL;
	  rc = transaction->errcode = ERR_STATE_FAIL;
	  goto out;
     } else {
	  perfile_lock_acquired = TRUE;
     }

     if (state->state.share.share_access | OPEN4_SHARE_ACCESS_READ) {
	  perfile->access_readers--;
     }
     if (state->state.share.share_access | OPEN4_SHARE_ACCESS_WRITE) {
	  perfile->access_writers--;
     }
     if (state->state.share.share_deny | OPEN4_SHARE_DENY_READ) {
	  perfile->deny_readers--;
     }
     if (state->state.share.share_deny | OPEN4_SHARE_DENY_READ) {
	  perfile->deny_readers--;
     }

     state->state.share.openref->refcount--;

     /* kill_share_state(state); */

     if (rc = maybe_kill_openref((transaction->share_state->
				  state.share.openref),
				 &errsource) != 0) {
	  transaction->errsource = errsource;
	  transaction->status = TRANSACT_PYRRHIC_VICTORY;
	  transaction->errcode = rc;
     }

     rc = ERR_STATE_NO_ERROR;

out:
     if (perfile_lock_acquired) {
	  pthread_rwlock_unlock(&(perfile->lock));
	  perfile_lock_acquired = FALSE;
     }
     return rc;
}

int
localstate_share_downgrade(state_share_trans_t* transaction,
			   fsal_handle_t* handle,
			   uint32_t share_access,
			   uint32_t share_deny)
{
     int rc = 0;
     perfile_state_t* perfile = NULL;
     uint32_t access_relinquished = 0;
     uint32_t deny_relinquished = 0;
     bool_t perfile_lock_acquired = FALSE;
     state_t* state;

     if (transaction->status != TRANSACT_LIVE) {
	  return ERR_STATE_DEAD_TRANSACTION;
     }
		      
     if (!transaction->share_state) {
	  transaction->errsource = ERROR_SOURCE_SAL;
	  transaction->status = TRANSACT_FAILED;
	  rc = transaction->errcode = ERR_STATE_BAD;
	  goto out;
     }
     
     if ((rc = acquire_perfile_state(handle, &perfile)) !=
	 ERR_STATE_NO_ERROR) {
	  LogMajor(COMPONENT_STATES,
		   "state_share_close: could not "
		   "find/create per-file state header.");
	  transaction->errsource = ERROR_SOURCE_SAL;
	  transaction->status = TRANSACT_FAILED;
	  rc = transaction->errcode = ERR_STATE_FAIL;
     } 
     
     if (pthread_rwlock_wrlock(&(perfile->lock)) != 0) {
	  transaction->status = TRANSACT_FAILED;
	  transaction->errsource = ERROR_SOURCE_SAL;
	  rc = transaction->errcode = ERR_STATE_FAIL;
	  goto out;
     } else {
	  perfile_lock_acquired = TRUE;
     }

     access_relinquished = state->state.share.share_access &~ share_access;
     deny_relinquished = state->state.share.share_deny &~ share_deny;

     if (access_relinquished | OPEN4_SHARE_ACCESS_READ) {
	  perfile->access_readers--;
     }
     if (access_relinquished | OPEN4_SHARE_ACCESS_WRITE) {
	  perfile->access_writers--;
     }
     if (deny_relinquished | OPEN4_SHARE_DENY_READ) {
	  perfile->deny_readers--;
     }
     if (deny_relinquished | OPEN4_SHARE_DENY_READ) {
	  perfile->deny_readers--;
     }

     state->state.share.share_access = share_access;
     state->state.share.share_deny = share_deny;

     rc = ERR_STATE_NO_ERROR;

out:
     if (perfile_lock_acquired) {
	  pthread_rwlock_unlock(&(perfile->lock));
	  perfile_lock_acquired = FALSE;
     }
     return rc;
}

int
localstate_share_commit(state_share_trans_t* transaction)
{
     if ((transaction->status != TRANSACT_LIVE) &&
	 (transaction->status != TRANSACT_PYRRHIC_VICTORY)) {
	  return ERR_STATE_DEAD_TRANSACTION;
     }

     transaction->status = ((transaction->status = TRANSACT_LIVE) ?
			    TRANSACT_COMPLETED :
			    TRANSACT_PYRRHIC_VICTORY);

     if (transaction->share_state) {
	  transaction->share_state->stateid.seqid++;
     }
     maybe_kill_open_owner(transaction->owner);

     return ((transaction->status == TRANSACT_LIVE) ?
	     ERR_STATE_NO_ERROR :
	     transaction->errcode);
}

int
localstate_share_abort(state_share_trans_t* transaction)
{
     if (transaction->status != TRANSACT_LIVE) {
	  return ERR_STATE_DEAD_TRANSACTION;
     }

     transaction->status = (TRANSACT_ABORTED);

     maybe_kill_open_owner(transaction->owner);

     return ERR_STATE_NO_ERROR;
}

int
localstate_share_dispose_transaction(state_share_trans_t* transaction)
{
     if (transaction->status = TRANSACT_LIVE) {
	  localstatestate_share_abort(transaction);
     }

     Mem_Free(transaction);
     return ERR_STATE_NO_ERROR;
}

int
localstate_share_get_stateid(state_share_trans_t* transaction,
			     stateid4* stateid)
{
     if ((transaction->status != TRANSACT_COMPLETED) ||
	  !transaction->share_state) {
	  return ERR_STATE_NOENT;
     }

     *stateid = transaction->share_state->stateid;

     return ERR_STATE_NO_ERROR;
}

int
localstate_share_get_nfs4err(state_share_trans_t* transaction,
			     nfsstat4* error)
{
     if ((transaction->status == TRANSACT_LIVE) ||
	 (transaction->status == TRANSACT_ABORTED)) {
	  return ERR_STATE_NOENT;
     }

     if (transaction->status == TRANSACT_COMPLETED) {
	  *error = NFS4_OK;
     } else if (transaction->errsource == ERROR_SOURCE_SAL) {
	  *error = staterr2nfs4err(transaction->errcode);
     } else if (transaction->errsource == ERROR_SOURCE_FSAL) {
	  fsal_status_t fsal_status;
	  fsal_status.major = transaction->errcode;
	  *error =
	       nfs4_Errno(cache_inode_error_convert(fsal_status));
     } else {
	  *error = NFS4ERR_SERVERFAULT;
     }

     return ERR_STATE_NO_ERROR;
}

int
localstate_start_anonread(fsal_handle_t* handle,
			  int uid,
			  fsal_op_context_t* context,
			  fsal_file_t** descriptor)
{
     return start_anon(handle,
		       uid,
		       descriptor,
		       context,
		       OPEN4_SHARE_ACCESS_READ);
}

int
localstate_start_anonwrite(fsal_handle_t* handle,
			   int uid,
			   fsal_op_context_t* context,
			   fsal_file_t** descriptor)
{
     return start_anon(handle,
		       uid,
		       descriptor,
		       context,
		       OPEN4_SHARE_ACCESS_WRITE);
}

int
localstate_end_anonread(fsal_handle_t* handle,
			int uid)
{
     return end_anon(handle, uid, OPEN4_SHARE_ACCESS_READ);
}

int
localstate_end_anonwrite(fsal_handle_t* handle,
			 int uid)
{
     return end_anon(handle, uid, OPEN4_SHARE_ACCESS_WRITE);
}

int
localstate_share_descriptor(fsal_handle_t* handle,
			    stateid4* stateid,
			    fsal_file_t** descriptor)
{
     int rc = 0;
     perfile_state_t* perfile;
     bool_t perfile_lock_acquired = FALSE;
     state_t* state;

     if ((rc = lookup_state(*stateid, &state))
	 != ERR_STATE_NO_ERROR) {
	  goto out;
     } else if (!state) {
	  rc = ERR_STATE_FAIL;
	  goto out;
     }

     if (pthread_rwlock_rdlock(&(state->perfile->lock)) != 0) {
	  rc = ERR_STATE_FAIL;
	  goto out;
     } else {
	  perfile_lock_acquired = TRUE;
     }

     *descriptor = &(state->state.share.openref->descriptor);
     rc = ERR_STATE_NO_ERROR;

out:
     if (perfile_lock_acquired) {
	  pthread_rwlock_unlock(&(perfile->lock));
     }

     return rc;
}
