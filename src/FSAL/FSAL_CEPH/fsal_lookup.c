/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright (C) 2010 The Linux Box, Inc.
 * Contributor : Adam C. Emerson <aemerson@linuxbox.com>
 *
 * Portions copyright CEA/DAM/DIF  (2008)
 * contributeur : Philippe DENIEL   philippe.deniel@cea.fr
 *                Thomas LEIBOVICI  thomas.leibovici@cea.fr
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
 * ------------- 
 */

/**
 * \file    fsal_lookup.c
 * \author  $Author: aemerson $
 * \date    $Date: 2010/07/02 17:00:54 $
* \version $Revision: 0.80 $
 * \brief   Lookup operations.
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "fsal.h"
#include "fsal_internal.h"
#include "fsal_convert.h"

/**
 * FSAL_lookup :
 * Looks up for an object into a directory.
 *
 * Note : if parent handle and filename are NULL,
 *        this retrieves root's handle.
 *
 * \param parent_directory_handle (input)
 *        Handle of the parent directory to search the object in.
 * \param filename (input)
 *        The name of the object to find.
 * \param p_context (input)
 *        Authentication context for the operation (user,...).
 * \param object_handle (output)
 *        The handle of the object corresponding to filename.
 * \param object_attributes (optional input/output)
 *        Pointer to the attributes of the object we found.
 *        As input, it defines the attributes that the caller
 *        wants to retrieve (by positioning flags into this structure)
 *        and the output is built considering this input
 *        (it fills the structure according to the flags it contains).
 *        It can be NULL (increases performances).
 *
 * \return Major error codes :
 *        - ERR_FSAL_NO_ERROR     (no error)
 *        - ERR_FSAL_STALE        (parent_directory_handle does not address an existing object)
 *        - ERR_FSAL_NOTDIR       (parent_directory_handle does not address a directory)
 *        - ERR_FSAL_NOENT        (the object designated by p_filename does not exist)
 *        - ERR_FSAL_XDEV         (tried to operate a lookup on a filesystem junction.
 *                                 Use FSAL_lookupJunction instead)
 *        - ERR_FSAL_FAULT        (a NULL pointer was passed as mandatory argument)
 *        - Other error codes can be returned :
 *          ERR_FSAL_ACCESS, ERR_FSAL_IO, ...
 *          
 */
fsal_status_t FSAL_lookup(fsal_handle_t * parent_directory_handle,      /* IN */
                          fsal_name_t * p_filename,     /* IN */
                          fsal_op_context_t * p_context,        /* IN */
                          fsal_handle_t * object_handle,        /* OUT */
                          fsal_attrib_list_t * object_attributes        /* [ IN/OUT ] */
    )
{

  int rc;
  fsal_status_t status;
  struct stat st;
  char name[FSAL_MAX_NAME_LEN];
  
  /* sanity checks
   * note : object_attributes is optionnal
   *        parent_directory_handle may be null for getting FS root.
   */
  if(!object_handle || !p_context)
    Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_lookup);


  /* retrieves root handle */

  if(!parent_directory_handle)
    {

      /* check that p_filename is NULL,
       * else, parent_directory_handle should not
       * be NULL.
       */
      if(p_filename != NULL)
        Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_lookup);

      /* Ceph seems to have a constant identifying the root inode.
	 Possible source of bugs, so check here if trouble */

      object_handle->vi.ino.val=CEPH_INO_ROOT;
      object_handle->vi.snapid.val=CEPH_NOSNAP;

      if(object_attributes)
        {
          status = FSAL_getattrs(object_handle, p_context, object_attributes);

          /* On error, we set a flag in the returned attributes */

          if(FSAL_IS_ERROR(status))
            {
              FSAL_CLEAR_MASK(object_attributes->asked_attributes);
              FSAL_SET_MASK(object_attributes->asked_attributes, FSAL_ATTR_RDATTR_ERR);
            }
        }

    }
  else                          /* this is a real lookup(parent, name)  */
    {
      /* the filename should not be null */
      if(p_filename == NULL)
        Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_lookup);

      FSAL_name2str(p_filename, name, FSAL_MAX_NAME_LEN);

      /* Ceph returns POSIX errors, so let's use them */
      
      rc=ceph_ll_lookup(parent_directory_handle->vi, name, &st,
			FSAL_OP_CONTEXT_TO_UID(p_context),
			FSAL_OP_CONTEXT_TO_GID(p_context));

      if(rc)
	{
	  Return(posix2fsal_error(rc), 0, INDEX_FSAL_lookup);
	}
      
      stat2fsal_fh(&st, object_handle);

      if(object_attributes)
        {
	  /* convert attributes */
	  status = posix2fsal_attributes(&st, object_attributes);
	  if(FSAL_IS_ERROR(status))
	    {
	      FSAL_CLEAR_MASK(object_attributes->asked_attributes);
	      FSAL_SET_MASK(object_attributes->asked_attributes, FSAL_ATTR_RDATTR_ERR);
	      Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_getattrs);
	    }
        }
    }

  /* lookup complete ! */
  Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_lookup);

}

/**
 * FSAL_lookupJunction :
 * Get the fileset root for a junction.
 *
 * \param p_junction_handle (input)
 *        Handle of the junction to be looked up.
 * \param cred (input)
 *        Authentication context for the operation (user,...).
 * \param p_fsroot_handle (output)
 *        The handle of root directory of the fileset.
 * \param p_fsroot_attributes (optional input/output)
 *        Pointer to the attributes of the root directory
 *        for the fileset.
 *        As input, it defines the attributes that the caller
 *        wants to retrieve (by positioning flags into this structure)
 *        and the output is built considering this input
 *        (it fills the structure according to the flags it contains).
 *        It can be NULL (increases performances).
 *
 * \return Major error codes :
 *        - ERR_FSAL_NO_ERROR     (no error)
 *        - ERR_FSAL_STALE        (p_junction_handle does not address an existing object)
 *        - ERR_FSAL_FAULT        (a NULL pointer was passed as mandatory argument)
 *        - Other error codes can be returned :
 *          ERR_FSAL_ACCESS, ERR_FSAL_IO, ...
 *          
 */
fsal_status_t FSAL_lookupJunction(fsal_handle_t * p_junction_handle,    /* IN */
                                  fsal_op_context_t * p_context,        /* IN */
                                  fsal_handle_t * p_fsoot_handle,       /* OUT */
                                  fsal_attrib_list_t * p_fsroot_attributes      /* [ IN/OUT ] */
    )
{
  int rc;
  fsal_status_t status;

  Return(ERR_FSAL_SERVERFAULT, 0, INDEX_FSAL_lookup);
}

/**
 * FSAL_lookupPath :
 * Looks up for an object into the namespace.
 *
 * Note : if path equals "/",
 *        this retrieves root's handle.
 *
 * \param path (input)
 *        The path of the object to find.
 * \param p_context (input)
 *        Authentication context for the operation (user,...).
 * \param object_handle (output)
 *        The handle of the object corresponding to filename.
 * \param object_attributes (optional input/output)
 *        Pointer to the attributes of the object we found.
 *        As input, it defines the attributes that the caller
 *        wants to retrieve (by positioning flags into this structure)
 *        and the output is built considering this input
 *        (it fills the structure according to the flags it contains).
 *        It can be NULL (increases performances).
 *
 * \return Major error codes :
 *        - ERR_FSAL_NO_ERROR     (no error)
 *        - ERR_FSAL_FAULT        (a NULL pointer was passed as mandatory argument)
 *        - ERR_FSAL_INVAL        (the path argument is not absolute)
 *        - ERR_FSAL_NOENT        (an element in the path does not exist)
 *        - ERR_FSAL_NOTDIR       (an element in the path is not a directory)
 *        - ERR_FSAL_XDEV         (tried to cross a filesystem junction,
 *                                 whereas is has not been authorized in the server
 *                                 configuration - FSAL::auth_xdev_export parameter)
 *        - Other error codes can be returned :
 *          ERR_FSAL_ACCESS, ERR_FSAL_IO, ...
 */

fsal_status_t FSAL_lookupPath(fsal_path_t * p_path,     /* IN */
                              fsal_op_context_t * p_context,    /* IN */
                              fsal_handle_t * object_handle,    /* OUT */
                              fsal_attrib_list_t * object_attributes    /* [ IN/OUT ] */
    )
{
  int rc;
  fsal_status_t status;
  struct stat st;
  char pathname[FSAL_MAX_PATH_LEN];
  
  /* sanity checks
   * note : object_attributes is optionnal
   *        parent_directory_handle may be null for getting FS root.
   */
  if(!p_path || !p_context || !object_handle)
    Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_lookupPath);

  FSAL_path2str(p_path, pathname, FSAL_MAX_PATH_LEN);

  /* retrieves root handle */

  if((strcmp(pathname, "/")==0))
    {
      /* Ceph seems to have a constant identifying the root inode.
	 Possible source of bugs, so check here if trouble */

      object_handle->vi.ino.val=CEPH_INO_ROOT;
      object_handle->vi.snapid.val=CEPH_NOSNAP;

      if(object_attributes)
        {
          status = FSAL_getattrs(object_handle, p_context, object_attributes);

          /* On error, we set a flag in the returned attributes */

          if(FSAL_IS_ERROR(status))
            {
              FSAL_CLEAR_MASK(object_attributes->asked_attributes);
              FSAL_SET_MASK(object_attributes->asked_attributes, FSAL_ATTR_RDATTR_ERR);
            }
        }

    }
  else                          /* this is a real lookup(parent, name)  */
    {
      /* Ceph returns POSIX errors, so let's use them */
      
      rc=ceph_ll_walk(pathname, &st);

      if(rc)
        {
	  Return(posix2fsal_error(rc), 0, INDEX_FSAL_lookupPath);
	}

      stat2fsal_fh(&st, object_handle);
      
      if(object_attributes)
        {
	  status = posix2fsal_attributes(&st, object_attributes);
	  if(FSAL_IS_ERROR(status))
	    {
	      FSAL_CLEAR_MASK(object_attributes->asked_attributes);
	      FSAL_SET_MASK(object_attributes->asked_attributes, FSAL_ATTR_RDATTR_ERR);
	      Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_getattrs);
	    }
        }
    }

  /* lookup complete ! */
  Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_lookup);
}
