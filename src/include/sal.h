/**
 *
 * \file sal.h
 * \author Adam C. Emerson 
 * \brief State Abstraction Layer definitions
 *
 * @section LICENSE
 *
 * Copyright (C) 2010, The Linux Box, inc.
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
 * \section DESCRIPTION
 *
 * This file contains the public data structures and function
 * prototypes that make up the State Abstraction Layer.  Functions in
 * any given State Realisation will convert between these types and
 * their own internal representations.
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
#include "cache_inode.h"
#include "nfs4.h"
#include "nfs_core.h"

/**
 * \section TYPES
 *
 * These types exist purely as a public abstraction and are not
 * intended to constrain implementations of the SAL.  They were
 * designed witht he assumptions of what I would want in implementing
 * Cache_Inode operations.  Functions in the realisation will convert
 * between these and their own internal formats.
 */

/* These defines are for compatibility between NFSv4.0 and NFSv4.1 */

#ifdef _USE_NFS4_0
typedef open_owner4 state_owner4;
#define NFS4_UINT32_MAX 0xffffffff
#endif

extern stateid4 state_anonymous_stateid;
extern stateid4 state_bypass_stateid;
extern stateid4 state_current_stateid;
extern stateid4 state_invalid_stateid;

/**
 * \typedef sharestate
 *
 * \brief Represents NFS share state
 *
 */

typedef struct __sharestate
{
    fsal_handle_t handle; /*!< The filehandle that is recorded as
			       opened. */
    clientid4 clientid; /*!< The clientid. */
    open_owner4 open_owner; /*!< The open owner.  (filehandle,
			         clientid, open_owner) completely
				 specifies the share state. */
    stateid4 stateid; /*!< The relevant stateid */
    uint32_t share_access; /*!< The open mode */
    uint32_t share_deny; /*!< Open modes other people can't have */
    bool_t locksheld; /*!< Whether any locks are held on the open
			   file. */
    cache_inode_openref_t* openref; /*!< A pointer to a reference
				         counted FSAL file descriptor
					 (with the appropriate
					 context.) */
} sharestate;

/**
 * \typedef delegationstate
 *
 * \brief Represents NFS delegation state
 * 
 */

typedef struct __delegationstate
{
    fsal_handle_t handle; /*!< The filehandle the state is associated
			       with. */
    clientid4 clientid; /*!< The clientid. (handle, clientid)
			     completely describes the delegation
			     state. */
    stateid4 stateid; /*!< The relevant stateid */
    open_delegation_type4 type; /*!< Specifies a read or write
				     delegation */
    nfs_space_limit4 limit; /*!< Space limit imposed on the client (so
			         the server can be sure it isn't
				 overcommitted.) */
} delegationstate;

#ifdef _USE_NFS4_1

/**
 * \typedef dir_delegationstate
 *
 * \brief Represents NFS directory delegation state
 */

typedef struct __dir_delegationstate
{
    fsal_handle_t handle; /*!< The filehandle that is recorded as
			       opened. */
    clientid4 clientid; /*!< The clientid. (handle, clientid)
			     completely describes the delegation
			     state. */
    stateid4 stateid; /*!< The relevant stateid */
    bitmap4 notification_types; /*!< Notifications for which the
				     client has registered interest. */
    attr_notice4 child_attr_delay; /*!<  Acceptable delay in being
				         notified of changes in
				         attributes of files in the
				         directory. */
    attr_notice4 dir_attr_delay; /*!< Acceptable delay in being
				      notified of changes in
				      attributes of the directory
				      itself. */
    bitmap4 child_attributes; /*!< Attributes of files in the
				   directory about which the client
				   cares. */
    bitmap4 dir_attributes; /*!< Attributes of the directory itself
			         about which the client cares. */
} dir_delegationstate;

#endif

/**
 * \typedef lockstate
 *
 * \brief Represents NFS lock state

 */

typedef struct __lockstate
{
    fsal_handle_t handle; /*!<  The filehandle associated witht he
			        state */
    clientid4 clientid; /*!< The clientid. */
    stateid4 open_stateid; /*!< The stateid of the associated open. */
    lock_owner4 lock_owner; /*!< The lock owner. (handle, clientid,
			         open_stateid, lock_owner) completely
				 identifies the lock state. */
    stateid4 stateid; /*!< The relevant stateid */
    fsal_lockdesc_t* lockdata; /*!< FSAL datum completely describing
				    all lock state appropriate to the
				    file. */
} lockstate;

#ifdef _USE_FSALMDS

/**
 * \typedef layoutstate
 *
 * \brief Represents the set of all layouts on a file.
 */

typedef struct __layoutstate
{
    fsal_handle_t handle; /*!< The filehandle associated with the
			       layout. */
    clientid4 clientid; /*!< The clientid. */
    stateid4 stateid; /*!< The types of layout segments
			   referenced. (handle, clientid, type)
			   completely identifies the state. */
    layouttype4 type; /*!< The relevant stateid */
} layoutstate;

/**
 * \typedef layoutsegment
 *
 * \brief Represents an individual layout4
 *
 * These are returned by functions that iterate individual layout
 * segments given a layout state.
 */

typedef struct __layoutsegment
{
    layouttype4 type; /*!< The layout type */
    layoutiomode4 iomode; /*!< Read or readwrite IO */
    offset4 offset; /*!< The start of the specified range */
    length4 length; /*!< The length of the specified range */
    bool_t return_on_close; /*!< Whether the layout should be returned
			         on file close. */
    fsal_layoutdata_t* layoutdata; /*!< FSAL specific data associated with
				    the layout */
    uint64_t segid; /*!< An opaque 64 bit value the SAL uses to
		         identify the layout. */
} layoutsegment;

#endif

/**
 * \typedef statelocktype
 * \brief Type of lock requested
 *
 * Specifies the type of lock we wish to have on a filehandle's
 * state.
 */

typedef enum __statelocktype
{
    readlock, /*!< We want to read */
    writelock /*!< We want to write */
} statelocktype;

/**
 * \typedef statetype
 *
 * \brief Type of state specified
 *
 * The type of state contained in a taggedstate structure or the type
 * being searched for in one of the iter calls.
 */

typedef enum __statetype
{
    STATE_ANY=-1, /*!< Specifies that we are searching for any state.
		       Should never be returned in a taggedstate
		       structure. */
    STATE_SHARE=0, /*!< A share */
    STATE_DELEGATION=1, /*!< A delegation */
    STATE_DIR_DELEGATION=2, /*!< A directory delegation */
    STATE_LOCK=3, /*!< A set of locks */
    STATE_LAYOUT=4 /*!< A set of layouts */
} statetype;

/**
 * \def NUMSTATETYPES
 * \brief The number of state types we support
 *
 * One more than the highest.  Someone increment this if NFSv4.2
 * implements something new.
 */

#define NUMSTATETYPES 5;

/**
 *
 * \typedef taggedstate
 * \brief Holds a type tag and a union
 */

typedef struct __taggedstate
{
    statetype tag; /*!< Type tag */
    union
    {
	sharestate share; /*!< The share */
	delegationstate delegation; /*!< The delegation */
#ifdef _USE_NFS4_1
	dir_delegationstate dir_delegation; /*!< The directory
					         delegation */
#endif
	lockstate lock; /*!< The lock state */
#ifdef _USE_FSALMDS
	layoutstate layout; /*!< The collection of layouts */
#endif
    } u; /*!< The encpasualted state */
} taggedstate;


/**
 * \section FUNCTIONS
 *
 * These are the functions that must be implemented by the State
 * Realisation.
 *
 * Any function that adds, deletes, or modifies state may return
 * ERR_STATE_NOMUTATE if the current model does not allow changing
 * state.  Examples would include a pNFS DS client with no MDS
 * support.  Any function may return ERR_STATE_FAIL, indicating some
 * likely catastrophic condition.  In the event of ERR_STATE_FAIL
 * state may be inconsistent.  Any function taking a stateid may fail
 * with ERR_STATE_BAD, ERR_STATE_STALE, ERR_STATE_NOENT,
 * ERR_STATE_BADSEQ, or ERR_STATE_OLDSEQ.
 */

/**
 * \subsection SHARE FUNCTIONS
 *
 * These functions all operate on shares.  Along with the
 * specified errors, they may return STATE_ERR_OBJTYPE if the
 * filehandle passed does not represent a regualr file.
 */

/**
 * state_create_share: Records an open share
 *
 * \param handle (in)
 *        The filehandle to be marked open
 * \param open_owner (in)
 *        The owner of the given open
 * \param clientid (in)
 *        The clientid
 * \param share_access (in)
 *        The access requested
 * \param share_deny (in)
 *        The access we want to deny others
 * \param openref (in)
 *        The reference counted file descriptor
 * \param stateid (out)
 *        On success, a new stateid whose seqid is 1.
 *
 * This function records the given share state on the given file,
 * assuming no conflicts.
 *
 * \retval ERR_STATE_NO_ERROR on success
 * \retval ERR_STATE_PREEXISTS if a share state already exists for the
 *         (filehandle, clientid, open_owner) triple.
 * \retval ERR_STATE_CONFLICT if the requested share would conflict
 *         with existing shares or delegations.  (No open may be
 *         performed if a write delegation is outstanding, no open
 *         requesting write or denying read may be performed while a
 *         read delegation is outstanding.  No DENY state will be
 *         granted conflicting with an anonymous read in progress.)
 */
 
int state_create_share(fsal_handle_t *handle, open_owner4 open_owner,
		       clientid4 clientid, uint32_t share_access,
		       uint32_t share_deny,
		       cache_inode_openref_t* openref,
		       stateid4* stateid);

/**
 * state_check_share: Check for possible share conflict
 *
 * \param handle (in)
 *        Handle for the file in question
 * \param share_access (in)
 *        The access being (theoretically) requested
 * \param share_deny (in)
 *        The access being (theoretically) denied others
 *
 * This function evalautes the open as if it were about to be
 * performed and responds appropriately.
 *
 * \retval STATE_ERR_NO_ERROR On success
 * \retval STATE_ERR_CONFLICT If the open would fail due to a
 *         conflict.
 */

int state_check_share(fsal_handle_t handle, uint32_t share_access,
		      uint32_t share_deny);


/**
 * state_upgrade_share: Increases the share/deny access associated with an open
 * \param share_access (in)
 *        Share access requested, must be a superset of the currently
 *        held access
 * \param share_deny (in)
 *        Share deny requested, must be a superset of the currenly
 *        held access.
 * \param stateid (in/out)
 *        The stateid for the given share.  its seqid is incremented
 *        on success.
 *
 * This function increases the share level specified by the current
 * state to the given levels.  Both access and deny must be supersets
 * of the currently held access.  On success stateid->seqid is
 * incremented.
 *
 * \retval ERR_STATE_NO_ERROR Success
 * \retval ERR_STATE_CONFLICT The requested access conflicts with
 *         currently existing shares/delegations.
 * \retval ERR_STATE_INVAL If the given share/access values are not
 *         supersets of currently held access.
 */

int state_upgrade_share(uint32_t share_access, uint32_t share_deny,
			stateid4* stateid);

/**
 * state_downgrade_share: Decrease the share/deny access associated with an open
 * \param share_access (in)
 *        Share access requested, must be a subset of the currently
 *        held access
 * \param share_deny (in)
 *        Share deny requested, must be a subset of the currenly
 *        held access.
 * \param stateid (in/out)
 *        The stateid for the given share.  its seqid is incremented
 *        on success.
 *
 * This function decreases the share level specified by the current
 * state to the given levels.  share_access and share_deny must be
 * subsets of their current values.  On success, stateid->seqid is
 * incremented.  No conflicts are possible.
 *
 * \retval ERR_STATE_NO_ERROR Success
 * \retval ERR_STATE_INVAL If the given share/access values are not
 *         subsets of currently held access.
 */


int state_downgrade_share(uint32_t share_access, uint32_t share_deny,
			  stateid4* stateid);

/**
 * state_delete_share: Deletes the given share state
 * \param stateid (in)
 *        The stateid for the desired state
 *
 * Deletes the given share state (i.e. records file close.)  No locks
 * must be held on the file.
 *
 * \retval ERR_STATE_NO_ERROR Success
 * \retval ERR_STATE_LOCKSHELD Attempt to delete share state while
 *                             locks held.
 */

int state_delete_share(stateid4 stateid);

/**
 * state_query_suare: Look up a share
 * \param handle (in)
 *        File handle to look up
 * \param clientid (in)
 *        clientid to look up
 * \param open_owner (in)
 *        open owner to look up
 * \param state (out)
 *        Poitner to a sharestate object.
 *
 * The (handle, clientid, open_owner) triple uniquely identifies each
 * share state.  If a share state is found, it is copied out to state.
 *
 * \retval ERR_STATE_NO_ERROR Success
 * \retval ERR_STATE_NOENT No such state exists
 */

int state_query_share(fsal_handle_t *handle, clientid4 clientid,
		      open_owner4 open_owner, sharestate* state);
/**
 * state_start_32read: Start a read with no open
 * \param handle (in)
 *        The file to read
 *
 * This function marks the beginning of a read with no associated
 * open.  The existence of a write delegation or a read deny share
 * conflicts.  This function is intended for use with NFS2 and NFS3.
 * it may also be used to implement the anonymous read/write special
 * stateid.  No checking of mandatory locks is performed.
 *
 * \retval ERR_STATE_NO_ERROR Success
 * \retval ERR_STATE_CONFLICT The read could not be initiated, due to
 *         a conflict
 */

int state_start_32read(fsal_handle_t *handle);

/**
 * state_start_32write: Starts a write with no open
 * \param handle (in)
 *        The file to write
 *
 * This function marks the beginning of a write with no associated
 * open.  The existence of a delegation or a write deny share
 * conflicts.  This function is intended for use with NFS2 and NFS3.
 * it may also be used to implement the anonymous read/write special
 * stateid.  No checking of mandatory locks is performed.
 *
 * \retval ERR_STATE_NO_ERROR Success
 * \retval ERR_STATE_CONFLICT Existing state conflicted with the
 *         request.
 */

int state_start_32write(fsal_handle_t *handle);

/**
 * state_end_32read: Ends an anonymous read
 * \param handle (in)
 *        The file having been read
 *
 * This function marks the end of a previously begun read.  No
 * conflicts are possible.
 *
 * \retval ERR_STATE_NO_ERROR Success.
 */

int state_end_32read(fsal_handle_t *handle);

/**
 * state_end_32write: Ends an anonymous read
 * \param handle (in)
 *        The file having been read
 *
 * This function marks the end of a previously begun write.  No
 * conflicts are possible.
 *
 * \retval ERR_STATE_NO_ERROR Success.
 */

int state_end_32write(fsal_handle_t *handle);

/**
 * \subsection DELEGATION FUNCTIONS
 *
 * These functions all operate on delegations.  Along with the
 * specified errors, they may return STATE_ERR_OBJTYPE if the
 * filehandle passed does not represent a regualr file
 */

/**
 * state_create_delegation: Creates a delegation
 * \param handle (out)
 *        Handle to delegate
 * \param clientid (out)
 *        Client it's delegated to
 * \param type (out)
 *        Delegation type (read/write)
 * \param limit (in)
 *        Space limit
 * \param stateid (out)
 *
 * Adds a delegation, if no conflicts, and yields a stateid.  Read
 * delegations may not be granted if the file is open for writing,
 * an anonymous write is in progress, or a write delegation is
 * outstanding.
 *
 * \retval ERR_STATE_NO_ERROR Success
 * \retval ERR_STATE_CONFLICT The delegation conflicted with existing
 *                            state. 
 */

int state_create_delegation(fsal_handle_t *handle, clientid4 clientid,
			    open_delegation_type4 type,
			    nfs_space_limit4 limit, stateid4* stateid);

/**
 * state_delete_delegation: Deletes a delegation
 * \param stateid (in)
 *
 * Deletes the delegation, if such delegation exists.
 *
 * \retval ERR_STATE_NO_ERROR Success
 * \retval ERR_STATE_NOENT No such stateid
 */
		            
int state_delete_delegation(stateid4 stateid);

/**
 * state_query_delegation: Looks up a delegation
 * \param handle (in)
 *        Handle of the given object
 * \param clientid (in)
 *        Client of interest
 * \param state (out)
 *        Pointer to a delegationstate structure to be filled in.
 *
 * Looks up a delegation by the unique (handle, clientid) pair
 * identifying it and copies it information into the structure pointed
 * to by state.
 *
 * \retval ERR_STATE_NO_ERROR Success
 * \retval ERR_STATE_NOENT No such delegation
 */

int state_query_delegation(fsal_handle_t *handle, clientid4 clientid,
			   delegationstate* state);

/**
 * state_check_delegation: Check for the existence of a delegation
 * \param handle (in)
 *        The file of interest
 * \param type(in)
 *        The kind of delegation we care about
 *
 * Intended to be a fast check for the existence of an oustanding
 * delegation as part of calls known to trigger recall (setattr
 * changing file size, for example.)
 *
 * \retval ERR_STATE_NO_ERROR No such delegation.
 * \retval ERR_STATE_CONFLICT The delegation exists.
 */

int state_check_delegation(fsal_handle_t *handle,
			   open_delegation_type4 type);

#ifdef _USE_NFS4_1

/**
 * \subsection DIRECTORY DELEGATION FUNCTIONS
 *
 * These functions all operate on directory delegations.  Along with
 * the specified errors, they may return STATE_ERR_OBJTYPE if the
 * filehandle passed does not represent a directory. 
 */

/**
 * state_create_dir_delegation: Create a directory delegation
 * \param handle (in)
 *        The directory of interest
 * \param clientid (in)
 *        The interested client
 * \param notiication_types (in)
 *        Specifies the notifications the client wants to receive.
 * \param child_attr_delay (in)
 *        How long the client is willing to wait to receive updates on
 *        on the attributes of files in the directory.
 * \param dir_attr_delay (in)
 *        How long the client is willing to wait to receive updates on
 *        the directory's attributes.
 * \param child_attributes (in)
 *        Which attributes, set on the files in the directory, the client
 *        cares about.
 * \param dir_attributes (in)
 *        Which attributes, set on the directory, the client cares
 *        about.
 * \param stateid (out)
 *        The id of the newly created state.
 *
 * This function creates a directory delegation.  No conflict checks
 * are performed as directory delegations conflict with no state,
 * however they should not be granted while an operation that would
 * cause their recall is in progress.
 *
 * \retval ERR_STATE_NO_ERROR Success
 */

int state_create_dir_delegation(fsal_handle_t *handle, clientid4 clientid,
				bitmap4 notification_types,
				attr_notice4 child_attr_delay,
				attr_notice4 dir_attr_delay,
				bitmap4 child_attributes,
				bitmap4 dir_attributes,
				stateid4* stateid);

/**
 * state_delete_dir_delegation: Delete a directory delegation
 * \param stateid (in)
 *        Stateid of the delegation to be deleted
 *
 * Deletes a directory delegation, if it exists.
 *
 * \retval ERR_STATE_NO_ERROR Success
 * \retval ERR_STATE_NOENT No such delegation
 */
		            
int state_delete_dir_delegation(stateid4 stateid);

/**
 * state_query_dir_delegation: Return a directory delegation
 * \param handle (in)
 *        The handle for the directory
 * \param clientid (in)
 *        The clientid
 * \param state (out)
 *        Pointer to a dir_delegationstate structure
 *
 * Fills the area pointed to by state with information on the
 * directory delegation specified by the (handle, clientid) pair, if
 * such a directory delegation exists.
 *
 * \retval ERR_STATE_NO_ERROR Success
 * \retval ERR_STATE_NOENT No such delegation
 */

int state_query_dir_delegation(fsal_handle_t *handle,
			       clientid4 clientid,
			       dir_delegationstate* state);

/**
 * state_check_dir_delegations: Check whether any delegations exist on the given directory
 * \param handle
 *        The handle in question
 *
 * Intended to quickly check for the presence of directory delegations
 * before operations that may cause notification or recall.
 *
 * \retval ERR_STATE_NO_ERROR No such delegation exists
 * \retval ERR_STATE_CONFLICT At least one directory delegation
 *         exists.
 */ 

int state_check_dir_delegation(fsal_handle_t *handle);

#endif

/**
 * \subsection LOCKS
 *
 * These functions associate an FSAL or lock manager dependent datum
 * giving the set of all locks on a file with a stateid and a
 * (filehandle, clientid, open-stateid, lock_owner) tuple.
 */

/**
 * state_create_lock_state: Associates an FSAL lock datum with NFSv4.1 state
 * \param handle (in)
 *        The handle in question
 * \param open_stateid (in)
 *        The stateid returned from open, associates the lock_owner with
 *        the open_owner.
 * \param lock_owner (in)
 *        An opaque value, specifies the owner of the locks.
 * \param clientid (in)
 *        The client
 * \param lockdata (in)
 *        The FSAL or lock-manager datum holding actual locks
 * \param stateid (out)
 *        The ID for the lock state.
 *
 * This function does no checking for conflicts, leaving that to the
 * lock manager
 *
 * \retval ERR_STATE_NO_ERRO SUCCESS
 */ 

int state_create_lock_state(fsal_handle_t *handle,
			    stateid4 open_stateid,
			    lock_owner4 lock_owner,
			    clientid4 clientid,
			    fsal_lockdesc_t* lockdata,
			    stateid4* stateid);

/**
 * state_delete_lock_state: Delete the state associated with a set of locks
 * \param stateid
 *        Stateid of the given set of locks
 *
 * The caller must take responsibility for freeing all locks held
 * through the FSAL or lock manager before calling this function
 *
 * \retval ERR_STATE_NO_ERROR Success
 */

int state_delete_lock_state(stateid4 stateid);

/**
 * state_query_lock_state: Loooks up lock state information
 * \param handle (in)
 *        The handle in question
 * \param open_stateid (in)
 *        The stateid returned from open, associates the lock_owner with
 *        the open_owner.
 * \param lock_owner (in)
 *        An opaque value, specifies the owner of the locks.
 * \param clientid (in)
 *        The client
 * \param outlockstate (out)
 *        Pointer to a structure to which information will be written.
 *
 * This function fills outlockstate with the information appropriate to
 * the state identified by the tuple (handle, clientid, open_stateid,
 * lock_owner).
 *
 * \retval ERR_STATE_NO_ERRO Success
 * \retval ERR_STATE_NOENT No such state exists
 */ 


int state_query_lock_state(fsal_handle_t *handle,
			   stateid4 open_stateid,
			   lock_owner4 lock_owner,
			   clientid4 clientid,
			   lockstate* outlockstate);

/**
 * state_inc_lock_state: Increments a stateid.seqid
 * \param stateid
 *        The stateid in question
 *
 * This function should be called after the successful comlpetion of a
 * sequence of one or more FSAL or lock manager operations constituting
 * one NFS lock-related operation.  This will generally take place in
 * Cache_Inode.
 *
 * \retval ERR_STATE_NO_ERROR Success
 */

int state_inc_lock_state(stateid4* stateid);

#ifdef _USE_FSALMDS

/**
 * state_create_layout_state: Creates a new state for layouts
 * \param handle (in)
 *        The filehandle
 * \param ostateid (in)
 *        The stateid for a current share
 * \param clientid (in)
 *        The client
 * \param type (in)
 *        Layout type
 * \param stateid (out)
 *        The layout state id
 *
 * This creates a state to reference all layout segments of a given
 * type for a given client on a given file.  it adds no segments,
 * itself.
 * \retval ERR_STATE_NO_ERROR Success
 */

int state_create_layout_state(fsal_handle_t* handle,
			      stateid4 ostateid,
			      clientid4 clientid,
			      layouttype4 type,
			      stateid4* stateid);

/**
 * state_delete_layout_state: Deletes a set of layouts
 * \param stateid
 *        State to delete
 *
 * This deletes the layout state and all segments
 *
 * \retval ERR_STATE_NO_ERROR Success
 */

int state_delete_layout_state(stateid4 stateid);

/**
 * state_query_layout_state: Looks up layout state information
 * \param handle (in)
 *        The handle in question
 * \param clientid (in)
 *        The client
 * \param type (in)
 *        The layout type
 * \param outlayoutstate (out)
 *        Pointer to a structure to which information will be written.
 *
 * This function fills outlockstate with the information appropriate to
 * the state identified by the tuple (handle, clientid, open_stateid,
 * lock_owner).
 *
 * \retval ERR_STATE_NO_ERRO Success
 * \retval ERR_STATE_NOENT No such state exists
 */ 

int state_query_layout_state(fsal_handle_t *handle,
			     clientid4 clientid,
			     layouttype4 type,
			     layoutstate* outlayoutstate);

/**
 * state_add_layout_segment: Add a new layout segment
 * \param type (in)
 *        Layout type
 * \param iomode (in)
 *        Read/write IO access
 * \param offset (in)
 *        Start
 * \param length (in)
 *        Length
 * \param return_on_close (in)
 *        Whether the layout range must be returned when the file is
 *        closed.
 * \param layoutdata (in)
 *        FSAL specific data related to the layout
 * \param stateid (in)
 *        The layout state id
 *
 * This adds a new segment to the set identified by the stateid.  No
 * conflict detection or other validation is done.
 *
 * \retval ERR_STATE_NO_ERROR Success
 * \retval ERR_STATE_INVAL The supplied type does not match that of
 *                         the state.
 */

int state_add_layout_segment(layouttype4 type,
			     layoutiomode4 iomode,
			     offset4 offset,
			     length4 length,
			     bool_t return_on_close,
			     fsal_layoutdata_t* layoutdata,
			     stateid4 stateid);

/**
 * state_mod_layout_segment: Change a layout segment
 * \param iomode (in)
 *        Read/write IO access
 * \param offset (in)
 *        Start
 * \param length (in)
 *        Length
 * \param layoutdata (in)
 *        FSAL specific data related to the layout
 * \param stateid (in)
 *        The layout state id
 * \param uint64_t (in)
 *        The segment id (returned from iter)
 *
 * This function changes the mode and dimension of a segment.
 *
 * \retval ERR_STATE_NO_ERROR Success
 */
int state_mod_layout_segment(layoutiomode4 iomode,
			     offset4 offset,
			     length4 length,
			     fsal_layoutdata_t* layoutdata,
			     stateid4 stateid,
			     uint64_t segid);
/**
 * state_free_layout_segment: Frees a layout segment
 * \param stateid (in)
 *        The layout stateid
 * \param segid (in)
 *        The segment ID
 *
 * This function frees a layout segment and should be called after the
 * FSAL specific call to free the associated resources.
 *
 * \retval ERR_STATE_NO_ERROR Success
 */

int state_free_layout_segment(stateid4 stateid,
			      uint64_t segid);

/**
 * state_layout_inc_state: Increment layout stateid
 * \param stateid (in/out)
 *
 * Increments the layout stateid.  Should be performed after a
 * collection of FSAL operations corresponding toone layoutget or
 * layoutreturn.
 *
 * \retval ERR_STATE_NO_ERROR Success
 */

int state_layout_inc_state(stateid4* stateid);

/**
 * state_iter_layout_entries: Iterates through all layout segments
 * \param stateid (in)
 *        A layout stateid
 * \param cookie (in/out)
 *        The pointed-to value should be set to zero on first call.
 * \param finished (out)
 *        Set to true when the last segment is reached
 * \param segment (out)
 *        Pointer to a layoutsegment structure, filled in by the
 *        function.
 *
 * This function iterates through all layout segments.  The cookie
 * must be treated as opaque.  Junk values may produce highly
 * undesirable results.
 *
 * \retval ERR_STATE_SUCCESS No error.
 */

int state_iter_layout_entries(stateid4 stateid,
			      uint64_t* cookie,
			      bool_t* finished,
			      layoutsegment* segment);

#endif

/**
 * state_lock_filehandle: Locks the filehandle for read or write
 * \param handle
 *        The handle
 * \param rw
 *        Specifies a read or write lock
 *
 * This function locks a filehandle's state information, such as
 * before a non-atomic operation (performing an FSAL operation then
 * modifying state on success.)
 *
 * \retval ERR_STATE_NO_ERROR Success
 * \retval ERR_STATE_NOENT There is no state information stored for
 *                         this filehandle and a read-lock was
 *                         requested.
 */

int state_lock_filehandle(fsal_handle_t *handle,
			  statelocktype rw);

/**
 * state_unlock_filehandle: Unlocks a filehandle
 * \param handle
 *        The handle in question.
 *
 * Releases the lock (whether read or write)
 *
 * \retval STATE_ERR_NO_ERROR Success
 */

int state_unlock_filehandle(fsal_handle_t *handle);

/**
 * state_iterate_by_filehandle: Iterate through states on a file
 * \param handle (in)
 *        The filehandle in question
 * \param type (in)
 *        The type of state of interest (STATE_ANY for any)
 * \param cookie (in/out)
 *        An opaque cookie, set to 0 on first call
 * \param finished (out)
 *        True on last state
 * \param state (out)
 *        Filled in by the function.
 *
 * This function iterates through all states on a filehandle of a
 * given type.
 *
 * \retval ERR_STATE_NO_ERROR Success
 */

int state_iterate_by_filehandle(fsal_handle_t *handle, statetype type,
				uint64_t* cookie, bool_t* finished,
				taggedstate* state);


/**
 * state_iterate_by_clientid: Iterate through states owned by a given client
 * \param clientid (in)
 *        The clientid in question
 * \param type (in)
 *        The type of state of interest (STATE_ANY for any)
 * \param cookie (in/out)
 *        An opaque cookie, set to 0 on first call
 * \param finished (out)
 *        True on last state
 * \param state (out)
 *        Filled in by the function.
 *
 * This function iterates through all states owned by a given clientid
 * for, e.g. layoutreturn or a lease expiry reaper.
 *
 * \retval ERR_STATE_NO_ERROR Success
 */

int state_iterate_by_clientid(clientid4 clientid, statetype type,
			      uint64_t* cookie, bool_t* finished,
			      taggedstate* state);

/**
 * state_lock_state_owner: Lock/check state_owner for NFSv4.0
 * \param state_owner (in)
 *        NFS open/lock owner
 * \param lock (in)
 *        Whether this is a lock owner or open owner
 * \param seqid (in)
 *        The client-supplied seqid
 * \param new (out)
 *        True if this is a newly seen state owner
 * \param response (out)
 *        Pointer to saved response
 *
 * This function implements NFSv4.0 open and lock owner semantics, it
 * should not be called from any nfs41_op_* function.  (NFSv4.1
 * supports parallel opens.)
 *
 * If the supplied seqid id correct, the state owner is locked and the
 * operation can continue.  if it is identical to the last seqid seen,
 * response is set to a non-NULL value pointing at the saved response
 * of the last operation.
 *
 * If the owner is newly created, the new flag will be set and the
 * open should be confirmed.
 *
 * A successful call to this function must be paired with an unlock.
 *
 * \retval ERR_STATE_NO_ERROR Success
 * \retval ERR_STATE_BADSEQ Bad sequence number
 */

int state_lock_state_owner(state_owner4 state_owner, bool_t lock,
			   seqid4 seqid, bool_t* new,
			   struct nfs_resop4** response);

/**
 * state_unlock_state_owner: Unlock a state owner
 * \param state_owner
 *        The state owner to unlock
 * \param lock
 *        True if this is an open owner, false if it is a lock owner.
 *
 * This function unlocks a state owner and should be called at the end
 * of any NFSv4.0 OPEN or LOCK call that locked it.
 *
 * \retval ERR_STATE_NO_ERROR Success
 */
int state_unlock_state_owner(state_owner4 state_owner, bool_t lock);

/**
 * state_save_response: Saves an NFSv4.0 response
 * \param state_owner (in)
 *        State owner requesting the operation
 * \param lock (in)
 *        Whether this is a lock owner (false for open owner)
 * \param seqid (in)
 *        The current seqid
 * \param seqid (out)
 *        The seqid to reqturn
 * \param response
 *        The NFSv4.0 response to save
 *
 * This function should be called after the NFSv4.0 response has been
 * filled in in for any open, close, or lock call.
 *
 * \retval ERR_STATE_NO_ERROR Success
 */

int state_save_response(state_owner4 state_owner, bool_t lock,
			struct nfs_resop4* response);

/**
 * state_retrieve_state: Look up state by stateid
 * \param stateid (in)
 *        The stateid to look up
 * \param taggedstate (out)
 *        The structure to be filled in with state information.
 *
 * This is the expected general use state query.  It returns a tagged
 * union of all possible state types.
 *
 * \retval ERR_STATE_NO_ERROR Success
 */
 
int state_retrieve_state(stateid4 stateid, taggedstate* state);

/**
 * state_anonymous_check: Check for anonymous state
 * \param stateid (in)
 *
 * Returns true if the supplied stateid is either the anonymous or the
 * read-bypass stateid.
 *
 * \returns true or false as appropriate
 */

bool_t state_anonymous_check(stateid4 stateid);

/**
 * state_anonymous_exact_check: Check for anonymous state
 * \param stateid (in)
 *
 * Returns true if the supplied stateid is the anonymous staateid.
 *
 * \returns true or false as appropriate
 */

bool_t state_anonymous_exact_check(stateid4 stateid);

/**
 * state_anonymous_check: Check for bypass state
 * \param stateid (in)
 *
 * Returns true if the supplied stateid is the read-bypass stateid.
 *
 * \returns true or false as appropriate
 */

bool_t state_bypass_check(stateid4 stateid);

/**
 * state_current_check: Check for current state
 * \param stateid (in)
 *
 * Returns true if the supplied stateid is the special stateid
 * indicating that the current stateid should be used.
 *
 * \returns true or false as appropriate
 */

bool_t state_current_check(stateid4 stateid);

/**
 * state_invalid_check: Check for invalid state
 * \param stateid (in)
 *
 * Returns true if the supplied stateid is the special invalid
 * stateid.
 *
 * \returns true or false as appropriate
 */

bool_t state_invalid_check(stateid4 stateid);

/**
 * state_init: Initialises the state realisation
 *
 * This function is called on Ganesha startup to perform whatever
 * initialisation is required by the state realisation
 */

int state_init(void);

/**
 * state_shutdown: Clean up state management
 *
 * This function is called on a clean shutdown by ganesha to perform
 * whatever cleanup is necessary for the state manager (perhaps
 * distributed state will signal peers of the shutdown, etc.)
 */

int state_shutdown(void);

/* Function to load library module if shared */

int state_loadlibrary(char* path);
void state_loadfunctions(void);

/* Error return codes */

uint32_t staterr2nfs4err(uint32_t staterr);

static family_error_t __attribute__ ((__unused__)) tab_errstatus_SAL[] =
{
#define ERR_STATE_NO_ERROR 0
  {ERR_STATE_NO_ERROR, "ERR_STATE_NO_ERROR", "No error"},
#define ERR_STATE_CONFLICT 1
  {ERR_STATE_CONFLICT, "ERR_STATE_CONFLICT", "Attempt to insert conflicting state"},
#define ERR_STATE_LOCKSHELD 2
  {ERR_STATE_LOCKSHELD, "ERR_STATE_LOCKSHELD", "Attempt to close file while locks held"},
#define ERR_STATE_OLDSEQ 3
  {ERR_STATE_OLDSEQ, "ERR_STATE_OLDSEQ", "Supplied seqid out of date."},
#define ERR_STATE_BADSEQ 4
  {ERR_STATE_BADSEQ, "ERR_STATE_BADSEQ", "Supplied seqid too high."},
#define ERR_STATE_STALE 5
  {ERR_STATE_STALE, "ERR_STATE_STALE", "Stale stateid."},
#define ERR_STATE_BAD 6
  {ERR_STATE_STALE, "ERR_STATE_BAD", "Bad stateid."},
#define ERR_STATE_NOENT 7
  {ERR_STATE_NOENT, "ERR_STATE_NOENT", "No such stateid."},
#define ERR_STATE_NOMUTATE 8
  {ERR_STATE_NOMUTATE, "ERR_STATE_NOMUTATE", "The current state realisation does not support mutation."},
#define ERR_STATE_PREEXISTS 9
  {ERR_STATE_PREEXISTS, "ERR_STATE_PREEXISTS", "Attempt to create a state of a type that already exists for the given identifying information."},
#define ERR_STATE_FAIL 10
  {ERR_STATE_FAIL, "ERR_STATE_FAIL", "Unspecified, internal error."},
#define ERR_STATE_OBJTYPE 11
  {ERR_STATE_OBJTYPE, "ERR_STATE_OBJTYPE", "Operation is undefined or not permitted for the type of object specified."},
#define ERR_STATE_INVAL 12
  {ERR_STATE_INVAL, "ERR_STATE_INVAL", "Invalid operation."}
};

/* Function pointer structures */

typedef struct __sal_functions
{
  int (*state_create_share)(fsal_handle_t *handle, open_owner4 open_owner,
			    clientid4 clientid, uint32_t share_access,
			    uint32_t share_deny,
			    cache_inode_openref_t* openref, stateid4* stateid);
  int (*state_check_share)(fsal_handle_t handle, uint32_t share_access,
			   uint32_t share_deny);
  int (*state_upgrade_share)(uint32_t share_access, uint32_t share_deny,
			     stateid4* stateid);
  int (*state_downgrade_share)(uint32_t share_access, uint32_t share_deny,
			       stateid4* stateid);
  int (*state_delete_share)(stateid4 stateid);
  int (*state_query_share)(fsal_handle_t *handle, clientid4 clientid,
			   open_owner4 open_owner,
			   sharestate* outshare);
  int (*state_start_32read)(fsal_handle_t *handle);
  int (*state_start_32write)(fsal_handle_t *handle);
  int (*state_end_32read)(fsal_handle_t *handle);
  int (*state_end_32write)(fsal_handle_t *handle);
  int (*state_create_delegation)(fsal_handle_t *handle, clientid4 clientid,
				 open_delegation_type4 type,
				 nfs_space_limit4 limit,
				 stateid4* stateid);
  int (*state_delete_delegation)(stateid4 stateid);
  int (*state_query_delegation)(fsal_handle_t *handle, clientid4 clientid,
				delegationstate* outdelegation);
  int (*state_check_delegation)(fsal_handle_t *handle,
				open_delegation_type4 type);
#ifdef _USE_NFS4_1
  int (*state_create_dir_delegation)(fsal_handle_t *handle, clientid4 clientid,
				     bitmap4 notification_types,
				     attr_notice4 child_attr_delay,
				     attr_notice4 dir_attr_delay,
				     bitmap4 child_attributes,
				     bitmap4 dir_attributes,
				     stateid4* stateid);
  int (*state_delete_dir_delegation)(stateid4 stateid);
  int (*state_query_dir_delegation)(fsal_handle_t *handle, clientid4 clientid,
				    dir_delegationstate* outdir_delegation);
#endif
  int (*state_create_lock_state)(fsal_handle_t *handle,
				 stateid4 open_stateid,
				 lock_owner4 lock_owner,
				 clientid4 clientid,
				 fsal_lockdesc_t* lockdata,
				 stateid4* stateid);
  int (*state_delete_lock_state)(stateid4 stateid);
  int (*state_query_lock_state)(fsal_handle_t *handle,
				stateid4 open_stateid,
				lock_owner4 lock_owner,
				clientid4 clientid,
				lockstate* outlockstate);
  int (*state_inc_lock_state)(stateid4* stateid);
  int (*state_lock_inc_state)(stateid4* stateid);
#ifdef _USE_FSALMDS
  int (*state_create_layout_state)(fsal_handle_t* handle,
				   stateid4 ostateid,
				   clientid4 clientid,
				   layouttype4 type,
				   stateid4* stateid);
  int (*state_delete_layout_state)(stateid4 stateid);
  int (*state_query_layout_state)(fsal_handle_t *handle,
				  clientid4 clientid,
				  layouttype4 type,
				  layoutstate* outlayoutstate);
  int (*state_add_layout_segment)(layouttype4 type,
				  layoutiomode4 iomode,
				  offset4 offset,
				  length4 length,
				  bool_t return_on_close,
				  fsal_layoutdata_t* layoutdata,
				  stateid4 stateid);
  int (*state_mod_layout_segment)(layoutiomode4 iomode,
				  offset4 offset,
				  length4 length,
				  fsal_layoutdata_t* layoutdata,
				  stateid4 stateid,
				  uint64_t segid);
  int (*state_free_layout_segment)(stateid4 stateid,
				   uint64_t segid);
  int (*state_layout_inc_state)(stateid4* stateid);
  int (*state_iter_layout_entries)(stateid4 stateid,
				   uint64_t* cookie,
				   bool_t* finished,
				   layoutsegment* segment);
#endif
  int (*state_lock_filehandle)(fsal_handle_t *handle,
			       statelocktype rw);
  int (*state_unlock_filehandle)(fsal_handle_t *handle);
  int (*state_iterate_by_filehandle)(fsal_handle_t *handle, statetype type,
				     uint64_t* cookie, bool_t* finished,
				     taggedstate* outstate);
  int (*state_iterate_by_clientid)(clientid4 clientid, statetype type,
				   uint64_t* cookie, bool_t* finished,
				   taggedstate* outstate);
  int (*state_retrieve_state)(stateid4 stateid,
			      taggedstate* outstate);
  int (*state_lock_state_owner)(state_owner4 state_owner, bool_t lock,
				seqid4 seqid, bool_t* new,
				nfs_resop4** response);
  
  int (*state_unlock_state_owner)(state_owner4 state_owner, bool_t lock);
  
  int (*state_save_response)(state_owner4 state_owner, bool_t lock,
			     nfs_resop4* response);
  int (*state_init)(void);
  int (*state_shutdown)(void);
} sal_functions_t;

#endif                          /* _SAL_H */
