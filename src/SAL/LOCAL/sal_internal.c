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

/************************************************************************
 * The Head of the Chain
 *
 * This chain exists entirely to facilitate iterating over all states.
 ***********************************************************************/

loclastate* statechain = NULL;

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
<<<<<<< HEAD
hash_table_t* concattable;
hash_table_t* ownertable;
=======
>>>>>>> 10d1be59b652f69fd7de00fe6f675f34d9ed69e9

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

<<<<<<< HEAD
int init_concattable(void)
{
    concattable = HashTable_Init(concatparams);
    return concattable;
}

unsigned long simple_hash_func(hash_parameter_t * p_hparam,
			       hash_buffer_t * buffclef);
unsigned int rbt_hash_func(hash_parameter_t * p_hparam,
			   hash_buffer_t * buffclef);
int simple_compare_func(hash_buffer_t *key1,
			hash_buffer_t *key2)
{
  return (key1.len != key2.len) ||
    memcmp(key1.pdata, key2.pdata, key1.len);
}

hash_parameter_t ownerparams = {
    .index_size=19,
    .alphabet_length=10,
    .nb_node_prealloc=100,
    .hash_func_key=simple_hash_func,
    .hash_func_rbt=simple_rbt_func,
    .compare_key=simple_compare_func,
    .key_to_str=dummy2str,
    .val_to_str=dummy2str
};

int init_ownertable(void)
{
  ownertable = HashTable_Init(ownerparams);
}

/*
 * Returns 0 if the given cache entry refers to a file, nonzero
 * otherwise
 */

int entryisfile(cache_entry_t* entry)
=======
int init_entrytable(void)
>>>>>>> 10d1be59b652f69fd7de00fe6f675f34d9ed69e9
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
<<<<<<< HEAD
	    if (create)
		{
		    GET_PREALLOC(concat, concatpool, 1, concatstates,
				 next_alloc);
		    memset(concat, 0, sizeof(concatstates));
		    /* Copy the key off the stack */
		    concat.key = keyval;
		    key.pdata = &(concaststates.key);
		    concat->header = header;
		    rc = Hash_Table_Test_and_Set(concattable, &key, &val,
						 HASHTABLE_SET_HOW_SET_NO_OVERWRITE);
		    
		    if (rc != HASHTABLE_SUCCESS)
			{
			    RELEASE_PREALLOC(concat, concatpool, next_alloc);
			    concat = NULL;
			}
		}
=======
	    header = val.pdata;
	    rc = pthread_rwlock_wrlock(&(header->lock));
	    if (rc != 0 || !(header->valid))
		return NULL;
	    else
		return header;
>>>>>>> 10d1be59b652f69fd7de00fe6f675f34d9ed69e9
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

<<<<<<< HEAD
int newclientstate(clientid4 clientid, state** newstate)
{
    hash_buffer_t key, val;
    int counter = 0;
    int rc = 0;

    if (!(*newstate = (GET_PREALLOC(newstate, statepool, 1,
				    state, next_alloc))))
	return ERR_STATE_FAIL;
=======
localstate* newstate(clientid4 clientid, entryheader* header)
{
    hash_buffer_t key, val;
    state* state;
    int counter = 0;
    int rc = 0;

    if (!(newstate = (GET_PREALLOC(state, statepool, 1,
				   state, next_alloc))))
	return NULL;
>>>>>>> 10d1be59b652f69fd7de00fe6f675f34d9ed69e9

    memset(*newstate, 0, sizeof(state));
    key.len = 12;
    val.pdata = *newstate;
    val.len = sizeof(state);
    state.type = any; /* Mark invalid, until filled in */

    do
	{
	    newstateidother(clientid, (*newstate)->stateid.other);
	    key.pdata = (*newstate)->stateid.other;
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
	    
	    RELEASE_PREALLOC(newstate, statepool, next_alloc);
	    *newstate=NULL;
	    ERR_STATE_FAIL;
	}

    return ERR_STATE_NO_ERROR;
}

/* This function is in direct violation of RFC 5661, section 9.5.
 * Rather than the prescribed behaviour for different open_owners
 * sharing the same lock_owner.  This implementation treats all
 * lock_owners belonging to different open_owners as living in
 * completely separate spaces.  RFC5661, section 9.5 is not
 * particularly wonderful, anyway, and in POSIX you can't even do
 * that.
 */

int newownedstate(clientid4 clientid, open_owner4* open_owner,
		  lock_owner4* lock_owner, state** newstate)
{
    int rc = 0;
    char* keybuff;
    hash_buffer_t key, val;
    size_t keylen
	= (open_owner->owner.owner_len +
	   (lock_owner ? lock_owner->owner.owner_len : 0) +
	   sizeof(clientid4) + 1);
    
    rc = newclientstate(clientid4, &newstate);
    if (rc != 0)
	return rc;

    keybuff = Mem_Alloc(keylen);
    if (!keybuff)
	{
	    killstate(newstate);
	    LogCrit(COMPONENT_STATES,
		    "Unable to allocate memory for owner key.");
	    return ERR_STATE_FAIL;
	}

    (*newstate)->assoc.owned.chunk = keybuff;
    *keybuff = lock_owner ? 1 : 0;
    *((clientid4*) (keybuff + 1)) = clientid;
    (*newstate)->assoc.owned.open_owner = (keybuff + 1 +
					   sizeof(clientid));
    memcpy((*newstate)->assoc.owned.open_owner,
	   open_owner.owner.owner_val,
	   open_owner.owner.owner_len);
    (*newstate)->assoc.owned.oolen = open_owner.owner.owner_len;
    if (lock_owner)
	{
	    (*newstate)->assoc.owned.lock_owner =
		((*newstate)->assoc.owned.open_owner +
		 (*newstate)->assoc.owned.oolen);
	    memcpy((*newstate)->assoc.owned.lock_owner,
		   lock_owner.owner.owner_val,
		   lock_owner.owner.owner_len);
	    (*newstate)->assoc.owned.lolen =
		lock_owner.owner.owner_len;
	}
    else
	{
	    (*newstate)->assoc.owned.lock_owner = NULL;
	    (*newstate)->assoc.owned.lolen = 0;
	}

    key.pdata = keybuff;
    key.len = keylen;

    val.pdata = *newstate;
    val.len = sizeof(state);

<<<<<<< HEAD
    state->assoc.owned.keylen = keylen;
    
    rc = Hash_Table_Test_and_Set(concattable, &key, &val,
				 HASHTABLE_SET_HOW_SET_NO_OVERWRITE);

    if (rc == HASHTABLE_ERROR_KEY_ALREADY_EXISTS)
	{
	    killstate(newstate);
	    LogDebug(COMPONENT_STATES,
		     "Pre-existing lock/share for owner/lockid combination.");
	    return ERR_STATE_PREEXISTS;
	}

    else if (rc != HASHTABLE_SUCCESS)
	{
	    killstate(newstate);
	    return ERR_STATE_FAIL;
	}

    return ERR_STATE_NO_ERROR;
=======
    chain(state, header);

    return state;
>>>>>>> 10d1be59b652f69fd7de00fe6f675f34d9ed69e9
}

/* Removes a state from relevant hash tables and deallocates resources
 * associated with it.  Have a write-lock on the entry_header.
 */

int killstate(state* state)
{
<<<<<<< HEAD
    hash_buffer_t key;
    int rc;

    /* All this state specific stuff goes first so we don't end up
     * with a state half-deallocated but still alive after an error
     */

    switch (state->type)
	{
	    /* Uninitialised state, it just gets the stateid removal and
	     * deallocated in the catch-all */
	case any:
	    break;

	    /* Check for locks.  if any exist, return an error */
	case share:
	    if (state->assoc.owned.share.locks)
		return ERR_STATE_LOCKSHELD;
	    break;

	    /* Free all lock entries */
	case deleg:
	case dir_deleg:
	    break;
	case lock:
	    freelocks(state);
	    break;
	case layout:
	    freelayouts(state);
	    break
	}

    unchain(state)

    /* If owned, reove from owner hash and deallocate*/
    if ((state->type == share) ||
	(state->type == lock))
	{
	    key.pdata = state->assoc.owned.chunk;
	    key.len = state->assoc.owned.keylen;
	    rc = HashTable_Del(ownertable, key, NULL, NULL);
	    if ((rc != HASHTABLE_SUCCESS) &&
		(rc != HASHTABLE_NO_SUCH_KEY))
		    LogError(COMPONENT_STATES,
			     "Error deleting owner key.");
	    Mem_Free(state->chunk);
	}

    key.pdata = state->stateid.other;
    key.len = 12;
    HashTable_Del(ownertable, key, NULL, NULL);
    if ((rc != HASHTABLE_SUCCESS) &&
	(rc != HASHTABLE_NO_SUCH_KEY))
	LogError(COMPONENT_STATES,
		 "Error deleting stateid key.");

    RELEASE_PREALLOC(state, statepool, next_alloc);
    return ERR_STATE_NO_ERROR;
}

/* Chain a state into the various associations
 *
 * share is NULL except in the case of a lock state.
 */

int chain(state* state, entryheader* header, state* share)
{
    concatstate* concat = NULL,

    if (state->type == any)
	{
	    LogCrit(COMPONENT_STATES,
		     "chain: attempt to chain invalid state.");
	    return ERR_STATE_FAIL;
	}

    if ((state->type != lock) && share)
	{
	    LogCrit(COMPONENT_STATES,
		    "chains: associated share supplied to non-lock.");
	    return ERR_STATE_FAIL;
	}
    
    if ((state->type == delegation) ||
	(state->type == dir_delegation) ||
	(state->type == layout))
	{
	    if (!(*concat = get_concat(entry, clientid, true)))
		{
		    LogMajor(COMPONENT_STATES,
			     "chain: could not find/create file/clientid entry.");
		    return ERR_STATE_FAIL;
		}
	    state->assoc.client.concats = *concats;
	}
    
	    
    /* Make sure we aren't being bad */

    if (((state->type == delegation) &&
	 ((*concat)->deleg)) ||
	((state->type == dir_delegation) &&
	 ((*concat)->dir_deleg))
	((state->type == layout) &&
	 ((*concat)->layout)))
	return ERR_STATE_PREEXISTS;

    /* Link to filehandle chain */

    state->prevfh = NULL;
    state->nextfh = entryheader->states;
    entryheader->states = state;

    state->header = entryheader;

    /* Link to the main chain */

    state->prev = NULL;
    state->next = statechain;
    statechain = state;

    /* Link to concats */


    if (state->type == delegation)
	*concats->deleg = state;
    if (state->type == dir_delegation)
	*concats->dir_deleg = state;
    if (state->type == layout)
	*concats->layout = state;

    if (state->type == lock)
	{
	    state->assoc.owned.state.lock.share = share;
	    state->assoc.owned.state.lock.prev = NULL;
	    state->assoc.owned.state.lock.next
		= share->assoc.owned.state.share.locks;
	    share->assoc.owned.state.share.locks = share;
	}
    
    return ERR_FSAL_NO_ERROR;
}

/* Unchain a state, essentially do the last thing in reverse */

int unchain(state* state)
{
    /* Since chain refuses to chain undefined states, they're already
       uchained. */

    if (state->type == any)
	return ERR_STATE_NO_ERROR;

    /* Unlink from the main chain */

    if (state->prev == NULL)
	{
	    statechain = state->next;
	    state->next->prev = NULL;
	}
    else
	{
	    state->prev->next = state->next;
	    state->next->prev = state->prev;
	}

    /* Remove link from filehandle chain */

    if (state->prevfh == NULL)
	{
	    state->header->states = state->nextfh;
	    state->nextfh->prevfh = NULL;
	}
    else
	{
	    state->prevfh->nextfh = state->nextfh;
	    state->nextfh->prevfh = state->prevfh;
	}

    if (state->type == delegation)
	state->assoc.client.concats->deleg = NULL;
    if (state->type == dir_delegation)
	state->assoc.client.concats->dir_deleg = NULL;
    if (state->type == layout)
	state->assoc.client.concats->layout = NULL;

    if (state->assoc.client.concats->deleg ==
	state->assoc.client.concats->dir_deleg ==
	state->assoc.client.concats->layout ==
	NULL)
	kill_concats;

    if (state->header.states == NULL)
	killheader(state->header);

    if (state->type == lock)
	{
	    if (state->assoc.owned.state.lock.prev == NULL)
		{
		    state->assoc.owned.state.share->locks
			= state->assoc.owned.state.lock.next;
		    state->assoc.owned.state.lock.next->prev = NULL;
		}
	    else
		{
		    state->assoc.owned.state.lock.prev->next
			= state->assoc.owned.state.lock.next;
		    state->assoc.owned.state.lock.next->prev
			= state->assoc.owned.state.lock.prev;
		}
	}
    return ERR_FSAL_NO_ERROR;
}

/* Harvest a leftover concatstates */

int killconcats(concatstate* concats)
{
    int rc = 0;
    
    key.pdata = &(concats.key);
    key.len = sizeof(struct concatkey);
    rc = HashTable_Del(ownertable, key, NULL, NULL);
    if ((rc != HASHTABLE_SUCCESS) &&
	(rc != HASHTABLE_NO_SUCH_KEY))
	LogError(COMPONENT_STATES,
		 "Error deleting owner key.");

    RELEASE_PREALLOC(concats, concatpool, next_alloc);
}

/* Harvest a leftover header */

int killheader(entryheader* entry)
{
    int rc = 0;

    if (!pthread_mutex_lock(&entrymutex))
	return ERR_STATE_FAIL;
    
    key.pdata = entry->fsaldata;
    key.len = sizeof(cache_inode_fsal_data_t);
    
    rc = HashTable_Del(entrytable, key, NULL, NULL);
    if ((rc != HASHTABLE_SUCCESS) &&
	(rc != HASHTABLE_NO_SUCH_KEY))
	LogError(COMPONENT_STATES,
		 "Error deleting entry header key.");

    pthread_mutex_unlock(&entrymutex);
    pthread_rwlock_unlock(&(entry->lock));
    RELEASE_PREALLOC(entry, entrypool, next_alloc);
    return ERR_STATE_NO_ERROR;
}


/* Traverse all states associated with a given entry.  Whatever you
 * plan to do with them, acquire the appropriate lock first.  Returns
 * NULL on failure to find a next entry.
 */
 
int next_entry_state(entryheader* entry, state** state)
{
    if (*state == NULL)
	*state = entryheader->states;
    else
	*state = state->next;

    return *state;
}


/* Retrieve a state by stateid */

int getstate(stateid4 stateid, state** state)
{
    hash_buffer_t key, val;
    int counter = 0;
    int rc = 0;

    key.pdate = stateid.other;
    key.len = 12;

    rc = HashTable_get(entrytable, &key, &val);
=======
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

nt lookup_state(stateid4 stateid, state** state)
{
    int rc = 0;
    hash_buffer_t key, val;
    
    key.pdata = stateid.other;
    key.len = 12;
    
    rc = HashTable_get(statetable, &key, &val);
    
>>>>>>> 10d1be59b652f69fd7de00fe6f675f34d9ed69e9
    if (rc == HASHTABLE_ERROR_NO_SUCH_KEY)
	return ERR_STATE_NOENT;
    else if (rc != HASHTABLE_SUCCESS)
	return ERR_STATE_FAIL;

    *state = val.pdata;
<<<<<<< HEAD
    if (stateid.seqid && (state.seqid < state.stateid.seqid))
	return ERR_STATE_OLDSEQ;
    else if (stateid.seqid && (state.seqid > state.stateid.seqid))
=======

    /* TODO Update this to handle wraparound once we figure out how to
       quickly count the number of slots total associated with a
       client */

    if (stateid.seqid == 0)
	return ERR_STATE_NO_ERROR;
    else if (stateid.seqid < state->stateid.seqid)
	return ERR_STATE_OLDSEQ;
    else if (stateid.seqid > state->stateid.seqid)
>>>>>>> 10d1be59b652f69fd7de00fe6f675f34d9ed69e9
	return ERR_STATE_BADSEQ;
    else
	return ERR_STATE_NO_ERROR;
}

<<<<<<< HEAD
int getownedstate(clientid4 clientid, open_owner4* open_owner,
		  lock_owner4* lock_owner, state** state)
{
    int rc = 0;
    char* keybuff;
    hash_buffer_t key, val;
    size_t keylen
	= (open_owner->owner.owner_len +
	   (lock_owner ? lock_owner->owner.owner_len : 0) +
	   sizeof(clientid4) + 1);
    
    keybuff = alloca(keylen);

    *keybuff = lock_owner ? 1 : 0;
    *((clientid4*) (keybuff + 1)) = clientid;
    (*newstate)->assoc.owned.open_owner = (keybuff + 1 +
					   sizeof(clientid));
    memcpy((keybuff + 1 + sizeof(clientid))
	   open_owner.owner.owner_val,
	   open_owner.owner.owner_len);
    (*newstate)->assoc.owned.oolen = open_owner.owner.owner_len;

    if (lock_owner)
	memcpy((keybuff + 1 + sizeof(clientid) +
		open_owner.owner.owner_len),
	       lock_owner.owner.owner_val,
	       lock_owner.owner.owner_len);

    key.pdata = keybuff;
    key.len = keylen;

    rc = HashTable_get(ownertable, &key, &val)

    if (rc == HASHTABLE_NO_SUCH_KEY)
	{
	    killstate(newstate);
	    LogDebug(COMPONENT_STATES,
		     "Could not find state for owner/client pair.");
	    return ERR_STATE_NOENT;
	}

    else if (rc != HASHTABLE_SUCCESS)
	return ERR_STATE_FAIL;

    return ERR_STATE_NO_ERROR;
=======
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

void filltaggedstate(state* state, taggedstate* outstate)
{
    memset(outstate, 0, sizeof(taggedstate));
    switch (state->type)
	{
	case share:
	    fillshare(state, &(outstate->u.share));
	    break;
	case delegation:
	    filldelegation(state, &(outstate->u.delegation));
	    break;
	case dir_delegation:
	    filldir_delegation(state, &(outstate->u.dir_delegation));
	    break;
	case lock:
	    filllock(state, &(outstate->u.lock));
	    break;
	case layout:
	    filllayout(state, &(outstate->u.layout));
	    break;
	default:
	    LogCrit(COMPONENT_STATES,
		    "filltaggedstate: invalid state (can't happen)!");
	}
>>>>>>> 10d1be59b652f69fd7de00fe6f675f34d9ed69e9
}
