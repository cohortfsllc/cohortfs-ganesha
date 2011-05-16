/*
 *
 * Copyright (C) 2010, The Linux Box, inc.
 * All Rights Reserved
 *
 * Contributor: Adam C. Emerson
 */

#include "sal.h"
#include "stuff_alloc.h"
#include "fsal.h"
#include "log_macros.h"
#include <sys/time.h>
#include <alloca.h>
#include "sal_internal.h"
#include "lookup3.h"

/************************************************************************
 * Global pools
 *
 * A few pools for local data structures.
 ***********************************************************************/

prealloc_pool perfile_state_pool;
prealloc_pool open_owner_pool;
prealloc_pool lock_owner_pool;
prealloc_pool state_pool;
prealloc_pool openref_pool;
prealloc_pool lock_pool;

/************************************************************************
 * Hash Tables
 *
 * I am not entirely happy with this data structure, but it will get
 * the job done for now.
 ***********************************************************************/

hash_table_t* stateid_table;
hash_table_t* perfile_state_table;
hash_table_t* open_owner_table;
hash_table_t* lock_owner_table;
hash_table_t* share_state_table;
hash_table_t* lock_state_table;
hash_table_t* openref_table;

/* State ID table */

/* All this stuff is taken from Philippe's nfs_state_id.c */

static int
display_stateid_key(hash_buffer_t * pbuff, char *str)
{
     unsigned int i = 0;
     unsigned int len = 0;

     for(i = 0; i < 12; i++)
	  len += sprintf(&(str[i * 2]), "%02x", (unsigned char)pbuff->pdata[i]);
     return len;
}

static int
display_stateid_val(hash_buffer_t * pbuff, char *str)
{
    return 0;
}

static int
compare_stateid(hash_buffer_t * buff1, hash_buffer_t * buff2)
{
     return memcmp(buff1->pdata, buff2->pdata, 12);
}

static unsigned long
stateid_value_hash_func(hash_parameter_t * p_hparam,
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
}

static unsigned long
stateid_rbt_hash_func(hash_parameter_t * p_hparam,
		      hash_buffer_t * buffclef)
{
     u_int32_t i1 = 0;
     u_int32_t i2 = 0;
     u_int32_t i3 = 0;

     if(isFullDebug(COMPONENT_STATES)) {
	  char str[25];

	  sprint_mem(str, (char *)buffclef->pdata, 12);
	  LogFullDebug(COMPONENT_SESSIONS,
		       "         ----- state_id_rbt_hash_func : %s", str);
     }

     memcpy(&i1, &(buffclef->pdata[0]), sizeof(u_int32_t));
     memcpy(&i2, &(buffclef->pdata[4]), sizeof(u_int32_t));
     memcpy(&i3, &(buffclef->pdata[8]), sizeof(u_int32_t));

     LogFullDebug(COMPONENT_STATES, "--->  state_id_rbt_hash_func=%lu",
		  (unsigned long)(i1 ^ i2 ^ i3));

     return (unsigned long)(i1 ^ i2 ^ i3);
}

static hash_parameter_t
stateid_params = {
     .index_size=17,
     .alphabet_length=10,
     .nb_node_prealloc=10,
     .hash_func_key=stateid_value_hash_func,
     .hash_func_rbt=stateid_rbt_hash_func,
     .compare_key=compare_stateid,
     .key_to_str=display_stateid_key,
     .val_to_str=display_stateid_val
};

hash_table_t*
init_stateid_table(void)
{
     stateid_table = HashTable_Init(stateid_params);
     return stateid_table;
}

static int
localsalstringnoop(hash_buffer_t * pbuff, char *str)
{
     return 0;
}

static unsigned long
handle_hash_func(hash_parameter_t * p_hparam,
		 hash_buffer_t * buffclef)
{
     return (FSAL_Handle_to_HashIndex((fsal_handle_t*) buffclef->pdata, 0,
				      p_hparam->alphabet_length,
				      p_hparam->index_size));
}

static unsigned long
handle_rbt_func(hash_parameter_t * p_hparam,
		hash_buffer_t * buffclef)
{
     return (FSAL_Handle_to_RBTIndex((fsal_handle_t*) buffclef->pdata, 0));
}

static int
handle_compare_key_fsal(hash_buffer_t * buff1, hash_buffer_t * buff2)
{
     fsal_status_t status;

     return (FSAL_handlecmp((fsal_handle_t*)buff1->pdata,
			    (fsal_handle_t*)buff2->pdata,
			    &status));
}

static hash_parameter_t
perfile_state_params = {
     .index_size=29,
     .alphabet_length=10,
     .nb_node_prealloc=1000,
     .hash_func_key=handle_hash_func,
     .hash_func_rbt=handle_rbt_func,
     .compare_key=handle_compare_key_fsal,
     .key_to_str=localsalstringnoop,
     .val_to_str=localsalstringnoop
};

hash_table_t*
init_perfile_state_table(void)
{
     perfile_state_table = HashTable_Init(perfile_state_params);
     return perfile_state_table;
}

/* Create a stateid.other.  The clientid and time stuff should make it
 * unlike to collide.
 */

static void
newstateidother(clientid4 clientid, char* other)
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

int
assign_stateid(state_t* state)
{
     hash_buffer_t key, val;
     int counter = 0;
     int rc = 0;

     key.len = 12;
     val.pdata = (caddr_t) state;
     val.len = sizeof(state_t);
     do
     {
	  newstateidother(state->clientid, state->stateid.other);
	  key.pdata = (caddr_t) state->stateid.other;
	  rc = HashTable_Test_And_Set(stateid_table, &key, &val,
				      HASHTABLE_SET_HOW_SET_NO_OVERWRITE);
     }
     while ((rc == HASHTABLE_ERROR_KEY_ALREADY_EXISTS) && ++counter <= 100);

     if (rc != HASHTABLE_SUCCESS)
     {
	  LogCrit(COMPONENT_STATES,
		  "Unable to create new stateid.  This should not happen.");

	  return ERR_STATE_FAIL;
     }

     return ERR_STATE_NO_ERROR;
}

int
lookup_state(stateid4 stateid, state_t** state)
{
     int rc = 0;
     hash_buffer_t key, val;

     key.pdata = stateid.other;
     key.len = 12;

     rc = HashTable_Get(stateid_table, &key, &val);

     *state = NULL;

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

int
acquire_perfile_state(fsal_handle_t *handle,
		      perfile_state_t** perfile)
{
     hash_buffer_t key, val;
     int rc = 0;

     *perfile = NULL;

     GetFromPool(*perfile, &perfile_state_pool, perfile_state_t);
     if (!(*perfile)) {
	  return ERR_STATE_FAIL;
     }

     (*perfile)->handle = *handle;

     key.pdata = (caddr_t) &((*perfile)->handle);
     key.len = sizeof(fsal_handle_t);

     pthread_rwlock_init(&((*perfile)->lock), NULL);
     (*perfile)->access_readers = 0;
     (*perfile)->access_writers = 0;
     (*perfile)->deny_readers = 0;
     (*perfile)->deny_writers = 0;
     (*perfile)->anon_readers = 0;
     (*perfile)->anon_writers = 0;

     rc = HashTable_Set_Or_Fetch(lock_state_table, &key, &val);
     if (rc == HASHTABLE_ERROR_KEY_ALREADY_EXISTS) {
	  ReleaseToPool(*perfile, &perfile_state_pool);
	  *perfile = (perfile_state_t*) val.pdata;
	  return ERR_STATE_NO_ERROR;
     } else if (rc != HASHTABLE_SUCCESS) {
	  ReleaseToPool(*perfile, &perfile_state_pool);
	  *perfile = NULL;
	  return ERR_STATE_FAIL;
     }

     return ERR_STATE_NO_ERROR;
}

int
lookup_perfile_state(fsal_handle_t* handle,
		     perfile_state_t** perfile)
{
    hash_buffer_t key, val;
    int rc = 0;

    key.pdata = (caddr_t)handle;
    key.len = sizeof(fsal_handle_t);

    rc = HashTable_Get(perfile_state_table, &key, &val);

    if (rc == HASHTABLE_ERROR_NO_SUCH_KEY) {
	 *perfile = NULL;
	 return ERR_STATE_NOENT;
    } else if (rc == HASHTABLE_SUCCESS) {
	 *perfile = (perfile_state_t*) val.pdata;
	 return ERR_STATE_NO_ERROR;
    } else {
	 *perfile = NULL;
	 return ERR_STATE_FAIL;
    }
}
