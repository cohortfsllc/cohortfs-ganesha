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

void update_dir_delegations(entryheader* entry)
{
    state* cur = NULL;
    
    entry->dir_delegations = 0;

    while (iterate_entry(entry, &cur))
	{
	    if (cur->type != dir_delegation)
		continue;
	    else
		{
		    entry->dir_delegations = 1;
		    break;
		}
	}
}

int localstate_create_dir_delegation(fsal_handle_t *handle, clientid4 clientid,
				     bitmap4 notification_types,
				     attr_notice4 child_attr_delay,
				     attr_notice4 dir_attr_delay,
				     bitmap4 child_attributes,
				     bitmap4 dir_attributes,
				     stateid4* stateid)
{
    entryheader* header;
    state* state;
    int rc = 0;

    /* Retrieve or create header for per-filehandle chain */

    if (!(header = header_for_write(handle)))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_create_dir_delegation: could not find/create header entry.");
	    return ERR_STATE_FAIL;
	}

    /* Create and fill in new entry */

    if (!(state = newstate(clientid, header)))
	{
	    pthread_rwlock_unlock(&(header->lock));
	    LogDebug(COMPONENT_STATES,
		     "state_create_dir_delegation: Unable to create new state.");
	    return ERR_STATE_FAIL;
	}

    state->type = dir_delegation;
    state->u.dir_delegation.notification_types = notification_types;
    state->u.dir_delegation.child_attr_delay = child_attr_delay;
    state->u.dir_delegation.dir_attr_delay = dir_attr_delay;
    state->u.dir_delegation.child_attributes = child_attributes;
    state->u.dir_delegation.dir_attributes = dir_attributes;
    
    *stateid = header->stateid;
    pthread_rwlock_unlock(&(header->lock));
    return ERR_STATE_NO_ERROR;
}

int localstate_delete_dir_delegation(stateid4 stateid);
{
    state* state;
    entryheader* header;
    int rc;

    if (rc = lookup_state_and_lock(stateid, &state, &header, true))
	{
	    LogError(COMPONENT_STATES,
		     "state_delete_dir_delegation: could not find state.");
	}

    state->type = any;
    update_dir_delegations(entry);
    rc = killstate(state);
    
    return ERR_STATE_NO_ERROR;
}

int localstate_query_dir_delegation(fsal_handle_t *handle, clientid4 clientid,
				    dir_delegationstate* outdir_delegation)
{
    entryheader* header;
    state* cur = NULL;
    int rc = 0;
    
    /* Retrieve or create header for per-filehandle chain */

    if (!(header = header_for_read(handle)))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_query_dir_delegation: could not find header entry.");
	    return ERR_STATE_FAIL;
	}

    while (iterate_entry(header, &cur))
	{
	    if ((cur->type = dir_delegation) &&
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

    memset(outdir_delegation, 0, sizeof(dir_delegationstate));

    filldir_delegationstate(cur, outdir_delegation, header);

    pthread_rwlock_unlock(&(header->lock));

    return ERR_STATE_NO_ERROR;
}

void filldir_delegationstate(state* cur,
			     dir_delegationstate* outdir_delegation,
			     entryheader* header)
{
    outdir_delegation->handle = header->handle;
    outdir_delegation->clientid = cur->clientid;
    outdir_delegation->stateid = cur->stateid;
    outdir_delegation->notification_types = cur->notification_types;
    outdir_delegation->child_attr_delay = cur->child_attr_delay;
    outdir_delegation->dir_attr_delay = cur->dir_attr_delay;
}

int localstate_check_delegation(fsal_handle_t *handle)
{
    entryheader* header;

    if (!(header = header_for_read(handle)))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_check_dir_delegation: could not find header entry.");
	    return 0;
	}

    unlock(&(header->lock));
    return header->write_delegations;
}
