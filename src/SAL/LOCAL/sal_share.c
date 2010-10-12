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
 * Share Functions
 *
 * These functions realise share state functionality.
 ***********************************************************************/

/* Make sure the maximum share/deny values are correct */

<<<<<<< HEAD
void updatemax(entryheader* entry)
{
    state* state = NULL;

    entry->max_share = 0;
    entry->max_deny = 0;

    while (next_entry_state(entry, &state))
	{
	    if (state->type != share)
		continue;
	    
	    entry->max_share |= state->assoc.owned.share.share_access;
	    entry->max_deny |= state->assoc.owned.share.share_deny;
	}
}

int share_conflict(uint32_t share_access, uint32_t share_deny,
		   entryheader* entry)
{
    if((share_access && entry->max_deny) ||
       (share_deny && entry->max_share) ||
       ((share_access & OPEN4_SHARE_ACCESS_READ) &&
	entry->read_delegations) ||
       ((share_deny & OPEN4_SHARE_DENY_READ) &&
	entry->read_delegations) ||
       entry->write_delegations)
	return 1;
    else
	return 0;
=======
void updatemax(entryheader* header)
{
    state* cur;
    
    header->max_share = 0;
    header->max_deny = 0;

    while (iterate_entry(header, &cur))
	{
	    if (cur->type != 1)
		continue;
	    header->max_share |= cur->u.share.share_access;
	    header->max_deny |= cur->u.share.share_deny;
	}
>>>>>>> 10d1be59b652f69fd7de00fe6f675f34d9ed69e9
}

int share_conflict(entryheader* header, open_owner4 open_owner,
		   uint32_t share_access, uint32_t share_deny)
{
    state* state = NULL;
    
    if (share_access == 0)
	return ERR_STATE_INVAL;

    if ((share_access & entry->max_deny) ||
	(share_deny & entry->max_share) ||
	((share_deny & OPEN4_SHARE_DENY_READ) &&
	 entry->nfs32readers) ||
	((share_deny & OPEN4_SHARE_DENY_WRITE) &&
	 entr->nfs32writers) ||
	(((share_access & OPEN_SHARE_ACCESS_WRITE) ||
	  (share_deny & SHARE_DENY_ACCESS_READ)) &&
	 entry->read_delegations) ||
	entry->write_delegations)
	return ERR_STATE_CONFLICT;

    while (iterate_entry(header, &state))
	{
	    if ((state->clientid == clientid) &&
		(open_owner.owner.owner_len ==
		 state->u.share.open_owner_len) &&
		(memcmp(open_owner.owner.owner_val,
			state->u.share.open_owner,
			open_owner.owner.owner_len) == 0))
		return ERR_STATE_PREEXISTS;
	}
    return ERR_STATE_NONE;
}

int localstate_create_share(fsal_handle_t *handle, open_owner4 open_owner,
			    clientid4 clientid, uint32_t share_access,
			    uint32_t share_deny, stateid4* stateid)
{
    entryheader* header;
    state* state;
    int rc = 0;
    
    /* Retrieve or create header for per-filehandle chain */

    if (!(header = header_for_write(handle)))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_create_share: could not find/create header entry.");
	    return ERR_STATE_FAIL;
	}

<<<<<<< HEAD
    if (share_conflict(share_access, share_deny, header))
=======
    /* Check for potential conflicts */

    if (rc = share_conflict(header, open_owner, share_access, share_deny))
	{
	    pthread_rwlock_unlock(&(header->lock));
	    LogDebug(COMPONENT_STATES,
		     "state_create_share: share conflict.");
	    return rc;
	}

    /* Create and fill in new entry */

    if (!(state = newstate(clientid, header)))
	{
	    pthread_rwlock_unlock(&(header->lock));
	    LogDebug(COMPONENT_STATES,
		     "state_create_share: Unable to create new state.");
	    return ERR_STATE_FAIL;
	}

    state->type = share;
    memcpy(state->u.share.open_owner, open_owner.owner.owner_val,
	   open_owner.owner.owner_len);
    state->u.share.open_owner_len = open_owner.owner.owner_len;
    state->u.share.share_access = share_access;
    state->u.share.share_deny = share_deny;


    /* Update maxima for quick lookups */

    header->max_share |= share_access;
    header->max_deny |= share_deny;

    *stateid = header->stateid;
    pthread_rwlock_unlock(&(header->lock));
    return ERR_STATE_NO_ERROR;
}

int localstate_upgrade_share(uint32_t share_access, uint32_t share_deny,
			     stateid4* stateid)
{
    state* state;
    entryheader* header;
    int rc;

    if (rc = lookup_state_and_lock(stateid, &state, &header, true))
>>>>>>> 10d1be59b652f69fd7de00fe6f675f34d9ed69e9
	{
	    LogDebug(COMPONENT_STATES,
		     "state_upgrade_share: could not find state.");
	}

    if (rc = share_conflict(header, open_owner, share_access, share_deny))
	{
	    pthread_rwlock_unlock(&(header->lock));
	    LogError(COMPONENT_STATES,
		     "state_upgrade_share: share conflict.");
	    return rc;
	}
    state->u.share.share_access |= share_access;
    state->u.share.share_deny |= share_deny;
    state->stateid.seqid++;
    
    header->max_share |= share_access;
    header->max_deny |= share_deny;
    
    *stateid = state->stateid;
    pthread_rwlock_unlock(&(header->lock));
    return ERR_STATE_NO_ERROR;
}

int localstate_downgrade_share(uint32_t share_access, uint32_t share_deny,
			       stateid4* stateid)
{
    state* state;
    entryheader* header;
    int rc;

    if (rc = lookup_state_and_lock(stateid, &state, &header, true))
	{
	    LogError(COMPONENT_STATES,
		     "state_downgrade_share: could not find state.");
	}

<<<<<<< HEAD
    rc = newownedstate(clientid4, open_owner, lock_owner, state);

    if (rc != ERR_STATE_NO_ERROR)
	{
	    pthread_rwlock_unlock(&(header->lock));
	    LogError(COMPONENT_STATES,
		     "state_create_share: Unable to create new state.");
	    return rc;
	}


    state->stateid.seqid = 1;
    state->type = share;
    state->assoc.owned.state.share.share_access = share_access;
    state->assoc.owned.state.share.share_deny = share_deny;
=======
    if ((share_access & ~state->u.share.share_access) ||
	(share_deny & ~state->u.share.share_deny))
	{
	    pthread_rwlock_unlock(&(header->lock));
	    LogError(COMPONENT_STATES,
		     "state_downgrade_share: Not actually a downgrade.");
	    return ERR_STATE_INVAL;
	}

    state->u.share.share_access = share_access;
    state->u.share.share_deny = share_deny;
    state->stateid.seqid++;
    
    update_max(header);
    
    *stateid = state->stateid;
    pthread_rwlock_unlock(&(header->lock));
    return ERR_STATE_NO_ERROR;
}
>>>>>>> 10d1be59b652f69fd7de00fe6f675f34d9ed69e9

int localstate_delete_share(stateid4 stateid)
{
    state* state;
    entryheader* header;
    int rc;

<<<<<<< HEAD
    chain(state, header);
=======
    if (rc = lookup_state_and_lock(stateid, &state, &header, true))
	{
	    LogError(COMPONENT_STATES,
		     "state_delete_share: could not find state.");
	}
>>>>>>> 10d1be59b652f69fd7de00fe6f675f34d9ed69e9

    if (state->u.share.locks)
	{
	    pthread_rwlock_unlock(&(header->lock));
	    LogError(COMPONENT_STATES,
		     "state_delete_share: attempt to delete share state while locks exist.");
	    return ERR_STATE_LOCKSHELD;
	}

    state->u.share.share_access = 0;
    state->u.share.share_deny = 0;
    
    update_max(header);

    rc = killstate(state);
    
    return ERR_STATE_NO_ERROR;
}

int localstate_query_share(fsal_handle_t *handle, clientid4 clientid,
			   open_owner4 open_owner, sharestate* outshare)
{
    entryheader* header;
    state* cur = NULL;
    int rc = 0;
    
    /* Retrieve or create header for per-filehandle chain */

    if (!(header = header_for_read(handle)))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_query_share: could not find header entry.");
	    return ERR_STATE_FAIL;
	}

    while (iterate_entry(header, &cur))
	{
	    if ((cur->type = share) &&
		(cur->clientid == clientid) &&
		(cur->u.share.open_owner_len ==
		 open_owner.owner.owner_len) &&
		(memcmp(cur->u.share.open_owner,
			open_owner.owner.owner_val,
			cur->u.share.open_owner_len) == 0))
		break;
	    else
		continue;
	}

    if (!cur)
	pthread_rwlock_unlock(&(header->lock));

    memset(outshare, 0, sizeof(sharestate));
    
    fillsharestate(cur, outshare);

    pthread_rwlock_unlock(&(header->lock));

    return ERR_STATE_NOSTATE;
}

void fillsharestate(state* cur, sharestate* outshare,
		    entryheader* header);
{
    outshare->handle = header->handle;
    outshare->stateid = cur->stateid;
    outshare->clientid = cur->clientid;
    memcpy(outshare->open_owner.owner.owner_val,
	   cur->u.share.open_owner,
	   cur->u.share.open_owner_len);
    outshare->open_owner.owner.owner_val = cur->u.open_owner_len;
    outshare->open_owner.clientid = cur->clientid;
    outshare->share_access = cur->u.share.share_access;
    outshare->share_deny = cur->u.share.share_deny;
    outshare->locksheld = cur->u.share.locksheld;
    outshare->openref = cur->u.share.openref;
}

int localstate_start_32read(fsal_handle_t *handle)
{
    entryheader* header;
    
    /* Retrieve or create header for per-filehandle chain */

    if (!(header = header_for_write(handle)))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_start_32read: could not find/create header entry.");
	    return ERR_STATE_FAIL;
	}

    /* Check for potential conflicts */

    if ((header->max_deny & OPEN4_SHARE_DENY_READ) ||
	header->write_delegations)
	{
	    LogError(COMPONENT_SATES,
		     "state_start_32read: conflicting state.");
	    return ERR_STATE_CoNFLICT;
	}

    header->nfs32readers++;
    pthread_rwlock_unlock(&(header->lock));
<<<<<<< HEAD
    return(ERR_STATE_NO_ERROR);
}

int state_upgrade_share(uint32_t share_access, uint32_t share_deny,
			stateid4 stateid)
{
    state* state;
    int rc = 0
    
    if (share_access == 0)
	{
	    LogDebug(COMPONENT_STATES,
		     "state_upgrade_share: rejecting invalid share mode.");
	    return ERR_STATE_INVAL;
	}

    rc = getstate(stateid, &state);
    if (rc != ERR_STATE_NO_ERROR)
	{
	    LogError(COMPONENT_STATES,
		     "state_upgrade_share: unable to retrieve state.");
	    return rc;
	}
    pthread_rwlock_wrlock(state->header->lock);
    
    if (share_conflict(share_access &~
		       state->assoc.owner.state.share.share_access,
		       share_deny &~
		       state->assoc.owner.state.share.share_access,
		       header))
	{
	    pthread_rwlock_unlock(&(state->header->lock));
	    LogDebug(COMPONENT_STATES,
		     "state_upgrade_share: share conflict.");
	    return ERR_STATE_CONFLICT;
	}

    state->assoc.owner.state.share.share_access |= share_access;
    state->assoc.owner.state.share.share_deny |= share_deny;

    ++state->stateid.seqid;
    
    /* Update maxima for quick lookups */

    updatemax(state->header);

    pthread_rwlock_unlock(&(state->header->lock));
=======
>>>>>>> 10d1be59b652f69fd7de00fe6f675f34d9ed69e9

    return ERR_STATE_NO_ERROR;
}

<<<<<<< HEAD
int state_downgrade_share(uint32_t share_access, uint32_t share_deny,
			  stateid4 stateid)
{
    state* state;
    int rc = 0
    
    if (share_access == 0)
	{
	    LogDebug(COMPONENT_STATES,
		     "state_downgrade_share: rejecting invalid share mode.");
	    return ERR_STATE_INVAL;
	}

    rc = getstate(stateid, &state);
    if (rc != ERR_STATE_NO_ERROR)
	{
	    LogError(COMPONENT_STATES,
		     "state_downgrade_share: unable to retrieve state.");
	    return rc;
	}

    pthread_rwlock_wrlock(state->header->lock);

    if ((share_access & ~state.assoc.owned.state.share.share_access)
	(share_deny & ~state.assoc.owned.state.share.share_deny))
	{
	    pthread_rwlock_unlock(&(state->header->lock));
	    LogError(COMPONENT_STATES,
		     "state_downgrade_share: Cannot downgrade whaty ou don't have.");
	    return ERR_STATE_INVAL;
	}
    
    ++state->stateid.seqid;
    
    /* Update maxima for quick lookups */

    updatemax(state->header);

    pthread_rwlock_unlock(&(state->header->lock));

    return ERR_STATE_NO_ERROR;
}

int state_delete_share(stateid4* stateid)
{
    state* state;
    int rc = 0
    
    rc = getstate(stateid, &state);
    if (rc != ERR_STATE_NO_ERROR)
	{
	    LogError(COMPONENT_STATES,
		     "state_downgrade_share: unable to retrieve state.");
	    return rc;
	}

    pthread_rwlock_wrlock(state->header->lock);
    rc = killstate(state);
    if (rc != ERR_STATE_NO_ERROR)
	pthread_rwlock_unlock(&(state->header->lock));

    return 0;
}

/* This locking scheme opens up all sorts of race conditions.
   Fix it later. */

int state_query_share(cache_entry_t *entry, clientid4 clientid,
		      open_owner4 open_owner, sharestate* state)
{
    state* state;
    int rc = 0
    
    rc = getownedstate(clientid, open_owner4* open_owner,
		       lock_owner4* lock_owner, state** state);


    if (rc != ERR_STATE_NO_ERROR)
	{
	    LogError(COMPONENT_STATES,
		     "state_query_share: unable to retrieve state.");
	    return rc;
	}

=======
int localstate_start_32write(fsal_handle_t *handle)
{
    entryheader* header;
>>>>>>> 10d1be59b652f69fd7de00fe6f675f34d9ed69e9
    
    /* Retrieve or create header for per-filehandle chain */

    if (!(header = header_for_write(handle)))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_start_32write: could not find/create header entry.");
	    return ERR_STATE_FAIL;
	}

    /* Check for potential conflicts */

    if ((header->max_deny & OPEN4_SHARE_DENY_WRITE) ||
	header->read_delegations ||
	header->write_delegations)
	{
	    LogError(COMPONENT_SATES,
		     "state_start_32write: conflicting state.");
	    return ERR_STATE_CONFLICT;
	}

    header->nfs32writers++;
    pthread_rwlock_unlock(&(header->lock));

    return ERR_STATE_NO_ERROR;
}

int localstate_end_32read(fsal_handle_t *handle)
{
    entryheader* header;
    
    /* Retrieve or create header for per-filehandle chain */

    if (!(header = header_for_write(handle)))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_end_32read: could not find/create header entry.");
	    return ERR_STATE_FAIL;
	}

    /* Check for potential conflicts */

    header->nfs32writers ? header->nfs32readers-- : 0;
    pthread_rwlock_unlock(&(header->lock));

    return ERR_STATE_NO_ERROR;
}

int localstate_end_32write(fsal_handle_t *handle)
{
    entryheader* header;
    
    /* Retrieve or create header for per-filehandle chain */

    if (!(header = header_for_write(handle)))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_end_32write: could not find/create header entry.");
	    return ERR_STATE_FAIL;
	}

    header->nfs21writers ? header->nfs32writers-- : 0;
    pthread_rwlock_unlock(&(header->lock));

    return ERR_STATE_NO_ERROR;
}
