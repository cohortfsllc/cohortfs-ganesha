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
#include "nfsv41.h"
#include "cache_inode.h"

/************************************************************************
 * Internal data Structures
 *
 * Our own internal state representations, later converted to public
 * structures.
 ***********************************************************************/

typedef struct __localshare
{
    char open_owner[NFS4_OPAQUE_LIMIT];
    size_t open_owner_len;
    uint32_t share_access;
    uint32_t share_deny;
    boolean locks;
    openref_t* openref;
} share;

typedef struct __localdeleg
{
    open_delegation_type4 type;
    nfs_space_limit4 limit;
} deleg;

typedef struct __localdir_deleg
{
    bitmap4 notification_types;
    attr_notice4 child_attr_delay;
    attr_notice4 dir_attr_delay;
    bitmap4 child_attributes;
    bitmap4 dir_attributes;
} dir_deleg;

typedef struct __locallockstate
{
<<<<<<< HEAD
    state* sharestate;
    state* prev;
    state* next; /* For multiple lock_owners */
} locallockstate;
=======
    state* openstate;
    char lock_owner[NFS4_OPAQUE_LIMIT];
    size_t lock_owner_len;
    fsal_lock_t* lockdata;
} lock;
>>>>>>> 10d1be59b652f69fd7de00fe6f675f34d9ed69e9

typedef struct __locallayoutstate
{
    layouttype4 type;
    locallayoutentry* layoutentries;
} layout;

typedef struct __locallayoutentry
{
    layouttype4 type;
    layoutiomode4 iomode;
    offset4 offset;
    length4 length;
    boolean return_on_close;
    fsal_layout_t* layoutdata;
    struct __locallayoutentry* next;
    struct __locallayoutentry* prev;
    struct __locallayoutentry* next_alloc;
} layoutentry;

typedef struct __state
{
    entryheader* header;
    stateid4 stateid;
    statetype type;
    union __assocstate
    {
<<<<<<< HEAD
	struct __ownedstate
	{
	    void* chunk;
	    char* open_owner;
	    size_t oolen;
	    char* lock_owner;
	    size_t lolen;
	    size_t keylen;
	    
	    union __actualstate
	    {
		localshare share;
		locallockstate lock;
	    } state;
	} owned;

	struct __clientstate
	{
	    concatstates* concats;
	    union __actualstate
	    {
		localdeleg deleg;
		localdir_deleg dir_deleg;
		locallayoutstate layoutstate;
	    } state;
	} client;
    } assoc;
    __state* prev;
    __state* next;
    __state* prevfh;
    __state* nextfh;
    __state* next_alloc;
} state;

/* We put this here so we can store our keys in our concatstates. */

struct concatkey
{
    cache_inode_fsal_data_t* pfsdata;
    clientid4 clientid;
};



typedef struct __concatstates
{
    struct concatkey key;
    entryheader* header;
    state* deleg;
    state* dir_deleg;
    state* layout;
    __concatstates* next_alloc;
} concatstates;

/* Likely add bitmap etc. to this later on */
=======
	share share;
	deleg deleg;
	dir_deleg dir_deleg;
	locks lock;
	layout layout;
    } u;
    __state* next;
    __state* prev;
    __state* nextfh;
    __state* prevfh;
    __state* next_alloc;
} state;
>>>>>>> 10d1be59b652f69fd7de00fe6f675f34d9ed69e9

typedef struct __entryheader
{
    fsal_handle_t handle; /* Filehandle */
    pthread_rwlock_t lock; /* Per-filehandle read/write lock */
    bool valid;         /* A check */
    uint32_t max_share; /* Most expansive share */
    uint32_t max_deny; /* Most restrictive deny */
    uint32_t anonreaders; /* Number of anonymous readers (old NFS or
			     all-zeroes) */
    uint32_t anonwriters; /* Number of anonymous writers (old NFS or
			     all-zeroes) */
    boolean read_delegations; /* if any read delegations exist */
    boolean write_delegation; /* If any write delegations exist */
    boolean dir_delegations; /* If any directory delegations exist */
    state* states;
    __entryheader* next_alloc;
} entryheader;

/************************************************************************
 * Global variables 
 *
 * Pools and hashtables
 ***********************************************************************/

extern pthread_mutex_t entrymutex;

extern locallayoutentry* layoutentrypool;
extern entryheader* entryheaderpool;
extern state* statepool;
<<<<<<< HEAD
extern concatstates* concatstatepool;

extern hash_table_t* stateidtable;
extern hash_table_t* entrytable;
extern hash_table_t* concattable;
extern hash_table_t* ownertable;

extern loclastate* statechain;
=======

extern hash_table_t* stateidtable;
extern hash_table_t* entrytable;
>>>>>>> 10d1be59b652f69fd7de00fe6f675f34d9ed69e9


/************************************************************************
 * Internal Functions
 ***********************************************************************/

int init_stateidtable(void);
int init_entrytable(void);
<<<<<<< HEAD
int init_concattable(void);
int entryisfile(cache_entry_t* entry);
int header_for_writing(cache_entry_t entry, entryheader** header);
concatstates* get_concat(entryheader* header, clientid4 clientid,
			 bool create);
int newclientstate(clientid4 clientid, state** newstate);
int newownedstate(clientid4 clientid, open_owner4* open_owner,
		  lock_owner4* lock_owner, state** newstate);
int getstate(stateid4 stateid, state** state)
int chain(state* state, entryheader* header, state* share);
int unchain(state* state);
int next_entry_state(entryheader* entry, state** state)
=======
int header_for_write(fsal_handle_t handle, entryheader** header);
int header_for_read(fsal_handle_t handle, entryheader** header);
localstate* newstate(clientid4 clientid);
void chain(localstate* state, entryheader* header);
state* iterate_entry(entryheader* entry, state** state);
int lookup_state_and_lock(stateid4 stateid, state** state,
			  entryheader** header, boolean write);
int lookup_state(stateid4 stateid, state** state);
void killstate(state* state);
void filltaggedstate(state* state, taggedstate* outstate);
void fillsharestate(state* cur, sharestate* outshare
		    entryheader* header);
void filldelegationstate(state* cur, delegationstate outdelegation,
			 entryheader* header);
void filldir_delegationstate(state* cur,
			     dir_delegationstate* outdir_delegation,
			     entryheader* header);
void filllockstate(state* cur, dir_delegationstate* outdir_delegation,
		   entryheader* header);
void filllayoutstate(state* cur, dir_delegationstate* outdir_delegation,
		     entryheader* header);

/* Prototypes for realisations */
>>>>>>> 10d1be59b652f69fd7de00fe6f675f34d9ed69e9

int localstate_create_share(fsal_handle_t *handle, open_owner4 open_owner,
			    clientid4 clientid, uint32_t share_access,
			    uint32_t share_deny, stateid4* stateid);
int localstate_upgrade_share(uint32_t share_access, uint32_t share_deny,
			     stateid4* stateid);
int localstate_downgrade_share(uint32_t share_access, uint32_t share_deny,
			       stateid4* stateid);
int localstate_delete_share(stateid4 stateid);
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
int localstate_check_delegation(fsal_handle_t *handle);
int localstate_create_lock_state(fsal_handle_t *handle,
				 stateid4 open_stateid,
				 lock_owner4 lock_owner,
				 fsal_lock_t* lockdata,
				 stateid4* stateid);
int localstate_delete_lock_state(stateid4 stateid);
int localstate_query_lock_state(fsal_handle_t *handle,
				stateid4 open_stateid,
				lock_owner4 lock_owner,
				lockstate* outlockstate);
int localstate_lock_inc_state(stateid4* stateid);
int localstate_create_layout_state(fsal_handle_t handle,
				   stateid4 ostateid,
				   layouttype4 type,
				   stateid4* stateid);
int localstate_delete_layout_state(stateid4 stateid);
int state_query_layout_state(fsal_handle_t *handle,
			     layouttype4 type,
			     lockstate* outlayoutstate);
int localstate_add_layout_segment(layouttype4 type,
				  layoutimode4 iomode,
				  offset4 offset,
				  length4 length,
				  boolean return_on_close,
				  fsal_layout_t* layoutdata,
				  stateid4* stateid);
int localstate_mod_layout_segment(layoutimode4 iomode,
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
				   boolean* finished,
				   layoutsegment* segment);
int localstate_lock_filehandle(fsal_handle_t *handle,
			       statelocktype rw);
int localstate_unlock_filehandle(fsal_handle_t *handle);
int localstate_iterate_by_filehandle(fsal_handle_t *handle, statetype type,
				     uint64_t* cookie, boolean* finished,
				     taggedstate* outstate);
int localstate_iterate_by_clientid(clientid4 clientid, statetype type,
				   uint64_t* cookie, boolean* finished,
				   state* outstate);
int localstate_retrieve_state(stateid4 stateid,
			      taggedstate* outstate);
#endif                                                /* _SAL_INTERNAL */
