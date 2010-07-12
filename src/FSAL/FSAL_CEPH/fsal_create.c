/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 */

/**
 *
 * \file    fsal_create.c
 * \author  $Author: leibovic $
 * \date    $Date: 2006/01/24 13:45:36 $
 * \version $Revision: 1.18 $
 * \brief   Filesystem objects creation functions.
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "fsal.h"
#include "fsal_internal.h"
#include "fsal_convert.h"

/**
 * FSAL_create:
 * Create a regular file.
 *
 * \param parent_directory_handle (input):
 *        Handle of the parent directory where the file is to be created.
 * \param p_filename (input):
 *        Pointer to the name of the file to be created.
 * \param cred (input):
 *        Authentication context for the operation (user, export...).
 * \param accessmode (input):
 *        Mode for the file to be created.
 *        (the umask defined into the FSAL configuration file
 *        will be applied on it).
 * \param object_handle (output):
 *        Pointer to the handle of the created file.
 * \param object_attributes (optionnal input/output): 
 *        The postop attributes of the created file.
 *        As input, it defines the attributes that the caller
 *        wants to retrieve (by positioning flags into this structure)
 *        and the output is built considering this input
 *        (it fills the structure according to the flags it contains).
 *        Can be NULL.
 *
 * \return Major error codes :
 *        - ERR_FSAL_NO_ERROR     (no error)
 *        - ERR_FSAL_STALE        (parent_directory_handle does not address an existing object)
 *        - ERR_FSAL_FAULT        (a NULL pointer was passed as mandatory argument)
 *        - Other error codes can be returned :
 *          ERR_FSAL_ACCESS, ERR_FSAL_EXIST, ERR_FSAL_IO, ...
 *            
 *        NB: if getting postop attributes failed,
 *        the function does not return an error
 *        but the FSAL_ATTR_RDATTR_ERR bit is set in
 *        the object_attributes->asked_attributes field.
 */
fsal_status_t FSAL_create(fsal_handle_t * parent_directory_handle,      /* IN */
                          fsal_name_t * p_filename,     /* IN */
                          fsal_op_context_t * p_context,        /* IN */
                          fsal_accessmode_t accessmode, /* IN */
                          fsal_handle_t * object_handle,        /* OUT */
                          fsal_attrib_list_t * object_attributes        /* [ IN/OUT ] */
    )
{

  int rc;
  struct stat st;
  mode_t mode;
  int uid=FSAL_OP_CONTEXT_TO_UID(p_context);
  int gid=FSAL_OP_CONTEXT_TO_GID(p_context);
  char filename[FSAL_MAX_NAME_LEN];

  /* sanity checks.
   * note : object_attributes is optional.
   */
  if(!parent_directory_handle || !p_context || !object_handle || !p_filename)
    Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_create);

  mode = fsal2unix_mode(accessmode);
  mode = mode & ~global_fs_info.umask;

  FSAL_name2str(p_filename, filename, FSAL_MAX_NAME_LEN);

  TakeTokenFSCall();

  rc=ceph_ll_create(parent_directory_handle->vi, filename,
		    mode, 0, &st, uid, gid);
  ceph_ll_close(rc);

  ReleaseTokenFSCall();

  if (rc < 0)
    Return(posix2fsal_error(rc), 0, INDEX_FSAL_create);

  stat2fsal_fh(&st, object_handle);

  if(object_attributes)
    {
      fsal_status_t status;
      status = posix2fsal_attributes(&st, object_attributes);
      if(FSAL_IS_ERROR(status))
	{
	  fsal_status_t status;
	  FSAL_CLEAR_MASK(object_attributes->asked_attributes);
	  FSAL_SET_MASK(object_attributes->asked_attributes, FSAL_ATTR_RDATTR_ERR);
	  Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_getattrs);
	}
    }

  /* OK */
  Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_create);
}

/**
 * FSAL_mkdir:
 * Create a directory.
 *
 * \param parent_directory_handle (input):
 *        Handle of the parent directory where
 *        the subdirectory is to be created.
 * \param p_dirname (input):
 *        Pointer to the name of the directory to be created.
 * \param cred (input):
 *        Authentication context for the operation (user,...).
 * \param accessmode (input):
 *        Mode for the directory to be created.
 *        (the umask defined into the FSAL configuration file
 *        will be applied on it).
 * \param object_handle (output):
 *        Pointer to the handle of the created directory.
 * \param object_attributes (optionnal input/output): 
 *        The attributes of the created directory.
 *        As input, it defines the attributes that the caller
 *        wants to retrieve (by positioning flags into this structure)
 *        and the output is built considering this input
 *        (it fills the structure according to the flags it contains).
 *        May be NULL.
 *
 * \return Major error codes :
 *        - ERR_FSAL_NO_ERROR     (no error)
 *        - ERR_FSAL_STALE        (parent_directory_handle does not address an existing object)
 *        - ERR_FSAL_FAULT        (a NULL pointer was passed as mandatory argument)
 *        - Other error codes can be returned :
 *          ERR_FSAL_ACCESS, ERR_FSAL_EXIST, ERR_FSAL_IO, ...
 *            
 *        NB: if getting postop attributes failed,
 *        the function does not return an error
 *        but the FSAL_ATTR_RDATTR_ERR bit is set in
 *        the object_attributes->asked_attributes field.
 */
fsal_status_t FSAL_mkdir(fsal_handle_t * parent_directory_handle,       /* IN */
                         fsal_name_t * p_dirname,       /* IN */
                         fsal_op_context_t * p_context, /* IN */
                         fsal_accessmode_t accessmode,  /* IN */
                         fsal_handle_t * object_handle, /* OUT */
                         fsal_attrib_list_t * object_attributes /* [ IN/OUT ] */
    )
{

  int rc;
  struct stat st;
  int uid=FSAL_OP_CONTEXT_TO_UID(p_context);
  int gid=FSAL_OP_CONTEXT_TO_GID(p_context);
  mode_t mode;
  char name[FSAL_MAX_NAME_LEN];
  
  /* sanity checks.
   * note : object_attributes is optional.
   */
  if(!parent_directory_handle || !p_context || !object_handle || !p_dirname)
    Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_mkdir);

  mode = fsal2unix_mode(accessmode);
  mode = mode & ~global_fs_info.umask;

  FSAL_name2str(p_dirname, name, FSAL_MAX_NAME_LEN);
  TakeTokenFSCall();

  rc=ceph_ll_mkdir(parent_directory_handle->vi, p_dirname->name,
		   mode, &st, uid, gid);

  ReleaseTokenFSCall();

  if (rc < 0)
    Return(posix2fsal_error(rc), 0, INDEX_FSAL_mkdir);

  stat2fsal_fh(&st, object_handle);

  if(object_attributes)
    {
      fsal_status_t status;
      status = posix2fsal_attributes(&st, object_attributes);
      if(FSAL_IS_ERROR(status))
	{
	  FSAL_CLEAR_MASK(object_attributes->asked_attributes);
	  FSAL_SET_MASK(object_attributes->asked_attributes, FSAL_ATTR_RDATTR_ERR);
	  Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_getattrs);
	}
    }

  /* OK */
  Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_mkdir);

}

/**
 * FSAL_link:
 * Create a hardlink.
 *
 * \param target_handle (input):
 *        Handle of the target object.
 * \param dir_handle (input):
 *        Pointer to the directory handle where
 *        the hardlink is to be created.
 * \param p_link_name (input):
 *        Pointer to the name of the hardlink to be created.
 * \param cred (input):
 *        Authentication context for the operation (user,...).
 * \param accessmode (input):
 *        Mode for the directory to be created.
 *        (the umask defined into the FSAL configuration file
 *        will be applied on it).
 * \param attributes (optionnal input/output): 
 *        The post_operation attributes of the linked object.
 *        As input, it defines the attributes that the caller
 *        wants to retrieve (by positioning flags into this structure)
 *        and the output is built considering this input
 *        (it fills the structure according to the flags it contains).
 *        May be NULL.
 *
 * \return Major error codes :
 *        - ERR_FSAL_NO_ERROR     (no error)
 *        - ERR_FSAL_STALE        (target_handle or dir_handle does not address an existing object)
 *        - ERR_FSAL_FAULT        (a NULL pointer was passed as mandatory argument)
 *        - Other error codes can be returned :
 *          ERR_FSAL_ACCESS, ERR_FSAL_EXIST, ERR_FSAL_IO, ...
 *            
 *        NB: if getting postop attributes failed,
 *        the function does not return an error
 *        but the FSAL_ATTR_RDATTR_ERR bit is set in
 *        the attributes->asked_attributes field.
 */
fsal_status_t FSAL_link(fsal_handle_t * target_handle,  /* IN */
                        fsal_handle_t * dir_handle,     /* IN */
                        fsal_name_t * p_link_name,      /* IN */
                        fsal_op_context_t * p_context,  /* IN */
                        fsal_attrib_list_t * attributes /* [ IN/OUT ] */
    )
{

  int rc;
  struct stat st;
  int uid=FSAL_OP_CONTEXT_TO_UID(p_context);
  int gid=FSAL_OP_CONTEXT_TO_GID(p_context);
  char name[FSAL_MAX_NAME_LEN];

  /* sanity checks.
   * note : attributes is optional.
   */
  if(!target_handle || !dir_handle || !p_context || !p_link_name)
    Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_link);

  /* Tests if hardlinking is allowed by configuration. */

  if(!global_fs_info.link_support)
    Return(ERR_FSAL_NOTSUPP, 0, INDEX_FSAL_link);

  FSAL_name2str(p_link_name, name, FSAL_MAX_NAME_LEN);

  TakeTokenFSCall();

  ceph_ll_link(target_handle->vi, dir_handle->vi,
	       name, &st, uid, gid);

  ReleaseTokenFSCall();

  if (rc < 0)
    Return(posix2fsal_error(rc), 0, INDEX_FSAL_link);

  if(attributes)
    {
      fsal_status_t status;
      status = posix2fsal_attributes(&st, attributes);
      if(FSAL_IS_ERROR(status))
	{
	  FSAL_CLEAR_MASK(attributes->asked_attributes);
	  FSAL_SET_MASK(attributes->asked_attributes, FSAL_ATTR_RDATTR_ERR);
	  Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_getattrs);
	}
    }

  /* OK */
  Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_link);

}

/**
 * FSAL_mknode:
 * Create a special object in the filesystem.
 * Not supported in upper layers in this GANESHA's version.
 *
 * \return ERR_FSAL_NOTSUPP.
 */
fsal_status_t FSAL_mknode(fsal_handle_t * parentdir_handle,     /* IN */
                          fsal_name_t * p_node_name,    /* IN */
                          fsal_op_context_t * p_context,        /* IN */
                          fsal_accessmode_t accessmode, /* IN */
                          fsal_nodetype_t nodetype,     /* IN */
                          fsal_dev_t * dev,     /* IN */
                          fsal_handle_t * p_object_handle,      /* OUT (handle to the created node) */
                          fsal_attrib_list_t * node_attributes  /* [ IN/OUT ] */
    )
{

  /* sanity checks.
   * note : link_attributes is optional.
   */
  if(!parentdir_handle || !p_context || !nodetype || !dev || !p_node_name)
    Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_mknode);

  /* Not implemented */
  Return(ERR_FSAL_NOTSUPP, 0, INDEX_FSAL_mknode);

}
