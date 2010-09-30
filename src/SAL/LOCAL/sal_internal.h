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
    open_owner4 open_owner;
    uint32_t share_access;
    uint32_t share_deny;
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
    lock_owner4 lock_owner;
    locallockentry* lockentries;
} locallockstate;

typedef struct __locallockentry
{
    nfs_lock_type4 locktype;
    offset4 offset;
    length4 length;
    fsal_lock_t lockdata;
    struct __locallockentry* prev;
    struct __locallockentry* next;
    struct __locallockentry* next_alloc;
} locallockentry;

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

typedef struct __localstate
{
    entryheader* header;
    clientid4 clientid;
    stateid4 stateid;
    concatstates* concats;
    statetype type;
    union __actualstate
    {
	localshare share;
	localdeleg deleg;
	localdir_deleg dir_deleg;
	locallockstate loclstate;
	locallayoutstate layoutstate;
    } u;
    __localstate* next;
    __localstate* nextfh;
    __localstate* next_alloc;
} localstate;

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
    clientid4 clientid;
    localstate* share;
    localstate* deleg;
    localstate* dir_deleg;
    localstate* loclstate;
    localstate* layoutstate;
    __concatstates* next_alloc;
} concatstates;

/* Likely add bitmap etc. to this later on */

typedef struct __entryheader
{
    cache_inode_fsal_data_t fsaldata; /* Filehandle */
    pthread_rwlock_t lock; /* Per-filehandle read/write lock */
    uint32_t max_share; /* Most expansive share */
    uint32_t max_deny; /* Most restrictive deny */
    uint32_t nfs23readers; /* Number of readers using NFSv2 and NFSv3 */
    uint32_t nfs23writers; /* Number of writers using NFSv2 and NFSv3 */
    boolean read_delegations; /* if any read delegations exist */
    boolean write_delegation; /* If any write delegations exist */
    boolean dir_delegations; /* If any directory delegations exist */
    localstate* states;
    __entryheader* next_alloc;
} entryheader;

/************************************************************************
 * Global variables 
 *
 * Pools and hashtables
 ***********************************************************************/

extern pthread_mutex_t entrymutex;

extern locallockentry* lockentrypool;
extern locallayoutentry* layoutentrypool;
extern entryheader* entryheaderpool;
extern localstate* localstatepool;
extern concatstates* concatstatepool;

extern hash_table_t* stateidtable;
extern hash_table_t* entrytable;
extern hash_table_t* concattable;


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
localstate* newstate(clientid4 clientid);

#endif                                                /* _SAL_INTERNAL */
