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
 * Delegation Functions
 *
 * These functions realise delegation state functionality.
 ***********************************************************************/

void update_dir_delegations(entryheader_t* entry)
{
    state_t* cur = NULL;
    
    entry->dir_delegations = 0;

    while (iterate_entry(entry, &cur))
	{
	    if (cur->type != STATE_DIR_DELEGATION)
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
    entryheader_t* header;
    state_t* state;
    int rc = 0;

    /* Retrieve or create header for per-filehandle chain */

    if (!(header = lookupheader(handle)))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_create_dir_delegation: could not find/create header entry.");
	    return ERR_STATE_FAIL;
	}

    /* Create and fill in new entry */

    if (!(state = newstate(clientid, header)))
	{
	    LogDebug(COMPONENT_STATES,
		     "state_create_dir_delegation: Unable to create new state.");
	    return ERR_STATE_FAIL;
	}

    state->type = STATE_DIR_DELEGATION;
    state->state.dir_delegation.notification_types = notification_types;
    state->state.dir_delegation.child_attr_delay = child_attr_delay;
    state->state.dir_delegation.dir_attr_delay = dir_attr_delay;
    state->state.dir_delegation.child_attributes = child_attributes;
    state->state.dir_delegation.dir_attributes = dir_attributes;
    
    *stateid = state->stateid;
    return ERR_STATE_NO_ERROR;
}

int localstate_delete_dir_delegation(stateid4 stateid)
{
    state_t* state;
    entryheader_t* header;
    int rc;

    if (rc = lookup_state(stateid, &state))
      return rc;

    header = state->header;

    state->type = STATE_ANY;
    update_dir_delegations(header);
    killstate(state);
    
    return ERR_STATE_NO_ERROR;
}

int localstate_query_dir_delegation(fsal_handle_t *handle, clientid4 clientid,
				    dir_delegationstate* outdir_delegation)
{
    entryheader_t* header;
    state_t* cur = NULL;
    int rc = 0;
    
    /* Retrieve or create header for per-filehandle chain */

    if (!(header = lookupheader(handle)))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_query_dir_delegation: could not find header entry.");
	    return ERR_STATE_FAIL;
	}

    while (iterate_entry(header, &cur))
	{
	    if ((cur->type = STATE_DIR_DELEGATION) &&
		(cur->clientid == clientid))
		break;
	    else
		continue;
	}

    if (!cur)
      return ERR_STATE_NOENT;

    memset(outdir_delegation, 0, sizeof(dir_delegationstate));

    filldir_delegationstate(cur, outdir_delegation, header);

    return ERR_STATE_NO_ERROR;
}

void filldir_delegationstate(state_t* cur,
			     dir_delegationstate* outdir_delegation,
			     entryheader_t* header)
{
    outdir_delegation->handle = header->handle;
    outdir_delegation->clientid = cur->clientid;
    outdir_delegation->stateid = cur->stateid;
    outdir_delegation->notification_types = cur->state.dir_delegation.notification_types;
    outdir_delegation->child_attr_delay = cur->state.dir_delegation.child_attr_delay;
    outdir_delegation->dir_attr_delay = cur->state.dir_delegation.dir_attr_delay;
}

int localstate_check_dir_delegation(fsal_handle_t *handle)
{
    entryheader_t* header;

    if (!(header = lookupheader(handle)))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_check_dir_delegation: could not find header entry.");
	    return 0;
	}

    return header->dir_delegations;
}
