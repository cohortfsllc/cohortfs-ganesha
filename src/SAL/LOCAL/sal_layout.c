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

int state_create_layout_state(fsal_handle_t handle,
			      stateid4 ostateid,
			      layouttype4 type,
			      stateid4* stateid);
{
    entryheader* header;
    state* state = NULL;
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

    if (rc != ERR_STATE_NO_ERROR)
	{
	    pthread_rwlock_unlock(&(header->lock));
	    LogError(COMPONENT_STATES,
		     "state_create_layout_state: could not find open state.");
	    return rc;
	}

    if (!((openstate->type == share) ||
	  (openstate->type == delegation) ||
	  (openstate->type == lock)))
      {
	  pthread_rwlock_unlock(&(header->lock));
	  LogError(COMPONENT_STATES,
		   "state_create_layout_state: open state of wrong type.");
	  return ERR_STATE_INVAL;
      }

    while (iterate_entry(header, &state))
	{
	    if ((state->type == layout) &&
		(state->clientid == clientid)
		(state->u.layout.type == type))
		break;
	    else
		continue;
	}

    if (cur)
	{
	    pthread_rwlock_unlock(&(header->lock));
	    LogDebug(COMPONENT_STATES,
		     "state_create_layout_state: corresponding layout state already exists.");
	    return ERR_STATE_PREEXISTS;
	}
    

    /* Create and fill in new entry */

    if (!(state = newstate(clientid, header)))
	{
	    pthread_rwlock_unlock(&(header->lock));
	    LogDebug(COMPONENT_STATES,
		     "state_create_layout_state: Unable to create new state.");
	    return ERR_STATE_FAIL;
	}

    state->type = layout;
    state->u.layout.type = type;
    
    *stateid = header->stateid;
    pthread_rwlock_unlock(&(header->lock));
    return ERR_STATE_NO_ERROR;
}

int state_delete_layout_state(stateid4 stateid);
{
    state* state;
    state* cur = NULL;
    entryheader* header;
    int rc;

    if (rc = lookup_state_and_lock(stateid, &state, &header, true))
	{
	    LogError(COMPONENT_STATES,
		     "state_delete_layout_state: could not find state.");
	    return ERR_STATE_FAIL;
	}

    if (state->u.layout.layoutentries)
	{
	    pthread_rwlock_unlock(&(header->lock));
	    LogDebug(COMPONENT_STATES,
		     "state_delete_layout_state: Layouts held.");
	    return ERR_STATE_LOCKSHELD;
	    
	}
    
    killstate(state);
    
    return ERR_STATE_NO_ERROR;
}

int state_query_layout_state(fsal_handle_t *handle,
			     layouttype4 type,
			     lockstate* outlayoutstate)
{
    entryheader* header;
    state* cur = NULL;
    int rc = 0;
    
    /* Retrieve or create header for per-filehandle chain */

    if (!(header = header_for_read(handle)))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_query_layout_state: could not find header entry.");
	    return rc;
	}

    while (iterate_entry(header, &cur))
	{
	    if ((cur->type == layout) &&
		(cur->clientid == clientid)
		(cur->u.layout.type == type))
		break;
	    else
		continue;
	}

    if (!cur)
	{
	    LogMajor(COMPONENT_STATES,
		     "state_query_layout_state: could not find state.");
	    pthread_rwlock_unlock(&(header->lock));
	    return ERR_STATE_NOENT;
	}

    memset(outlayoutstate, 0, sizeof(layout));

    outlayoutstate->handle = header->handle;
    outlayoutstate->clientid = cur->clientid;
    outlayoutstate->stateid = cur->stateid;
    outlayoutstate->type = cur->u.layout.type;
    
    pthread_rwlock_unlock(&(header->lock));

    return ERR_STATE_NO_ERROR;
}

int state_add_layout_segment(layouttype4 type,
			     layoutimode4 iomode,
			     offset4 offset,
			     length4 length,
			     boolean return_on_close,
			     fsal_layout_t* layoutdata,
			     stateid4* stateid)
{
    state* state;
    layoutentry* layoutentry;
    int rc = 0;
    
    GET_PREALLOC(header, entryheaderpool, 1, entryheader,
		 next_alloc);
    
    if (!layoutentry)
	{
	    LogError(COMPONENT_STATES,
		     "state_add_layout_segment: cannot allocate segment.");
	    return ERR_STATE_FAIL;
	}

    if (rc = lookup_state_and_lock(stateid, &state, &header, true))
	{
	    LogError(COMPONENT_STATES,
		     "state_add_layout_segment: could not find state.");
	    return ERR_STATE_FAIL;
	}
    if (state->type != layout)
	{
	    LogError(COMPONENT_STATES,
		     "state_add_layout_segment: supplied state of wrong type.");
	    pthread_rwlock_unlock(&(state->header->lock));
	    return ERR_STATE_INVAL;
	}
    
    layoutentry->type = type;
    layoutentry->iomode = iomode;
    layoutentry->offset = offset;
    layoutentry->length = length;
    layoutentry->return_on_close = return_on_close;
    layoutentry->layoutdata = layoutdata;
    layoutentry->prev = NULL;
    layoutentry->next = state->entries;
    state->u.layout.entries = layoutentry;

    pthread_rwlock_unlock(&(state->header->lock));
    return ERR_STATE_NO_ERROR;
}

int state_mod_layout_segment(layoutimode4 iomode,
			     offset4 offset,
			     length4 length,
			     fsal_layout_t* layoutdata,
			     stateid4 stateid,
			     uint64_t segid)
{
    layoutentry* layoutentry = (layoutentry* )segid;
    
    layoutentry->iomode = iomode;
    layoutentry->offset = offset;
    layoutentry->length = length;
    layoutentry->layoutdata = layoutdata;

    pthread_rwlock_unlock(&(state->header->lock));
    return ERR_STATE_NO_ERROR;
}

int state_free_layout_segment(stateid4 stateid,
			      uint64_t segid)
{
    layoutentry* layoutentry = (layoutentry* )segid;
    state* state;
    int rc = 0;
    
    if (rc = lookup_state_and_lock(stateid, &state, &header, true))
	{
	    LogError(COMPONENT_STATES,
		     "state_free_layout_segment: could not find state.");
	    return ERR_STATE_FAIL;
	}
    if (state->type != layout)
	{
	    LogError(COMPONENT_STATES,
		     "state_free_layout_segment: supplied state of wrong type.");
	    pthread_rwlock_unlock(&(state->header->lock));
	    return ERR_STATE_INVAL;
	}
    
    if (layoutentry->prev == NULL)
	{
	    if (layoutentry->next)
		layoutentry->next->prev = NULL;
	    state->u.layout.entries = layoutentry->next;
	}
    else
	{
	    layoutentry->prev->next = layoutentry->next;
	    if (layoutentry->next)
		layoutentry->next->prev = layoutentry->prev;
	}
    
    RELEASE_PREALLOC(layoutentry, layoutentrypool, next_alloc);

    pthread_rwlock_unlock(&(state->header->lock));
    return ERR_STATE_NO_ERROR;
}

int state_layout_inc_state(stateid4* stateid)
{
    state* state;
    int rc = 0;
    
    if (rc = lookup_state_and_lock(stateid, &state, &header, true))
	{
	    LogError(COMPONENT_STATES,
		     "state_inc_layout_state: could not find state.");
	    return ERR_STATE_FAIL;
	}
    if (state->type != layout)
	{
	    LogError(COMPONENT_STATES,
		     "state_inc_layout_state: supplied state of wrong type.");
	    pthread_rwlock_unlock(&(state->header->lock));
	    return ERR_STATE_INVAL;
	}

    ++state->stateid.seqid;

    *stateid = state->stateid;

    pthread_rwlock_unlock(&(state->header->lock));
    return ERR_STATE_NO_ERROR;
}

int state_iter_layout_entries(stateid4 stateid,
			      uint64_t* cookie,
			      boolean* finished,
			      layoutsegment* segment)
{
    state* state = NULL;
    layoutentry* layoutentry = NULL;
    int rc = 0;

    *finished = false;
    
    if (*cookie)
	layoutentry = (layoutentry* )cookie;
    else
	{
	    if (rc = lookup_state_and_lock(stateid, &state, &header,
					   false))
		{
		    LogError(COMPONENT_STATES,
			     "state_inc_layout_state: could not find state.");
		    return rc;
		}
	    if (state->type != layout)
		{
		    LogError(COMPONENT_STATES,
			     "state_inc_layout_state: supplied state of wrong type.");
		    pthread_rwlock_unlock(&(state->header->lock));
		    return ERR_STATE_INVAL;
		}
	    layoutentry = state->u.layout.entries;
	    if (!layoutentry)
		return ERR_STATE_NOENT;
	}

    segment->type = layoutentry->type;
    segment->iomode = layotuentry->iomode;
    segment->offset = layoutentry->offset;
    segment->length = layoutentry->length;
    segment->return_on_close = layoutentry->return_on_close;
    segment->layoutdata layoutentry->layoutdata;
    segment->segid = (uint64_t) layoutentry;
    *cookie = (uint64_t) (layoutentry->next);

    if (*cookie == 0)
	*finished = true;

    if (state)
	pthread_rwlock_unlock(&(state->header->lock));

    return ERR_STATE_NO_ERROR;
}

