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

int state_create_share(fsal_handle_t *handle, open_owner4 open_owner,
		       clientid4 clientid, uint32_t share_access,
		       uint32_t share_deny,
		       cache_inode_openref_t* openref, stateid4* stateid)
{
    return sal_functions.state_create_share(handle, open_owner,
					    clientid, share_access,
					    share_deny, openref, stateid);
}

int state_upgrade_share(uint32_t share_access, uint32_t share_deny,
			stateid4* stateid)
{
    return sal_functions.state_upgrade_share(share_access, share_deny,
			stateid);
}

int state_downgrade_share(uint32_t share_access, uint32_t share_deny,
			  stateid4* stateid)
{
    return sal_functions.state_downgrade_share(share_access,
			  share_deny, stateid);
}

int state_delete_share(stateid4 stateid)
{
    return sal_functions.state_delete_share(stateid);
}

int state_query_share(fsal_handle_t *handle, clientid4 clientid,
		      open_owner4 open_owner,
		      sharestate* outshare)
{
    return sal_functions.state_query_share(handle, clientid, open_owner,
					   outshare);
}

int state_check_share(fsal_handle_t handle, uint32_t share_access,
		      uint32_t share_deny)
{
  return sal_functions.state_check_share(handle, share_access,
					 share_deny);
}


int state_start_32read(fsal_handle_t *handle)
{
    return sal_functions.state_start_32read(handle);
}

int state_start_32write(fsal_handle_t *handle)
{
    return sal_functions.state_start_32write(handle);
}

int state_end_32read(fsal_handle_t *handle)
{
    return sal_functions.state_end_32read(handle);
}

int state_end_32write(fsal_handle_t *handle)
{
    return sal_functions.state_end_32write(handle);
}

int state_create_delegation(fsal_handle_t *handle, clientid4 clientid,
			    open_delegation_type4 type,
			    nfs_space_limit4 limit,
			    stateid4* stateid)
{
    return sal_functions.state_create_delegation(handle, clientid,
						 type, limit,
						 stateid);
}

int state_delete_delegation(stateid4 stateid)
{
    return sal_functions.state_delete_delegation(stateid);
}

int state_query_delegation(fsal_handle_t *handle, clientid4 clientid,
			   delegationstate* outdelegation)
{
    return sal_functions.state_query_delegation(handle, clientid,
				  outdelegation);
}

int state_check_delegation(fsal_handle_t *handle,
			   open_delegation_type4 type)
{
    return sal_functions.state_check_delegation(handle,
						type);
}

#ifdef _USE_NFS4_1
int state_create_dir_delegation(fsal_handle_t *handle, clientid4 clientid,
				bitmap4 notification_types,
				attr_notice4 child_attr_delay,
				attr_notice4 dir_attr_delay,
				bitmap4 child_attributes,
				bitmap4 dir_attributes,
				stateid4* stateid)
{
    return sal_functions.state_create_dir_delegation(handle, clientid,
						     notification_types,
						     child_attr_delay,
						     dir_attr_delay,
						     child_attributes,
						     dir_attributes,
						     stateid);
}

int state_delete_dir_delegation(stateid4 stateid)
{
    return sal_functions.state_delete_dir_delegation(stateid);
}

int state_query_dir_delegation(fsal_handle_t *handle, clientid4 clientid,
			       dir_delegationstate* outdir_delegation)
{
    return sal_functions.state_query_dir_delegation(handle, clientid,
						    outdir_delegation);
}
#endif

int state_create_lock_state(fsal_handle_t *handle,
			    stateid4 open_stateid,
			    lock_owner4 lock_owner,
			    clientid4 clientid,
			    fsal_lockdesc_t* lockdata,
			    stateid4* stateid)
{
    return sal_functions.state_create_lock_state(handle,
						 open_stateid,
						 lock_owner,
						 clientid,
						 lockdata,
						 stateid);
}

int state_delete_lock_state(stateid4 stateid)
{
    return sal_functions.state_delete_lock_state(stateid);
}

int state_query_lock_state(fsal_handle_t *handle,
			   stateid4 open_stateid,
			   lock_owner4 lock_owner,
			   clientid4 clientid,
			   lockstate* outlockstate)
{
    return sal_functions.state_query_lock_state(handle,
						open_stateid,
						lock_owner,
						clientid,
						outlockstate);
}

int state_inc_lock_state(stateid4* stateid)
{
    return sal_functions.state_inc_lock_state(stateid);
}

int state_lock_inc_state(stateid4* stateid)
{
    return state_lock_inc_state(stateid);
}

#ifdef _USE_FSALMDS

int state_create_layout_state(fsal_handle_t* handle,
			      stateid4 ostateid,
			      clientid4 clientid,
			      layouttype4 type,
			      stateid4* stateid)
{
    return sal_functions.state_create_layout_state(handle,
						   ostateid,
						   clientid,
						   type,
						   stateid);
}

int state_delete_layout_state(stateid4 stateid)
{
    return sal_functions.state_delete_layout_state(stateid);
}

int state_query_layout_state(fsal_handle_t *handle,
			     clientid4 clientid,
			     layouttype4 type,
			     layoutstate* outlayoutstate)
{
    return sal_functions.state_query_layout_state(handle,
						  type,
						  clientid,
						  outlayoutstate);
}

int state_add_layout_segment(layouttype4 type,
			     layoutiomode4 iomode,
			     offset4 offset,
			     length4 length,
			     bool_t return_on_close,
			     fsal_layoutdata_t* layoutdata,
			     stateid4 stateid)
{
    return sal_functions.state_add_layout_segment(type, iomode, offset,
						  length,
						  return_on_close,
						  layoutdata, stateid);
}

int state_mod_layout_segment(layoutiomode4 iomode,
			     offset4 offset,
			     length4 length,
			     fsal_layoutdata_t* layoutdata,
			     stateid4 stateid,
			     uint64_t segid)
{
    return sal_functions.state_mod_layout_segment(iomode, offset,
						  length, layoutdata,
						  stateid, segid);
}

int state_free_layout_segment(stateid4 stateid,
			      uint64_t segid)
{
    return sal_functions.state_free_layout_segment(stateid,
						   segid);
}

int state_layout_inc_state(stateid4* stateid)
{
    return sal_functions.state_layout_inc_state(stateid);
}

int state_iter_layout_entries(stateid4 stateid,
			      uint64_t* cookie,
			      bool_t* finished,
			      layoutsegment* segment)
{
    return sal_functions.state_iter_layout_entries(stateid, cookie,
						   finished, segment);
}

#endif

int state_lock_filehandle(fsal_handle_t *handle,
			  statelocktype rw)
{
    return sal_functions.state_lock_filehandle(handle, rw);
}

int state_unlock_filehandle(fsal_handle_t *handle)
{
    return sal_functions.state_unlock_filehandle(handle);
}

int state_iterate_by_filehandle(fsal_handle_t *handle, statetype type,
				uint64_t* cookie, bool_t* finished,
				taggedstate* outstate)
{
    return sal_functions.state_iterate_by_filehandle(handle, type,
						     cookie, finished,
						     outstate);
}

int state_iterate_by_clientid(clientid4 clientid, statetype type,
			      uint64_t* cookie, bool_t* finished,
			      taggedstate* outstate)
{
    return sal_functions.state_iterate_by_clientid(clientid, type,
						   cookie, finished,
						   outstate);
}

int state_retrieve_state(stateid4 stateid, taggedstate* outstate)
{
    return sal_functions.state_retrieve_state(stateid, outstate);
}

int state_lock_state_owner(state_owner4 state_owner, bool_t lock,
			   seqid4 seqid, bool_t* new,
			   nfs_resop4** response)
{
    return sal_functions.state_lock_state_owner(state_owner, lock,
						seqid, new, response);
}

int state_unlock_state_owner(state_owner4 state_owner, bool_t lock)
{
    return sal_functions.state_unlock_state_owner(state_owner, lock);
}

int state_save_response(state_owner4 state_owner, bool_t lock,
			nfs_resop4* response)
{
    return sal_functions.state_save_response(state_owner, lock,
					     response);
}

int state_init(void)
{
    return sal_functions.state_init();
}

int state_shutdown(void)
{
    return sal_functions.state_shutdown();
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
