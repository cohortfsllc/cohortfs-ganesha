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
}

int state_create_share(cache_entry_t *entry, open_owner4 open_owner,
		       clientid4 clientid, uint32_t share_access,
		       uint32_t share_deny, stateid4* stateid)
{
    entryheader* header;
    state* state;
    
    if (share_access == 0)
	{
	    LogDebug(COMPONENT_STATES,
		     "state_create_share: rejecting invalid share mode.");
	    return ERR_STATE_INVAL;
	}
    if (!(entryisfile(entry)))
	{
	    LogEror(COMPONENT_STATES,
		    "state_create_share: attempt to add share state to a non-file.");
	    return(ERR_STATE_OBJTYPE);
	}

    /* Retrieve or create header for per-filehandle chain */

    if (!(header = header_for_write(entry)))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_create_share: could not find/create header entry.");
	    return ERR_STATE_FAIL;
	}

    if (share_conflict(share_access, share_deny, header))
	{
	    pthread_rwlock_unlock(&(header->lock));
	    LogDebug(COMPONENT_STATES,
		     "state_create_share: share conflict.");
	    return ERR_STATE_CONFLICT;
	}

    /* Create and fill in new entry */

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

    /* Link it in */

    chain(state, header);

    /* Update maxima for quick lookups */

    updatemax(header, state);

    pthread_rwlock_unlock(&(header->lock));
    return(ERR_STATE_NO_ERROR);
}

int state_upgrade_share(uint32_t share_access, uint32_t share_deny,
			stateid4 stateid)
{
    entryheader* header;
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

    return ERR_STATE_NO_ERROR;
}

int state_downgrade_share(uint32_t share_access, uint32_t share_deny,
			  stateid4 stateid)
{
    entryheader* header;
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
