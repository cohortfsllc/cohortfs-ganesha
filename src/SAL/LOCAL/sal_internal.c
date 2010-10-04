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

locallockentry* lockentrypool;
locallayoutentry* layoutentrypool;
cache_inode_fsal_data_t* fsaldatapool;
localstate* localstatepool;
concatstates* concatstatepool;

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
hash_table_t* concattable;

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

/* Entry table */

/* This table is separate from cache_inode's state table, since in
 * distribute state realisations it may not make sense to store
 * pointers to states in cache entries.
 */


int dummy2str(hash_buffer_t * pbuff, char *str)
{
    str="DUMMY";
    return 0;
}

hash_parameter_t entryparams = {
    .index_size=29,
    .alphabet_length=10,
    .nb_node_prealloc=1000,
    .hash_func_key=cache_inode_fsal_hash_func,
    .hash_func_rbt=cache_inode_fsal_rbt_func,
    .compare_key=cache_inode_compare_key_fsal,
    .key_to_str=dummy2str,
    .val_to_str=dummy2str
};

int init_entrytable(void)
{
    entrytable = HashTable_Init(entryparams);
    return entrytable;
}

/* Concatenated entry/clientid table */

/* This table exists to allow quick lookups of pre-existing states on
 * a given cleint/file pair
 */

    
unsigned long concat_hash_func(hash_parameter_t * p_hparam,
			       hash_buffer_t * buffclef)
{
    unsigned long h = 0;
    cache_inode_fsal_data_t* pfsdata
	= ((struct concatkey*) buffclef->pdata).pfsdata;
    clientid4 clientid = ((concatkey*) buffclef->pdata).clientid;
    unsigned int cookie = (pfsdata->cookie ^
			   (0x00000000ffffffff & clientid) ^
			   (clientid >> 0x20));

    h = FSAL_Handle_to_HashIndex(&pfsdata->handle, cookie,
                                 p_hparam->alphabet_length,
                                 p_hparam->index_size);

    return h;
}                               /* cache_inode_fsal_hash_func */

unsigned long concat_rbt_func(hash_parameter_t * p_hparam,
			      hash_buffer_t * buffclef)
{
    unsigned long h = 0;
    cache_inode_fsal_data_t* pfsdata
	= ((struct concatkey*) buffclef->pdata).pfsdata;
    clientid4 clientid = ((concatkey*) buffclef->pdata).clientid;
    unsigned int cookie = (pfsdata->cookie ^
			   (0x00000000ffffffff & clientid) ^
			   (clientid >> 0x20));

    h = FSAL_Handle_to_RBTIndex(&pfsdata->handle, cookie);

    return h;
}                               /* cache_inode_fsal_rbt_func */

int concat_compare_key(hash_buffer_t * buff1, hash_buffer_t * buff2)
{
    struct concatkey* key1 = (struct concatkey*) buff1->pdata;
    struct concatkey* key2 = (struct concatkey*) buff2->pdata;
    
    if(key1 == NULL)
	return key2 == NULL ? 0 : 1;
    else if(key2 == NULL)
        return 1;              /* left member is the greater one */
    else
        {
	    fsal_status_t status;
	    cache_inode_fsal_data_t *pfsdata1 = key1.pfsdata;
	    cache_inode_fsal_data_t *pfsdata2 = key2.pfsdata;

	    if (pfsdata1 != pfsdata2)
		return 1;
	    else
		return FSAL_handlecmp(&pfsdata1->handle,
				      &pfsdata2->handle, &status);
        }
}

hash_parameter_t concatparams = {
    .index_size=19,
    .alphabet_length=10,
    .nb_node_prealloc=100,
    .hash_func_key=concat_hash_func,
    .hash_func_rbt=concat_rbt_func,
    .compare_key=concat_compare_key,
    .key_to_str=dummy2str,
    .val_to_str=dummy2str
};

int init_concattable(void)
{
    concattable = HashTable_Init(concatparamgs);
    return concattable;
}



/*
 * Returns 0 if the given cache entry refers to a file, nonzero
 * otherwise
 */

int entryisfile(cache_entry_t* entry)
{
    return !(entry.internal_md.type == REGULAR_FILE);
}

/*
 * Fetches an entry header and write locks it.  If the header does not
 * exist, creates it in the table.
 */


int header_for_write(cache_entry_t* entry)
{
    hash_buffer_t key, bal;
    cache_inode_fsal_data_t fsaldata;
    entryheader* header;
    int rc=0;

    memset(fsaldata, 0, sizeof(cache_inode_fsal_data_t));
    fsaldata.handle = entry->object.file.handle;
    
    key.pdata = &fsaldata;
    key.len = sizeof(cache_inode_fsal_data_t);

    rc = HashTable_get(entrytable, &key, &val);

    if (rc == HASHTABLE_SUCCESS)
	{
	    header = val.pdata;
	    pthread_rwlock_wrlock(&(header->lock));
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
		    pthread_mutex_unlock(&entrymutex);
		    header = val.pdata;
		    pthread_rwlock_wrlock(&(header->lock));
		    return header;
		}
	    if (rc != HASHTABLE_ERROR_NO_SUCH_KEY)
		{
		    /* Really should be impossible */
		    pthread_mutex_unlock(&entrymutex);
		    return NULL;
		}

	    /* We may safely create the entry */
	    
	    GET_PREALLOC(newheader, entryheaderpool, 1, entryheader,
			 next_alloc);

	    if (!newheader)
		{
		    pthread_mutex_unlock(&entrymutex);
		    return NULL;
		}

	    memset(newheader, 0, sizeof(entryheader));
	    
	    /* Copy, since it looks like the HashTable code depends on
	       keys not going away */
	    
	    newheader.fsaldata = fsaldata;
	    key.pdata = &(newheader.fsaldata);

	    header->lock = PTHREAD_RWLOCK_INITIALIZER;

	    pthread_rwlock_wrlock(&(header->lock));

	    header->valid = 1;
	    val.pdata = newheader;
	    val.len = sizeof(entryheader);
	    
	    rc = Hash_Table_Test_and_Set(entrytable, &key, &val,
					 HASHTABLE_SET_HOW_SET_NO_OVERWRITE);

	    pthread_mutex_unlock(&entrymutex);
	    if (rc == HASHTABLE_SUCCESS)
		return newentry;
	    else
		RELEASE_PREALLOC(newentry, entrypool, next_alloc);
	}
    return NULL ;
}

/* This fetches the concatstates structure for the given
 * entry/clientid pair.  If create is set, it creates it.  The
 * appropriate lock MUST have been acquired before calling this
 * function.
 */

concatstates* get_concat(entryheader* header, clientid4 clientid,
			 bool create)
{
    hash_buffer_t key, val;
    struct concatkey keyval;
    concatstates* concat;
    int rc = 0;
    
    keyval.pfsdata = &(entryheader->fsaldata);
    keyval.clientid = clientid;
    key.pdata = &keyval;
    key.len = sizeof(struct concatkey);
    rc = HashTable_get(entrytable, &key, &val);
    
    if (rc == HASHTABLE_SUCCESS)
	concat = val.pdata;
    else if (rc == HASHTABLE_NO_SUCH_KEY)
	{
	    if (create)
		{
		    GET_PREALLOC(concat, concatpool, 1, concatstates,
				 next_alloc);
		    memset(concat, 0, sizeof(concatstates));
		    /* Copy the key off the stack */
		    concatstates.key = keyval;
		    key.pdata = &(concaststates.key);
		    concatstates->header = header;
		    concatstates->clientid = clientid;
		    rc = Hash_Table_Test_and_Set(concattable, &key, &val,
						 HASHTABLE_SET_HOW_SET_NO_OVERWRITE);
		    
		    if (rc != HASHTABLE_SUCCESS)
			{
			    RELEASE_PREALLOC(concat, concatpool, next_alloc);
			    concat = NULL;
			}
		}
	}
    else
	concat = NULL;

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

localstate* newstate(clientid4 clientid)
{
    hash_buffer_t key, val;
    localstate* newstate;
    int counter = 0;
    int rc = 0;

    if (!(newstate = (GET_PREALLOC(newstate, localstatepool, 1,
				   localstate, next_alloc))))
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

    if (rc != HASHTABLE_SUCCESS)
	{
	    LogCrit(COMPONENT_STATES,
		    "Unable to create new stateid.  This should not happen.");
	    
	    RELEASE_PREALLOC(newstate, localstatepool, next_alloc);
	    return NULL;
	}

    return newstate;
}

/* Chain a state onto the filehandle and entire linked lists.  This
 * should probably lock the main one.
 */

void chain(localstate* state, entryheader* header)
{
    state->next = statechain;
    statechain = state;
    state->next_fh = header->states;
    header->states = state;
}
