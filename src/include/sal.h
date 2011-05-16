/*
 * Copyright (C) 2010, Linux Box Corporation
 * All Rights Reserved
 *
 * Contributor: Adam C. Emerson
 */

/**
 *
 * \file sal.h
 * \author Adam C. Emerson 
 * \date 11 May 2011
 * \brief State Abstraction Layer definitions
 *
 * \section DESCRIPTION
 *
 * This file contains prototypes of functions provided regardless of
 * SAL realisation used and also those provided by a given
 * realisation.  It also includes definitions of public,
 * realisation-independent data types.
 */

#ifndef _SAL_H
#define _SAL_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef _SOLARIS
#include "solaris_port.h"
#endif /* _SOLARIS */

#include "log_macros.h"
#include "fsal_types.h"
#include "cache_inode.h"
#include "nfs4.h"
#include "nfs_core.h"
#include "SAL/SAL_LOCAL/sal_types.h"

/**
 * \section TYPES
 *
 * These types exist purely as a public abstraction and are not
 * intended to constrain implementations of the SAL.
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
 * \struct state_lockowner_t
 *
 * This structure exists to abstract away the differences between
 * NFSv3 and NFSv4 lock ownership.  It is passed to the FSAL.
 */

typedef struct lockowner__
{
     enum {
	  LOCKOWNER_NFS3, /*!< Lock is owned by an NFSv3 client */
	  LOCKOWNER_NFS4, /*!< Lock is owned by an NFSv4 client */
	  LOCKOWNER_INTERNAL, /*!< Find out who owns the lock later */
	  LOCKOWNER_EXTERNAL /*!< Lock is owned by a client not using
			          NFS */
     } owner_type; /*!< This controls interpretation of the union */
     union {
	  void* nfs3_owner; /*!< NFSv3 owner information, to be filled
			         in later */
	  struct {
	       clientid4 clientid; /*!< Clientid of owning client */
	       char owner_val[1024]; /*!< Owner opaque.  Probably
				       better to dynamically allocate
				       this than use the whole 1024
				       bytes maximum allowed per
				       owner */
	       size_t owner_len; /*!< The owner string */
	  } nfs4_owner; /*!< NFSv4 Owner information */
     } u;
} state_lock_owner_t;


/**
 * \typedef statetype
 *
 * \brief An identifier giving the type of a state
 *
 * This is used both internally to store state types, and in
 * specifying the type of state to search for in iterative
 * operations.
 */

typedef enum state_type__
{
    STATE_ANY=-1, /*!< Specifies that we are searching for any state.
		       Should never be returned in a taggedstate
		       structure. */
    STATE_SHARE=0, /*!< A share */
    STATE_DELEGATION=1, /*!< A delegation */
    STATE_DIR_DELEGATION=2, /*!< A directory delegation */
    STATE_LOCK=3, /*!< A set of locks */
    STATE_LAYOUT=4 /*!< A set of layouts */
} state_type_t;

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
 * catastrophic condition.  In the event of ERR_STATE_FAIL state may
 * be inconsistent.  Any function taking a stateid may fail with
 * ERR_STATE_BAD, ERR_STATE_STALE, ERR_STATE_BADSEQ, or
 * ERR_STATE_OLDSEQ.
 */

/**
 * \subsection SHARE FUNCTIONS
 *
 * These functions all operate on shares.  Along with the
 * specified errors, they may return STATE_ERR_OBJTYPE if the
 * filehandle passed does not represent a regualr file.
 */

int state_open_owner_begin41(clientid4 clientid,
			     open_owner4 nfs_open_owner,
			     state_share_trans_t** transaction);
int state_open_stateid_begin41(stateid4 stateid,
				    state_share_trans_t**
				    transaction);
int state_share_open(state_share_trans_t* transaction,
		     fsal_handle_t* handle,
		     fsal_op_context_t* context,
		     uint32_t share_access,
		     uint32_t share_deny,
		     uint32_t uid);
int state_share_close(state_share_trans_t* transaction,
		      fsal_handle_t* handle,
		      fsal_op_context_t* context);
int state_share_downgrade(state_share_trans_t* transaction,
			  fsal_handle_t* handle,
			  uint32_t share_access,
			  uint32_t share_deny);
int state_share_commit(state_share_trans_t* transaction);
int state_share_abort(state_share_trans_t* transaction);
int state_share_dispose_transaction(state_share_trans_t* transaction);
int state_share_get_stateid(state_share_trans_t* transaction,
			    stateid4* stateid);
int state_share_get_nfs4err(state_share_trans_t* transaction,
				 nfsstat4* error);
int state_start_anonread(fsal_handle_t* handle,
			 int uid,
			 fsal_op_context_t* context,
			 fsal_file_t** descriptor);
int state_start_anonwrite(fsal_handle_t* handle,
			  int uid,
			  fsal_op_context_t* context,
			  fsal_file_t** descriptor);
int state_end_anonread(fsal_handle_t* handle,
		       int uid);
int state_end_anonwrite(fsal_handle_t* handle,
			int uid);
int state_share_descriptor(fsal_handle_t* handle,
			   stateid4* stateid,
			   fsal_file_t** descriptor);


/**
 * \subsection LOCKS
 *
 * These functions associate an FSAL or lock manager dependent datum
 * giving the set of all locks on a file with a stateid and a
 * (filehandle, clientid, open-stateid, lock_owner) tuple.
 */

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
  {ERR_STATE_BAD, "ERR_STATE_BAD", "Bad stateid."},
#define ERR_STATE_NOENT 7
  {ERR_STATE_NOENT, "ERR_STATE_NOENT", "Unable to locate requested resource."},
#define ERR_STATE_NOMUTATE 8
  {ERR_STATE_NOMUTATE, "ERR_STATE_NOMUTATE", "The current state realisation does not support mutation."},
#define ERR_STATE_FAIL 9
  {ERR_STATE_FAIL, "ERR_STATE_FAIL", "Unspecified, internal error."},
#define ERR_STATE_OBJTYPE 10
  {ERR_STATE_OBJTYPE, "ERR_STATE_OBJTYPE", "Operation is undefined or not permitted for the type of object specified."},
#define ERR_STATE_INVAL 11
  {ERR_STATE_INVAL, "ERR_STATE_INVAL", "Invalid operation."},
#define ERR_STATE_DEAD_TRANSACTION 12
  {ERR_STATE_DEAD_TRANSACTION, "ERR_STATE_DEAD_TRANSACTION", "Attempt to operate on a failed, committed, or aborted transaction."}
};

/* Function pointer structures */

typedef struct __sal_functions
{
     int (*state_open_owner_begin41)(clientid4 clientid,
				     open_owner4 nfs_open_owner,
				     state_share_trans_t** transaction);
     int (*state_open_stateid_begin41)(stateid4 stateid,
				       state_share_trans_t**
				       transaction);
     int (*state_share_open)(state_share_trans_t* transaction,
			     fsal_handle_t* handle,
			     fsal_op_context_t* context,
			     uint32_t share_access,
			     uint32_t share_deny,
			     uint32_t uid);
     int (*state_share_close)(state_share_trans_t* transaction,
			      fsal_handle_t* handle,
			      fsal_op_context_t* context);
     int (*state_share_downgrade)(state_share_trans_t* transaction,
				  fsal_handle_t* handle,
				  uint32_t share_access,
				  uint32_t share_deny);
     int (*state_share_commit)(state_share_trans_t* transaction);
     int (*state_share_abort)(state_share_trans_t* transaction);
     int (*state_share_dispose_transaction)(state_share_trans_t* transaction);
     int (*state_share_get_stateid)(state_share_trans_t* transaction,
				    stateid4* stateid);
     int (*state_share_get_nfs4err)(state_share_trans_t* transaction,
				    nfsstat4* error);
     int (*state_start_anonread)(fsal_handle_t* handle,
				 int uid,
				 fsal_op_context_t* context,
				 fsal_file_t** descriptor);
     int (*state_start_anonwrite)(fsal_handle_t* handle,
				  int uid,
				  fsal_op_context_t* context,
				  fsal_file_t** descriptor);
     int (*state_end_anonread)(fsal_handle_t* handle,
			       int uid);
     int (*state_end_anonwrite)(fsal_handle_t* handle,
				int uid);
     int (*state_share_descriptor)(fsal_handle_t* handle,
				   stateid4* stateid,
				   fsal_file_t** descriptor);
     int (*state_init)(void);
     int (*state_shutdown)(void);
} sal_functions_t;

#endif                          /* _SAL_H */
