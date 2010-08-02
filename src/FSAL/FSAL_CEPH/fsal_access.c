/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright (C) 2010, The Linux Box Corporation
 * Contributor : Adam C. Emerson <aemerson@linuxbox.com>
 *
 * Some portions Copyright CEA/DAM/DIF  (2008)
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
 * \file    fsal_access.c
 * \brief   FSAL access permissions functions.
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "fsal.h"
#include "fsal_internal.h"
#include "fsal_convert.h"

/**
 * FSAL_access :
 * Tests whether the user or entity identified by the context structure
 * can access the object identified by filehandle
 * as indicated by the access_type parameter.
 *
 * \param filehandle (input):
 *        The handle of the object to test permissions on.
 * \param context (input):
 *        Authentication context for the operation (export entry, user,...).
 * \param access_type (input):
 *        Indicates the permissions to be tested.
 *        This is an inclusive OR of the permissions
 *        to be checked for the user specified by context.
 *        Permissions constants are :
 *        - FSAL_R_OK : test for read permission
 *        - FSAL_W_OK : test for write permission
 *        - FSAL_X_OK : test for exec permission
 *        - FSAL_F_OK : test for file existence
 * \param object_attributes (optional input/output):
 *        The post operation attributes for the object.
 *        As input, it defines the attributes that the caller
 *        wants to retrieve (by positioning flags into this structure)
 *        and the output is built considering this input
 *        (it fills the structure according to the flags it contains).
 *        Can be NULL.
 *
 * \return Major error codes :
 *        - ERR_FSAL_NO_ERROR     (no error, asked permission is granted)
 *        - ERR_FSAL_ACCESS       (object permissions doesn't fit asked access type)
 *        - ERR_FSAL_STALE        (object_handle does not address an existing object)
 *        - ERR_FSAL_FAULT        (a NULL pointer was passed as mandatory argument)
 *        - Other error codes when something anormal occurs.
 */
fsal_status_t CEPHFSAL_access(cephfsal_handle_t * filehandle,        /* IN */
			      cephfsal_op_context_t * context,        /* IN */
			      fsal_accessflags_t access_type,       /* IN */
			      fsal_attrib_list_t * object_attributes        /* [ IN/OUT ] */
    )
{

  struct stat_precise st;
  fsal_accessflags_t missing_access;
  fsal_accessmode_t mode;
  int rc, is_grp, i;
  int uid=FSAL_OP_CONTEXT_TO_UID(context);
  int gid=FSAL_OP_CONTEXT_TO_GID(context);
  
  missing_access = access_type;
  
  /* sanity checks.
   * note : object_attributes is optional in FSAL_access.
   */
  if(!filehandle || !context)
    Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_access);


  TakeTokenFSCall();

  rc=ceph_ll_getattr_precise(VINODE(filehandle), &st, uid, gid);

  ReleaseTokenFSCall();

  mode=unix2fsal_mode(st.st_mode);

  /* Look through flags and construct return code */

  if(access_type & FSAL_F_OK)
    ReturnCode(ERR_FSAL_INVAL, 0);

  if(uid == 0)
    ReturnCode(ERR_FSAL_NO_ERROR, 0);
  
  if (uid == st.st_uid)
    {
      if(mode & FSAL_MODE_RUSR)
        missing_access &= ~FSAL_R_OK;

      if(mode & FSAL_MODE_WUSR)
        missing_access &= ~FSAL_W_OK;

      if(mode & FSAL_MODE_XUSR)
        missing_access &= ~FSAL_X_OK;

      if(missing_access == 0)
        ReturnCode(ERR_FSAL_NO_ERROR, 0);
      else
        ReturnCode(ERR_FSAL_ACCESS, 0);
    }

  is_grp = (gid == st.st_gid);

  if (!is_grp)
    {
      for(i=0; i<context->credential.nbgroups; i++)
	{
	  is_grp = (context->credential.alt_groups[i]);
	  if (is_grp)
	    break;
	}
    }

  if (is_grp)
    {
      if(mode & FSAL_MODE_RGRP)
        missing_access &= ~FSAL_R_OK;

      if(mode & FSAL_MODE_WGRP)
        missing_access &= ~FSAL_W_OK;

      if(mode & FSAL_MODE_XGRP)
        missing_access &= ~FSAL_X_OK;

      if(missing_access == 0)
        ReturnCode(ERR_FSAL_NO_ERROR, 0);
      else
        ReturnCode(ERR_FSAL_ACCESS, 0);
    }
  if(mode & FSAL_MODE_ROTH)
    missing_access &= ~FSAL_R_OK;
 
  if(mode & FSAL_MODE_WOTH)
    missing_access &= ~FSAL_W_OK;
 
  if(mode & FSAL_MODE_XOTH)
    missing_access &= ~FSAL_X_OK;

  if(missing_access == 0)
    ReturnCode(ERR_FSAL_NO_ERROR, 0);
  else
    ReturnCode(ERR_FSAL_ACCESS, 0);
 
  /*
   * If an error occures during conversion, an error bit is set in the
   * output structure.
   */

  if(object_attributes)
    {
      fsal_status_t status;

      status = posix2fsal_attributes(&st, object_attributes);
      if(FSAL_IS_ERROR(status))
	{
	  FSAL_CLEAR_MASK(object_attributes->asked_attributes);
	  FSAL_SET_MASK(object_attributes->asked_attributes, FSAL_ATTR_RDATTR_ERR);
	  Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_access);
	}
    }

  Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_access);

}
