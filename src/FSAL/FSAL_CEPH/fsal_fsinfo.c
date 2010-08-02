/*
 * Copyright (C) 2010 The Linx Box Corporation
 * Contributor : Adam C. Emerson
 *
 * Some Portions Copyright CEA/DAM/DIF  (2008)
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
 * ---------------------------------------
 */

/**
 *
 * \file    fsal_fsinfo.c
 * \brief   functions for retrieving filesystem info.
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "fsal.h"
#include "fsal_internal.h"
#include "fsal_convert.h"

/**
 * FSAL_static_fsinfo:
 * Return static filesystem info such as
 * behavior, configuration, supported operations...
 *
 * \param filehandle (input):
 *        Handle of an object in the filesystem
 *        whom info is to be retrieved.
 * \param cred (input):
 *        Authentication context for the operation (user,...).
 * \param staticinfo (output):
 *        Pointer to the static info of the filesystem.
 *
 * \return Major error codes:
 *      - ERR_FSAL_NO_ERROR     (no error)
 *      - ERR_FSAL_FAULT        (a NULL pointer was passed as mandatory argument)
 *      - Other error codes can be returned :
 *        ERR_FSAL_IO, ...
 */
fsal_status_t CEPHFSAL_static_fsinfo(cephfsal_handle_t * filehandle,    /* IN */
				     fsal_op_context_t * p_context, /* IN */
				     fsal_staticfsinfo_t * staticinfo       /* OUT */
    )
{
  /* sanity checks. */
  /* for HPSS, handle and credential are not used. */
  if(!staticinfo)
    Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_static_fsinfo);

  /* returning static info about the filesystem */
  (*staticinfo) = global_fs_info;

  Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_static_fsinfo);

}

/**
 * FSAL_dynamic_fsinfo:
 * Return dynamic filesystem info such as
 * used size, free size, number of objects...
 *
 * \param filehandle (input):
 *        Handle of an object in the filesystem
 *        whom info is to be retrieved.
 * \param p_context (input):
 *        Authentication context for the operation (user,...).
 * \param dynamicinfo (output):
 *        Pointer to the static info of the filesystem.
 *
 * \return Major error codes:
 *      - ERR_FSAL_NO_ERROR     (no error)
 *      - ERR_FSAL_FAULT        (a NULL pointer was passed as mandatory argument)
 *      - Other error codes can be returned :
 *        ERR_FSAL_IO, ...
 */
fsal_status_t CEPHFSAL_dynamic_fsinfo(cephfsal_handle_t * filehandle,   /* IN */
				      cephfsal_op_context_t * p_context,        /* IN */
				      fsal_dynamicfsinfo_t * dynamicinfo    /* OUT */
    )
{
  int rc;
  struct statvfs st;

  /* sanity checks. */
  if(!filehandle || !dynamicinfo || !p_context)
    Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_dynamic_fsinfo);

  TakeTokenFSCall();

  rc=ceph_ll_statfs(filehandle->vi, &st);

  ReleaseTokenFSCall();

  if (rc < 0)
    Return(posix2fsal_error(rc), 0, INDEX_FSAL_dynamic_fsinfo);
  
  memset(dynamicinfo,sizeof(fsal_dynamicfsinfo_t), 0);
  dynamicinfo->total_bytes=st.f_frsize*st.f_blocks;
  dynamicinfo->free_bytes=st.f_frsize*st.f_bfree;
  dynamicinfo->avail_bytes=st.f_frsize*st.f_bavail;
  dynamicinfo->total_files=st.f_files;
  dynamicinfo->free_files=st.f_ffree;
  dynamicinfo->avail_files=st.f_favail;
  dynamicinfo->time_delta.seconds=0;
  dynamicinfo->time_delta.nseconds=1000;

  Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_dynamic_fsinfo);

}
