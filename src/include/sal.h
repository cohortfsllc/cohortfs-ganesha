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

#ifndef _SAL_H
#define _SAL_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif                          /* HAVE_CONFIG_H */ 

#ifdef _SOLARIS
#include "solaris_port.h"
#endif                          /* _SOLARIS */

#include "log_macros.h"
#include "fsal_types.h"

/************************************************************************
 * Data structures filled in by the State Realisitaion * Functions
 *
 * The intent is that these be entirely external representation, not
 * that they constrain the implementation.
 * 
 * It is still unclear to me that filehandle is the right reference in
 * here.  It might be better to key on a cache_inode.  I need to see
 * if those are guaranteed to map one-to-one to a filehandle.  Also,
 * this would expose details of cache_inode to the FSAL.  I'm fine
 * with this and Philippe seemed open to the idea of exposing limited
 * information about cache_inode as part of Cache_Inode++/FSAL++
 ***********************************************************************/

typedef struct __sharestate
{
    fsal_handle_t handle;
    stateid4 stateid;
    open_owner4 open_owner;
    clientid4 clientid;
    uint32_t share_access;
    uint32_t share_deny;
    boolean_t locksheld;
} sharestate;

/*
 * it seems reasonable to me to store the space limit in the state
 * table so that the NFS server could recall delegations if free
 * space decreases to the point where there would be insufficient
 * space to honour all delegations.
 */  

typedef struct __delegationstate
{
    fsal_handle_t handle;
    clientid4 clientid;
    stateid4 stateid;
    open_delegation_type4 type;
    nfs_space_limit4 limit;
} delegationstate;

typedef struct __dir_delegationstate
{
    fsal_handle_t handle;
    clientid4 clientid;
    stateid4 stateid;
    bitmap4 notification_types;
    attr_notice4 child_attr_delay;
    attr_notice4 dir_attr_delay;
    bitmap4 child_attributes;
    bitmap4 dir_attributes;
} dir_delegationstate;

typedef struct __lockstate
{
    fsal_handle_t handle;
    stateid4 open_stateid;
    lock_owner4 lock_owner;
    stateid4 lock_stateid;
    fsal_lock_t* lockdata;
} lockstate;

typedef struct __layoutstate
{
    fsal_handle_t handle;
    clientid4 clientid;
    stateid4 stateid;
    layouttype4 type;
} layoutstate;

typedef struct __layoutsegment
{
    layouttype4 type;
    layoutiomode4 iomode;
    offset4 offset;
    length4 length;
    boolean return_on_close;
    fsal_layout_t* layoutdata;
    uint64_t segid;
} layoutsegment;

typedef enum __ statelocktype
    {readlock, writelock}
    statelocktype;

typedef enum __statetype
  {any=-1, share=0, delegation=1, dir_delegation=2, lock=3, layout=4}
    statetype;

#define NUMSTATETYPES=5;

typedef struct __taggedstate
{
    statetype tag;
    union
    {
	struct sharestate share;
	struct delegationstate delegation;
	struct dir_delegationstate dir_delegation;
	struct lockstate lock;
	struct layoutstate layout;
    } u;
} taggedstate;


/************************************************************************
 * Functions
 *
 * These are the functions that must be implemented by the State
 * realisation.
 ***********************************************************************/

/*
 * Any function that adds, deletes, or modifies state may return
 * ERR_STATE_NOMUTATE if the current model does not allow changing
 * state.  Examples would include a pNFS DS client with no MDS
 * support.
 */

/* Share */

/*
 * This function will record share state for a given file, yielding a
 * stateid whose seqid is 1.  If a share already exists for the
 * (filehandle, openowner) pair, an error is returned.  If a
 * conflicting share state exists, an error is returned.
 */
 
int state_create_share(fsal_handle_t *handle, open_owner4 open_owner,
		       clientid4 clientid, uint32_t share_access,
		       uint32_t share_deny, stateid4* stateid);
/*
 * This function changes the share/deny flags associated with the file
 * to the flags provided.  If the stateid is not a valid stateid from a
 * previous open, an error is returned.  if the updated flags would
 * conflict with other shares, an error is returned.  If the seqid is
 * invalid, an error is returned.  If the call succeeds, the seqid is
 * incremented.
 */   

int state_upgrade_share(uint32_t share_access, uint32_t share_deny,
			stateid4* stateid);

int state_downgrade_share(uint32_t share_access, uint32_t share_deny,
			  stateid4* stateid);
/*
 * Deletes the given share.  An error is returned if a lock state
 * exists.  An error is returned if no such share state exists.
 */ 

int state_delete_share(stateid4 stateid);

/*
 * Retrieves share state for a given (file, clientid, open_owner)
 * triple.  Returns an error if no such share exists.
 */
  

int state_query_share(fsal_handle_t *handle, clientid4 clientid,
		      open_owner4 open_owner, sharestate* state);

int state_start_32read(fsal_handle_t *handle);
int state_start_32write(fsal_handle_t *handle);
int state_end_32read(fsal_handle_t *handle);
int state_end_32write(fsal_handle_t *handle);

/* Delegations */

/*
 * Adds a delegation, yielding the stateid.  Returns an error in case
 * of conflicting share.
 */

int state_create_delegation(fsal_handle_t *handle, clientid4 clientid,
			    open_delegation_type4 type,
			    nfs_space_limit4 limit, stateid4* stateid);

/* 
 * Deletes the delegation if such exists.  Otherwise, an error is
 * returned.
 */
		            
int state_delete_delegation(stateid4 stateid);

/*
 * Retrieves the delegation for a given (filehandle, clientid)
 * pair, if any.
 */

int state_query_delegation(fsal_handle_t *handle, clientid4 clientid,
			   delegationstate* state);

/*
 * Returns true if delegations of the given type are held on the
 * given filehandle.  Could be called before setattr/open/etc. to
 * trigger recall.  Allows the implementation to provide a faster
 * check.  If it returns true, delegations could be perused with
 * state_iter_by_filehandle.
 */

int state_check_delegation(fsal_handle_t *handle,
			   open_delegation_type4 type);


/* Directory Delegations */

/*
 * Adds a directory delegation.  (It appears no states conflict with
 * directory delegations, but operations do.)
 */

int state_create_dir_delegation(fsal_handle_t *handle, clientid4 clientid,
				bitmap4 notification_types,
				attr_notice4 child_attr_delay,
				attr_notice4 dir_attr_delay,
				bitmap4 child_attributes,
				bitmap4 dir_attributes,
				stateid4* stateid);

/*
 * Deletes the delegation.  An error is returned if it does not exist.
 */ 
		            
int state_delete_dir_delegation(stateid4 stateid);

/*
 * Retrieves the delegation for a given (filehandle, clientid)
 * pair, if any.
 */

int state_query_dir_delegation(fsal_handle_t *handle,
			       clientid4 clientid,
			       dir_delegationstate* state);

/*
 * Returns true if a directory delegation exists on the given
 * directory.
 */

int state_check_dir_delegation(fsal_handle_t *handle);

/* Locks */

/*
 * Creates a new lock state for an open file with no existing locks.
 * If there is a conflict, an error is returned.  If there is a
 * pre-existing lock state, an error is returned.  If there is no
 * corresponding open for open_stateid, an error is returned.
 */ 

int state_create_lock_state(fsal_handle_t *handle,
			    stateid4 open_stateid,
			    lock_owner4 lock_owner,
			    fsal_lock_t* lockdata,
			    stateid4* stateid);

/*
 * Deletes the entire state associated with the give file lock.
 */

int state_delete_lock_state(stateid4 stateid);

/* Returns the state for a given lock */

int state_query_lock_state(fsal_handle_t *handle,
			   stateid4 open_stateid,
			   lock_owner4 lock_owner,
			   lockstate* lockstateout);


/* Increment the lock seqid (after performing some lock operations
   inthe FSAL */

int state_inc_lock_state(stateid4* stateid);

/* Retrieves the layout state associated with the
   client/filehandle/type */

int state_create_layout_state(fsal_handle_t handle,
			      stateid4 ostateid,
			      layouttype4 type,
			      stateid4* stateid);

/*
 * Deletes the entire layout state.
 */

int state_delete_layout_state(stateid stateid);

/*
 * Adds a new layout to the layout state for the file.  As the semantics
 * of differing layout types are wildly varying, no attempt is made to
 * check for conflicts.
 */

int state_add_layout_segment(layoutimode4 iomode,
			     offset4 offset,
			     length4 length,
			     boolean return_on_close,
			     fsal_layout_t* layoutdata,
			     stateid4 stateid);
/*
 * Changes a layout segment
 */

int state_mod_layout_segment(layoutimode4 iomode,
			     offset4 offset,
			     length4 length,
			     fsal_layout_t* layoutdata,
			     stateid4 stateid,
			     uint64_t segid);
/*
 * Frees a layout or sublayout.  Should be called after whatever call is
 * necessary to free resources.
 */

int state_free_layout_segment(stateid4 stateid,
			      uint64_t segid);

/* Increment the lock seqid (after performing some lock operations
   inthe FSAL */

int state_layout_inc_state(stateid4* stateid);

/*
 * Iterates through the layouts on a file.  &stateid may be an open
 * stateid or a layout stateid.  If there is no layout state, an error
 * is returned.  cookie should be set to 0 on first call.  It will be
 * updated on success so a subsequent call retrieves the next value.
 */

int state_iter_layout_entries(stateid4 stateid,
			      uint64_t* cookie,
			      boolean* finished,
			      layoutsegment* segment);

/* General Use */

/*
 * Locks the filehandle for reading or writing state.
 */

int state_lock_filehandle(fsal_handle_t *handle, statelocktype rw);

/*
 * Unlocks the filehandle.
 */

int state_unlock_filehandle(fsal_handle_t *handle);

/*
 * Fills in state progressively with all states existing on a
 * particular filehandle, regardless of owner.  type may be set to any
 * to iterate through all states, or restricted to a particular type.
 */

int state_iterate_by_filehandle(fsal_handle_t *handle, statetype type,
				uint64_t* cookie, boolean* finished,
				taggedstate* state);

/*
 * Fills in state progressively with all states associated with a
 * given clientid.
 */

int state_iterate_by_clientid(clientid4 clientid, statetype type,
			      uint64_t* cookie, boolean* finished,
			      taggedstate* state);

/* Retrieves a state by stateid */

int state_retrieve_state(stateid4 stateid, taggedstate* state);

/* Initialise state realisation */

int state_init(void);

/* Finish state realisation */

int state_shutdown(void);

/* Function to load library module if shared */

int state_loadlibrary(char* path);

/* Error return codes */

uint32_t staterr2nfs4err(uint32_t staterr);

static family_error_t __attribute__ ((__unused__)) tab_errstatus_SAL[] =
{
#define ERR_STATE_NO_ERROR 0
  {
  ERR_STATE_NO_ERROR, "ERR_STATE_NO_ERROR", "No error"},
#define ERR_STATE_CONFLICT 1
  {
  ERR_STATE_CONFLICT, "ERR_STATE_CONFLICT", "Attempt to insert conflicting state"},
#define ERR_STATE_LOCKSHELD 2
  {
  ERR_STATE_LOCKSHELD, "ERR_STATE_LOCKSHELD", "Attempt to close file while locks held"},
#define ERR_STATE_OLDSEQ 3
  {
  ERR_STATE_OLDSEQ, "ERR_STATE_OLDSEQ", "Supplied seqid out of date."},
#define ERR_STATE_BADSEQ 4
  {
  ERR_STATE_BADSEQ, "ERR_STATE_BADSEQ", "Supplied seqid too high."},
#define ERR_STATE_STALE 5
  {
  ERR_STATE_STALE, "ERR_STATE_STALE", "Stale stateid."},
#define ERR_STATE_BAD 6
  {
  ERR_STATE_STALE, "ERR_STATE_BAD", "Bad stateid."},
#define ERR_STATE_NOENT 7
  {
  ERR_STATE_NOENT, "ERR_STATE_NOENT", "No such stateid."},
#define ERR_STATE_NOMUTATE 8
  {
  ERR_STATE_NOTSUPP, "ERR_STATE_NOMUTATE", "The current state realisation does not support mutation."},
#define ERR_STATE_PREEXISTS 9
  {
  ERR_STATE_PREEXISTS, "ERR_STATE_PREEXISTS", "Attempt to create a state of a type that already exists."},
#define ERR_STATE_FAIL 10
  {
  ERR_STATE_FAIL, "ERR_STATE_FAIL", "Unspecified, internal error."}
#define ERR_STATE_OBJTYPE 11
  {
  ERR_STATE_OBJTYPE, "ERR_STATE_OBJTYPE", "Operation is undefined or not permitted for the type of object specified."}
};

/* Function pointer structures */

typedef struct __sal_functions
{
  int (*state_create_share)(fsal_handle_t *handle, open_owner4 open_owner,
			    clientid4 clientid, uint32_t share_access,
			    uint32_t share_deny, stateid4* stateid);
  int (*state_upgrade_share)(uint32_t share_access, uint32_t share_deny,
			     stateid4* stateid);
  int (*state_downgrade_share)(uint32_t share_access, uint32_t share_deny,
			       stateid4* stateid);
  int (*state_delete_share)(stateid4 stateid);
  
  int (*state_query_share)(fsal_handle_t *handle, clientid4 clientid,
			   open_owner4 open_owner, sharestate* state);
  int (*state_start_32read)(fsal_handle_t *handle);
  int (*state_start_32write)(fsal_handle_t *handle);
  int (*state_end_32read)(fsal_handle_t *handle);
  int (*state_end_32write)(fsal_handle_t *handle);
  int (*state_create_delegation)(fsal_handle_t *handle, clientid4 clientid,
				 open_delegation_type4 type,
				 nfs_space_limit4 limit, stateid4* stateid);
  int (*state_delete_delegation)(stateid4 stateid);
  int (*state_query_delegation)(fsal_handle_t *handle, clientid4 clientid,
				delegationstate* state);
  int (*state_check_delegation)(fsal_handle_t *handle,
				open_delegation_type4 type);
  int (*state_create_dir_delegation)(fsal_handle_t *handle, clientid4 clientid,
				     bitmap4 notification_types,
				     attr_notice4 child_attr_delay,
				     attr_notice4 dir_attr_delay,
				     bitmap4 child_attributes,
				     bitmap4 dir_attributes,
				     stateid4* stateid);
  int (*state_delete_dir_delegation)(stateid4 stateid);
  int (*state_query_dir_delegation)(fsal_handle_t *handle,
				    clientid4 clientid,
				    dir_delegationstate* state);
  int (*state_check_dir_delegation)(fsal_handle_t *handle);
  int (*state_create_lock_state)(fsal_handle_t *handle,
				 stateid4 open_stateid,
				 lock_owner4 lock_owner,
				 fsal_lock_t* lockdata,
				 stateid4* stateid);
  int (*state_delete_lock_state)(stateid4 stateid);
  int (*state_query_lock_state)(fsal_handle_t *handle,
				stateid4 open_stateid,
				lock_owner4 lock_owner,
				lockstate* lockstateout);
  int (*state_inc_lock_state)(stateid4* stateid);
  int (*state_create_layout_state)(fsal_handle_t handle,
				   stateid4 ostateid,
				   layouttype4 type,
				   stateid4* stateid);
  int (*state_delete_layout_state)(stateid stateid);
  int (*state_add_layout_segment)(layoutimode4 iomode,
				  offset4 offset,
				  length4 length,
				  boolean return_on_close,
				  fsal_layout_t* layoutdata,
				  stateid4 stateid);
  int (*state_mod_layout_segment)(layoutimode4 iomode,
				  offset4 offset,
				  length4 length,
				  fsal_layout_t* layoutdata,
				  stateid4 stateid,
				  uint64_t segid);
  int (*state_free_layout_segment)(stateid4 stateid,
				   uint64_t segid);
  int (*state_layout_inc_state)(stateid4* stateid);
  int state_iter_layout_entries(stateid4 stateid,
				uint64_t* cookie,
				boolean* finished,
				layoutsegment* segment);
  int (*state_lock_filehandle)(fsal_handle_t handle, statelocktype rw);
  int (*state_unlock_filehandle)(fsal_handle_t handle);
  int (*state_iterate_by_filehandle)(fsal_handle_t handle, statetype type,
				     uint64_t* cookie, boolean* finished,
				     taggedstate* state);
  int (*state_iterate_by_clientid)(clientid4 clientid, statetype type,
				   uint64_t* cookie, boolean* finished,
				   taggedstate* state);
  int (*state_init)(void);
  int (*state_shutdown)(void);
  int (*state_retrieve_state)(stateid4 stateid, taggedstate* state);
} sal_functions_t;
  
#endif                          /* _SAL_H */
