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



int state_create_share(nfs_fh4 filehandle, open_owner4 open_owner,
		       clientid4 clientid, uint32_t share_access,
		       uint32_t share_deny, stateid4* stateid)
{
  return sal_functions.state_create_share(filehandle, open_owner,
					  clientid, share_access,
					  share_deny, stateid);
}

int state_update_share(uint32_t share_access, uint32_t share_deny,
		       stateid4* stateid)
{
  return sal_functions.state_update_share(share_access, share_deny,
					  stateid);
}

int state_delete_share(stateid4* stateid);
{
  return sal_functions.state_delete_share(stateid);
}

int state_query_share(nfs_fh4 filehandle, clientid4 clientid,
		      open_owner4 open_owner, sharestate* state)
{
  return sal_functions.state_query_share(filehandle, clientid,
					 open_owner, state);
}

int state_create_delegation(nfs_fh4 filehandle, clientid4 clientid,
			    open_delegation_type4 type,
			    nfs_space_limit4 limit, stateid4* stateid)
{
  return sal_functions.state_create_delegation(filehandle, clientid,
					       type, limit, stateid);
}

int state_delete_delegation(stateid4* stateid)
{
  return sal_functions.state_delete_delegation(stateid);
}

int state_query_delegation(nfs_fh4 filehandle, clientid4 clientid,
			   delegationstate* state)
{
  return sal_functions.state_query_delegation(filehandle, clientid,
					      state);
}

int state_check_delegation(nfs_fh4 filehandle,
			   open_delegation_type4 type);
{
  return sal_functions.state_check_delegation(filehandle, type);
}

int state_create_dir_delegation(nfs_fh4 filehandle, clientid4 clientid,
				bitmap4 notification_types,
				attr_notice4 child_attr_delay,
				attr_notice4 dir_attr_delay,
				bitmap4 child_attributes,
				bitmap4 dir_attributes,
				stateid4* stateid)
{
  return sal_functions.state_create_dir_delegation(filehandle, clientid,
						   notification_types,
						   child_attr_delay,
						   dir_attr_delay,
						   child_attributes,
						   dir_attributes,
						   stateid);
}

int state_delete_dir_delegation(stateid4* stateid)
{
  return sal_functions.state_delete_dir_delegation(stateid);
}

int state_query_dir_delegation(nfs_fh4 filehandle,
			       clientid4 clientid,
			       dir_delegationstate* state)
{
  return sal_functions.state_query_dir_delegation(filehandle,
				    clientid,
				    state);
}

int state_check_dir_delegation(nfs_fh4 filehandle)
{
  return sal_functions.state_check_dir_delegation(filehandle);
}

int state_create_lock_state(nfs_fh4 filehandle,
			    open_stateid4 open_stateid,
			    lock_owner4 lock_owner,
			    nfs_lock_type4 locktype,
			    offset4 offset,
			    length4 length,
			    fsal_lock_t lockdata,
			    stateid4* stateid)
{
  return sal_functsions.state_create_lock_state(filehandle,
					       open_stateid,
					       lock_owner,
					       locktype,
					       offset,
					       length,
					       lockdata,
					       stateid);
}

int state_add_lock_range(nfs_lock_type4 type,
			 offset4 offset,
			 length4 length,
			 fsal_lock_t lockdata,
			 stateid4* stateid)
{
  return sal_functions.state_add_lock_range(type, offset, length,
					    lockdata, stateid);
}

int state_add_lock_merge(nfs_lock_type4 type,
			 offset4 offset,
			 length4 length,
			 fsal_lock_t lockdata,
			 stateid4* stateid)
{
  return sal_functions.state_add_lock_merge(type, offset, length,
					    lockdata, stateid);
}

int state_free_lock_range(nfs_locK_type4 type,
			  offset4 offset,
			  length4 length,
			  stateid4* stateid)
{
  return sal_functions.state_free_lock_range(type, offset, length,
					     stateid);
}

int state_delete_lock_state(stateid4* stateid)
{
  return sal_functions.state_delete_lock_state(stateid);
}

int state_iter_lock_ranges(stateid4* stateid, uint64_t* cookie,
			   boolean* finished, lockstate* state);
{
  return sal_functions.state_iter_lock_ranges(stateid, cookie,
					      finished, state);
}


int state_test_lock_range(nfs_fh4 filehandle, offset4 offset,
			  length4 length, nfs_lock_type4 type)
{
  return sal_functions.state_test_lock_range(filehandle, offset,
					     length, type);
}

int state_query_lock_range(nfs_fh4 filehandle, offset4 offset,
			   length4 length, nfs_lock_type4 type,
			   lockstate* state)
{
  return sal_functions.state_query_lock_range(filehandle, offset,
					      length, type,
					      lockstate* state);
}

int state_create_layout_state(nfs_fh4 filehandle,
			      layouttype4 type,
			      layoutiomode4 iomode,
			      offset4 offset,
			      length4 length,
			      boolean return_on_close,
			      fsal_layout_t layoutdata)
{
  return sal_functions.state_create_layout_state(filehandle,
						 type,
						 iomode,
						 offset,
						 length,
						 return_on_close,
						 layoutdata);
}

int state_add_layout(layouttype4 type,
		     layoutimode4 iomode,
		     offset4 offset,
		     length4 length,
		     boolean return_on_close,
		     fsal_layout_t layoutdata,
		     stateid4* stateid)
{
  return sal_functions.state_add_layout(type,
					iomode,
					offset,
					length,
					return_on_close,
					layoutdata,
					stateid);
}

int state_add_layout_merge(layouttype4 type,
			   layoutimode4 iomode,
			   offset4 offset,
			   length4 length,
			   boolean return_on_close,
			   fsal_layout_t layoutdata,
			   stateid4* stateid)
{
  return sal_functions.state_add_layout_merge(type, iomode,
					      offset, length,
					      return_on_close,
					      layoutdata, stateid);
}

int state_free_layout(layouttype4 type,
		      layoutimode4 iomode,
		      offset4 offset,
		      length4 length,
		      stateid4* stateid)
{
  return sal_functions.state_free_layout(type, iomode, offset,
					 length, stateid);
}

int state_delete_layout_state(stateid* stateid);
{
  return sal_functions.state_delete_layout_state(stateid* stateid);
}


int state_iter_layouts(stateid4* stateid,
		       uint64_t* cookie,
		       boolean* finished,
		       layoutstate* state)
{
  return sal_functions.state_iter_layouts(stateid, cookie,
					  finished, state);
}

int state_lock_filehandle(nfs_fh4 filehandle, statelocktype rw)
{
  return sal_functions.state_lock_filehandle(filehandle, rw);
}

int state_unlock_filehandle(nfs_fh4 filehandle, statelocktype rw)
{
  return sal_functions.state_unlock_filehandle(filehandle, rw);
}

int state_iterate_by_filehandle(nfs_fh4 filehandle, statetype type,
				uint64_t* cookie, boolean* finished,
				state* state)
{
  return sal_functions.state_iterate_by_filehandle(filehandle, type,
						   cookie, finished,
						   state);
}

int state_iterate_by_clientid(clientid4 clientid, statetype type,
			      uint64_t* cookie, boolean* finished,
			      state* state)
{
  return sal_functions.state_iterate_by_clientid(clientid, type,
						 cookie, finished,
						 state);
}

int state_iterate_all(statetype type, uint64_t* cookie,
		      boolean* finished, state* state)

{
  return sal_functions.state_iterate_all(type, cookie, finished, state); 
}

int state_init(void)
{
  return sal_functions.fsal_init();
}

int state_shutdown()
{
  return sal_functions.state_shutdown();
}

#ifdef _USE_SHARED_SAL
int state_loadlibrary(char *path)
{
  void *handle;
  char *error;

  LogEvent(COMPONENT_STATE, "Load shared SAL: %s", path);

  if((handle = dlopen(path, RTLD_LAZY)) == NULL)
    {
      LogMajor(COMPONENT_STATE, "state_loadlibrary: could not load sal: %s", dlerror());
      return 0;
    }

  /* Clear any existing error : dlerror will be used to check if dlsym succeeded or not */
  dlerror();

  /* Map state_getfunctions */
  *(void **)(&getfunctions) = dlsym(handle, "state_getfuctions");
  if((error = dlerror()) != NULL)
    {
      LogMajor(COMPONENT_STATE, "state_loadlibrary: Could not map symbol state_getfunctions:%s", error);
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
