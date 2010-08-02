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
 *
 * \file    fsal_unlink.c
 * \brief   object removing function.
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "fsal.h"
#include "fsal_internal.h"
#include "fsal_convert.h"

/**
 * FSAL_unlink:
 * Remove a filesystem object .
 *
 * \param parentdir_handle (input):
 *        Handle of the parent directory of the object to be deleted.
 * \param p_object_name (input):
 *        Name of the object to be removed.
 * \param p_context (input):
 *        Authentication context for the operation (user,...).
 * \param parentdir_attributes (optionnal input/output): 
 *        Post operation attributes of the parent directory.
 *        As input, it defines the attributes that the caller
 *        wants to retrieve (by positioning flags into this structure)
 *        and the output is built considering this input
 *        (it fills the structure according to the flags it contains).
 *        May be NULL.
 *
 * \return Major error codes :
 *        - ERR_FSAL_NO_ERROR     (no error)
 *        - ERR_FSAL_STALE        (parentdir_handle does not address an existing object)
 *        - ERR_FSAL_NOTDIR       (parentdir_handle does not address a directory)
 *        - ERR_FSAL_NOENT        (the object designated by p_object_name does not exist)
 *        - ERR_FSAL_NOTEMPTY     (tried to remove a non empty directory)
 *        - ERR_FSAL_FAULT        (a NULL pointer was passed as mandatory argument)
 *        - Other error codes can be returned :
 *          ERR_FSAL_ACCESS, ERR_FSAL_IO, ...
 */

fsal_status_t CEPHFSAL_unlink(cephfsal_handle_t * parentdir_handle,     /* IN */
			      fsal_name_t * p_object_name,  /* IN */
			      cephfsal_op_context_t * p_context,        /* IN */
			      fsal_attrib_list_t * parentdir_attributes     /* [IN/OUT ] */
    )
{

  fsal_status_t status;
  int rc;
  char name[FSAL_MAX_NAME_LEN];
  struct stat_precise st;
  int uid=FSAL_OP_CONTEXT_TO_UID(p_context);
  int gid=FSAL_OP_CONTEXT_TO_GID(p_context);

  /* sanity checks.
   * note : parentdir_attributes are optional.
   *        parentdir_handle is mandatory,
   *        because, we do not allow to delete FS root !
   */

  if(!parentdir_handle || !p_context || !p_object_name)
    Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_unlink);

  if(parentdir_attributes)
    {
      status = CEPHFSAL_getattrs(parentdir_handle, p_context, parentdir_attributes);
      
      if(FSAL_IS_ERROR(status))
	{
	  FSAL_CLEAR_MASK(parentdir_attributes->asked_attributes);
	  FSAL_SET_MASK(parentdir_attributes->asked_attributes, FSAL_ATTR_RDATTR_ERR);
	}
    }

  FSAL_name2str(p_object_name, name, FSAL_MAX_NAME_LEN);

  TakeTokenFSCall();
  rc=ceph_ll_lookup_precise(VINODE(parentdir_handle), name, &st, uid, gid);
  ReleaseTokenFSCall();

  if (rc < 0)
    Return(posix2fsal_error(rc), 0, INDEX_FSAL_unlink);
  
  if(S_ISDIR(st.st_mode))
    rc=ceph_ll_rmdir(VINODE(parentdir_handle), name, uid, gid);
  else
    rc=ceph_ll_unlink(VINODE(parentdir_handle), name, uid, gid);

  if (rc < 0)
    Return(posix2fsal_error(rc), 0, INDEX_FSAL_unlink);

  /* OK */
  Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_unlink);

}
