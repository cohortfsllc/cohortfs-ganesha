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
 * Lock Functions
 *
 * These functions realise lock state functionality.
 ***********************************************************************/

int localstate_create_layout_state(fsal_handle_t* handle,
				   stateid4 ostateid,
				   clientid4 clientid,
				   layouttype4 type,
				   stateid4* stateid)
{
    entryheader_t* header;
    state_t* state = NULL;
    state_t* openstate = NULL;
    int rc = 0;

    /* Retrieve or create header for per-filehandle chain */

    if (!(header = lookupheader(handle)))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_create_lock_state: could not find/create header entry.");
	    return ERR_STATE_FAIL;
	}

    rc = lookup_state(ostateid, &openstate);

    if (rc != ERR_STATE_NO_ERROR)
      return rc;

    if (openstate->type == STATE_LAYOUT)
      if (ostateid.seqid == 0)
	return ERR_STATE_BADSEQ;
      else
	{
	  *stateid = ostateid;
	  return ERR_STATE_NO_ERROR;
	}

    if (!((openstate->type == STATE_SHARE) ||
	  (openstate->type == STATE_DELEGATION) ||
	  (openstate->type == STATE_LOCK)))
      return ERR_STATE_INVAL;

    while (iterate_entry(header, &state))
	{
	    if ((state->type == STATE_LAYOUT) &&
		(state->clientid == clientid) &&
		(state->state.layout.type == type))
		break;
	    else
		continue;
	}

    if (state)
      {
	*stateid = state->stateid;
	return ERR_STATE_NO_ERROR;
      }
    

    /* Create and fill in new entry */

    if (!(state = newstate(clientid, header)))
	{
	    LogDebug(COMPONENT_STATES,
		     "state_create_layout_state: Unable to create new state.");
	    return ERR_STATE_FAIL;
	}

    state->type = STATE_LAYOUT;
    state->state.layout.type = type;
    state->stateid.seqid = 0; /* The addition of the first segment
				 will bump this */
    
    *stateid = state->stateid;
    return ERR_STATE_NO_ERROR;
}

int localstate_delete_layout_state(stateid4 stateid)
{
    state_t* state;
    entryheader_t* header;
    int rc;

    if (rc = lookup_state(stateid, &state))
      return ERR_STATE_FAIL;

    header = state->header;
    
    if (state->state.layout.layoutentries)
	{
	    LogDebug(COMPONENT_STATES,
		     "state_delete_layout_state: Layouts held.");
	    return ERR_STATE_LOCKSHELD;
	    
	}
    
    killstate(state);
    
    return ERR_STATE_NO_ERROR;
}

int localstate_query_layout_state(fsal_handle_t *handle,
				  clientid4 clientid,
				  layouttype4 type,
				  layoutstate* outlayoutstate)
{
    entryheader_t* header;
    state_t* cur = NULL;
    int rc = 0;
    
    /* Retrieve or create header for per-filehandle chain */

    if (!(header = lookupheader(handle)))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_query_layout_state: could not find header entry.");
	    return rc;
	}

    while (iterate_entry(header, &cur))
	{
	    if ((cur->type == STATE_LAYOUT) &&
		(cur->clientid == clientid) &&
		(cur->state.layout.type == type))
		break;
	    else
		continue;
	}

    if (!cur)
	{
	    LogMajor(COMPONENT_STATES,
		     "state_query_layout_state: could not find state.");
	    return ERR_STATE_NOENT;
	}

    memset(outlayoutstate, 0, sizeof(layoutstate));

    return ERR_STATE_NO_ERROR;
}

void filllayoutstate(state_t* cur, layoutstate* outlayoutstate,
		     entryheader_t* header)
{
    outlayoutstate->handle = header->handle;
    outlayoutstate->clientid = cur->clientid;
    outlayoutstate->stateid = cur->stateid;
    outlayoutstate->type = cur->state.layout.type;
}

int localstate_add_layout_segment(layouttype4 type,
				  layoutiomode4 iomode,
				  offset4 offset,
				  length4 length,
				  bool_t return_on_close,
				  fsal_layoutdata_t* layoutdata,
				  stateid4 stateid)
{
    state_t* state;
    entryheader_t* header;
    locallayoutentry_t* thentry;
    int rc = 0;
    
    GET_PREALLOC(thentry, layoutentrypool, 1, locallayoutentry_t,
		 next_alloc);
    
    if (!thentry)
	{
	    LogMajor(COMPONENT_STATES,
		     "state_add_layout_segment: cannot allocate segment.");
	    return ERR_STATE_FAIL;
	}

    if (rc = lookup_state(stateid, &state))
      return ERR_STATE_FAIL;

    header = state->header;

    if (state->type != STATE_LAYOUT)
	{
	    LogMajor(COMPONENT_STATES,
		     "state_add_layout_segment: supplied state of wrong type.");
	    return ERR_STATE_INVAL;
	}
    
    thentry->type = type;
    thentry->iomode = iomode;
    thentry->offset = offset;
    thentry->length = length;
    thentry->return_on_close = return_on_close;
    thentry->layoutdata = layoutdata;
    thentry->prev = NULL;
    thentry->next = state->state.layout.layoutentries;
    state->state.layout.layoutentries = thentry;

    return ERR_STATE_NO_ERROR;
}

int localstate_mod_layout_segment(layoutiomode4 iomode,
				  offset4 offset,
				  length4 length,
				  fsal_layoutdata_t* layoutdata,
				  stateid4 stateid,
				  uint64_t segid)
{
    locallayoutentry_t* layoutentry = (locallayoutentry_t*)segid;
    
    layoutentry->iomode = iomode;
    layoutentry->offset = offset;
    layoutentry->length = length;
    layoutentry->layoutdata = layoutdata;

    return ERR_STATE_NO_ERROR;
}

int localstate_free_layout_segment(stateid4 stateid,
			      uint64_t segid)
{
    locallayoutentry_t* layoutentry = (locallayoutentry_t*)segid;
    entryheader_t* header;
    state_t* state;
    int rc = 0;
    
    if (rc = lookup_state(stateid, &state))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_free_layout_segment: could not find state.");
	    return ERR_STATE_FAIL;
	}

    header = state->header;
    
    if (state->type != STATE_LAYOUT)
	{
	    LogMajor(COMPONENT_STATES,
		     "state_free_layout_segment: supplied state of wrong type.");
	    return ERR_STATE_INVAL;
	}
    
    if (layoutentry->prev == NULL)
	{
	    if (layoutentry->next)
		layoutentry->next->prev = NULL;
	    state->state.layout.layoutentries = layoutentry->next;
	}
    else
	{
	    layoutentry->prev->next = layoutentry->next;
	    if (layoutentry->next)
		layoutentry->next->prev = layoutentry->prev;
	}
    
    RELEASE_PREALLOC(layoutentry, layoutentrypool, next_alloc);

    return ERR_STATE_NO_ERROR;
}

int localstate_layout_inc_state(stateid4* stateid)
{
    state_t* state;
    entryheader_t* header;
    int rc = 0;
    
    if (rc = lookup_state(*stateid, &state))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_inc_layout_state: could not find state.");
	    return ERR_STATE_FAIL;
	}

    header = state->header;
    
    if (state->type != STATE_LAYOUT)
	{
	    LogMajor(COMPONENT_STATES,
		     "state_inc_layout_state: supplied state of wrong type.");
	    return ERR_STATE_INVAL;
	}

    ++state->stateid.seqid;

    *stateid = state->stateid;

    return ERR_STATE_NO_ERROR;
}

int localstate_iter_layout_entries(stateid4 stateid,
				   uint64_t* cookie,
				   bool_t* finished,
				   layoutsegment* segment)
{
    state_t* state = NULL;
    entryheader_t* header;
    locallayoutentry_t* layoutentry = NULL;
    int rc = 0;

    *finished = false;
    
    if (*cookie)
	layoutentry = (locallayoutentry_t*)cookie;
    else
	{
	  if (rc = lookup_state(stateid, &state))
	    {
	      LogMajor(COMPONENT_STATES,
		       "state_inc_layout_state: could not find state.");
	      return rc;
	    }
	  header = state->header;
	  if (state->type != STATE_LAYOUT)
	    {
	      LogMajor(COMPONENT_STATES,
		       "state_inc_layout_state: supplied state of wrong type.");
	      return ERR_STATE_INVAL;
	    }
	  layoutentry = state->state.layout.layoutentries;
	  if (!layoutentry)
	    return ERR_STATE_NOENT;
	}
    
    segment->type = layoutentry->type;
    segment->iomode = layoutentry->iomode;
    segment->offset = layoutentry->offset;
    segment->length = layoutentry->length;
    segment->return_on_close = layoutentry->return_on_close;
    segment->layoutdata = layoutentry->layoutdata;
    segment->segid = (uint64_t) layoutentry;
    *cookie = (uint64_t) (layoutentry->next);
    
    if (*cookie == 0)
      *finished = true;
    
    return ERR_STATE_NO_ERROR;
}

