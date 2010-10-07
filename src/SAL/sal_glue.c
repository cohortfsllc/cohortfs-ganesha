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
#endif                          /* _USE_SHARED_FSAL */

int state_create_share(fsal_handle_t *handle, open_owner4 open_owner,
		       clientid4 clientid, uint32_t share_access,
		       uint32_t share_deny, stateid4* stateid)
{
    return (sal_functions.state_create_share(handle, open_owner,
					     clientid, share_access,
					     share_deny, stateid4*
					     stateid));
}

int state_upgrade_share(uint32_t share_access, uint32_t share_deny,
			stateid4* stateid)
{
    return (sal_functions.state_upgrade_share(share_access, share_deny,
					      stateid));
}

int state_downgrade_share(uint32_t share_access, uint32_t share_deny,
			  stateid4* stateid)
{
    return (sal_functions.state_downgrade_share(share_access,
						share_deny, stateid));
}

int state_delete_share(stateid4 stateid)
{
    return (sal_functions.state_delete_share(stateid));
}

int state_query_share(fsal_handle_t *handle, clientid4 clientid,
		      open_owner4 open_owner, sharestate* state)
{
    return (sal_functions.state_query_share(handle, clientid,
					    open_owner, state));
}

int state_start_32read(fsal_handle_t *handle)
{
    return (sal_functions.state_start_32read(handle));
}

int state_start_32write(fsal_handle_t *handle)
{
    return (sal_functions.state_start_32write(handle));
}

int state_end_32read(fsal_handle_t *handle)
{
    return (sal_functions.state_end_32read(handle));
}

int state_end_32write(fsal_handle_t *handle)
{
    return (sal_functions.state_end_32write(handle));
}

int state_create_delegation(fsal_handle_t *handle, clientid4 clientid,
			    open_delegation_type4 type,
			    nfs_space_limit4 limit, stateid4* stateid)
{
    return (sal_functions.state_create_delegation(handle, clientid,
						   type, limit,
						   stateid));
}
		            
int state_delete_delegation(stateid4 stateid)
{
    return (sal_functions.state_delete_delegation(stateid));
}

int state_query_delegation(fsal_handle_t *handle, clientid4 clientid,
			   delegationstate* state)
{
    return (sal_functions.state_query_delegation(handle, clientid,
						 state));
}

int state_check_delegation(fsal_handle_t *handle,
			   open_delegation_type4 type)
{
    return (sal_functions.state_check_delegation(handle, type));
}


int state_create_dir_delegation(fsal_handle_t *handle, clientid4 clientid,
				bitmap4 notification_types,
				attr_notice4 child_attr_delay,
				attr_notice4 dir_attr_delay,
				bitmap4 child_attributes,
				bitmap4 dir_attributes,
				stateid4* stateid)
{
    return (sal_functions.state_create_dir_delegation(handle, clientid, notification_types,
						      child_attr_delay, dir_attr_delay,
						      child_attributes, dir_attributes,
						      stateid));
}

int state_delete_dir_delegation(stateid4 stateid)
{
    return (sal_functions.state_delete_dir_delegation(stateid));
}

int state_query_dir_delegation(fsal_handle_t *handle,
			       clientid4 clientid,
			       dir_delegationstate* state)
{
    return (sal_functions.state_query_dir_delegation(handle, clientid,
						     outstate));
}

int state_check_dir_delegation(fsal_handle_t *handle)
{
    return (sal_functions.state_check_dir_delegation(handle));
}

int state_create_lock_state(fsal_handle_t *handle,
			    stateid4 open_stateid,
			    lock_owner4 lock_owner,
			    fsal_lock_t* lockdata,
			    stateid4* stateid);
{
    return (sal_functions.state_create_lock_state(handle, open_stateid,
						  lock_owner, lockdata,
						  stateid));
}

int state_delete_lock_state(stateid4 stateid)
{
    return (state_delete_lock_state(stateid4 stateid));
}

int state_query_lock_state(fsal_handle_t *handle,
			   stateid4 open_stateid,
			   lock_owner4 lock_owner,
			   lockstate* lockstateout)
{
    return (sal_functions.state_query_lock_state(handle, open_stateid,
						 lock_owner,
						 lockstateout));
}

int state_inc_lock_state(stateid4 stateid)
{
    return (sal_functions.state_inc_lock_state(stateid));
}

int state_create_layout_state(fsal_handle_t handle,
			      stateid4 ostateid,
			      layouttype4 type,
			      stateid4* stateid)
{
    return (sal_functions.state_create_layout_state(handle, ostateid,
						    type, stateid));
}

int state_delete_layout_state(stateid stateid)
{
    return (sal_functions.state_delete_layout_state(stateid))
}

int state_add_layout_segment(layoutimode4 iomode,
			     offset4 offset,
			     length4 length,
			     boolean return_on_close,
			     fsal_layout_t* layoutdata,
			     stateid4 stateid);
{
    return (state_add_layout_segment(iomode, offset,
				     length, return_on_close,
				     layoutdata, stateid));
}

int state_mod_layout_segment(layoutimode4 iomode,
			     offset4 offset,
			     length4 length,
			     fsal_layout_t* layoutdata,
			     stateid4 stateid,
			     uint64_t segid)
{
    return (sal_state.state_mod_layout_segment(iomode, offset,
					       length, layoutdata,
					       stateid, segid));
}

int state_free_layout_segment(stateid4 stateid,
			      uint64_t segid)
{
    return (sal_functions.state_free_layout_segment(stateid, segid));
}

int state_layout_inc_state(stateid4* stateid)
{
    return (sal_functions.state_layout_inc_state(stateid));
}

int state_iter_layout_entries(stateid4 stateid,
			      uint64_t* cookie,
			      boolean* finished,
			      layoutsegment* segment)
{
    return (sal_functions.state_iter_layout_entries(stateid, cookie,
						    finished,
						    segment));
}

int state_lock_filehandle(fsal_handle_t *handle, statelocktype rw)
{
    return (sal_functions.state_lock_filehandle(handle, rw));
}

int state_unlock_filehandle(fsal_handle_t *handle)
{
    return (sal_functions.state_unlock_filehandle(handle));
}

int state_iterate_by_filehandle(fsal_handle_t *handle, statetype type,
				uint64_t* cookie, boolean* finished,
				taggedstate* state)
{
    return (sal_functions.state_iterate_by_filehandle(handle, type,
						      cookie, finished,
						      state));
}

int state_iterate_by_clientid(clientid4 clientid, statetype type,
			      uint64_t* cookie, boolean* finished,
			      taggedstate* state)
{
    return (sal_functions.state_iterate_by_clientid(clientid, type,
						    cookie, finished,
						    state));
}

int state_retrieve_state(stateid4 stateid, taggedstate* state)
{
    return (sal_functions.state_retrieve_state(stateid, state));
}

int state_init(void)
{
    return (sal_functions.state_init());
}

int state_shutdown(void)
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
  fsal_functions = state_getfunctions();
}

#endif
