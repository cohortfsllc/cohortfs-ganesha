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
#include <alloca.h>
#include "sal_internal.h"

/************************************************************************
 * The Head of the Chain
 *
 * This chain exists entirely to facilitate iterating over all states.
 ***********************************************************************/

state_t* statechain = NULL;

/************************************************************************
 * Mutexes 
 *
 * So far, only one.  This mutex is used only for adding or deleting a
 * an entry from the entry hash, to prevent a possible race
 * condition.  it would only be used for the creation of the first
 * state on a file or the deletion of the last state.
 ***********************************************************************/

pthread_mutex_t entrymutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ownermutex = PTHREAD_MUTEX_INITIALIZER;

/************************************************************************
 * Global pools 
 *
 * A few pools for local data structures.
 ***********************************************************************/

entryheader_t* entryheaderpool;
#ifdef _USE_FSALMDS
locallayoutentry_t* layoutentrypool;
#endif
state_t* statepool;
state_owner_t* ownerpool;

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
hash_table_t* openownertable;
hash_table_t* lockownertable;

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
    .hash_func_key=state_id_value_hash_func,
    .hash_func_rbt=state_id_rbt_hash_func,
    .compare_key=compare_state_id,
    .key_to_str=display_state_id_key,
    .val_to_str=display_state_id_val
};

hash_table_t* init_stateidtable(void)
{
    stateidtable = HashTable_Init(stateidparams);
    return stateidtable;
}

int localsalstringnoop(hash_buffer_t * pbuff, char *str)
{
  return 0;
}

unsigned long handle_hash_func(hash_parameter_t * p_hparam,
			       hash_buffer_t * buffclef)
{
  return (FSAL_Handle_to_HashIndex((fsal_handle_t*) buffclef->pdata, 0,
				   p_hparam->alphabet_length,
				   p_hparam->index_size));
}

unsigned long handle_rbt_func(hash_parameter_t * p_hparam,
			      hash_buffer_t * buffclef)
{
  return (FSAL_Handle_to_RBTIndex((fsal_handle_t*) buffclef->pdata, 0));
}

int handle_compare_key_fsal(hash_buffer_t * buff1, hash_buffer_t * buff2)
{
    fsal_status_t status;

    return (FSAL_handlecmp((fsal_handle_t*)buff1->pdata,
			   (fsal_handle_t*)buff2->pdata,
			   &status));
}

hash_parameter_t entryparams = {
    .index_size=29,
    .alphabet_length=10,
    .nb_node_prealloc=1000,
    .hash_func_key=handle_hash_func,
    .hash_func_rbt=handle_rbt_func,
    .compare_key=handle_compare_key_fsal,
    .key_to_str=localsalstringnoop,
    .val_to_str=localsalstringnoop
};

hash_table_t* init_entrytable(void)
{
    entrytable = HashTable_Init(entryparams);
    return entrytable;
}

unsigned long simple_hash_func(hash_parameter_t * p_hparam,
			       hash_buffer_t * buffclef);
unsigned long rbt_hash_func(hash_parameter_t * p_hparam,
			   hash_buffer_t * buffclef);

int owner_cmp_func(hash_buffer_t* key1,
		   hash_buffer_t* key2)
{
    if (key1->len == key2->len)
	return memcmp(key1->pdata, key2->pdata, key1->len);
    else
	return -1;
}

hash_parameter_t ownerparams = {
    .index_size=29,
    .alphabet_length=10,
    .nb_node_prealloc=1000,
    .hash_func_key=simple_hash_func,
    .hash_func_rbt=rbt_hash_func,
    .compare_key=owner_cmp_func,
    .key_to_str=localsalstringnoop,
    .val_to_str=localsalstringnoop
};

hash_table_t* init_openownertable(void)
{
    openownertable= HashTable_Init(ownerparams);
    return openownertable;
}

hash_table_t* init_lockownertable(void)
{
    lockownertable = HashTable_Init(ownerparams);
    return lockownertable;
}

entryheader_t* lookupheader(fsal_handle_t* handle)
{
    hash_buffer_t key, val;
    entryheader_t* header;
    int rc = 0;

    key.pdata = (caddr_t)handle;
    key.len = sizeof(fsal_handle_t);

    rc = HashTable_Get(entrytable, &key, &val);

    if (rc == HASHTABLE_SUCCESS)
	{
	    header = (entryheader_t*)val.pdata;
	    return header;
	}
    return NULL;
}

/* Create a stateid.other.  The clientid and time stuff should make it
 * unlike to collide.
 */

void newstateidother(clientid4 clientid, char* other)
{
    uint64_t* first64 = (uint64_t*) other;
    uint32_t* first32 = (uint32_t*) other;
    uint64_t* second64 = (uint64_t*) (other + 4);
    struct timeval tv;
    struct timezone tz;

    memset(other, 0, 12);

    *first64 = clientid;

    gettimeofday(&tv, &tz);

    *first32 = *first32 ^ tv.tv_usec;
    *second64 = *second64 ^ tv.tv_sec;
}

/* Allocate a new state with stateid */

state_t* newstate(clientid4 clientid, entryheader_t* header)
{
    hash_buffer_t key, val;
    state_t* state;
    int counter = 0;
    int rc = 0;

    GET_PREALLOC(state, statepool, 1, state_t, next_alloc);
		
    if (!state)
	return NULL;
    
    key.len = 12;
    val.pdata = (caddr_t)state;
    val.len = sizeof(state);
    state->type = STATE_ANY; /* Mark invalid, until filled in */

    do
	{
	    newstateidother(clientid, state->stateid.other);
	    key.pdata = (caddr_t) state->stateid.other;
	    rc = HashTable_Test_And_Set(stateidtable, &key, &val,
					HASHTABLE_SET_HOW_SET_NO_OVERWRITE);
	}
    while ((rc == HASHTABLE_ERROR_KEY_ALREADY_EXISTS) && ++counter <= 100);

    state->header = header;
    state->clientid = clientid;
    state->stateid.seqid = 1;

    if (rc != HASHTABLE_SUCCESS)
	{
	    LogCrit(COMPONENT_STATES,
		    "Unable to create new stateid.  This should not happen.");
	    
	    RELEASE_PREALLOC(state, statepool, next_alloc);
	    state=NULL;
	    ERR_STATE_FAIL;
	}

    return state;
}

/* Removes a state from relevant hash tables and deallocates resources
 * associated with it.  Have a write-lock on the entry_header.
 */

state_t* iterate_entry(entryheader_t* entry, state_t** state)
{
    if (*state == NULL)
	*state = entry->states;
    else
	*state = (*state)->next;
    return *state;
}

int lookup_state(stateid4 stateid, state_t** state)
{
    int rc = 0;
    hash_buffer_t key, val;
    
    key.pdata = stateid.other;
    key.len = 12;
    
    rc = HashTable_Get(stateidtable, &key, &val);
    
    if (rc == HASHTABLE_ERROR_NO_SUCH_KEY)
	return ERR_STATE_NOENT;
    else if (rc != HASHTABLE_SUCCESS)
	return ERR_STATE_FAIL;

    *state = (state_t*)val.pdata;

    /* TODO Update this to handle wraparound once we figure out how to
       quickly count the number of slots total associated with a
       client */

    if (stateid.seqid == 0)
	return ERR_STATE_NO_ERROR;
    else if (stateid.seqid < (*state)->stateid.seqid)
	return ERR_STATE_OLDSEQ;
    else if (stateid.seqid > (*state)->stateid.seqid)
	return ERR_STATE_BADSEQ;
    else
      return ERR_STATE_NO_ERROR;
}

void unchain(state_t* state)
{
    if (state->prevfh == NULL)
	{
	    state->header->states = state->nextfh;
	    if (state->nextfh)
		state->nextfh->prevfh = NULL;
	}
    else
	{
	    if (state->nextfh)
		state->nextfh->prevfh = state->prevfh;
	    state->prevfh->nextfh = state->nextfh;
	}
    if (state->prev == NULL)
	{
	    statechain = state->next;
	    if (state->next)
		state->next->prev = NULL;
	}
    else
	{
	    if (state->next)
		state->next->prev = state->prev;
	    state->prev->next = state->next;
	}
}

void killstate(state_t* state)
{
    entryheader_t* header = state->header;
    hash_buffer_t key;

    unchain(state);

    key.pdata = state->stateid.other;
    key.len = 12;
    
    if (HashTable_Del(stateidtable, &key, NULL, NULL) !=
	HASHTABLE_SUCCESS)
	LogMajor(COMPONENT_STATES,
		 "killstate: unable to remove stateid from hash table.");
	
    RELEASE_PREALLOC(state, statepool, next_alloc);
}

void filltaggedstate(state_t* state, taggedstate* outstate)
{
    memset(outstate, 0, sizeof(taggedstate));
    switch (state->type)
	{
	case STATE_SHARE:
	    fillsharestate(state, &(outstate->u.share), state->header);
	    break;
	case STATE_DELEGATION:
	    filldelegationstate(state, &(outstate->u.delegation),
				state->header);
	    break;
	case STATE_DIR_DELEGATION:
	    filldir_delegationstate(state,
				    &(outstate->u.dir_delegation), state->header);
	    break;
	case STATE_LOCK:
	    filllockstate(state, &(outstate->u.lock), state->header);
	    break;
#ifdef _USE_FSALMDS
	case STATE_LAYOUT:
	    filllayoutstate(state, &(outstate->u.layout),
			    state->header);
	    break;
#endif
	default:
	    LogCrit(COMPONENT_STATES,
		    "filltaggedstate: invalid state (can't happen)!");
	}
}

state_owner_t* acquire_owner(char* name, size_t len,
			     clientid4 clientid, bool_t lock,
			     bool_t wantmutex, bool_t* created)
{
    hash_buffer_t key, val;
    owner_key_t okey;
    state_owner_t* owner = NULL;
    int rc;
    hash_table_t* table = lock ? lockownertable : openownertable;

    if (created)
	*created = 0;

    memset(&okey, 0, sizeof(owner_key_t));
    memcpy(okey.owner_val, name, len);
    okey.owner_len = len;
    okey.clientid = clientid;
    key.pdata = (caddr_t)&okey;
    key.len = sizeof(owner_key_t);

    rc = HashTable_Get(table, &key, &val);

    if (rc == HASHTABLE_SUCCESS)
	{
	    owner = (state_owner_t*)val.pdata;
	    if (wantmutex)
		{
		    rc = pthread_mutex_lock(&(owner->mutex));
		    if (rc < 0)
			return NULL;
		}
	    else
		return owner;
	}
    else if (rc == HASHTABLE_ERROR_NO_SUCH_KEY)
	{
	    if (pthread_mutex_lock(&ownermutex) != 0)
		return NULL;

	    /* Make sure it didn't get created while we were waiting */

	    rc = HashTable_Get(table, &key, &val);
	    
	    if (rc == HASHTABLE_SUCCESS)
		{
		    owner = (state_owner_t*) val.pdata;
		    if (wantmutex)
			{
			    rc = pthread_mutex_lock(&(owner->mutex));
			    if (rc < 0)
				return NULL;
			}
		    pthread_mutex_unlock(&ownermutex);
		    return owner;
		}

	    if (rc != HASHTABLE_ERROR_NO_SUCH_KEY)
		{
		    /* Really should be impossible */
		    pthread_mutex_unlock(&entrymutex);
		    return NULL;
		}
	    
	    /* We may safely create the entry */
	    
	    GET_PREALLOC(owner, ownerpool, 1, state_owner_t,
			 next_alloc);

	    if (!owner)
		{
		    pthread_mutex_unlock(&ownermutex);
		    return NULL;
		}

	    owner->key = okey;
	    key.pdata = (caddr_t)&(owner->key);

	    owner->seqid = 0;
	    owner->refcount = 0;
	    owner->lock = lock;
	    owner->last_response = NULL;
	    pthread_mutex_init(&(owner->mutex), NULL);
	    owner->related_owner = NULL;

	    val.pdata = (caddr_t)owner;
	    val.len = sizeof(state_owner_t);
	    if (wantmutex)
		{
		    rc = pthread_mutex_lock(&(owner->mutex));
		    pthread_mutex_unlock(&ownermutex);
		    if (rc < 0)
			{
			    RELEASE_PREALLOC(owner, ownerpool, next_alloc);
			    return NULL;
			}
		}
	    
	    rc = HashTable_Test_And_Set(table, &key, &val,
					HASHTABLE_SET_HOW_SET_NO_OVERWRITE);
	    pthread_mutex_unlock(&ownermutex);
	    if (rc == HASHTABLE_SUCCESS)
		{
		    if (created)
			*created = true;
		    return owner;
		}
	    else
		{
		    pthread_mutex_unlock(&(owner->mutex));
		    RELEASE_PREALLOC(owner, ownerpool, next_alloc);
		}
	}
    return NULL ;
}

int killowner(state_owner_t* owner)
{
    hash_buffer_t key;
    hash_table_t* table = owner->lock ? lockownertable : openownertable;
    
    key.pdata = (caddr_t) &(owner->key);
    key.len = sizeof(owner_key_t);

    pthread_mutex_lock(&ownermutex);
    if (HashTable_Del(table, &key, NULL, NULL) !=
	HASHTABLE_SUCCESS)
	LogMajor(COMPONENT_STATES,
		 "killowner: unable to remove owner from hash table.");

    if (owner->last_response)
	Mem_Free(owner->last_response);

    pthread_mutex_unlock(&(owner->mutex));

    RELEASE_PREALLOC(owner, ownerpool, next_alloc);

    pthread_mutex_unlock(&ownermutex);
    return 0;
}
