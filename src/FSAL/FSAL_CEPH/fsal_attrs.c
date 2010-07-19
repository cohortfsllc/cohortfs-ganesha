/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 */

/**
 *
 * \file    fsal_attrs.c
 * \author  $Author: leibovic $
 * \date    $Date: 2005/09/09 15:22:49 $
 * \version $Revision: 1.19 $
 * \brief   Attributes functions.
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "fsal.h"
#include "fsal_internal.h"
#include "fsal_convert.h"

/**
 * FSAL_getattrs:
 * Get attributes for the object specified by its filehandle.
 *
 * \param filehandle (input):
 *        The handle of the object to get parameters.
 * \param p_context (input):
 *        Authentication context for the operation (user, export...).
 * \param object_attributes (mandatory input/output):
 *        The retrieved attributes for the object.
 *        As input, it defines the attributes that the caller
 *        wants to retrieve (by positioning flags into this structure)
 *        and the output is built considering this input
 *        (it fills the structure according to the flags it contains).
 *
 * \return Major error codes :
 *        - ERR_FSAL_NO_ERROR     (no error)
 *        - ERR_FSAL_STALE        (object_handle does not address an existing object)
 *        - ERR_FSAL_FAULT        (a NULL pointer was passed as mandatory argument) 
 *        - Another error code if an error occured.
 */
fsal_status_t FSAL_getattrs(fsal_handle_t * filehandle, /* IN */
                            fsal_op_context_t * p_context,      /* IN */
                            fsal_attrib_list_t * object_attributes      /* IN/OUT */
    )
{

  int rc;
  fsal_status_t status;
  struct stat_precise st;
  int uid=FSAL_OP_CONTEXT_TO_UID(p_context);
  int gid=FSAL_OP_CONTEXT_TO_GID(p_context);

  /* sanity checks.
   * note : object_attributes is mandatory in FSAL_getattrs.
   */
  if(!filehandle || !p_context || !object_attributes)
    Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_getattrs);

  TakeTokenFSCall();

  rc=ceph_ll_getattr_precise(filehandle->vi, &st, uid, gid);

  ReleaseTokenFSCall();

  if (rc < 0)
    Return(posix2fsal_error(rc), 0, INDEX_FSAL_getattrs);

  /* convert attributes */
  status = posix2fsal_attributes(&st, object_attributes);
  if(FSAL_IS_ERROR(status))
    {
      FSAL_CLEAR_MASK(object_attributes->asked_attributes);
      FSAL_SET_MASK(object_attributes->asked_attributes, FSAL_ATTR_RDATTR_ERR);
      Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_getattrs);
    }

  Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_getattrs);

}

/**
 * FSAL_setattrs:
 * Set attributes for the object specified by its filehandle.
 *
 * \param filehandle (input):
 *        The handle of the object to get parameters.
 * \param p_context (input):
 *        Authentication context for the operation (user,...).
 * \param attrib_set (mandatory input):
 *        The attributes to be set for the object.
 *        It defines the attributes that the caller
 *        wants to set and their values.
 * \param object_attributes (optionnal input/output):
 *        The post operation attributes for the object.
 *        As input, it defines the attributes that the caller
 *        wants to retrieve (by positioning flags into this structure)
 *        and the output is built considering this input
 *        (it fills the structure according to the flags it contains).
 *        May be NULL.
 *
 * \return Major error codes :
 *        - ERR_FSAL_NO_ERROR     (no error)
 *        - ERR_FSAL_STALE        (object_handle does not address an existing object)
 *        - ERR_FSAL_INVAL        (tried to modify a read-only attribute)
 *        - ERR_FSAL_ATTRNOTSUPP  (tried to modify a non-supported attribute)
 *        - ERR_FSAL_FAULT        (a NULL pointer was passed as mandatory argument)
 *        - Another error code if an error occured.
 *        NB: if getting postop attributes failed,
 *        the function does not return an error
 *        but the FSAL_ATTR_RDATTR_ERR bit is set in
 *        the object_attributes->asked_attributes field.
 */

fsal_status_t FSAL_setattrs(fsal_handle_t * filehandle, /* IN */
                            fsal_op_context_t * p_context,      /* IN */
                            fsal_attrib_list_t * attrib_set,    /* IN */
                            fsal_attrib_list_t * object_attributes      /* [ IN/OUT ] */
    )
{

  int rc;
  int mask=0;
  struct stat_precise st;
  int uid;
  int gid;
  fsal_attrib_list_t attrs;

  uid=FSAL_OP_CONTEXT_TO_UID(p_context);
  gid=FSAL_OP_CONTEXT_TO_GID(p_context);

  /* sanity checks.
   * note : object_attributes is optional.
   */
  if(!filehandle || !p_context || !attrib_set)
    Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_setattrs);

  /* local copy of attributes */
  attrs = *attrib_set;

  /* First, check that FSAL attributes changes are allowed. */

  /* Is it allowed to change times ? */

  if(!global_fs_info.cansettime)
    {
      if(attrs.asked_attributes
         & (FSAL_ATTR_ATIME | FSAL_ATTR_CREATION | FSAL_ATTR_CTIME | FSAL_ATTR_MTIME))
        {

          /* handled as an unsettable attribute. */
          Return(ERR_FSAL_INVAL, 0, INDEX_FSAL_setattrs);
        }

    }

  /* apply umask, if mode attribute is to be changed */

  if(FSAL_TEST_MASK(attrs.asked_attributes, FSAL_ATTR_MODE))
    {
      attrs.mode &= (~global_fs_info.umask);
    }

  /* Build flags and struct stat_precise */

  if(FSAL_TEST_MASK(attrs.asked_attributes, FSAL_ATTR_MODE))
    {
      mask |= CEPH_SETATTR_MODE;
      st.st_mode = fsal2unix_mode(attrs.mode);
    }
  if(FSAL_TEST_MASK(attrs.asked_attributes, FSAL_ATTR_OWNER))
    {
      mask |= CEPH_SETATTR_UID;
      st.st_uid = attrs.owner;
    }
  if(FSAL_TEST_MASK(attrs.asked_attributes, FSAL_ATTR_GROUP))
    {
      mask |= CEPH_SETATTR_UID;
      st.st_gid = attrs.group;
    }
  if(FSAL_TEST_MASK(attrs.asked_attributes, FSAL_ATTR_ATIME))
    {
      mask |= CEPH_SETATTR_ATIME;
      st.st_atime_sec=attrs.atime.seconds;
      st.st_atime_micro=attrs.atime.nseconds/1000;
    }
  if (FSAL_TEST_MASK(attrs.asked_attributes, FSAL_ATTR_MTIME))
    {
      mask |= CEPH_SETATTR_MTIME;
      st.st_mtime_sec=attrs.mtime.seconds;
      st.st_mtime_micro=attrs.mtime.nseconds/1000;
    }
  if (FSAL_TEST_MASK(attrs.asked_attributes, FSAL_ATTR_CTIME))
    {
      mask |= CEPH_SETATTR_CTIME;
      st.st_ctime_sec=attrs.ctime.seconds;
      st.st_ctime_micro=attrs.ctime.nseconds/1000;
    }

  TakeTokenFSCall();

  rc=ceph_ll_setattr_precise(filehandle->vi, &st, mask, uid, gid);

  ReleaseTokenFSCall();

  if (rc < 0)
    Return(posix2fsal_error(rc), 0, INDEX_FSAL_getattrs);

  if(object_attributes)
    {
      fsal_status_t status;
      status = FSAL_getattrs(filehandle, p_context, object_attributes);

      /* on error, we set a special bit in the mask. */
      if(FSAL_IS_ERROR(status))
        {
          FSAL_CLEAR_MASK(object_attributes->asked_attributes);
          FSAL_SET_MASK(object_attributes->asked_attributes, FSAL_ATTR_RDATTR_ERR);
        }

    }

  Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_setattrs);
}
