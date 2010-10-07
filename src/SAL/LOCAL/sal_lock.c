/*
 *
 * Copyright (C) 2010, The Linux Box, inc.
 *
 * Contributor: Adam C. Emerson
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

#include "sal.h"
#include "stuff_alloc.h"
#include "nfs_core.h"
#include "nfs_log.h"
#include "log_macros.h"
#include "sal_internal.h"

/************************************************************************
 * Lock Functions
 *
 * These functions realise lock state functionality.
 ***********************************************************************/

int localstate_create_lock_state(fsal_handle_t *handle,
				 stateid4 open_stateid,
				 lock_owner4 lock_owner,
				 fsal_lock_t* lockdata,
				 stateid4* stateid)
{
    entryheader* header;
    state* state;
    state* openstate = NULL;
    int rc = 0;

    /* Retrieve or create header for per-filehandle chain */

    if (!(header = header_for_write(handle)))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_create_lock_state: could not find/create header entry.");
	    return ERR_STATE_FAIL;
	}

    rc = lookup_state(open_stateid, &openstate);

    if (rc == ERR_STATE_NO_ERROR)
	{
	    pthread_rwlock_unlock(&(header->lock));
	    LogError(COMPONENT_STATES,
		     "state_create_lock_state: could not find open state.");
	    return rc;
	}

    if (!((state->type != open) ||
	  (state->type != delegation)))
	{
	    pthread_rwlock_unlock(&(header->lock));
	    LogError(COMPONENT_STATES,
		     "state_create_lock_state: supplied state of invalid type.");
	    return ERR_STATE_INVAL;
	}

    /* Create and fill in new entry */
    
    if (!(state = newstate(clientid, header)))
	{
	    pthread_rwlock_unlock(&(header->lock));
	    LogDebug(COMPONENT_STATES,
		     "state_create_lock_state: Unable to create new state.");
	    return ERR_STATE_FAIL;
	}

    openstate->u.share.locks = 1;

    state->type = lock;
    state->u.lock.openstate = openstate;
    memcpy(state->u.lock.lock_owner,
	   lock_owner.owner.owner_val,
	   lock_owner.owner.owner_len);
    state->u.lock.lock_owner_len = lock_owner.owner.owner_len;
    state->u.lock.lockdata = lockdata;
    
    *stateid = header->stateid;
    pthread_rwlock_unlock(&(header->lock));
    return ERR_STATE_NO_ERROR;
}

int localstate_delete_lock_state(stateid4 stateid)
{
    state* state;
    state* cur = NULL;
    entryheader* header;
    int rc;

    if (rc = lookup_state_and_lock(stateid, &state, &header, true))
	{
	    LogError(COMPONENT_STATES,
		     "state_lock_state: could not find state.");
	}

    state->type = any;
    state->u.share.openstate->u.share.locks = 0;

    while (iterate_entry(entry, &cur))
	{
	    if (!((cur->type == lock)
		  (cur->u.lock.openstate == state->u.lock.openstate)))
		continue;
	    
	    openstate->u.share.locks = 1;
	}
    killstate(state);
    
    return ERR_STATE_NO_ERROR;
}

int localstate_query_lock_state(fsal_handle_t *handle,
				stateid4 open_stateid,
				lock_owner4 lock_owner,
				lockstate* outlockstate)
{
    entryheader* header;
    state* cur = NULL;
    int rc = 0;
    
    /* Retrieve or create header for per-filehandle chain */

    if (!(header = header_for_read(handle)))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_query_lock: could not find header entry.");
	    return ERR_STATE_FAIL;
	}

    while (iterate_entry(header, &cur))
	{
	    if ((cur->type == lock) &&
		(cur->clientid == clientid) &&
		(cur->u.lock.lock_owner_len ==
		 lock_owner.owner.owner_len)
		(memcmp(cur->u.lock_owner, lock_owner.owner.owner_val,
			lock_owner.owner.owner_len) == 0))
		break;
	    else
		continue;
	}

    if (!cur)
	{
	    pthread_rwlock_unlock(&(header->lock));
	    return ERR_STATE_NOENT;
	}

    memset(outlockstate, 0, sizeof(dir_lock));

    pthread_rwlock_unlock(&(header->lock));

    return ERR_STATE_NO_ERROR;
}

void filllockstate(state* cur, lockstate* outlockstate,
		   entryheader* header)
{
    outlockstate->handle = header->handle;
    outlockstate->clientid = cur->clientid;
    outlockstate->stateid = cur->stateid;
    outlockstate->lock_owner.clientid = cur->clientid;
    outlockstate->lock_owner.owner.owner_len = cur->u.lock.lock_owner_len;
    memcpy(outlockstate->lock_owner.owner.owner_val,
	   cur->u.lock.lock_owner,
	   cur->u.lock.lock_owner_len);
    outlockstate->lockdata = cur->u.lock.lockdata;
}
  

int localstate_lock_inc_state(stateid4* stateid)
{
    state* state;
    
    if (rc = lookup_state_and_lock(stateid, &state, &header, true))
	{
	    LogError(COMPONENT_STATES,
		     "state_inc_lock_state: could not find state.");
	    return ERR_STATE_FAIL;
	}
    if (state->type != lock)
	{
	    LogError(COMPONENT_STATES,
		     "state_inc_lock_state: supplied state of wrong type.");
	    pthread_rwlock_unlock(&(state->header->lock));
	    return ERR_STATE_INVAL;
	}

    ++state->stateid.seqid;

    *stateid = state->stateid;

    pthread_rwlock_unlock(&(state->header->lock));
    return ERR_STATE_NO_ERROR;
}
