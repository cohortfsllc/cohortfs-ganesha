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
 * \file    fsal_dirs.c
 * \brief   Directory browsing operations.
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "fsal.h"
#include "fsal_internal.h"
#include "fsal_convert.h"
#include <string.h>

/**
 * FSAL_opendir :
 *     Opens a directory for reading its content.
 *     
 * \param dir_handle (input)
 *         the handle of the directory to be opened.
 * \param p_context (input)
 *         Permission context for the operation (user, export context...).
 * \param dir_descriptor (output)
 *         pointer to an allocated structure that will receive
 *         directory stream informations, on successfull completion.
 * \param dir_attributes (optional output)
 *         On successfull completion,the structure pointed
 *         by dir_attributes receives the new directory attributes.
 *         Can be NULL.
 * 
 * \return Major error codes :
 *        - ERR_FSAL_NO_ERROR     (no error)
 *        - ERR_FSAL_ACCESS       (user does not have read permission on directory)
 *        - ERR_FSAL_STALE        (dir_handle does not address an existing object)
 *        - ERR_FSAL_FAULT        (a NULL pointer was passed as mandatory argument)
 *        - Other error codes can be returned :
 *          ERR_FSAL_IO, ...
 */
fsal_status_t CEPHFSAL_opendir(cephfsal_handle_t * dir_handle,  /* IN */
			       cephfsal_op_context_t * p_context,       /* IN */
			       cephfsal_dir_t * dir_descriptor, /* OUT */
			       fsal_attrib_list_t * dir_attributes  /* [ IN/OUT ] */
    )
{
  fsal_status_t status;
  int rc;
  int uid=FSAL_OP_CONTEXT_TO_UID(p_context);
  int gid=FSAL_OP_CONTEXT_TO_GID(p_context);
  void* dh;

  /* sanity checks
   * note : dir_attributes is optionnal.
   */
  if(!dir_handle || !p_context || !dir_descriptor)
    Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_opendir);

  TakeTokenFSCall();
  rc=ceph_ll_opendir(VINODE(dir_handle), &dh, uid, gid);
  ReleaseTokenFSCall();

  if (rc < 0)
    Return(posix2fsal_error(rc), 0, INDEX_FSAL_opendir);

  dir_descriptor->dh=(DIR*) dh;
  dir_descriptor->vi=VINODE(dir_handle);
  dir_descriptor->ctx=*p_context;

  if(dir_attributes)
    {
      status = CEPHFSAL_getattrs(dir_handle, p_context, dir_attributes);

      if(FSAL_IS_ERROR(status))
        {
          FSAL_CLEAR_MASK(dir_attributes->asked_attributes);
          FSAL_SET_MASK(dir_attributes->asked_attributes, FSAL_ATTR_RDATTR_ERR);
        }
    }

  Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_opendir);

}

/**
 * FSAL_readdir :
 *     Read the entries of an opened directory.
 *     
 * \param dir_descriptor (input):
 *        Pointer to the directory descriptor filled by FSAL_opendir.
 * \param start_position (input):
 *        Cookie that indicates the first object to be read during
 *        this readdir operation.
 *        This should be :
 *        - FSAL_READDIR_FROM_BEGINNING for reading the content
 *          of the directory from the beginning.
 *        - The end_position parameter returned by the previous
 *          call to FSAL_readdir.
 * \param get_attr_mask (input)
 *        Specify the set of attributes to be retrieved for directory entries.
 * \param buffersize (input)
 *        The size (in bytes) of the buffer where
 *        the direntries are to be stored.
 * \param pdirent (output)
 *        Adresse of the buffer where the direntries are to be stored.
 * \param end_position (output)
 *        Cookie that indicates the current position in the directory.
 * \param nb_entries (output)
 *        Pointer to the number of entries read during the call.
 * \param end_of_dir (output)
 *        Pointer to a boolean that indicates if the end of dir
 *        has been reached during the call.
 * 
 * \return Major error codes :
 *        - ERR_FSAL_NO_ERROR     (no error)
 *        - ERR_FSAL_FAULT        (a NULL pointer was passed as mandatory argument) 
 *        - Other error codes can be returned :
 *          ERR_FSAL_IO, ...
 */
fsal_status_t CEPHFSAL_readdir(cephfsal_dir_t * dir_descriptor, /* IN */
			       cephfsal_cookie_t start_position,        /* IN */
			       fsal_attrib_mask_t get_attr_mask,    /* IN */
			       fsal_mdsize_t buffersize,    /* IN */
			       fsal_dirent_t * pdirent,     /* OUT */
			       cephfsal_cookie_t * end_position,        /* OUT */
			       fsal_count_t * nb_entries,   /* OUT */
			       fsal_boolean_t * end_of_dir  /* OUT */
    )
{

  fsal_status_t status;
  struct dirent de;
  int rc;
  unsigned int max_entries=buffersize/sizeof(fsal_dirent_t);
  int uid=dir_descriptor->ctx.credential.user;
  int gid=dir_descriptor->ctx.credential.group;

  /* sanity checks */

  if(!dir_descriptor || !pdirent || !end_position || !nb_entries || !end_of_dir)
    Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_readdir);

  *end_of_dir=FALSE;
  *nb_entries=0;

  TakeTokenFSCall();

  ceph_seekdir(DH(dir_descriptor), COOKIE(start_position));

  while ((*nb_entries <= max_entries) && !(*end_of_dir))
    {
      struct stat st; /* No easy way to make a precise
			 readdirplus_r */

      memset(&pdirent[*nb_entries], sizeof(fsal_dirent_t),0);
      memset(&de, sizeof(struct dirent), 0);
      memset(&st, sizeof(struct stat), 0);

      TakeTokenFSCall();
      rc=ceph_readdirplus_r(DH(dir_descriptor), &de, &st, 0);
      if (rc < 0) /* Error */
	Return(posix2fsal_error(rc), 0, INDEX_FSAL_readdir);

      else if (rc == 1) /* Got a dirent */
	{
          /* skip . and .. */
          if(!strcmp(de.d_name, ".") || !strcmp(de.d_name, ".."))
            continue;
	  pdirent[*nb_entries].handle.data.vi.ino.val=st.st_ino;
	  pdirent[*nb_entries].handle.data.vi.snapid.val=st.st_dev;

	  status = FSAL_str2name(de.d_name, FSAL_MAX_NAME_LEN,
				 &(pdirent[*nb_entries].name));
          if(FSAL_IS_ERROR(status))
            ReturnStatus(status, INDEX_FSAL_readdir);
	    
	  COOKIE(pdirent[*nb_entries].cookie)=ceph_telldir(DH(dir_descriptor));
	  pdirent[*nb_entries].attributes.asked_attributes=get_attr_mask;

	  status =
	    CEPHFSAL_getattrs(&(pdirent[*nb_entries].handle),
			      &(dir_descriptor->ctx),
			      &(pdirent[*nb_entries].attributes));
	  if(FSAL_IS_ERROR(status))
	    {
	      FSAL_CLEAR_MASK(pdirent[*nb_entries].attributes.asked_attributes);
	      FSAL_SET_MASK(pdirent[*nb_entries].attributes.asked_attributes,
			    FSAL_ATTR_RDATTR_ERR); 
	    }
	  if (*nb_entries != 0)
	    {
	      pdirent[(*nb_entries)-1].nextentry=&(pdirent[*nb_entries]);
	    }
	  (*nb_entries)++;
	}

      else if (rc == 0) /* EOF */
	*end_of_dir=TRUE;

      else /* Can't happen */
	{
	  abort();
	}
    }
  end_position->cookie=ceph_telldir(DH(dir_descriptor));

  Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_readdir);
}

/**
 * FSAL_closedir :
 * Free the resources allocated for reading directory entries.
 *     
 * \param dir_descriptor (input):
 *        Pointer to a directory descriptor filled by FSAL_opendir.
 * 
 * \return Major error codes :
 *        - ERR_FSAL_NO_ERROR     (no error)
 *        - ERR_FSAL_FAULT        (a NULL pointer was passed as mandatory argument)
 *        - Other error codes can be returned :
 *          ERR_FSAL_IO, ...
 */
fsal_status_t CEPHFSAL_closedir(fsal_dir_t * dir_descriptor /* IN */
    )
{

  int rc;

  /* sanity checks */
  if(!dir_descriptor)
    Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_closedir);

  TakeTokenFSCall();
  rc=ceph_ll_releasedir(DH(dir_descriptor));
  ReleaseTokenFSCall();

  if (rc < 0)
    Return(posix2fsal_error(rc), 0, INDEX_FSAL_closedir);
  
  Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_closedir);

}
