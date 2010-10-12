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
    uint32_t share_access;
    uint32_t share_deny;
    state* locks;
} localshare;

typedef struct __localdeleg
{
    open_delegation_type4 type;
    nfs_space_limit4 limit;
} localdeleg;

typedef struct __localdir_deleg
{
    bitmap4 notification_types;
    attr_notice4 child_attr_delay;
    attr_notice4 dir_attr_delay;
    bitmap4 child_attributes;
    bitmap4 dir_attributes;
} localdir_deleg;

typedef struct __locallockstate
{
    state* sharestate;
    state* prev;
    state* next; /* For multiple lock_owners */
} locallockstate;

typedef struct __locallayoutstate
{
    locallayoutentry* layoutentries;
} locallayoutstate;

typedef struct __locallayoutentry
{
    layouttype4 type;
    layoutiomode4 iomode;
    offset4 offset;
    length4 length;
    bool return_on_close;
    fsal_layout_t layoutdata;
    struct __locallayoutentry* next;
    struct __locallayoutentry* prev;
    struct __locallayoutentry* next_alloc;
} locallayoutentry;

typedef struct __state
{
    entryheader* header;
    stateid4 stateid;
    statetype type;
    union __assocstate
    {
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

typedef struct __entryheader
{
    cache_inode_fsal_data_t fsaldata; /* Filehandle */
    pthread_rwlock_t lock; /* Per-filehandle read/write lock */
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
extern concatstates* concatstatepool;

extern hash_table_t* stateidtable;
extern hash_table_t* entrytable;
extern hash_table_t* concattable;
extern hash_table_t* ownertable;

extern loclastate* statechain;


/************************************************************************
 * Internal Functions
 ***********************************************************************/

int init_stateidtable(void);
int init_entrytable(void);
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

#endif                                                /* _SAL_INTERNAL */
