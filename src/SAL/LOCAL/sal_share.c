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

void updatemax(entryheader* header, localstate* state)
{
    
}

int state_create_share(cache_entry_t *entry, open_owner4 open_owner,
		       clientid4 clientid, uint32_t share_access,
		       uint32_t share_deny, stateid4* stateid)
{
    entryheader* header;
    concatstates* concat;
    localstate* state;
    
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

    /* Create or retrieve per-file/client record.  Ensure no
     * pre-existing share state exists
     */

    if (!(concat = get_concat(entry, clientid, true)))
	{
	    pthread_rwlock_unlock(&(header->lock));
	    LogMajor(COMPONENT_STATES,
		     "state_create_share: could not find/create file/clientid entry.");
	    return ERR_STATE_FAIL;
	}
    if (concat->share)
	{
	    pthread_rwlock_unlock(&(header->lock));
	    LogDebug(COMPONENT_STATES,
		     "state_create_share: share already exists.");
	    return ERR_STATE_PREEXISTS;
	}

    /* Check for potential conflicts */

    if (share_conflict(share_access, share_deny, concat))
	{
	    pthread_rwlock_unlock(&(header->lock));
	    LogDebug(COMPONENT_STATES,
		     "state_create_share: share conflict.");
	    return ERR_STATE_CONFLICT;
	}

    /* Create and fill in new entry */

    if (!(state = newstate(clientid4)))
	{
	    pthread_rwlock_unlock(&(header->lock));
	    LogDebug(COMPONENT_STATES,
		     "state_create_share: Unable to create new state.");
	    return ERR_STATE_FAIL;
	}

    state->header = header;
    state->concats = concat;
    state->clientid = clientid;
    state->stateid.seqid = 1;
    state->type = share;
    state->u.share.open_owner = open_owner;
    state->u.share_access = share_access;
    state->u.share.share_deny = share_deny;

    /* Link it in */

    chain(state, header);
    concats->share = state;

    /* Update maxima for quick lookups */

    updatemax(header, state);

    pthread_rwlock_unlock(&(header->lock));
    return(ERR_STATE_NO_ERROR);
    
}
