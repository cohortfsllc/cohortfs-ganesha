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
#include <errno.h>
#include <stdio.h>

/************************************************************************
 * General state functions
 ***********************************************************************/

int localstate_lock_filehandle(fsal_handle_t *handle, statelocktype rw)
{
    entryheader_t* header;
    hash_buffer_t key, val;
    int rc = 0;
    
    key.pdata = (caddr_t)handle;
    key.len = sizeof(fsal_handle_t);
    
    rc = HashTable_Get(entrytable, &key, &val);
    
    /* First, try to fetch and lock */
  
    if (rc == HASHTABLE_SUCCESS)
	{
	    header = (entryheader_t*)val.pdata;
	    if (rw == readlock)
		rc = pthread_rwlock_rdlock(&(header->lock));
	    else if (rw == writelock)
		rc = pthread_rwlock_wrlock(&(header->lock));
	    else
		return ERR_STATE_INVAL;
	    if (rc != 0)
		return ERR_STATE_FAIL;

	    return ERR_STATE_NO_ERROR;
	}
    else if (rc == HASHTABLE_ERROR_NO_SUCH_KEY)
	{
	    /* Create,for a write lock */
	    if (rw == writelock)
		{
		  if ((errno = pthread_mutex_lock(&entrymutex)) != 0)
		    {
		      perror("pthread_mutex_lock");
		      return ERR_STATE_FAIL;
		    }
		    
		    /* Make sure no one created the entry while we were
		       waiting for the mutex */
		    
		    rc = HashTable_Get(entrytable, &key, &val);
		    
		    if (rc == HASHTABLE_SUCCESS)
			{
			    header = (entryheader_t*) val.pdata;
			    rc = pthread_rwlock_wrlock(&(header->lock));
			    pthread_mutex_unlock(&entrymutex);
			    if (rc != 0)
				return ERR_STATE_FAIL;
			    else
				return ERR_STATE_NO_ERROR;
			}
		    if (rc != HASHTABLE_ERROR_NO_SUCH_KEY)
			{
			    /* Really should be impossible */
			    pthread_mutex_unlock(&entrymutex);
			    return ERR_STATE_FAIL;
			}
		    
		    /* We may safely create the entry */
		    
		    GET_PREALLOC(header, entryheaderpool, 1, entryheader_t,
				 next_alloc);
		    
		    if (!header)
			{
			    pthread_mutex_unlock(&entrymutex);
			    return ERR_STATE_FAIL;
			}
		    
		    /* Copy, since it looks like the HashTable code depends on
		       keys not going away */

		    header->handle = *handle;
		    key.pdata = (caddr_t)&(header->handle);
		    
		    pthread_rwlock_init(&(header->lock), NULL);
		    
		    pthread_rwlock_wrlock(&(header->lock));

		    header->max_share = 0;
		    header->max_deny = 0;
		    header->anonreaders = 0;
		    header->anonwriters = 0;
		    header->read_delegations = 0;
		    header->write_delegation = 0;
		    header->dir_delegations = 0;
		    header->states = NULL;
		    
		    val.pdata = (caddr_t)header;
		    val.len = sizeof(entryheader_t);
		    
		    rc = HashTable_Test_And_Set(entrytable, &key, &val,
						HASHTABLE_SET_HOW_SET_NO_OVERWRITE);
		    
		    pthread_mutex_unlock(&entrymutex);
		    
		    if (rc == HASHTABLE_SUCCESS)
			return ERR_STATE_NO_ERROR;
		    else
			{
			    pthread_rwlock_unlock(&(header->lock));
			    RELEASE_PREALLOC(header, entryheaderpool, next_alloc);
			    return ERR_STATE_FAIL;
			}
		}
	    /* For a readlock, return not found */
	    else if (rw == readlock)
		return ERR_STATE_NOENT;
	    else
		return ERR_STATE_INVAL;
	}
    else
	return ERR_STATE_FAIL;
}
    
/*
 * Unlocks the filehandle.
 */

int localstate_unlock_filehandle(fsal_handle_t *handle)
{
    hash_buffer_t key, val;
    entryheader_t* header;
    int rc = 0;

    key.pdata = (caddr_t)handle;
    key.len = sizeof(fsal_handle_t);

    rc = HashTable_Get(entrytable, &key, &val);

    if (rc == HASHTABLE_SUCCESS)
	{
	    header = (entryheader_t*)val.pdata;
	    if (!(header->states))
		{
		    key.pdata = (caddr_t)&(header->handle);
		    key.len = sizeof(fsal_handle_t);
		    if (HashTable_Del(entrytable, &key, NULL, NULL) !=
			HASHTABLE_SUCCESS)
			LogMajor(COMPONENT_STATES,
				 "killstate: unable to remove header from hash table.");
		    rc = pthread_rwlock_unlock(&(header->lock));
		    RELEASE_PREALLOC(header, entryheaderpool,
				     next_alloc);
		}
	    else
		rc = pthread_rwlock_unlock(&(header->lock));
	    return rc ? ERR_STATE_FAIL : ERR_STATE_NO_ERROR;
	}
    else if (rc == HASHTABLE_ERROR_NO_SUCH_KEY)
	return ERR_STATE_NOENT;
    else
	return ERR_STATE_FAIL;
}

int localstate_iterate_by_filehandle(fsal_handle_t *handle, statetype type,
				     uint64_t* cookie, bool_t* finished,
				     taggedstate* outstate)
{
    entryheader_t* header;
    state_t* cur = NULL;
    state_t* next = NULL;

    *finished = false;

    if (!(header = lookupheader(handle)))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_iterate_by_filehandle: could not find/create header entry.");
	    return ERR_STATE_FAIL;
	}

    if (*cookie)
	cur = (state_t*) cookie;
    else
	cur = header->states;

    while (cur != NULL)
      {
	if ((type == STATE_ANY) || (cur->type == type))
	  break;
	else
	  cur = cur->nextfh;
      }

    if (cur == NULL)
      return ERR_STATE_NOENT;

    next = cur->nextfh;

    while (next != NULL)
      {
	if ((type == STATE_ANY) || (next->type == type))
	  break;
	else
	  next = next->nextfh;
      }

    *cookie = (uint64_t)next;

    if (!(*cookie))
	*finished = true;

    filltaggedstate(cur, outstate);

    return(ERR_STATE_NO_ERROR);
}


int localstate_iterate_by_clientid(clientid4 clientid, statetype type,
				   uint64_t* cookie, bool_t* finished,
				   taggedstate* outstate)
{
    state_t* cur = NULL;
    state_t* next = NULL;

    *finished = false;

    if (*cookie)
	cur = (state_t*) cookie;
    else
	cur = statechain;

    while (cur != NULL)
      {
	if ((type == STATE_ANY) || (cur->type == type))
	  break;
	else if (cur->clientid == clientid)
	  break;
	else
	  cur = cur->next;
      }

    if (cur == NULL)
	return ERR_STATE_NOENT;

    next = cur->next;

    while (next != NULL)
      {
	if ((type == STATE_ANY) || (next->type == type))
	  break;
	else if (next->clientid == clientid)
	  break;
	else
	  next = next->next;
      }
	
    *cookie = (uint64_t)next;

    if (!(*cookie))
      *finished = true;

    filltaggedstate(cur, outstate);

    return(ERR_STATE_NO_ERROR);
}

int localstate_retrieve_state(stateid4 stateid, taggedstate* outstate)
{
    state_t* state = NULL;
    int rc = 0;

    rc = lookup_state(stateid, &state);
    if (rc != ERR_STATE_NO_ERROR)
	return rc;

    filltaggedstate(state, outstate);

    return ERR_STATE_NO_ERROR;
}

int localstate_lock_state_owner(state_owner4 state_owner, bool_t lock,
				seqid4 seqid, bool_t* new,
				nfs_resop4** response)
{
    state_owner_t* owner;
    bool_t created;
    
    owner = acquire_owner(state_owner.owner.owner_val,
			  state_owner.owner.owner_len,
			  state_owner.clientid, lock,
			  true, &created);

    if (!owner)
	return ERR_STATE_FAIL;
    if (owner->seqid == seqid)
	{
	    *response = owner->last_response;
	    pthread_mutex_unlock(&(owner->mutex));
	    return ERR_STATE_NO_ERROR;
	}
    if ((owner->seqid < seqid) || (owner->seqid > seqid + 1))
	{
	    pthread_mutex_unlock(&(owner->mutex));
	    return ERR_STATE_BADSEQ;
	}

    return ERR_STATE_NO_ERROR;
}

int localstate_unlock_state_owner(state_owner4 state_owner,
				  bool_t lock)
{
    state_owner_t* owner;

    owner = acquire_owner(state_owner.owner.owner_val,
			  state_owner.owner.owner_len,
			  state_owner.clientid, lock,
			  false, NULL);

    pthread_mutex_unlock(&(owner->mutex));
}

int localstate_save_response(state_owner4 state_owner, bool_t lock,
			     nfs_resop4* response)
{
    state_owner_t* owner;

    owner = acquire_owner(state_owner.owner.owner_val,
			  state_owner.owner.owner_len,
			  state_owner.clientid, lock,
			  false, NULL);

    if (!owner->last_response)
	owner->last_response = (nfs_resop4*)Mem_Alloc(sizeof(nfs_resop4));

    memcpy(owner->last_response, response, sizeof(nfs_resop4));
    owner->seqid++;

    return ERR_STATE_NO_ERROR;
}
