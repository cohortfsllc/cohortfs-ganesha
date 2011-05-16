/*
 *
 * Copyright (C) 2010, The Linux Box, inc.
 * All Rights Reserved
 *
 * Contributor: Adam C. Emerson
 */

#ifndef _SAL_INTERNAL

#include "sal.h"
#include "stuff_alloc.h"
#include "nfs_core.h"
#include "fsal.h"
#include "nfs4.h"
#include "cache_inode.h"

/************************************************************************
 * Internal data Structures
 *
 * Our own internal state representations, later converted to public
 * structures.
 ***********************************************************************/

typedef struct open_owner_key__
{
     clientid4 clientid;
     char owner_val[1024];
     size_t owner_len;
} open_owner_key_t;

typedef struct open_owner__
{
     open_owner_key_t key;
     uint32_t seqid;
     uint32_t refcount;
     bool_t new;
     struct nfs_resop4* last_response;
     pthread_mutex_t mutex; /* Only used for NFS4.0, since operations on
			       the same owner must be serialised */
} open_owner_t;

typedef struct lock_owner__
{
     state_lock_owner_t key;
     uint32_t seqid;
     uint32_t refcount;
     struct nfs_resop4* last_response;
     pthread_mutex_t mutex;
} lock_owner_info_t;

typedef struct openref_key__
{
     fsal_handle_t handle;
     uid_t uid;
} openref_key_t;

typedef struct openref__
{
     openref_key_t key;
     fsal_file_t descriptor;
     fsal_openflags_t openflags;
     uint32_t refcount;
} openref_t;

typedef struct share_state__
{
     open_owner_t* open_owner;
     uint32_t share_access;
     uint32_t share_deny;
     openref_t* openref;
} share_state_t;

typedef struct lock_state__
{
     struct state__* open_state;
     lock_owner_info_t* lock_owner;
     bool_t acquired_a_lock;
     fsal_filelockinfo_t filelockinfo;
} lock_state_t;

typedef struct lock__ {
     uint64_t offset;
     uint64_t length;
     bool_t exclusive;
     bool_t blocking;
     struct state__* state;
     struct lock__* prev;
     struct lock__* next;
} lock_t;

typedef enum lock_overlap__ {
     LOCKS_DISJOINT = 0,
     LOCKS_EQUAL = 1,
     LOCK1_SUPERSET = 2,
     LOCK1_SUBSET = 3,
     LOCK1_BEGINS_BEFORE = 4,
     LOCK1_ENDS_AFTER = 5
} lock_overlap_t;

typedef struct state__
{
     struct perfile_state__* perfile;
     stateid4 stateid;
     clientid4 clientid;
     state_type_t type;
     union
     {
	  share_state_t share;
	  lock_state_t lock;
     } state;
} state_t;

typedef struct perfile_state__
{
     fsal_handle_t handle; /* Filehandle */
     pthread_rwlock_t lock; /* Per-filehandle read/write lock */
     uint32_t refcount;
     uint32_t access_readers; /* Number of clients with read access */
     uint32_t access_writers; /* Number of clients with write access */
     uint32_t deny_readers; /* Number of clients blocking read access */
     uint32_t deny_writers; /* Number of clients blocking write access */
     uint32_t anon_readers; /* Number of anonymous readers (old NFS or
			       all-zeroes) */
     uint32_t anon_writers; /* Number of anonymous writers (old NFS or
			      all-zeroes) */
     lock_t* locks; /* All locks on this file */
} perfile_state_t;

/************************************************************************
 * Global variables
 *
 * Pools and hashtables
 ***********************************************************************/

extern prealloc_pool perfile_state_pool;
extern prealloc_pool open_owner_pool;
extern prealloc_pool lock_owner_pool;
extern prealloc_pool state_pool;
extern prealloc_pool openref_pool;
extern prealloc_pool lock_pool;

extern hash_table_t* stateid_table;
extern hash_table_t* perfile_state_table;
extern hash_table_t* open_owner_table;
extern hash_table_t* lock_owner_table;
extern hash_table_t* share_state_table;
extern hash_table_t* lock_state_table;
extern hash_table_t* openref_table;

/************************************************************************
 * Internal Functions
 ***********************************************************************/

/* Initialisation functions */

hash_table_t* init_stateid_table(void);
hash_table_t* init_perfile_state_table(void);
hash_table_t* init_open_owner_table(void);
hash_table_t* init_lock_owner_table(void);
hash_table_t* init_share_state_table(void);
hash_table_t* init_lock_state_table(void);

/* Generic state management */

int assign_stateid(state_t* state);
int lookup_state(stateid4 stateid, state_t** state);
int acquire_perfile_state(fsal_handle_t *handle,
			  perfile_state_t** perfile);
int lookup_perfile_state(fsal_handle_t* handle,
			 perfile_state_t** perfile); 

/* Unexported share functionality */
int open_owners_equal(open_owner_key_t* owner1,
		      open_owner_key_t* owner2);
hash_table_t* init_open_owner_table(void);
hash_table_t* init_share_state_table(void);
hash_table_t* init_openref_table(void);

/* Share functionality realisations */
int localstate_open_owner_begin41(clientid4 clientid,
				  open_owner4 nfs_open_owner,
				  state_share_trans_t** transaction);
int localstate_open_stateid_begin41(stateid4 stateid,
				    state_share_trans_t**
				    transaction);
int localstate_share_open(state_share_trans_t* transaction,
			  fsal_handle_t* handle,
			  fsal_op_context_t* context,
			  uint32_t share_access,
			  uint32_t share_deny,
			  uint32_t uid);
int localstate_share_close(state_share_trans_t* transaction,
			   fsal_handle_t* handle,
			   fsal_op_context_t* context);
int localstate_share_downgrade(state_share_trans_t* transaction,
			       fsal_handle_t* handle,
			       uint32_t share_access,
			       uint32_t share_deny);
int localstate_share_commit(state_share_trans_t* transaction);
int localstate_share_abort(state_share_trans_t* transaction);
int localstate_share_dispose_transaction(state_share_trans_t*
					 transaction);
int localstate_share_get_stateid(state_share_trans_t* transaction,
				 stateid4* stateid);
int localstate_share_get_nfs4err(state_share_trans_t* transaction,
				 nfsstat4* error);
int localstate_start_anonread(fsal_handle_t* handle,
			      int uid,
			      fsal_op_context_t* context,
			      fsal_file_t** descriptor);
int localstate_start_anonwrite(fsal_handle_t* handle,
			       int uid,
			       fsal_op_context_t* context,
			       fsal_file_t** descriptor);
int localstate_end_anonread(fsal_handle_t* handle,
			    int uid);
int localstate_end_anonwrite(fsal_handle_t* handle,
			     int uid);
int localstate_share_descriptor(fsal_handle_t* handle,
				stateid4* stateid,
				fsal_file_t** descriptor);

/* Unexported lock functionality */
unsigned int hash_lock_owner(state_lock_owner_t* owner, uint32_t* h1,
			     uint32_t* h2);

/* Lock functionality realisations */
int localstate_open_to_lock_owner_begin41(fsal_handle_t *handle,
					  clientid4 clientid, 
					  stateid4 open_stateid,
					  lock_owner4 nfs_lock_owner,
					  state_lock_trans_t** transaction);

int localstate_exist_lock_owner_begin41(fsal_handle_t *handle,
					clientid4 clientid,
					stateid4 lock_stateid,
					state_lock_trans_t** transaction);

int localstate_lock(state_lock_trans_t* transaction,
		    uint64_t offset,
		    uint64_t length,
		    bool_t exclusive,
		    bool_t blocking);

int localstate_unlock(state_lock_trans_t* transaction,
		       uint64_t offset,
		       uint64_t length);

int localstate_lock_commit(state_lock_trans_t* transaction);

int localstate_lock_abort(state_lock_trans_t* transaction);

int localstate_lock_dispose_transaction(state_lock_trans_t* transaction);
     
int localstate_lock_get_stateid(state_lock_trans_t* transaction,
			   stateid4* stateid);

int localstate_lock_get_nfs4err(state_lock_trans_t* transaction,
				nfsstat4* error);

int localstate_lock_get_nfs4conflict(state_lock_trans_t* transaction,
				     uint64_t* offset,
				     uint64_t* length,
				     uint32_t* type,
				     lock_owner4* lock_owner);

/* Init/Shutdown functions */
int localstate_init(void);
int localstate_shutdown(void);
#endif                                                /* _SAL_INTERNAL */
