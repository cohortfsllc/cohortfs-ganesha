/*
 *
 * Copyright (C) 2010, The Linux Box, inc.
 * All Rights Reserved
 *
 * Contributor: Adam C. Emerson
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif                          /* HAVE_CONFIG_H */ 

#ifdef _SOLARIS
#include "solaris_port.h"
#endif                          /* _SOLARIS */

#ifdef _USE_SHARED_SAL
#include <stdlib.h>
#include <dlfcn.h>              /* For dlopen */
#endif


#include "sal.h"
#include "log_macros.h"
#include "fsal_types.h"

sal_functions_t sal_functions;

#ifdef _USE_SHARED_SAL
sal_functions_t(*getfunctions) (void);
#else
sal_functions_t state_getfunctions(void);

#endif                          /* _USE_SHARED_FSAL */

int
state_open_owner_begin41(clientid4 clientid,
			 open_owner4 nfs_open_owner,
			 state_share_trans_t** transaction)
{
     return (sal_functions.
	    state_open_owner_begin41(clientid,
				     nfs_open_owner,
				     transaction));
}

int
state_open_stateid_begin41(stateid4 stateid,
			   state_share_trans_t** transaction)
{
     return (sal_functions.state_open_stateid_begin41(stateid,
						      transaction));
}


int
state_share_open(state_share_trans_t* transaction,
		 fsal_handle_t* handle,
		 fsal_op_context_t* context,
		 uint32_t share_access,
		 uint32_t share_deny,
		 uint32_t uid)
{
     return (sal_functions.state_share_open(transaction,
					    handle,
					    context,
					    share_access,
					    share_deny,
					    uid));
}

int
state_share_close(state_share_trans_t* transaction,
		  fsal_handle_t* handle,
		  fsal_op_context_t* context)
{
     return (sal_functions.state_share_close(transaction,
					     handle,
					     context));
	  
}

int
state_share_downgrade(state_share_trans_t* transaction,
		      fsal_handle_t* handle,
		      uint32_t share_access,
		      uint32_t share_deny)
{
     return (sal_functions.state_share_downgrade(transaction,
						 handle,
						 share_access,
						 share_deny));
}

int
state_share_commit(state_share_trans_t* transaction)
{
     return (sal_functions.state_share_commit(transaction));
}

int
state_share_abort(state_share_trans_t* transaction)
{
     return (sal_functions.state_share_abort(transaction));
}

int
state_share_dispose_transaction(state_share_trans_t* transaction)
{
     return (sal_functions.
	     state_share_dispose_transaction(transaction));
}

int
state_share_get_stateid(state_share_trans_t* transaction,
			stateid4* stateid)
{
     return (sal_functions.
	     state_share_get_stateid(transaction,
				     stateid));
}

int
state_share_get_nfs4err(state_share_trans_t* transaction,
			nfsstat4* error)
{
     return (sal_functions.
	     state_share_get_nfs4err(transaction,
				     error));
}

int
state_start_anonread(fsal_handle_t* handle,
		     int uid,
		     fsal_op_context_t* context,
		     fsal_file_t** descriptor)
{
     return (sal_functions.
	     state_start_anonread(handle,
				  uid,
				  context,
				  descriptor));
	     
}

int
state_start_anonwrite(fsal_handle_t* handle,
		      int uid,
		      fsal_op_context_t* context,
		      fsal_file_t** descriptor)
{

     return (sal_functions.
	     state_start_anonwrite(handle,
				   uid,
				   context,
				   descriptor));
	     
}

int
state_end_anonread(fsal_handle_t* handle,
		   int uid)
{
     return (sal_functions.
	     state_end_anonread(handle,
				uid));
}

int
state_end_anonwrite(fsal_handle_t* handle,
		    int uid)
{
     return (sal_functions.
	     state_end_anonwrite(handle,
				 uid));
	  
}

int
state_share_descriptor(fsal_handle_t* handle,
		       stateid4* stateid,
		       fsal_file_t** descriptor)
{
     return (sal_functions.
	     state_share_descriptor(handle,
				    stateid,
				    descriptor));
}

int
state_open_to_lock_owner_begin41(fsal_handle_t *handle,
				 clientid4 clientid, 
				 stateid4 open_stateid,
				 lock_owner4 nfs_lock_owner,
				 state_lock_trans_t** transaction)
{
     return (sal_functions.
	     state_open_to_lock_owner_begin41(handle, clientid,
					      open_stateid,
					      nfs_lock_owner, 
					      transaction));
}

int
state_exist_lock_owner_begin41(fsal_handle_t *handle,
			       clientid4 clientid,
			       stateid4 lock_stateid,
			       state_lock_trans_t** transaction)
{
     return (sal_functions.
	     state_exist_lock_owner_begin41(handle,
					    clientid,
					    lock_stateid,
					    transaction));
}

int
state_lock(state_lock_trans_t* transaction,
	   uint64_t offset,
	   uint64_t length,
	   bool_t exclusive,
	   bool_t blocking)
{
     return (sal_functions.state_lock(transaction,
				      offset,
				      length,
				      exclusive,
				      blocking));
}

int
state_unlock(state_lock_trans_t* transaction,
	     uint64_t offset,
	     uint64_t length)
{
     return (sal_functions.state_unlock(transaction,
					offset,
					length));
}

int
state_lock_commit(state_lock_trans_t* transaction)
{
     return (sal_functions.
	     state_lock_commit(transaction));
}

int
state_lock_abort(state_lock_trans_t* transaction)
{
     return (sal_functions.state_lock_abort(transaction));
}

int
state_lock_dispose_transaction(state_lock_trans_t* transaction)
{
     return (sal_functions.
	     state_lock_dispose_transaction( transaction));
}
     
int
state_lock_get_stateid(state_lock_trans_t* transaction,
		       stateid4* stateid)
{
     return (sal_functions.
	     state_lock_get_stateid(transaction,
				    stateid));
}

int
state_lock_get_nfs4err(state_lock_trans_t* transaction,
		       nfsstat4* error)
{
     return (sal_functions.
	     state_lock_get_nfs4err(transaction,
				    error));
}

int
state_lock_get_nfs4conflict(state_lock_trans_t* transaction,
			    uint64_t* offset,
			    uint64_t* length,
			    uint32_t* type,
			    lock_owner4* lock_owner)
{
     return (sal_functions.
	     state_lock_get_nfs4conflict(transaction,
					 offset,
					 length,
					 type,
					 lock_owner));
	     
}

int
state_init(void)
{
     return (sal_functions.state_init());
}

int
state_shutdown(void)
{
     return (sal_functions.state_shutdown());
}


#ifdef _USE_SHARED_SAL
int state_loadlibrary(char *path)
{
  void *handle;
  char *error;

  LogEvent(COMPONENT_STATES, "Load shared SAL: %s", path);

  if((handle = dlopen(path, RTLD_LAZY)) == NULL)
    {
      LogMajor(COMPONENT_STATES, "state_loadlibrary: could not load sal: %s", dlerror());
      return 0;
    }

  /* Clear any existing error : dlerror will be used to check if dlsym succeeded or not */
  dlerror();

  /* Map state_getfunctions */
  *(void **)(&getfunctions) = dlsym(handle, "state_getfuctions");
  if((error = dlerror()) != NULL)
    {
      LogMajor(COMPONENT_STATES, "state_loadlibrary: Could not map symbol state_getfunctions:%s", error);
      return 0;
    }

  return 1;
}                               /* FSAL_LoadLibrary */

void state_loadfunctions(void)
{
  sal_functions = (*getfunctions) ();
}

#else
int state_loadlibrary(char *path)
{
  return 1;                     /* Does nothing, this is the "static" case */
}

void state_loadfunctions(void)
{
  sal_functions = state_getfunctions();
}

#endif
