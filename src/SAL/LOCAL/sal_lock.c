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
#include "fsal.h"

/************************************************************************
 * File Private Lock Functions
 *
 * This functionality is used only by exported locking calls and
 * nowhere else.
 ***********************************************************************/

void
link_lock(lock_t** chain,
	  lock_t* lock)
{
     if (*chain) {
	  lock->next = *chain;
	  lock->prev = NULL;
	  (*chain)->prev = lock;
     } else {
	  lock->next = NULL;
     }
     lock->prev = NULL;
     *chain = lock;
}

void
unlink_lock(lock_t** chain,
	    lock_t* lock)
{
     if (lock->prev) {
	  lock->prev->next = lock->next;
     } else {
	  *chain = lock->next;
     }

     if (lock->next) {
	  lock->next->prev = lock->prev;
     }
}

lock_overlap_t
overlap(lock_t* lock1,
	lock_t* lock2)
{
     uint64_t lock1_last_byte = 0;
     uint64_t lock2_last_byte = 0;

     lock1_last_byte = ((lock1->length == NFS4_UINT64_MAX) ?
			NFS4_UINT64_MAX :
			lock1->offset + lock1->length - 1);
     lock2_last_byte = ((lock2->length == NFS4_UINT64_MAX) ?
			NFS4_UINT64_MAX :
			lock2->offset + lock2->length - 1);

     if (lock1->offset < lock2->offset) {
	  if (lock1_last_byte < lock2->offset) {
	       return LOCKS_DISJOINT;
	  } else if (lock1_last_byte > lock2_last_byte) {
	       return LOCK1_SUPERSET;
	  } else {
	       return LOCK1_BEGINS_BEFORE;
	  }
     } else if (lock1->offset > lock2->offset) {
	  if (lock1_last_byte > lock2_last_byte) {
	       return LOCKS_DISJOINT;
	  } else if (lock1_last_byte < lock2_last_byte) {
	       return LOCK1_SUBSET;
	  } else {
	       return LOCK1_ENDS_AFTER;
	  }
     } else {
	  return LOCKS_EQUAL;
     }
}

lock_t*
find_conflict(lock_t* chain,
	      lock_t* candidate)
{
     lock_t* conflicting = NULL;
     lock_t* index;
     for (index = chain; index; index = index->next) {
	  if (overlap(candidate, index)) {
	       if (candidate->exclusive ||
		   index->exclusive) {
		    if (!(state_compare_lock_owner(index->state->state.lock.
						   lock_owner->key,
						   (candidate->state->state.lock.
						    lock_owner->key))))
		    {
			 conflicting = index;
			 break;
		    }
	       }
	  }
     }
}

void
set_lock(lock_t* lock, lock_t** chain)
{
     /* We assume all conflict detection has already been done.  We
	merely update locks sharing our state.  This is not quite
	correct, so I need to come back and add in the 9.5 semantics. */

     lock_t* index;

     if (!(*chain)) {
	  link_lock(chain, lock);
     } else {
	  for (index = *chain; index; index = index->next) {
	       if (index->state != lock->state)
		    continue;
	       switch (overlap(lock, index)) {
	       case LOCKS_DISJOINT:
		    link_lock(chain, lock);
		    break;
		    
	       case LOCKS_EQUAL:
		    index->exclusive = lock->exclusive;
		    ReleaseToPool(lock, &lock_pool);
		    break;
		    
	       case LOCK1_SUPERSET:
		    unlink_lock(chain, index);
		    ReleaseToPool(index, &lock_pool);
		    link_lock(chain, lock);
		    break;
		    
	       case LOCK1_SUBSET:
		    if (lock->exclusive == index->exclusive) {
			 ReleaseToPool(lock, &lock_pool);
		    } else {
			 lock_t* after;
			 GetFromPool(after, &lock_owner_pool, lock_t);
			 *after = *index;
			 index->length = lock->offset - index->offset;
			 after->offset = lock->offset + lock->length;
			 after->length = (index->offset + after->length
					  - after->offset);
			 link_lock(chain, lock);
			 link_lock(chain, after);
		    }
		    break;
		    
	       case LOCK1_BEGINS_BEFORE:
		    if (lock->exclusive == index->exclusive) {
			 if (index->length == NFS4_UINT64_MAX) {
			      index->offset = lock->offset;
			 } else {
			      index->length = (index->length + index->offset -
					       lock->offset);
			      index->offset = lock->offset;
			 }
			 ReleaseToPool(lock, &lock_pool);
		    } else {
			 if (index->length == NFS4_UINT64_MAX) {
			      index->offset = lock->offset + lock->length;
			 } else {
			      index->length = (index->length + index->offset -
					       (lock->length + lock->offset));
			      index->offset = lock->offset + lock->length;
			 }
			 link_lock(chain, lock);
		    }
		    
	       case LOCK1_ENDS_AFTER:
		    if (lock->exclusive == index->exclusive) {
			 index->length = (lock->offset + lock->length -
					  index->offset);
			 ReleaseToPool(lock, &lock_pool);
		    } else {
			 index->length = lock->offset - index->offset;
			 link_lock(chain, lock);
		    }
	       }
	  }
     }
}
     
void
clear_lock(lock_t* lock, lock_t** chain)
{
     lock_t* index;

     for (index = *chain; index; index = index->next) {
	  if (index->state != lock->state)
	       continue;
	  switch (overlap(lock, index)) {
	  case LOCKS_DISJOINT:
	       break;

	  case LOCKS_EQUAL:
	       unlink_lock(chain, index);
	       ReleaseToPool(index, &lock_pool);
	       break;

	  case LOCK1_SUPERSET:
	       unlink_lock(chain, index);
	       ReleaseToPool(index, &lock_pool);
	       break;

	  case LOCK1_SUBSET: {
	       lock_t* after;
	       GetFromPool(after, &lock_owner_pool, lock_t);
	       *after = *index;
	       index->length = lock->offset - index->offset;
	       after->offset = lock->offset + lock->length;
	       after->length = (index->offset + after->length
				- after->offset);
	       break;

	  }
	  case LOCK1_BEGINS_BEFORE:
	       if (index->length == NFS4_UINT64_MAX) {
		    index->offset = lock->offset + lock->length;
	       } else {
		    index->length = (index->length + index->offset -
				     (lock->length + lock->offset));
		    index->offset = lock->offset + lock->length;
	       }

	  case LOCK1_ENDS_AFTER:
	       index->length = lock->offset - index->offset;
	  }
     }
}

static int
lock_owner_cmp_func(hash_buffer_t* key1,
		    hash_buffer_t* key2)
{
     state_lock_owner_t* owner1 = (state_lock_owner_t*) key1->pdata;
     state_lock_owner_t* owner2 = (state_lock_owner_t*) key2->pdata;

     return !(state_compare_lock_owner(owner1, owner2));
}


static unsigned int
hash_nfs3_lock_owner(state_lock_owner_t* owner, uint32_t* h1, uint32_t* h2)
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

     rc = hash_lock_owner(owner, &h1, &h2);

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

static int
lock_state_cmp_func(hash_buffer_t* key1,
		    hash_buffer_t* key2)
{
     fsal_status_t status;
     state_t* state1 = (state_t*) key1->pdata;
     state_t* state2 = (state_t*) key2->pdata;
     fsal_handle_t* handle1 = &state1->perfile->handle;
     fsal_handle_t* handle2 = &state2->perfile->handle;
     state_lock_owner_t* lock_owner1 =
	  &(state1->state.lock.lock_owner->key);
     state_lock_owner_t* lock_owner2 =
	  &(state2->state.lock.lock_owner->key);
     open_owner_key_t* open_owner1 =
	  &(state1->state.lock.open_state->state.share.open_owner->key);
     open_owner_key_t* open_owner2 =
	  &(state2->state.lock.open_state->state.share.open_owner->key);

     if (FSAL_handlecmp(handle1, handle2,
			&status)) {
	  if (open_owners_equal(open_owner1, open_owner2)) {
	       return !(state_compare_lock_owner(lock_owner1, lock_owner2));
	  }
     } else {
	  return 0;
     }
}

static unsigned int
lock_state_hash_func(hash_parameter_t* hashparm,
		     hash_buffer_t* keybuff,
		     uint32_t* hashval,
		     uint32_t* rbtval)
{
     uint32_t h1 = 0;
     uint32_t h2 = 0;
     state_t* lock_state = (state_t*) keybuff->pdata;
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
     rc = hash_lock_owner(lock_owner, &h1, &h2);
     if (open_owner) {
	  hash_open_owner(open_owner, &h1, &h2);
     }

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
	 return ERR_STATE_FAIL;
    }

    GetFromPool(*owner, &lock_owner_pool, lock_owner_info_t);
    if (!*owner) {
	 return ERR_STATE_FAIL;
    }

    (*owner)->key = *ownerkey;
    key.pdata = (caddr_t) &((*owner)->key);
    key.len = sizeof(state_lock_owner_t);
    (*owner)->seqid = 0;
    (*owner)->refcount = 1; /* Every lock_owner gets created with a
			     refcount of 1, to prevent races. */
    (*owner)->last_response = NULL;
    pthread_mutex_init(&((*owner)->mutex), NULL);
    val.pdata = (caddr_t) *owner;
    val.len = sizeof(lock_owner_info_t);
    rc = HashTable_Set_Or_Fetch(lock_owner_table, &key, &val);
    if (rc == HASHTABLE_SUCCESS) {
	 if (created) {
	      *created = TRUE;
	      return ERR_STATE_NO_ERROR;
	 }
    } else if (rc == HASHTABLE_ERROR_KEY_ALREADY_EXISTS) {
	 ReleaseToPool(*owner, &lock_owner_pool);
	 *owner = (lock_owner_info_t*) val.pdata;
	 return ERR_STATE_NO_ERROR;
    } else {
	 ReleaseToPool(*owner, &lock_owner_pool);
	 *owner = NULL;
	 return ERR_STATE_FAIL;
    }
}

static void
maybe_kill_lock_owner(lock_owner_info_t* owner)
{
     hash_buffer_t key;

     pthread_mutex_lock(&(owner->mutex));
     if (--(owner->refcount) == 0) {
	  key.pdata = (caddr_t) &(owner->key);
	  key.len = sizeof(open_owner_key_t);
	  HashTable_Del(lock_owner_table, &key, NULL, NULL);
	  pthread_mutex_unlock(&owner->mutex);
	  ReleaseToPool(owner, &lock_owner_pool);
     } else {
	  pthread_mutex_unlock(&(owner->mutex));
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

    (*lock_state)->state.lock.lock_owner = owner;
    (*lock_state)->state.lock.open_state = open_state;
    (*lock_state)->perfile = perfile;
    key.pdata = (caddr_t) *lock_state;
    key.len = sizeof(state_t);
    (*lock_state)->type = STATE_LOCK;
    val.pdata = (caddr_t) *lock_state;
    val.len = sizeof(state_t);
    rc = HashTable_Set_Or_Fetch(lock_state_table, &key, &val);
    if (rc == HASHTABLE_ERROR_KEY_ALREADY_EXISTS) {
	 ReleaseToPool(*lock_state, &state_pool);
	 (*lock_state) = (state_t*) val.pdata;
	 return ERR_STATE_NO_ERROR;
    } else if (rc != HASHTABLE_SUCCESS) {
	 ReleaseToPool(*lock_state, &state_pool);
	 *lock_state = NULL;
	 return ERR_STATE_FAIL;
    }

    if (owner->key.owner_type == LOCKOWNER_NFS3) {
	 /* Do NFS3 Things */
    } else if (owner->key.owner_type == LOCKOWNER_NFS4) {
	 (*lock_state)->clientid = owner->key.u.nfs4_owner.clientid;
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
	 return ERR_STATE_FAIL;
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
	  rc = hash_nfs3_lock_owner(owner, h1, h2);
	  break;

     case LOCKOWNER_NFS4:
	  *h1 = *h2 = (('N' << 0x18) |
		       ('F' << 0x10) |
		       ('S' << 0x08) |
		       ( 4  << 0x00));
	  rc = hash_nfs4_lock_owner(owner, h1, h2);
	  break;

     default:
	  rc = 0;
	  LogCrit(COMPONENT_STATES,
		  "open_owner_hash_func: Owner type %d should never be stored "
		  "in the hash table.\n",
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
	  pthread_rwlock_unlock(&(perfile->lock));
	  return rc;
     }

     if (!open_state) {
	  pthread_rwlock_unlock(&(perfile->lock));
	  return ERR_STATE_FAIL;
     }

     if ((open_state->type != STATE_SHARE) ||
	 (open_state->clientid != clientid)) {
	  pthread_rwlock_unlock((&perfile->lock));
	  return ERR_STATE_BAD;
     }

     owner_key.owner_type = LOCKOWNER_NFS4;
     owner_key.u.nfs4_owner.clientid = clientid;
     owner_key.u.nfs4_owner.owner_len
	  = nfs_lock_owner.owner.owner_len;
     memcpy(owner_key.u.nfs4_owner.owner_val,
	    nfs_lock_owner.owner.owner_val,
	    nfs_lock_owner.owner.owner_len);

     if ((rc = acquire_lock_owner(&owner_key,
				  &owner_created,
				  &owner))
	 != ERR_STATE_NO_ERROR) {
	  pthread_rwlock_unlock(&(perfile->lock));
	  return rc;
     }

     if (!owner_created) {
	  if ((rc = pthread_mutex_lock(&(owner->mutex))) != 0) {
	       pthread_rwlock_unlock((&perfile->lock));
	       maybe_kill_lock_owner(owner);
	       return ERR_STATE_FAIL;
	  }

	  owner->refcount++;
	  pthread_mutex_unlock(&(owner->mutex));
     }

     /* Every lock state increments the reference count on a lock
	owner by 1.  If this is a new owner, the reference count has
	already been set to 1. */

     if ((rc = acquire_lock_state(owner, open_state, perfile, NULL,
				  &state)) != 0) {
	  pthread_rwlock_unlock(&(perfile->lock));
	  maybe_kill_lock_owner(owner);
	  return rc;
     }

     *transaction
	  = (state_lock_trans_t*) Mem_Alloc(sizeof(state_lock_trans_t));
     memset(*transaction, 0, sizeof(state_lock_trans_t));
     (*transaction)->status = TRANSACT_LIVE;
     (*transaction)->lock_state = state;

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
	  pthread_rwlock_unlock(&(perfile->lock));
	  return rc;
     }

     if (!lock_state) {
	  pthread_rwlock_unlock((&perfile->lock));
	  return ERR_STATE_FAIL;
     }

     if ((lock_state->type != STATE_LOCK) ||
	 (lock_state->clientid != clientid)) {
	  pthread_rwlock_unlock(&(perfile->lock));
	  return ERR_STATE_BAD;
     }

     *transaction
	  = (state_lock_trans_t*) Mem_Alloc(sizeof(state_lock_trans_t));
     memset(*transaction, 0, sizeof(state_lock_trans_t));
     (*transaction)->status = TRANSACT_LIVE;
     (*transaction)->lock_state = lock_state;

     return ERR_STATE_NO_ERROR;
}

int
localstate_lock(state_lock_trans_t* transaction,
		uint64_t offset,
		uint64_t length,
		bool_t exclusive,
		bool_t blocking)
{
     int rc = 0;
     state_t* lock_state = NULL;
     perfile_state_t* perfile;
     uint16_t locktype;
     state_lock_owner_t lock_owner;
     fsal_status_t fsal_status;
     lock_t* conflicting;
     lock_t* to_add;
     fsal_lockpromise_t promise;

     if (transaction->status != TRANSACT_LIVE) {
	  return ERR_STATE_DEAD_TRANSACTION;
     }

     GetFromPool(transaction->to_set, &lock_pool, lock_t);
     transaction->to_set->offset = offset;
     transaction->to_set->length = length;
     transaction->to_set->exclusive = exclusive;
     transaction->to_set->blocking = blocking;
     transaction->to_set->state = transaction->lock_state;

     /* Check for conflicts of our own */

     if ((conflicting = find_conflict(transaction->lock_state->perfile->locks,
				      transaction->to_set))) {
	  GetFromPool(transaction->conflicting, &lock_pool, lock_t);
	  *transaction->conflicting = *conflicting;
	  transaction->status = TRANSACT_FAILED;
	  transaction->errcode = ERR_STATE_CONFLICT;
	  transaction->errsource = ERROR_SOURCE_SAL;
	  pthread_rwlock_unlock(&(perfile->lock));
	  return ERR_STATE_CONFLICT;
     }
     /* Push down to the FSAL */

     locktype = ((exclusive && FSAL_LOCKTYPE_EXCLUSIVE) |
		 (blocking && FSAL_LOCKTYPE_BLOCK));

     lock_owner = transaction->lock_state->state.lock.lock_owner->key;

     fsal_status
	  = FSAL_lock(&(transaction->lock_state->state.lock.
			open_state->state.share.openref->descriptor),
		      &offset,
		      (fsal_size_t*) &length,
		      &locktype,
		      &lock_owner,
		      &(transaction->lock_state->state.lock.filelockinfo),
		      FALSE,
		      &promise);

     if (FSAL_IS_ERROR(fsal_status)) {
	  if (fsal_status.major == ERR_FSAL_CONFLICT) {
	       transaction->errcode = ERR_STATE_CONFLICT;
	       transaction->errsource = ERROR_SOURCE_SAL;
	       transaction->status = TRANSACT_FAILED;
	       GetFromPool(transaction->conflicting, &lock_pool, lock_t);
	       transaction->conflicting->offset = offset;
	       transaction->conflicting->length = length;
	       transaction->conflicting->exclusive
		    = (locktype || FSAL_LOCKTYPE_EXCLUSIVE);
	       transaction->conflicting->blocking
		    = (locktype || FSAL_LOCKTYPE_BLOCK);
	       transaction->conflicting->state = NULL;
	       pthread_rwlock_unlock(&(perfile->lock));
	       return ERR_STATE_CONFLICT;
	  } else {
	       transaction->errcode = fsal_status.major;
	       transaction->errsource = ERROR_SOURCE_FSAL;
	       transaction->status = TRANSACT_FAILED;
	       pthread_rwlock_unlock(&(perfile->lock));
	       return ERR_STATE_CONFLICT;
	  }
     }

     /* Update the state */

     GetFromPool(to_add, &lock_pool, lock_t);
     *to_add = *transaction->to_set;

     set_lock(to_add,
	      &(transaction->lock_state->perfile->locks));

     return ERR_STATE_NO_ERROR;
}

int
localstate_unlock(state_lock_trans_t* transaction,
		  uint64_t offset,
		  uint64_t length)
{
     int rc = 0;
     state_t* lock_state = NULL;
     perfile_state_t* perfile;
     uint16_t locktype;
     state_lock_owner_t lock_owner;
     fsal_status_t fsal_status;

     if (transaction->status != TRANSACT_LIVE) {
	  return ERR_STATE_DEAD_TRANSACTION;
     }

     GetFromPool(transaction->to_free, &lock_pool, lock_t);
     transaction->to_free->offset = offset;
     transaction->to_free->length = length;
     transaction->to_free->exclusive = 0;
     transaction->to_free->blocking = 0;
     transaction->to_free->state = transaction->lock_state;

     /* Push down to the FSAL */

     locktype = 0;

     lock_owner = transaction->lock_state->state.lock.lock_owner->key;

     fsal_status
	  = FSAL_unlock(&(transaction->lock_state->state.lock.
			  open_state->state.share.openref->descriptor),
			offset,
			length,
			locktype,
			&lock_owner,
			&(transaction->lock_state->state.lock.filelockinfo));

     if (FSAL_IS_ERROR(fsal_status)) {
	       transaction->errcode = fsal_status.major;
	       transaction->errsource = ERROR_SOURCE_FSAL;
	       transaction->status = TRANSACT_FAILED;
	       pthread_rwlock_unlock(&(perfile->lock));
	       return ERR_STATE_CONFLICT;
	  }

     /* Update the state */

     clear_lock(transaction->to_free,
		&(transaction->lock_state->perfile->locks));
     return ERR_STATE_NO_ERROR;
}

int
localstate_lock_commit(state_lock_trans_t* transaction)
{
     if ((transaction->status != TRANSACT_LIVE) &&
	 (transaction->status != TRANSACT_PYRRHIC_VICTORY)) {
	  return ERR_STATE_DEAD_TRANSACTION;
     }

     transaction->status = ((transaction->status == TRANSACT_LIVE) ?
			    TRANSACT_COMPLETED :
			    TRANSACT_PYRRHIC_VICTORY);

     transaction->lock_state->stateid.seqid++;

     pthread_rwlock_unlock(&(transaction->lock_state->perfile->lock));

     return ((transaction->status == TRANSACT_LIVE) ?
	     ERR_STATE_NO_ERROR :
	     transaction->errcode);
}

int
localstate_lock_abort(state_lock_trans_t* transaction)
{
     if (transaction->status != TRANSACT_LIVE) {
	  return ERR_STATE_DEAD_TRANSACTION;
     }

     transaction->status = TRANSACT_ABORTED;
     pthread_rwlock_unlock(&(transaction->lock_state->perfile->lock));

     return ERR_STATE_NO_ERROR;
}

int
localstate_lock_dispose_transaction(state_lock_trans_t* transaction)
{
     if (transaction->status = TRANSACT_LIVE) {
	  localstatestate_share_abort(transaction);
     }

     if (transaction->to_set) {
	  ReleaseToPool(transaction->to_set, &lock_pool);
     }
     if (transaction->to_free) {
	  ReleaseToPool(transaction->to_free, &lock_pool);
     }
     if (transaction->conflicting) {
	  ReleaseToPool(transaction->conflicting, &lock_pool);
     }

     Mem_Free(transaction);
     return ERR_STATE_NO_ERROR;
}

int
localstate_lock_get_stateid(state_lock_trans_t* transaction,
			    stateid4* stateid)
{
     if (transaction->status != TRANSACT_COMPLETED) {
	  return ERR_STATE_NOENT;
     }

     *stateid = transaction->lock_state->stateid;

     return ERR_STATE_NO_ERROR;
}

int
localstate_lock_get_nfs4err(state_lock_trans_t* transaction,
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
localstate_lock_get_nfs4conflict(state_lock_trans_t* transaction,
				 uint64_t* offset,
				 uint64_t* length,
				 uint32_t* type,
				 lock_owner4* lock_owner)
{
     if (transaction->errcode != ERR_STATE_CONFLICT ||
	 !transaction->conflicting) {
	  return ERR_STATE_NOENT;
     }

     *offset = transaction->conflicting->offset;
     *length = transaction->conflicting->length;
     *type = (transaction->conflicting->exclusive ?
	      WRITE_LT :
	      READ_LT);
     if (transaction->lock_state->state.lock.lock_owner->key.owner_type
	 == LOCKOWNER_NFS4) {
	  lock_owner->clientid
	       = (transaction->lock_state->state.lock.lock_owner->
		  key.u.nfs4_owner.clientid);
	  lock_owner->owner.owner_len
	       = (transaction->lock_state->state.lock.lock_owner->
		  key.u.nfs4_owner.owner_len);
	  lock_owner->owner.owner_val
	       = (transaction->lock_state->state.lock.lock_owner->
		  key.u.nfs4_owner.owner_val);
     } else {
	  lock_owner->clientid = 0;
	  lock_owner->owner.owner_len = 0;
	  lock_owner->owner.owner_val = NULL;
     }
     return ERR_STATE_NO_ERROR;
}
