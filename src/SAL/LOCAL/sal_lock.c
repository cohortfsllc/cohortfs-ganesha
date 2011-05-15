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

#if 0

/************************************************************************
 * File Private Lock Functions
 *
 * This functionality is used only by exported locking calls and
 * nowhere else.
 ***********************************************************************/

static int
lock_owner_cmp_func(hash_buffer_t* key1,
		    hash_buffer_t* key2)
{
     state_lock_owner_t* owner1 = (state_lock_owner_t*) key1->pdata;
     state_lock_owner_t* owner2 = (state_lock_owner_t*) key2->pdata;
     
     return !(state_compare_lockowner(owner1, owner2));
}


static unsigned int
hash_nfs3_lock_owner(state_lock_owner_t owner, uint32_t* h1, uint32_t* h2)
{
     LogCrit(COMPONENT_STATES,
	     "NFSv3 locks not yet implemented.\n");
     return 1;
}

static unsigned int
hash_nfs4_lock_owner(state_lock_owner_t* owner, uint32_t* h1, uint32_t* h2)
{
     Lookup3_hash_buff_dual((char*)&(owner->u.nfs4_owner.clientid),
			    sizeof(clientid4), 
			    h1, h2);
     
     Lookup3_hash_buff_dual((char*)(owner->u.nfs4_owner.owner_val),
			    owner->u.nfs4_owner.owner_len,
			    h1, h2);
     return 1;
}

static unsigned int 
lock_owner_hash_func(hash_parameter_t* hashparm,
		     hash_buffer_t* keybuff,
		     uint32_t* hashval,
		     uint32_t* rbtval)
{
     uint32_t h1 = 0;
     uint32_t h2 = 0;
     state_lock_owner_t* owner = (state_lock_owner_t*) keybuff;
     unsigned int rc = 0;

     rc = hash_lockowner(owner, &h1, &h2);

     h1 %= hashparm->index_size;
     *hashval = h1;
     *rbtval = h2;
     return rc;
}

static hash_parameter_t
lock_owner_hash_params = {
     .index_size = 29,
     .alphabet_length = 10,
     .nb_node_prealloc = 1000,
     .hash_func_key = NULL,
     .hash_func_rbt = NULL,
     .hash_func_both = lock_owner_hash_func,
     .compare_key = lock_owner_cmp_func,
     .key_to_str = NULL,
     .val_to_str = NULL 
};

static unsigned int 
lock_state_hash_func(hash_parameter_t* hashparm,
		     hash_buffer_t* keybuff,
		     uint32_t* hashval,
		     uint32_t* rbtval)
{
     uint32_t h1 = 0;
     uint32_t h2 = 0;
     state_t* lock_state = (state_t*) keybuff;
     fsal_handle_t* handle = &lock_state->perfile->handle;
     open_owner_key_t* open_owner
	  = (lock_state->state.lock.open_state ?
	     &(lock_state->state.lock.
	       open_state->state.share.open_owner->key) :
	     NULL);
     state_lock_owner_t* lock_owner
	  = &(lock_state->state.lock.lock_owner->key);

     unsigned int rc = 1;

     h1 = (FSAL_Handle_to_HashIndex(handle, 0,
				    hashparm->alphabet_length,
				    hashparm->index_size));
     h2 = (FSAL_Handle_to_RBTIndex(handle, 0));
     rc = hash_lockowner(lock_owner, &h1, &h2);
     if (open_owner) {
	  hash_open_owner(open_owner, &h1, &h2);
     }

     h1 %= hashparm->index_size;
     *hashval = h1;
     *rbtval = h2;
     return rc;
}

static hash_parameter_t
lock_state_hash_params = {
     .index_size = 29,
     .alphabet_length = 10,
     .nb_node_prealloc = 1000,
     .hash_func_key = NULL,
     .hash_func_rbt = NULL,
     .hash_func_both = lock_state_hash_func,
     .compare_key = lock_state_cmp_func,
     .key_to_str = localsalstringnoop,
     .val_to_str = localsalstringnoop
};

static int
acquire_lock_owner(state_lock_owner_t* ownerkey,
		   bool_t* created,
		   lock_owner_info_t** owner)
{
    hash_buffer_t key, val;
    int rc;

    if (created) {
	 *created = 0;
    }

    if ((ownerkey->owner_type == LOCKOWNER_INTERNAL) ||
	(ownerkey->owner_type == LOCKOWNER_EXTERNAL)) {
	 *owner = NULL;
	 LogCrit("acquire_lock_owner: Attempt made to create owner of special type.\n");
	 return ERR_STATE_FAIL;
    }

    GetFromPool(*owner, &lock_owner_pool, lock_owner_info_t);
    if (!*owner) {
	 return ERR_STATE_FAIL;
    }

    *owner->key = ownerkey;
    key.pdata = (caddr_t) &(owner->key);
    key.len = sizeof(state_lock_owner_t);
    *owner->seqid = 0;
    *owner->refcount = 1; /* Every lock_owner gets created with a
			     refcount of 1, to prevent races. */
    *owner->last_response = NULL;
    pthread_mutex_init(&(*owner->mutex), NULL);
    val.pdata = (caddr_t) *owner;
    val.len = sizeof(lock_owner_info_t);
    rc = HashTable_Set_Or_Fetch(lock_table, &key, &val);
    if (rc == HASHTABLE_SUCCESS) {
	 if (created) {
	      *created = TRUE;
	      return ERR_STATE_NO_ERROR;
	 }
    } else if (rc == HASHTABLE_ERROR_KEY_ALREADY_EXISTS) {
	 ReleaseToPool(*owner, &ownerpool);
	 *owner = (lock_owner_info_t*) val.pdata;
	 return ERR_STATE_NO_ERROR;
    } else {
	 ReleaseToPool(*owner, &ownerpool);
	 *owner = NULL;
	 return ERR_STATE_FAIL;
    }
}

static void
maybe_kill_lock_owner(&owner)
{
     hash_buffer_t key;

     pthread_mutex_lock(&(owner->mutex));
     if (--(owner->refcount) == 0) {
	  key.pdata = (caddr_t) &(owner->key);
	  key.len = sizeof(open_owner_key_t);
	  HashTable_Del(lock_owner_table, &key, NULL, NULL);
	  pthread_mutex_ulock(&owner->mutex);
	  ReleaseToPool(*owner, &owner_pool);
     } else {
	  pthread_mutex_ulock(&owner->mutex);
     }
}

static int
acquire_lock_state(lock_owner_info_t* owner,
		   state_t* open_state,
		   perfile_state_t* perfile,
		   bool_t* created,
		   state_t** lock_state)
{
    hash_buffer_t key, val;
    int rc;

    if (created) {
	 *created = 0;
    }

    GetFromPool(*lock_state, &state_pool, state_t);
    if (!(*lock_state)) {
	 return ERR_STATE_FAIL;
    }

    *lock_state->state.lock.lock_owner = owner;
    *lock_state->state.lock.open_state = open_state;
    *lock_state->perfile = perfile;
    key.pdata = (caddr_t) *lock_state;
    key.len = sizeof(state_t);
    *lock_state->type = LOCKSTATE;
    val.pdata = (caddr_t) *lock_state;
    val.len = sizeof(state_t);
    rc = HashTable_Set_Or_Fetch(lock_state_table, &key, &val);
    if (rc == HASHTABLE_ERROR_KEY_ALREADY_EXISTS) {
	 ReleaseToPool(*lock_state, &state_pool);
	 *lock_state = (lock_owner_info_t*) val.pdata;
	 return ERR_STATE_NO_ERROR;
    } else if (rc != HASHTABLE_SUCCESS) {
	 ReleaseToPool(*lock_state, &state_pool);
	 *lock_state = NULL;
	 return ERR_STATE_FAIL;
    }

    if (owner->key.owner_type == LOCKOWNER_NFS3) {
	 /* Do NFS3 Things */
    } else if (owner->key.owner_type == LOCKOWNER_NFS4) {
	 lock_state->clientid = owner->key.u.nfs4_owner.clientid;
	 if (rc = assign_stateid(*lock_state) != 0) {
	      ReleaseToPool(*lock_state, &state_pool);
	      *lock_state = NULL;
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
    } else {
	 LogCrit(COMPONENT_STATES, "Attempt to initialise lock state for "
		 "unknown type of owner.\n");
	 return ERR_STATE_FIL;
    }
}

/************************************************************************
 * Unexported Lock Functions
 *
 * Locking functionality used by other systems within this SAL
 * realisation, but not exported.
 ***********************************************************************/

unsigned int
hash_lock_owner(state_lock_owner_t* owner, uint32_t* h1, uint32_t* h2)
{
     unsigned int rc = 0;
     
     switch(owner->owner_type) {
     case LOCKOWNER_NFS3:
	  *h1 = *h2 = (('N' << 0x18) |
		       ('F' << 0x10) |
		       ('S' << 0x08) |
		       ( 3  << 0x00));
	  rc = hash_nfs3_lockowner(owner, h1, h2);
	  break;

     case LOCKOWNER_NFS4:
	  *h1 = *h2 = (('N' << 0x18) |
		       ('F' << 0x10) |
		       ('S' << 0x08) |
		       ( 4  << 0x00));
	  rc = hash_nfs4_lockowner(owner, h1, h2);
	  break;

     default:
	  rc = 0;
	  LogCrit(COMPONENT_STATES,
		  "open_owner_hash_func: Owner type %d should never be stored in the hash table.\n",
		  owner->owner_type);
     }
}

hash_table_t*
init_lock_owner_table(void)
{
     lock_owner_table = HashTable_Init(lock_owner_hash_params);
     return lock_owner_table;
}


hash_table_t*
init_lock_state_table(void)
{
     lock_state_table = HashTable_Init(lock_state_hash_params);
     return lock_owner_table;
}



/************************************************************************
 * Public Lock Functions
 *
 * These functions realise lock state functionality.
 ***********************************************************************/

/**
 * localstate_open_to_lock_owner_begin41
 *
 * \param handle (in)
 *        The handle for the file to be locked
 * \param clientid (in)
 *        The clientid for this operation
 * \param open_stateid (in)
 *        The stateid supplied in the open_to_lock_owner4 structure
 * \param nfs_lock_owner (in)
 *        The lock owner for this operation
 * \param transaction (out)
 *        A pointer to the transaction for this operation
 *
 * This function initialises a transaction for a locking operation.
 * The goal is to keep all such operations consistent between
 * Ganesha's internal state and the substrate filesystem.  The
 * transaction begin operations exist partly to create a lock state
 * while honouring the semantics of the three supported protocols
 * (NLMv4, NFSv4.0, NFSv4.1) that can then be passed to operate,
 * commit, or abort without those functions needing version specific
 * variants.  Additionally, the begin, operate, write pattern will
 * prove useful in blocking locks.  In this implementation, it also
 * acquires a lock on filehandle state.  (This may not be required in
 * other implementations.)
 *
 * This function is for NFSv4.1 (we ignore seqids and don't worry
 * about saving responses.) and handles the case of the first lock on
 * a file.
 *
 * On success, transaction is set to point to a new transaction, on
 * failure it is NULL and an error code is returned.
 */

int
localstate_open_to_lock_owner_begin41(fsal_handle_t *handle,
				      clientid4 clientid,
				      stateid4 open_stateid,
				      lock_owner4 nfs_lock_owner,
				      state_lock_trans_t** transaction)
{
     int rc = 0;
     perfile_state_t* perfile;
     state_t* state = NULL;
     state_t* open_state = NULL;
     state_lock_owner_t owner_key;
     lock_owner_info_t* owner;
     int rc = 0;
     bool_t owner_created = FALSE;
     
     /* Retrieve or create header for per-filehandle chain */
     
     if ((rc = acquire_perfile_state(handle, &perfile)) !=
	 ERR_STATE_NO_ERROR) {
	  LogMajor(COMPONENT_STATES,
		   "state_open_to_lock_owner_begin41: could not "
		   "find/create per-file state header.");
	  return rc;
     }
     
     if (pthread_rwlock_wrlock(&(perfile->lock)) != 0) {
	  return ERR_STATE_FAIL;
     }
     
     if ((rc = lookup_state(open_stateid, &open_state))
	 != ERR_STATE_NO_ERROR) {
	  pthread_rwlock_unlock(perfile->lock);
	  return rc;
     }
     
     if (!open_state) {
	  pthread_rwlock_unlock(&(perfile->lock));
	  return ERR_STATE_FAIL;
     }
     
     if ((open_state->type != STATE_SHARE) ||
	 (open_state->clientid != clientid)) {
	  pthread_rwlock_unlock(perfile->lock);
	  return ERR_STATE_BAD;
     }
     
     ownerkey.owner_type = LOCKOWNER_NFS4;
     ownerkey.nfs4_owner.clientid = clientid;
     ownerkey.nfs4_owner.owner.owner_len
	  = nfs_lock_owner.owner.owner_len;
     memcpy(ownerkey.nfs4_owner.owner.val,
	    nfs_lock_owner.owner.owner_val, 
	    nfs_lock_owner.owner.owner_len);
     
     if ((rc = acquire_lock_owner(&owner_key,
				  &owner_created,
				  &owner))
	 != ERR_STATE_NO_ERROR) {
	  pthread_rwlock_unlock(perfile->lock);
	  return rc;
     }
     
     if ((rc = acquire_lock_state(&owner, open_state, perfile, NULL,
				  &state)) != 0) { 
	  pthread_rwlock_unlock(perfile->lock);
	  maybe_kill_lock_owner(&owner);
	  return rc;
     }

     /* Every lock state increments the reference count on a lock
	owner by 1.  If this is a new owner, the reference count has
	already been set to 1. */

     if (!owner_created) {
	  if ((rc = pthread_lock_mutex(lock_owner->mutex)) != 0) {
	       pthread_rwlock_unlock(perfile->lock);
	       maybe_kill_lock_owner(&owner);
	       return ERR_STATE_FAIL;
	  }

	  lock_owner->refcount++;
	  pthread_unlock_mutex(lock_owner->mutex);
     }
     
     *transaction = Mem_Alloc(sizeof(state_lock_transaction_t));
     *transaction->status = TRANSACT_LIVE;
     *transaction->lock_state = state;

     return ERR_STATE_NO_ERROR;
}

/**
 * localstate_exist_lock_owner_begin41
 *
 * \param handle (in)
 *        The handle for the file to be locked
 * \param clientid (in)
 *        The clientid for this operation
 * \param lock_stateid (in)
 *        The client supplied stateid
 * \param transaction (out)
 *        A pointer to the transaction for this operation
 *
 * This function initialises a lock transaction when we already have a
 * lock stateid.
 *
 * On success, transaction is set to point to a new transaction, on
 * failure it is NULL and an error code is returned.
 */

int
localstate_exist_lock_owner_begin41(fsal_handle_t *handle,
				    clientid4 clientid,
				    stateid4 lock_stateid,
				    state_lock_trans_t** transaction)
{
     int rc = 0;
     state_t* lock_state = NULL;
     int rc = 0;
     perfile_state_t* perfile;

     
     /* Retrieve or create header for per-filehandle chain */

     if ((rc = acquire_perfile_state(handle, &perfile)) !=
	 ERR_STATE_NO_ERROR) {
	  LogMajor(COMPONENT_STATES,
		   "state_exist_lock_owner_begin41: could not "
		   "find/create per-file state header.");
	  return rc;
     }
     
     if (pthread_rwlock_wrlock(&(perfile->lock)) != 0) {
	  return ERR_STATE_FAIL;
     }
     
     if ((rc = lookup_state(lock_stateid, &lock_state))
	 != ERR_STATE_NO_ERROR) {
	  pthread_rwlock_unlock(perfile->lock);
	  return rc;
     }
     
     if (!lock_state) {
	  pthread_rwlock_unlock(perfile->lock);
	  return ERR_STATE_FAIL;
     }
     
     if ((lock_state->type != STATE_LOCK) ||
	 (open_state->clientid != clientid)) {
	  pthread_rwlock_unlock(perfile->lock);
	  return ERR_STATE_BAD;
     }

     *transaction = Mem_Alloc(sizeof(state_lock_transaction_t));
     *transaction->status = TRANSACT_LIVE;
     *transaction->lock_state = state;
     
     return ERR_STATE_NO_ERROR;
}

#endif /* 0 */
