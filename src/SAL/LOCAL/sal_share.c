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
 * Share Functions
 *
 * These functions realise share state functionality.
 ***********************************************************************/

/* Make sure the maximum share/deny values are correct */

void updatemax(entryheader_t* header)
{
    state_t* cur;
    
    header->max_share = 0;
    header->max_deny = 0;

    while (iterate_entry(header, &cur))
	{
	    if (cur->type != 1)
		continue;
	    header->max_share |= cur->state.share.share_access;
	    header->max_deny |= cur->state.share.share_deny;
	}
}

int share_conflict(entryheader_t* header, state_owner_t* owner,
		   uint32_t share_access, uint32_t share_deny)
{
    state_t* state = NULL;
    
    if (share_access == 0)
	return ERR_STATE_INVAL;

    if ((share_access & header->max_deny) ||
	(share_deny & header->max_share) ||
	((share_deny & OPEN4_SHARE_DENY_READ) &&
	 header->anonreaders) ||
	((share_deny & OPEN4_SHARE_DENY_WRITE) &&
	 header->anonwriters) ||
	(((share_access & OPEN4_SHARE_ACCESS_WRITE) ||
	  (share_deny & OPEN4_SHARE_DENY_READ)) &&
	 header->read_delegations) ||
	header->write_delegation)
	return ERR_STATE_CONFLICT;

    if (owner)
	while (iterate_entry(header, &state))
	    if ((state->type == STATE_SHARE) &&
		(state->state.share.open_owner == owner))
		return ERR_STATE_PREEXISTS;

    return ERR_STATE_NO_ERROR;
}

int localstate_create_share(fsal_handle_t *handle, open_owner4 open_owner,
			    clientid4 clientid, uint32_t share_access,
			    uint32_t share_deny,
			    cache_inode_openref_t* openref,
			    stateid4* stateid)
{
    entryheader_t* header;
    state_t* state;
    state_owner_t* owner;
    int rc = 0;
    
    /* Retrieve or create header for per-filehandle chain */

    if (!(header = lookupheader(handle)))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_create_share: could not find/create header entry.");
	    return ERR_STATE_FAIL;
	}

    if (!(owner
	  = acquire_owner(open_owner.owner.owner_val,
			  open_owner.owner.owner_len, clientid, false,
			  false, NULL)))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_create_share: could not find/create state owner entry.");
	    return ERR_STATE_FAIL;
	}
				

    /* Check for potential conflicts */

    if (rc = share_conflict(header, owner, share_access, share_deny))
	{
	    LogDebug(COMPONENT_STATES,
		     "state_create_share: share conflict.");
	    return rc;
	}

    /* Create and fill in new entry */

    if (!(state = newstate(clientid, header)))
	{
	    LogDebug(COMPONENT_STATES,
		     "state_create_share: Unable to create new state.");
	    return ERR_STATE_FAIL;
	}
    state->type = STATE_SHARE;
    state->state.share.share_access = share_access;
    state->state.share.open_owner = NULL;
    state->state.share.share_deny = share_deny;


    /* Update maxima for quick lookups */

    header->max_share |= share_access;
    header->max_deny |= share_deny;

    owner->refcount++;

    *stateid = state->stateid;
    return ERR_STATE_NO_ERROR;
}

int localstate_check_share(fsal_handle_t handle, uint32_t share_access,
		      uint32_t share_deny)
{
  entryheader_t* header;
  int rc;

  if (!(header = lookupheader(&handle)))
    {
      /* No header, no conflict */
      return ERR_STATE_NO_ERROR;
    }
  rc = share_conflict(header, NULL, share_access, share_deny);
  return rc;
}

int localstate_upgrade_share(uint32_t share_access, uint32_t share_deny,
			     stateid4* stateid)
{
    state_t* state;
    entryheader_t* header;
    int rc;

    if (rc = lookup_state(*stateid, &state))
	{
	    LogDebug(COMPONENT_STATES,
		     "state_upgrade_share: could not find state.");
	}

    header = state->header;
    if (rc = share_conflict(header, NULL, share_access, share_deny))
      return rc;
    state->state.share.share_access |= share_access;
    state->state.share.share_deny |= share_deny;
    state->stateid.seqid++;
    state->state.share.open_owner->seqid++;
    
    header->max_share |= share_access;
    header->max_deny |= share_deny;
    
    *stateid = state->stateid;
    return ERR_STATE_NO_ERROR;
}

int localstate_downgrade_share(uint32_t share_access, uint32_t share_deny,
			       stateid4* stateid)
{
    state_t* state;
    entryheader_t* header;
    int rc;

    if (rc = lookup_state(*stateid, &state))
	    return rc;

    header = state->header;
    if ((share_access & ~state->state.share.share_access) ||
	(share_deny & ~state->state.share.share_deny))
      return ERR_STATE_INVAL;

    state->state.share.share_access = share_access;
    state->state.share.share_deny = share_deny;
    state->stateid.seqid++;

    state->state.share.open_owner->seqid++;
    
    updatemax(header);
    
    *stateid = state->stateid;
    return ERR_STATE_NO_ERROR;
}

int localstate_delete_share(stateid4 stateid)
{
    state_t* state;
    entryheader_t* header;
    int rc;

    if (rc = lookup_state(stateid, &state))
	return rc;

    header = state->header;
    if (state->state.share.locks)
      return ERR_STATE_LOCKSHELD;

    state->state.share.share_access = 0;
    state->state.share.share_deny = 0;
    state->state.share.open_owner->seqid++;
    state->state.share.open_owner->refcount--;

    if (state->state.share.open_owner->refcount == 0)
	killowner(state->state.share.open_owner);
    
    updatemax(header);

    killstate(state);
    
    return ERR_STATE_NO_ERROR;
}

int localstate_query_share(fsal_handle_t *handle, clientid4 clientid,
			   open_owner4 open_owner, sharestate* outshare)
{
    entryheader_t* header;
    state_t* cur = NULL;
    int rc = 0;
    state_owner_t* owner;
    bool_t created = false;
    
    /* Retrieve or create header for per-filehandle chain */

    if (!(header = lookupheader(handle)))
	{
	  /* No header, no state */
	  return ERR_STATE_NOENT;
	}

    if (!(owner
	  = acquire_owner(open_owner.owner.owner_val,
			  open_owner.owner.owner_len, clientid, false,
			  false, &created)))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_query_share: could not find/create state owner entry.");
	    return ERR_STATE_FAIL;
	}

    if (created)
	{
	    killowner(owner);
	    return ERR_STATE_NOENT;
	}

    while (iterate_entry(header, &cur))
	{
	    if ((cur->type == STATE_SHARE) &&
		(cur->state.share.open_owner == owner))
		break;
	    else
		continue;
	}

    if (!cur)
      return ERR_STATE_NOENT;

    memset(outshare, 0, sizeof(sharestate));
    
    fillsharestate(cur, outshare, header);

    return ERR_STATE_NO_ERROR;
}

void fillsharestate(state_t* cur, sharestate* outshare,
		    entryheader_t* header)
{
    outshare->handle = header->handle;
    outshare->stateid = cur->stateid;
    outshare->clientid = cur->clientid;
    memcpy(outshare->open_owner.owner.owner_val,
	   cur->state.share.open_owner->key.owner_val,
	   cur->state.share.open_owner->key.owner_len);
    outshare->open_owner.owner.owner_len
	= cur->state.share.open_owner->key.owner_len;
    outshare->open_owner.clientid = cur->clientid;
    outshare->share_access = cur->state.share.share_access;
    outshare->share_deny = cur->state.share.share_deny;
    outshare->locksheld = cur->state.share.locks;
    outshare->openref = cur->state.share.openref;
}

int localstate_start_32read(fsal_handle_t *handle)
{
    entryheader_t* header;
    
    /* Retrieve or create header for per-filehandle chain */

    if (!(header = lookupheader(handle)))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_start_32read: could not find/create header entry.");
	    return ERR_STATE_FAIL;
	}

    /* Check for potential conflicts */

    if ((header->max_deny & OPEN4_SHARE_DENY_READ) ||
	header->write_delegation)
	return ERR_STATE_CONFLICT;

    header->anonreaders++;

    return ERR_STATE_NO_ERROR;
}

int localstate_start_32write(fsal_handle_t *handle)
{
    entryheader_t* header;
    
    /* Retrieve or create header for per-filehandle chain */

    if (!(header = lookupheader(handle)))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_start_32write: could not find/create header entry.");
	    return ERR_STATE_FAIL;
	}

    /* Check for potential conflicts */

    if ((header->max_deny & OPEN4_SHARE_DENY_WRITE) ||
	header->read_delegations ||
	header->write_delegation)
	return ERR_STATE_CONFLICT;

    header->anonwriters++;

    return ERR_STATE_NO_ERROR;
}

int localstate_end_32read(fsal_handle_t *handle)
{
    entryheader_t* header;
    
    /* Retrieve or create header for per-filehandle chain */

    if (!(header = lookupheader(handle)))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_end_32read: could not find/create header entry.");
	    return ERR_STATE_FAIL;
	}

    /* Check for potential conflicts */

    header->anonreaders ? header->anonreaders-- : 0;

    return ERR_STATE_NO_ERROR;
}

int localstate_end_32write(fsal_handle_t *handle)
{
    entryheader_t* header;
    
    /* Retrieve or create header for per-filehandle chain */

    if (!(header = lookupheader(handle)))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_end_32write: could not find/create header entry.");
	    return ERR_STATE_FAIL;
	}

    header->anonwriters ? header->anonwriters-- : 0;

    return ERR_STATE_NO_ERROR;
}
