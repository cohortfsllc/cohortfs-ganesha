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
 * General state functions
 ***********************************************************************/

int localstate_lock_filehandle(fsal_handle_t *handle, statelocktype rw)
{
    entryheader* header;
    
    /* Retrieve or create header for per-filehandle chain */
  
    if (statelocktype)
	{
	    if (!(header = header_for_write(handle)))
		{
		    LogMajor(COMPONENT_STATES,
			     "state_lock_filehandle: could not find/create header entry.");
		    return ERR_STATE_FAIL;
		}
	}
    else
	{
	    if (!(header = header_for_read(handle)))
		{
		    LogMajor(COMPONENT_STATES,
			     "state_lock_filehandle: could not find/create header entry.");
		    return ERR_STATE_FAIL;
		}
	}

    return ERR_STATE_NO_ERROR;
}

/*
 * Unlocks the filehandle.
 */

int localstate_unlock_filehandle(fsal_handle_t *handle)
{
    hash_buffer_t key, val;
    entryheader* header;
    int rc = 0;

    key.pdata = handle;
    key.len = sizeof(fsal_handle_t);

    rc = HashTable_get(entrytable, &key, &val);

    if (rc == HASHTABLE_SUCCESS)
	{
	    header = val.pdata;
	    rc = pthread_rwlock_unlock(&(header->lock));
	    if (rc != 0 || !(header->valid))
		return ERR_STATE_FAIL;
	    else
		return ERR_STATE_SUCCESS;
	}
    else if (rc == HASHTABLE_ERROR_NO_SUCH_KEY)
	return ERR_STATE_NOENT;
    else
	return ERR_STATE_FAIL;
}

int localstate_iterate_by_filehandle(fsal_handle_t *handle, statetype type,
				     uint64_t* cookie, boolean* finished,
				     taggedstate* outstate)
{
    entryheader* header;
    state* cur = NULL;
    state* next = NULL;

    *finished = false;

    if (!(header = header_for_read(handle)))
	{
	    LogMajor(COMPONENT_STATES,
		     "state_iterate_by_filehandle: could not find/create header entry.");
	    return ERR_STATE_FAIL;
	}

    if (*cookie)
	*cur = (state*) cookie;
    else
	*cur = header->states;

    while (type != any && cur->type != type && cur != NULL)
	cur = cur->nextfh;

    if (cur == NULL)
	{
	    pthread_rwlock_unlock(&(header->lock));
	    return ERR_STATE_NOENT;
	}

    next = cur->nextfh;

    while (type != any && next->type != type && next != NULL)
	next = next->nextfh;

    *cookie = (uint64_t)next;

    if (!(*cookie))
	*finished = true;

    filltaggedstate(state, outstate);

    pthread_rwlock_unlock(&(header->lock));

    return(ERR_STATE_NO_ERROR);
}


int localstate_iterate_by_clientid(clientid4 clientid, statetype type,
				   uint64_t* cookie, boolean* finished,
				   state* outstate)
{
    state* cur = NULL;
    state* next = NULL;

    *finished = false;

    if (*cookie)
	*cur = (state*) cookie;
    else
	*cur = statechain;

    while ((cur->clientid != clientid) &&
	   (type != any) &&
	   (cur->type != type) &&
	   (cur != NULL))
	cur = cur->next;

    if (cur == NULL)
	return ERR_STATE_NOENT;

    next = cur->next;

    while ((cur->clientid != clientid) &&
	   (type != any) &&
	   (cur->type != type) &&
	   (cur != NULL))
	next = next->next;
	
    if (!(*cookie))
	*finished = true;

    filltaggedstate(state, outstate);

    return(ERR_STATE_NO_ERROR);
}

int localstate_retrieve_state(stateid4 stateid, taggedstate* outstate)
{
    state* state = NULL;
    int rc = 0;

    rc = lookup_state(stateid, &state);
    if (rc != STATE_ERR_NO_ERROR)
	return rc;

    filltaggedstate(state, taggedstate);

    return ERR_STATE_NO_ERROR;
}
