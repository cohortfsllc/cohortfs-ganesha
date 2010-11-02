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

typedef struct __owner_key
{
  clientid4 clientid;
  char owner_val[MAXNAMLEN];
  size_t owner_len;
} owner_key_t;

typedef struct __state_owner
{
    owner_key_t key;
    uint32_t seqid;
    uint32_t refcount;
    bool_t lock;
    struct nfs_resop4* last_response;
    pthread_mutex_t mutex; /* Only used for NFS4.0, since operations on
			      the same owner must be serialised */
    struct __state_owner *related_owner;
    struct __state_owner *next_alloc;
} state_owner_t;

typedef struct __localshare
{
    state_owner_t* open_owner;
    uint32_t share_access;
    uint32_t share_deny;
    bool_t locks;
    cache_inode_openref_t* openref;
} localshare_t;

typedef struct __localdelegation
{
    open_delegation_type4 type;
    nfs_space_limit4 limit;
} localdelegation_t;

typedef struct __localdir_delegation
{
    bitmap4 notification_types;
    attr_notice4 child_attr_delay;
    attr_notice4 dir_attr_delay;
    bitmap4 child_attributes;
    bitmap4 dir_attributes;
} localdir_delegation_t;

typedef struct __locallock
{
    struct __state* openstate;
    state_owner_t* lock_owner;
    fsal_lockdesc_t* lockdata;
} locallock_t;

#ifdef _USE_FSALMDS

typedef struct __locallayout
{
    layouttype4 type;
    struct __locallayoutentry* layoutentries;
} locallayout_t;

typedef struct __locallayoutentry
{
    layouttype4 type;
    layoutiomode4 iomode;
    offset4 offset;
    length4 length;
    bool_t return_on_close;
    fsal_layout_t* layoutdata;
    struct __locallayoutentry* next;
    struct __locallayoutentry* prev;
    struct __locallayoutentry* next_alloc;
} locallayoutentry_t;

#endif

typedef struct __state
{
    struct __entryheader* header;
    stateid4 stateid;
    clientid4 clientid;
    statetype type;
    union
    {
	localshare_t share;
	locallock_t lock;
	localdelegation_t delegation;
	localdir_delegation_t dir_delegation;
#ifdef _USE_FSALMDS
	locallayout_t layout;
#endif
    } state;
    struct __state* prev;
    struct __state* next;
    struct __state* prevfh;
    struct __state* nextfh;
    struct __state* next_alloc;
} state_t;

typedef struct __entryheader
{
    fsal_handle_t handle; /* Filehandle */
    pthread_rwlock_t lock; /* Per-filehandle read/write lock */
    uint32_t max_share; /* Most expansive share */
    uint32_t max_deny; /* Most restrictive deny */
    uint32_t anonreaders; /* Number of anonymous readers (old NFS or
			     all-zeroes) */
    uint32_t anonwriters; /* Number of anonymous writers (old NFS or
			     all-zeroes) */
    bool_t read_delegations; /* if any read delegations exist */
    bool_t write_delegation; /* If any write delegations exist */
    bool_t dir_delegations; /* If any directory delegations exist */
    state_t* states;
    struct __entryheader* next_alloc;
} entryheader_t;

/************************************************************************
 * Global variables 
 *
 * Pools and hashtables
 ***********************************************************************/

extern state_t* statechain;

extern pthread_mutex_t entrymutex;
extern pthread_mutex_t ownermutex;

#ifdef _USE_FSALMDS
extern locallayoutentry_t* layoutentrypool;
#endif
extern entryheader_t* entryheaderpool;
extern state_owner_t* ownerpool;
extern state_t* statepool;

extern hash_table_t* stateidtable;
extern hash_table_t* entrytable;
extern hash_table_t* openownertable;
extern hash_table_t* lockownertable;

/************************************************************************
 * Internal Functions
 ***********************************************************************/

hash_table_t* init_stateidtable(void);
hash_table_t* init_entrytable(void);
hash_table_t* init_openownertable(void);
hash_table_t* init_lockownertable(void);
entryheader_t* lookupheader(fsal_handle_t* handle);
state_t* newstate(clientid4 clientid, entryheader_t* header);
void chain(state_t* state, entryheader_t* header);
state_t* iterate_entry(entryheader_t* entry, state_t** state);
int lookup_state(stateid4 stateid, state_t** state);
void killstate(state_t* state);
void filltaggedstate(state_t* state, taggedstate* outstate);
void fillsharestate(state_t* cur, sharestate* outshare,
		    entryheader_t* header);
void filldelegationstate(state_t* cur, delegationstate* outdelegation,
			 entryheader_t* header);
void filldir_delegationstate(state_t* cur,
			     dir_delegationstate* outdir_delegation,
			     entryheader_t* header);
void filllockstate(state_t* cur, lockstate* outdir_delegation,
		   entryheader_t* header);
state_owner_t* acquire_owner(char* name, size_t len,
			     clientid4 clientid, bool_t lock,
			     bool_t wantmutex, bool_t* created);

int killowner(state_owner_t* owner);

/* Prototypes for realisations */

int localstate_create_share(fsal_handle_t *handle, open_owner4 open_owner,
			    clientid4 clientid, uint32_t share_access,
			    uint32_t share_deny,
			    cache_inode_openref_t* openref,
			    stateid4* stateid);
int localstate_upgrade_share(uint32_t share_access, uint32_t share_deny,
			     stateid4* stateid);
int localstate_downgrade_share(uint32_t share_access, uint32_t share_deny,
			       stateid4* stateid);
int localstate_delete_share(stateid4 stateid);
int localstate_check_share(fsal_handle_t handle, uint32_t share_access,
			   uint32_t share_deny);
int localstate_query_share(fsal_handle_t *handle, clientid4 clientid,
			   open_owner4 open_owner, sharestate*
			   outshare);
int localstate_start_32read(fsal_handle_t *handle);
int localstate_start_32write(fsal_handle_t *handle);
int localstate_end_32read(fsal_handle_t *handle);
int localstate_end_32write(fsal_handle_t *handle);
int localstate_create_delegation(fsal_handle_t *handle, clientid4 clientid,
				 open_delegation_type4 type,
				 nfs_space_limit4 limit,
				 stateid4* stateid);
int localstate_delete_delegation(stateid4 stateid);
int localstate_query_delegation(fsal_handle_t *handle, clientid4 clientid,
				delegationstate* outdelegation);
int localstate_check_delegation(fsal_handle_t *handle,
				open_delegation_type4 type);
int localstate_create_dir_delegation(fsal_handle_t *handle, clientid4 clientid,
				     bitmap4 notification_types,
				     attr_notice4 child_attr_delay,
				     attr_notice4 dir_attr_delay,
				     bitmap4 child_attributes,
				     bitmap4 dir_attributes,
				     stateid4* stateid);
int localstate_delete_dir_delegation(stateid4 stateid);
int localstate_query_dir_delegation(fsal_handle_t *handle, clientid4 clientid,
				    dir_delegationstate* outdir_delegation);
int localstate_check_delegation(fsal_handle_t *handle,
				open_delegation_type4 type);
int localstate_create_lock_state(fsal_handle_t *handle,
				 stateid4 open_stateid,
				 lock_owner4 lock_owner,
				 clientid4 clientid,
				 fsal_lockdesc_t* lockdata,
				 stateid4* stateid);
int localstate_delete_lock_state(stateid4 stateid);
int localstate_query_lock_state(fsal_handle_t *handle,
				stateid4 open_stateid,
				lock_owner4 lock_owner,
				clientid4 clientid,
				lockstate* outlockstate);
int localstate_lock_inc_state(stateid4* stateid);
#ifdef _USE_FSALMDS
int localstate_create_layout_state(fsal_handle_t* handle,
				   stateid4 ostateid,
				   clientid4 clientid,
				   layouttype4 type,
				   stateid4* stateid);
int localstate_delete_layout_state(stateid4 stateid);
int localstate_query_layout_state(fsal_handle_t *handle,
				  layouttype4 type,
				  lockstate* outlayoutstate);
int localstate_add_layout_segment(layouttype4 type,
				  layoutiomode4 iomode,
				  offset4 offset,
				  length4 length,
				  bool_t return_on_close,
				  fsal_layout_t* layoutdata,
				  stateid4* stateid);
int localstate_mod_layout_segment(layoutiomode4 iomode,
				  offset4 offset,
				  length4 length,
				  fsal_layout_t* layoutdata,
				  stateid4 stateid,
				  uint64_t segid);
int localstate_free_layout_segment(stateid4 stateid,
				   uint64_t segid);
int localstate_layout_inc_state(stateid4* stateid);
int localstate_iter_layout_entries(stateid4 stateid,
				   uint64_t* cookie,
				   bool_t* finished,
				   layoutsegment* segment);
void filllayoutstate(state_t* cur, layoutstate* outlayout,
		     entryheader_t* header);
#endif
int localstate_lock_filehandle(fsal_handle_t *handle,
			       statelocktype rw);
int localstate_unlock_filehandle(fsal_handle_t *handle);
int localstate_iterate_by_filehandle(fsal_handle_t *handle, statetype type,
				     uint64_t* cookie, bool_t* finished,
				     taggedstate* outstate);
int localstate_iterate_by_clientid(clientid4 clientid, statetype type,
				   uint64_t* cookie, bool_t* finished,
				   taggedstate* outstate);
int localstate_retrieve_state(stateid4 stateid,
			      taggedstate* outstate);
int localstate_lock_state_owner(state_owner4 state_owner, bool_t lock,
			   seqid4 seqid, bool_t* new,
			   nfs_resop4** response);

int localstate_unlock_state_owner(state_owner4 state_owner, bool_t lock);

int localstate_save_response(state_owner4 state_owner, bool_t lock,
			     nfs_resop4* response);
int localstate_init(void);
int localstate_shutdown(void);
#endif                                                /* _SAL_INTERNAL */
