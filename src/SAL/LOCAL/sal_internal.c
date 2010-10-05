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
#include "fsal.h"
#include "log_macros.h"
#include <sys/time.h>

/************************************************************************
 * The Head of the Chain
 *
 * This chain exists entirely to facilitate iterating over all states.
 ***********************************************************************/

loclastate* statechain;

/************************************************************************
 * Mutexes 
 *
 * So far, only one.  This mutex is used only for adding or deleting a
 * an entry from the entry hash, to prevent a possible race
 * condition.  it would only be used for the creation of the first
 * state on a file or the deletion of the last state.
 ***********************************************************************/

pthread_mutex_t entrymutex = PTHREAD_MUTEX_INITIALIZER;

/************************************************************************
 * Global pools 
 *
 * A few pools for local data structures.
 ***********************************************************************/

locallayoutentry* layoutentrypool;
state* statepool;

/************************************************************************
 * Hash Tables 
 *
 * I am not entirely happy with this data structure, but it will get
 * the job done for now.  Currently we have three hash tables: cache
 * entry, stateid, and entry/clientid concatenation.  The first two
 * link to data structures for quick lookups, the latter links
 * directly to to a state entry.  Entries are linked for iteration.
 *
 * This is not my favourite design in the universe, but it's fast to
 * put together and fits the interface (which is, I think, what I
 * would want to use when writing other programs.)  I fully expect to
 * replace the data structures.
 *
 * My intitial design used the filehandle rather than the cache entry,
 * however everything BUT the FSAL knows about the cache entry
 * structures, and filehandle alone does not support the use of
 * multipel filesystems.  (The fsdata structure supports having
 * multiple exported filesystems without worrying about filehandle
 * collisions.)  FSAL integration can be handled with a callback,
 * without the FSAL having to have knowledge of multiple exorts.
 *
 * The general rationale behind the structure is that we definitely
 * need to be able to look up stateids quickly.  We also have to look
 * up filehandles quickly to check for conflicting states, etc.  And
 * the filehandle/clientid pair is useful for checking for
 * pre-existing states.
 ***********************************************************************/

hash_table_t* stateidtable;
hash_table_t* entrytable;

/* State ID table */

/* All this stuff is taken from Philippe's nfs_state_id.c */

int display_state_id_key(hash_buffer_t * pbuff, char *str)
{
    unsigned int i = 0;
    unsigned int len = 0;
    
    for(i = 0; i < 12; i++)
	len += sprintf(&(str[i * 2]), "%02x", (unsigned char)pbuff->pdata[i]);
    return len;
}                               /* display_state_id_val */

int display_state_id_val(hash_buffer_t * pbuff, char *str)
{
    /* Fix all this stuff later */
    
/*
    cache_inode_state_t *pstate = (cache_inode_state_t *) (pbuff->pdata);

    return sprintf(str,
		   "state %p is associated with pentry=%p type=%u seqid=%u prev=%p next=%p",
		   pstate, pstate->pentry, pstate->state_type, pstate->seqid, pstate->prev,
		   pstate->next);
*/
    return 0;
}                               /* display_state_id_val */

int compare_state_id(hash_buffer_t * buff1, hash_buffer_t * buff2)
{
    return memcmp(buff1->pdata, buff2->pdata, 12);        /* The value 12 is fixed by RFC3530 */
}                               /* compare_state_id */

unsigned long state_id_value_hash_func(hash_parameter_t * p_hparam,
                                       hash_buffer_t * buffclef)
{
    unsigned int sum = 0;
    unsigned int i = 0;
    unsigned char c;

    /* Compute the sum of all the characters */
    for(i = 0; i < 12; i++)
	{
	    c = ((char *)buffclef->pdata)[i];
	    sum += c;
	}

    LogFullDebug(COMPONENT_STATES, "---> state_id_value_hash_func=%lu",
	       (unsigned long)(sum % p_hparam->index_size));

    return (unsigned long)(sum % p_hparam->index_size);
}                               /*  client_id_reverse_value_hash_func */

unsigned long state_id_rbt_hash_func(hash_parameter_t * p_hparam,
                                     hash_buffer_t * buffclef)
{
    u_int32_t i1 = 0;
    u_int32_t i2 = 0;
    u_int32_t i3 = 0;
    
    if(isFullDebug(COMPONENT_STATES))
	{
	    char str[25];
	    
	    sprint_mem(str, (char *)buffclef->pdata, 12);
	    LogFullDebug(COMPONENT_SESSIONS, "         ----- state_id_rbt_hash_func : %s", str);
	}
    
    memcpy(&i1, &(buffclef->pdata[0]), sizeof(u_int32_t));
    memcpy(&i2, &(buffclef->pdata[4]), sizeof(u_int32_t));
    memcpy(&i3, &(buffclef->pdata[8]), sizeof(u_int32_t));
    
    LogFullDebug(COMPONENT_STATES, "--->  state_id_rbt_hash_func=%lu", (unsigned long)(i1 ^ i2 ^ i3));
    
    return (unsigned long)(i1 ^ i2 ^ i3);
}                               /* state_id_rbt_hash_func */

hash_parameter_t stateidparams = {
    .index_size=17,
    .alphabet_length=10,
    .nb_node_prealloc=10,
    .hash_func_key=state_id_hash_func,
    .hash_func_rbt=state_id_rbt_func,
    .compare_key=compare_state_id,
    .key_to_str=display_state_id_key,
    .val_to_str=display_state_id_val
};

int init_stateidtable(void)
{
    stateidtable = HashTable_Init(stateidparams);
    return stateidtable;
}

int dummy2str(hash_buffer_t * pbuff, char *str)
{
    str="DUMMY";
    return 0;
}

unsigned long state_id_value_hash_func(hash_parameter_t * p_hparam,
                                       hash_buffer_t * buffclef)
{
    return (FSAL_Handle_to_HashIndex(buffclef->pdata, 0,
				     p_hparam->alphabet_length,
				     p_hparam->index_size));
}

unsigned long state_id_rbt_hash_func(hash_parameter_t * p_hparam,
                                     hash_buffer_t * buffclef)
{
    return (FSAL_Handle_to_RBTIndex(buffclef->pdata, 0));
}

int compare_state_id(hash_buffer_t * buff1, hash_buffer_t * buff2)
{
    fsal_status_t status;

    return (FSAL_handlecmp(buff1->pdata, buff2->pdata, &status));
}

hash_parameter_t entryparams = {
    .index_size=29,
    .alphabet_length=10,
    .nb_node_prealloc=1000,
    .hash_func_key=handle_hash_func,
    .hash_func_rbt=handle_rbt_func,
    .compare_key=handle_compare_key_fsal,
    .key_to_str=dummy2str,
    .val_to_str=dummy2str
};

int init_entrytable(void)
{
    entrytable = HashTable_Init(entryparams);
    return entrytable;
}

/*
 * Fetches an entry header and write locks it.  If the header does not
 * exist, creates it in the table.
 */

int header_for_write(fsal_handle_t* handle)
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
	    rc = pthread_rwlock_wrlock(&(header->lock));
	    if (rc != 0 || !(header->valid))
		return NULL;
	    else
		return header;
	}
    else if (rc == HASHTABLE_ERROR_NO_SUCH_KEY)
	{
	    if (!pthread_mutex_lock(&entrymutex))
		return NULL;

	    /* Make sure no one created the entry while we were
	       waiting for the mutex */

	    rc = HashTable_get(entrytable, &key, &val);
	    
	    if (rc == HASHTABLE_SUCCESS)
		{
		    rc = pthread_rwlock_wrlock(&(header->lock));
		    pthread_mutex_unlock(&entrymutex);
		    header = val.pdata;
		    if (rc != 0 || !(header->valid))
			return NULL;
		    else
			return header;
		}
	    if (rc != HASHTABLE_ERROR_NO_SUCH_KEY)
		{
		    /* Really should be impossible */
		    pthread_mutex_unlock(&entrymutex);
		    return NULL;
		}

	    /* We may safely create the entry */
	    
	    GET_PREALLOC(header, entryheaderpool, 1, entryheader,
			 next_alloc);

	    if (!header)
		{
		    pthread_mutex_unlock(&entrymutex);
		    return NULL;
		}

	    memset(header, 0, sizeof(entryheader));
	    
	    /* Copy, since it looks like the HashTable code depends on
	       keys not going away */
	    
	    header.handle = *handle;
	    key.pdata = &(newheader.handle);

	    header->lock = PTHREAD_RWLOCK_INITIALIZER;

	    pthread_rwlock_wrlock(&(header->lock));

	    header->valid = 1;
	    val.pdata = header;
	    val.len = sizeof(entryheader);
	    
	    rc = Hash_Table_Test_and_Set(entrytable, &key, &val,
					 HASHTABLE_SET_HOW_SET_NO_OVERWRITE);

	    pthread_mutex_unlock(&entrymutex);
	    if (rc == HASHTABLE_SUCCESS)
		return header;
	    else
		{
		    pthread_rwlock_unlock(&(header->lock));
		    header->valid = 0;
		    RELEASE_PREALLOC(newentry, entrypool, next_alloc);
		}
	}
    return NULL ;
}

int header_for_read(fsal_handle_t* handle)
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
	    rc = pthread_rwlock_wrlock(&(header->lock));
	    if (rc != 0 || !(header->valid))
		return NULL;
	    else
		return header;
	}
    return NULL;
}

/* Create a stateid.other.  The clientid and time stuff should make it
 * unlike to collide.
 */

void newstateidother(clientid4 clientid, char* other)
{
    uint64_t* first64 = (uint64_t) other;
    uint32_t* first32 = (uint32_t*) other;
    uint32_t* second64 = (uint64_t*) (other + 4);
    struct timeval_t tv;
    struct timezone tz;

    memset(other, 0, 12);

    assert(((void*) second32) - (void*) first64 == sizeof(uint32_t));

    *first64 = clientid4;

    gettimeofday(&tv, &tz);

    *first32 = first32 ^ tv.tv_usec;
    *second64 = second64 ^ tv.tv_sec;
}

/* Allocate a new state with stateid */

localstate* newstate(clientid4 clientid, entryheader* header)
{
    hash_buffer_t key, val;
    state* state;
    int counter = 0;
    int rc = 0;

    if (!(newstate = (GET_PREALLOC(state, statepool, 1,
				   state, next_alloc))))
	return NULL;

    memset(newstate, 0, sizeof(localstate));
    key.len = 12;
    val.pdata = newstate;
    val.len = sizeof(localstate);

    do
	{
	    newstateidother(clientid, newstate->stateid.other);
	    key.pdata = newstate->stateid.other;
	    rc = Hash_Table_Test_and_Set(concattable, &key, &val,
					 HASHTABLE_SET_HOW_SET_NO_OVERWRITE);
	}
    while ((rc != HASHTABLE_ERROR_KEY_ALREADY_EXISTS) && rc <= 100);

    state.clientid = clientid;
    state.stateid.seqid = 1;

    if (rc != HASHTABLE_SUCCESS)
	{
	    LogCrit(COMPONENT_STATES,
		    "Unable to create new stateid.  This should not happen.");
	    
	    RELEASE_PREALLOC(newstate, localstatepool, next_alloc);
	    return NULL;
	}

    chain(state, header);

    return state;
}

/* Chain a state onto the filehandle and entire linked lists.  This
 * should probably lock the main one.
 */

void chain(localstate* state, entryheader* header)
{
    state->header = header;

    state->prevfh = NULL;
    if (header->states == NULL)
	{
	    header->states = state;
	    state->nextfh = NULL;
	}
    else
	{
	    state->nextfh = header->states;
	    header->states = state;
	}

    state->prev = NULL;
    if (statechain == NULL)
	{
	    statechain = state;
	    state->next = NULL;
	}
    else
	{
	    state->next = statechin;
	    statechain = state;
	}
}

state* iterate_entry(entryheader* entry, state** state)
{
    if (*state == NULL)
	*state = entry->states;
    else
	*state = state->next;
    return *state;
}

int lookup_state_and_lock(stateid4 stateid, state** state,
			  entryheader** header, boolean write)
{
    int rc = 0;
    
    rc = lookup_state(stateid, state);
    if (rc != ERR_STATE_NO_ERROR)
	return rc;
	
    rc = 0;
    if (write)
	rc = pthread_rwlock_wrlock(&(state->header->lock));
    else
	rc = pthread_rwlock_rdlock(&(state->header->lock));

    *header = (*state)->header;

    if (rd || !(state->header->valid))
	return ERR_STATE_NOENT;

    return ERR_STATE_NO_ERROR;
}

int lookup_state(stateid4 stateid, state** state)
{
    int rc = 0;
    hash_buffer_t key, val;
    
    key.pdata = stateid.other;
    key.len = 12;
    
    rc = HashTable_get(statetable, &key, &val);
    
    if (rc == HASHTABLE_ERROR_NO_SUCH_KEY)
	return ERR_STATE_NOENT;
    else if (rc != HASHTABLE_SUCCESS)
	return ERR_STATE_FAIL;

    *state = val.pdata;

    /* TODO Update this to handle wraparound once we figure out how to
       quickly count the number of slots total associated with a
       client */

    if (stateid.seqid == 0)
	return ERR_STATE_NO_ERROR;
    else if (stateid.seqid < state->stateid.seqid)
	return ERR_STATE_OLDSEQ;
    else if (stateid.seqid > state->stateid.seqid)
	return ERR_STATE_BADSEQ;
    else
	return ERR_STATE_NO_ERROR;
}

void unchain(state* state)
{
    if (state->prevfh == NULL)
	{
	    state->header->states = state->nextfh;
	    state->nextfh->prevfh = NULL;
	}
    else
	{
	    state->nextfh->prevfh = state->prevfh;
	    state->prevfh->nextfh = state->nextfh;
	}
    if (state->prev == NULL)
	{
	    statechain = state->next;
	    state->next->prev = NULL;
	}
    else
	{
	    state->next->prev = state->prev;
	    state->prev->next = state->next;
	}
}

void killstate(state* state)
{
    entryheader* header = state->header;
    hash_buffer_t key;

    unchain(state);

    key.pdata = state->stateid.other;
    key.len = 12;
    
    if (HashTable_Del(statetable, &key, NULL, NULL) !=
	HASHTABLE_SUCCESS)
	LogMajor(COMPONENT_STATES,
		 "killstate: unable to remove stateid from hash table.");
	
    RELEASE_PREALLOC(state, statepool, next_alloc);

    if (!(header->states))
	{
	    header->valid = 0;
	    key.pdata = &(header->handle);
	    key.len = sizeof(fsal_handle_t);
	    if (HashTable_Del(entrytable, &key, NULL, NULL) !=
		HASHTABLE_SUCCESS)
		LogMajor(COMPONENT_STATES,
			 "killstate: unable to remove header from hash table.");
	    pthread_rwlock_unlock(&(header->lock));
	    RELEASE_PREALLOC(header, entrypool, next_alloc);
	}
    else
	pthread_rwlock_unlock(&(header->lock));
}
