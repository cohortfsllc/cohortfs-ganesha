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

/************************************************************************
 * Lock Functions
 *
 * These functions realise lock state functionality.
 ***********************************************************************/

#if 0

int localstate_create_lock_state(fsal_handle_t *handle,
				 stateid4 open_stateid,
				 lock_owner4 lock_owner,
				 clientid4 clientid,
				 fsal_lockdesc_t* lockdata,
				 stateid4* stateid)
{
    entryheader_t* header;
    state_t* state;
    state_t* openstate = NULL;
    state_owner_t* owner;
    int rc = 0;

    /* Retrieve or create header for per-filehandle chain */

    if (!(header = lookupheader(handle)))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_create_lock_state: could not find/create header entry.");
	    return ERR_STATE_FAIL;
	}

    rc = lookup_state(open_stateid, &openstate);

    if (rc == ERR_STATE_NO_ERROR)
      return rc;

    if (!(owner
	  = acquire_owner(lock_owner.owner.owner_val,
			  lock_owner.owner.owner_len, clientid, TRUE,
			  FALSE, NULL)))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_create_lock_state: could not find/create state owner entry.");
	    return ERR_STATE_FAIL;
	}

    if (!(state->type != STATE_SHARE))
      return ERR_STATE_INVAL;

    /* Create and fill in new entry */
    
    if (!(state = newstate(clientid, header)))
	{
	    LogDebug(COMPONENT_STATES,
		     "state_create_lock_state: Unable to create new state.");
	    return ERR_STATE_FAIL;
	}

    openstate->state.share.locks = 1;

    owner->refcount++;
    owner->lock = 1;
    owner->related_owner = openstate->state.share.open_owner;

    state->type = STATE_LOCK;
    state->state.lock.openstate = openstate;
    state->state.lock.lock_owner = owner;
    state->state.lock.lockdata = lockdata;
    
    *stateid = state->stateid;
    return ERR_STATE_NO_ERROR;
}

int localstate_delete_lock_state(stateid4 stateid)
{
    state_t* state;
    state_t* cur = NULL;
    entryheader_t* header;
    int rc;

    if (rc = lookup_state(stateid, &state))
	return rc;

    header = state->header;

    state->type = STATE_ANY;

    state->state.lock.openstate->state.share.locks = 0;

    while (iterate_entry(header, &cur))
	{
	    if (!((cur->type == STATE_LOCK) ||
		  (cur->state.lock.openstate == state->state.lock.openstate)))
		continue;
	    
	    state->state.lock.openstate->state.share.locks = 1;
	}
    
    state->state.lock.lock_owner->refcount--;
    if (state->state.lock.lock_owner->refcount == 0)
	killowner(state->state.lock.lock_owner);

    killstate(state);
    
    return ERR_STATE_NO_ERROR;
}

int localstate_query_lock_state(fsal_handle_t* handle,
				stateid4 open_stateid,
				lock_owner4 lock_owner,
				clientid4 clientid,
				lockstate* outlockstate)
{
    entryheader_t* header;
    state_t* cur = NULL;
    int rc = 0;
    state_owner_t* owner;
    bool_t created;
    
    /* Retrieve or create header for per-filehandle chain */

    if (!(header = lookupheader(handle)))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_query_lock: could not find header entry.");
	    return ERR_STATE_FAIL;
	}

    if (!(owner
	  = acquire_owner(lock_owner.owner.owner_val,
			  lock_owner.owner.owner_len, clientid, TRUE,
			  FALSE, &created)))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_query_lock_state: could not find/create state owner entry.");
	    return ERR_STATE_FAIL;
	}

    if (created)
	{
	    killowner(owner);
	    return ERR_STATE_NOENT;
	}
    
    while (iterate_entry(header, &cur))
	{
	    if ((cur->type == STATE_LOCK) &&
		(cur->clientid == clientid) &&
		(cur->state.lock.lock_owner == owner))
		break;
	    else
		continue;
	}

    if (!cur)
      return ERR_STATE_NOENT;

    memset(outlockstate, 0, sizeof(lockstate));

    return ERR_STATE_NO_ERROR;
}

void filllockstate(state_t* cur, lockstate* outlockstate,
		   entryheader_t* header)
{
    outlockstate->handle = header->handle;
    outlockstate->clientid = cur->clientid;
    outlockstate->stateid = cur->stateid;
    memcpy(outlockstate->lock_owner.owner.owner_val,
	   cur->state.lock.lock_owner->key.owner_val,
	   cur->state.lock.lock_owner->key.owner_len);
    outlockstate->lock_owner.owner.owner_len
	= cur->state.lock.lock_owner->key.owner_len;
    outlockstate->lock_owner.clientid = cur->clientid;
    outlockstate->lockdata = cur->state.lock.lockdata;
}
  
int localstate_lock_inc_state(stateid4* stateid)
{
    state_t* state;
    entryheader_t* header;
    int rc;
    
    if (rc = lookup_state(*stateid, &state))
	return ERR_STATE_FAIL;

    header = state->header;

    if (state->type != STATE_LOCK)
      return ERR_STATE_INVAL;

    ++state->stateid.seqid;

    *stateid = state->stateid;

    return ERR_STATE_NO_ERROR;
}

#endif /* 0 */
