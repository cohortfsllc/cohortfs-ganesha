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
#include "log_macros.h"
#include "sal_internal.h"

/************************************************************************
 * Delegation Functions
 *
 * These functions realise delegation state functionality.
 ***********************************************************************/

void update_delegations(entryheader_t* entry)
{
    state_t* cur = NULL;
    
    entry->read_delegations = 0;
    entry->write_delegation = 0;

    while (iterate_entry(entry, &cur))
	{
	    if (cur->type != STATE_DELEGATION)
		continue;
	    if (cur->state.delegation.type == OPEN_DELEGATE_READ)
		{
		    entry->read_delegations = 1;
		    break;
		}
	    else if (cur->state.delegation.type == OPEN_DELEGATE_WRITE)
		{
		    entry->write_delegation = 1;
		    break;
		}
	}
}

int localstate_create_delegation(fsal_handle_t *handle, clientid4 clientid,
				 open_delegation_type4 type,
				 nfs_space_limit4 limit, stateid4* stateid)
{
    entryheader_t* header;
    state_t* state;
    int rc = 0;

    if ((type != OPEN_DELEGATE_READ) &&
	(type != OPEN_DELEGATE_WRITE))
      {
	LogDebug(COMPONENT_STATES,
		 "state_create_delegation: attempt to create delegation of invalid type.");
	return ERR_STATE_INVAL;
      }
    
    /* Retrieve or create header for per-filehandle chain */

    if (!(header = lookupheader(handle)))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_create_delegation: could not find/create header entry.");
	    return ERR_STATE_FAIL;
	}

    /* Check for potential conflicts */

    if ((header->max_share & OPEN4_SHARE_ACCESS_WRITE) ||
	(header->anonwriters) ||
	(header->max_deny & OPEN4_SHARE_DENY_READ) ||
	(header->write_delegation) ||
	(type == OPEN_DELEGATE_WRITE &&
	 (header->max_share ||
	  header->anonwriters ||
	  header->read_delegations)))
	{
	    LogDebug(COMPONENT_STATES,
		     "state_create_delegation: share conflict.");
	    return rc;
	}

    /* Create and fill in new entry */

    if (!(state = newstate(clientid, header)))
	{
	    LogDebug(COMPONENT_STATES,
		     "state_create_share: Unable to create new state.");
	    return ERR_STATE_FAIL;
	}

    state->type = STATE_DELEGATION;
    state->state.delegation.type =  type;
    state->state.delegation.limit = limit;

    *stateid = state->stateid;
    return ERR_STATE_NO_ERROR;
}

int localstate_delete_delegation(stateid4 stateid)
{
    state_t* state;
    entryheader_t* header;
    int rc;

    if (rc = lookup_state(stateid, &state))
	return rc;

    header = state->header;
    state->state.delegation.type = 0;
    update_delegations(header);
    killstate(state);
    
    return ERR_STATE_NO_ERROR;
}

int localstate_query_delegation(fsal_handle_t *handle, clientid4 clientid,
				delegationstate* outdelegation)
{
    entryheader_t* header;
    state_t* cur = NULL;
    int rc = 0;
    
    /* Retrieve or create header for per-filehandle chain */

    if (!(header = lookupheader(handle)))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_query_delegation: could not find header entry.");
	    return ERR_STATE_FAIL;
	}

    while (iterate_entry(header, &cur))
	{
	    if ((cur->type = STATE_DELEGATION) &&
		(cur->clientid == clientid))
		break;
	    else
		continue;
	}

    if (!cur)
      return ERR_STATE_NOENT;

    memset(outdelegation, 0, sizeof(delegationstate));

    filldelegationstate(cur, outdelegation, header);

    return ERR_STATE_NO_ERROR;
}

void filldelegationstate(state_t* cur, delegationstate* outdelegation,
			 entryheader_t* header)
{
    outdelegation->handle = header->handle;
    outdelegation->stateid = cur->stateid;
    outdelegation->clientid = cur->clientid;
    outdelegation->type = cur->state.delegation.type;
    outdelegation->limit = cur->state.delegation.limit;
}

int localstate_check_delegation(fsal_handle_t *handle,
				open_delegation_type4 type)
{
    entryheader_t* header;
    
    if ((type != OPEN_DELEGATE_READ) &&
	(type != OPEN_DELEGATE_WRITE))
      {
	LogDebug(COMPONENT_STATES,
		 "state_check_delegation: attempt to interrogate delegation of invalid type.");
	return ERR_STATE_INVAL;
      }
    
    /* Retrieve or create header for per-filehandle chain */

    if (!(header = lookupheader(handle)))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_check_delegation: could not find header entry.");
	    return 0;
	}

    if (type == OPEN_DELEGATE_READ)
	return header->read_delegations;
    else
	return header->write_delegation;
}
