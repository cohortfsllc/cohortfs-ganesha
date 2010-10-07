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
 * Delegation Functions
 *
 * These functions realise delegation state functionality.
 ***********************************************************************/

void update_delegations(entryheader* entry)
{
    state* cur = NULL;
    
    entry->read_delegations = 0;
    entry->write_delegations = 0;

    while (iterate_entry(entry, &cur))
	{
	    if (cur->type != delegation)
		continue;
	    if (cur->u.delegation.type == OPEN_DELEGATE_READ)
		{
		    header->read_delegations = 1;
		    break;
		}
	    else if (cur->u.delegation.type == OPEN_DELEGATE_WRITE)
		{
		    header->write_delegations = 1;
		    break;
		}
	}
}

int localstate_create_delegation(fsal_handle_t *handle, clientid4 clientid,
				 open_delegation_type4 type,
				 nfs_space_limit4 limit, stateid4* stateid)
{
    entryheader* header;
    state* state;
    int rc = 0;

    if ((type != OPEN_DELEGATE_READ) &&
	(type != OPEN_DELEGATE_WRITE))
      {
	LogDebug(COMPONENT_STATES,
		 "state_create_delegation: attempt to create delegation of invalid type.");
	return ERR_STATE_INVAL;
      }
    
    /* Retrieve or create header for per-filehandle chain */

    if (!(header = header_for_write(handle)))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_create_delegation: could not find/create header entry.");
	    return ERR_STATE_FAIL;
	}

    /* Check for potential conflicts */

    if ((header->max_share & OPEN4_SHARE_ACCESS_WRITE) ||
	(header->nfs23writers) ||
	(header->max_deny & OPEN4_SHARE_DENY_READ) ||
	(header->write_delegations) ||
	(type == OPEN_DELEGATE_WRITE &&
	 (header->max_share ||
	  header->nfs23writers ||
	  header->read_delegations)))
	{
	    pthread_rwlock_unlock(&(header->lock));
	    LogDebug(COMPONENT_STATES,
		     "state_create_delegation: share conflict.");
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

    state->type = delegation;
    state->u.delegation.type =  type;
    state->u.delegation.limit = limit;

    *stateid = header->stateid;
    pthread_rwlock_unlock(&(header->lock));
    return ERR_STATE_NO_ERROR;
}

int localstate_delete_delegation(stateid4 stateid)
{
    state* state;
    entryheader* header;
    int rc;

    if (rc = lookup_state_and_lock(stateid, &state, &header, true))
	{
	    LogError(COMPONENT_STATES,
		     "state_delete_delegation: could not find state.");
	}

    state->u.delegation.type = 0;
    update_delegations(entry);
    rc = killstate(state);
    
    return ERR_STATE_NO_ERROR;
}

int localstate_query_delegation(fsal_handle_t *handle, clientid4 clientid,
				delegationstate* outdelegation)
{
    entryheader* header;
    state* cur = NULL;
    int rc = 0;
    
    /* Retrieve or create header for per-filehandle chain */

    if (!(header = header_for_read(handle)))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_query_delegation: could not find header entry.");
	    return ERR_STATE_FAIL;
	}

    while (iterate_entry(header, &cur))
	{
	    if ((cur->type = delegation) &&
		(cur->clientid == clientid))
		break;
	    else
		continue;
	}

    if (!cur)
	{
	    pthread_rwlock_unlock(&(header->lock));
	    return ERR_STATE_NOENT;
	}

    memset(outdelegation, 0, sizeof(delegationstate));

    filldelegation(cur, outdelegation, header);

    pthread_rwlock_unlock(&(header->lock));

    return ERR_STATE_NO_ERROR;
}

void filldelegationstate(state* cur, delegationstate outdelegation,
			 entryheader* header)
{
    outdelegation->handle = header->handle;
    outdelegation->stateid = cur->stateid;
    outdelegation->clientid = cur->clientid;
    outdelegation->u.delegation.type = cur->u.delegation.type;
    outdelegation->u.delegation.limit = cur->u.delegation.limit;
}

int localstate_check_delegation(fsal_handle_t *handle,
				open_delegation_type4 type)
{
    entryheader* header;
    
    if ((type != OPEN_DELEGATE_READ) &&
	(type != OPEN_DELEGATE_WRITE))
      {
	LogDebug(COMPONENT_STATES,
		 "state_check_delegation: attempt to create delegation of invalid type.");
	return ERR_STATE_INVAL;
      }
    
    /* Retrieve or create header for per-filehandle chain */

    if (!(header = header_for_read(handle)))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_check_delegation: could not find header entry.");
	    return 0;
	}

    pthread_rwlock_unlock(&(header->lock));

    if (type == OPEN_DLEGATE_READ)
	return header->read_delegations;
    else
	return header->write_delegations;
}
