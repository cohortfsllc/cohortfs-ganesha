/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 */

/**
 * \file    fsal_fileop.c
 * \author  $Author: leibovic $
 * \date    $Date: 2006/02/15 14:26:10 $
 * \version $Revision: 1.11 $
 * \brief   Files operations.
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "fsal.h"
#include "fsal_internal.h"
#include "fsal_convert.h"
#include <stdio.h>

/**
 * FSAL_open:
 * Open a regular file for reading/writing its data content.
 *
 * \param filehandle (input):
 *        Handle of the file to be read/modified.
 * \param cred (input):
 *        Authentication context for the operation (user,...).
 * \param openflags (input):
 *        Flags that indicates behavior for file opening and access.
 *        This is an inclusive OR of the following values
 *        ( such of them are not compatible) : 
 *        - FSAL_O_RDONLY: opening file for reading only.
 *        - FSAL_O_RDWR: opening file for reading and writing.
 *        - FSAL_O_WRONLY: opening file for writting only.
 *        - FSAL_O_APPEND: always write at the end of the file.
 *        - FSAL_O_TRUNC: truncate the file to 0 on opening.
 * \param file_descriptor (output):
 *        The file descriptor to be used for FSAL_read/write operations.
 * \param file_attributes (optionnal input/output):
 *        Post operation attributes.
 *        As input, it defines the attributes that the caller
 *        wants to retrieve (by positioning flags into this structure)
 *        and the output is built considering this input
 *        (it fills the structure according to the flags it contains).
 *
 * \return Major error codes:
 *      - ERR_FSAL_NO_ERROR     (no error)
 *      - ERR_FSAL_ACCESS       (user doesn't have the permissions for opening the file)
 *      - ERR_FSAL_STALE        (filehandle does not address an existing object) 
 *      - ERR_FSAL_INVAL        (filehandle does not address a regular file,
 *                               or open flags are conflicting)
 *      - ERR_FSAL_FAULT        (a NULL pointer was passed as mandatory argument)
 *      - Other error codes can be returned :
 *        ERR_FSAL_IO, ...
 */
fsal_status_t FSAL_open(fsal_handle_t * filehandle,     /* IN */
                        fsal_op_context_t * p_context,  /* IN */
                        fsal_openflags_t openflags,     /* IN */
                        fsal_file_t * file_descriptor,  /* OUT */
                        fsal_attrib_list_t * file_attributes    /* [ IN/OUT ] */
    )
{

  int rc;
  int uid=FSAL_OP_CONTEXT_TO_UID(p_context);
  int gid=FSAL_OP_CONTEXT_TO_GID(p_context);
  int posix_flags=0;
  Fh *desc;

  /* sanity checks.
   * note : file_attributes is optional.
   */
  if(!filehandle || !p_context || !file_descriptor)
    Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_open);

  rc = fsal2posix_openflags(openflags, &posix_flags);

  /* flags conflicts. */
  if(rc)
    {
      DisplayLogJdLevel(fsal_log, NIV_EVENT, "Invalid/conflicting flags : %#X",
                        openflags);
      Return(rc, 0, INDEX_FSAL_open);
    }

  TakeTokenFSCall();

  /*  rc=ceph_ll_open(filehandle->vi, posix_flags, file_descriptor, uid, gid); */
  rc=ceph_ll_open(filehandle->vi, posix_flags, &desc, uid, gid);

  *file_descriptor=desc;

  ReleaseTokenFSCall();

  if (rc < 0)
    Return(posix2fsal_error(rc), 0, INDEX_FSAL_open);

  if(file_attributes)
    {
      fsal_status_t status;
      
      status=FSAL_getattrs(filehandle, p_context, file_attributes);
      if(FSAL_IS_ERROR(status))
        {
          FSAL_CLEAR_MASK(file_attributes->asked_attributes);
          FSAL_SET_MASK(file_attributes->asked_attributes, FSAL_ATTR_RDATTR_ERR);
        }
    }

  Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_open);
}

/**
 * FSAL_open_byname:
 * Open a regular file for reading/writing its data content.
 *
 * \param dirhandle (input):
 *        Handle of the directory that contain the file to be read/modified.
 * \param filename (input):
 *        Name of the file to be read/modified
 * \param cred (input):
 *        Authentication context for the operation (user,...).
 * \param openflags (input):
 *        Flags that indicates behavior for file opening and access.
 *        This is an inclusive OR of the following values
 *        ( such of them are not compatible) : 
 *        - FSAL_O_RDONLY: opening file for reading only.
 *        - FSAL_O_RDWR: opening file for reading and writing.
 *        - FSAL_O_WRONLY: opening file for writting only.
 *        - FSAL_O_APPEND: always write at the end of the file.
 *        - FSAL_O_TRUNC: truncate the file to 0 on opening.
 * \param file_descriptor (output):
 *        The file descriptor to be used for FSAL_read/write operations.
 * \param file_attributes (optionnal input/output):
 *        Post operation attributes.
 *        As input, it defines the attributes that the caller
 *        wants to retrieve (by positioning flags into this structure)
 *        and the output is built considering this input
 *        (it fills the structure according to the flags it contains).
 *
 * \return Major error codes:
 *      - ERR_FSAL_NO_ERROR     (no error)
 *      - ERR_FSAL_ACCESS       (user doesn't have the permissions for opening the file)
 *      - ERR_FSAL_STALE        (filehandle does not address an existing object) 
 *      - ERR_FSAL_INVAL        (filehandle does not address a regular file,
 *                               or open flags are conflicting)
 *      - ERR_FSAL_FAULT        (a NULL pointer was passed as mandatory argument)
 *      - Other error codes can be returned :
 *        ERR_FSAL_IO, ...
 */

fsal_status_t FSAL_open_by_name(fsal_handle_t * dirhandle,      /* IN */
                                fsal_name_t * filename, /* IN */
                                fsal_op_context_t * p_context,  /* IN */
                                fsal_openflags_t openflags,     /* IN */
                                fsal_file_t * file_descriptor,  /* OUT */
                                fsal_attrib_list_t * file_attributes /* [ IN/OUT ] */ )
{
  fsal_status_t fsal_status;
  fsal_handle_t filehandle;

  if(!dirhandle || !filename || !p_context || !file_descriptor)
    Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_open_by_name);

  fsal_status = FSAL_lookup(dirhandle, filename, p_context, &filehandle, file_attributes);
  if(FSAL_IS_ERROR(fsal_status))
    return fsal_status;

  return FSAL_open(&filehandle, p_context, openflags, file_descriptor, file_attributes);
}

/**
 * FSAL_read:
 * Perform a read operation on an opened file.
 *
 * \param file_descriptor (input):
 *        The file descriptor returned by FSAL_open.
 * \param seek_descriptor (optional input):
 *        Specifies the position where data is to be read.
 *        If not specified, data will be read at the current position.
 * \param buffer_size (input):
 *        Amount (in bytes) of data to be read.
 * \param buffer (output):
 *        Address where the read data is to be stored in memory.
 * \param read_amount (output):
 *        Pointer to the amount of data (in bytes) that have been read
 *        during this call.
 * \param end_of_file (output):
 *        Pointer to a boolean that indicates whether the end of file
 *        has been reached during this call.
 *
 * \return Major error codes:
 *      - ERR_FSAL_NO_ERROR     (no error)
 *      - ERR_FSAL_INVAL        (invalid parameter)
 *      - ERR_FSAL_NOT_OPENED   (tried to read in a non-opened fsal_file_t)
 *      - ERR_FSAL_FAULT        (a NULL pointer was passed as mandatory argument)
 *      - Other error codes can be returned :
 *        ERR_FSAL_IO, ...
 */
fsal_status_t FSAL_read(fsal_file_t * file_descriptor,  /* IN */
                        fsal_seek_t * seek_descriptor,  /* [IN] */
                        fsal_size_t buffer_size,        /* IN */
                        caddr_t buffer, /* OUT */
                        fsal_size_t * read_amount,      /* OUT */
                        fsal_boolean_t * end_of_file    /* OUT */
    )
{
  int nb_read=0;
  int whence;
  fsal_off_t offset;

  /* sanity checks. */

  if(!file_descriptor || !buffer || !read_amount || !end_of_file)
    Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_read);

  if (seek_descriptor)
    {
      if (seek_descriptor->whence == FSAL_SEEK_SET)
	whence = SEEK_SET;
      else if (seek_descriptor->whence == FSAL_SEEK_CUR)
	whence = SEEK_CUR;
      else if (seek_descriptor->whence == FSAL_SEEK_END)
	whence = SEEK_END;
    }

  TakeTokenFSCall();

  if (seek_descriptor)
    {
      offset=ceph_ll_lseek(*file_descriptor, seek_descriptor->offset,
			   whence);
    }
  else
    {
      offset=ceph_ll_lseek(*file_descriptor, 0, SEEK_CUR);
    }
      
  ReleaseTokenFSCall();

  if (offset < 0)
    Return(posix2fsal_error(offset), 0, INDEX_FSAL_read);

  TakeTokenFSCall();

  nb_read=ceph_ll_read(*file_descriptor, offset, buffer_size, buffer);

  ReleaseTokenFSCall();

  if (nb_read < 0)
    Return(posix2fsal_error(nb_read), 0, INDEX_FSAL_read);
  else if ((buffer_size != 0) &&
	   (nb_read == 0))
    *end_of_file=TRUE;

  *read_amount=nb_read;

  Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_read);
}

/**
 * FSAL_write:
 * Perform a write operation on an opened file.
 *
 * \param file_descriptor (input):
 *        The file descriptor returned by FSAL_open.
 * \param seek_descriptor (optional input):
 *        Specifies the position where data is to be written.
 *        If not specified, data will be written at the current position.
 * \param buffer_size (input):
 *        Amount (in bytes) of data to be written.
 * \param buffer (input):
 *        Address in memory of the data to write to file.
 * \param write_amount (output):
 *        Pointer to the amount of data (in bytes) that have been written
 *        during this call.
 *
 * \return Major error codes:
 *      - ERR_FSAL_NO_ERROR     (no error)
 *      - ERR_FSAL_INVAL        (invalid parameter)
 *      - ERR_FSAL_NOT_OPENED   (tried to write in a non-opened fsal_file_t)
 *      - ERR_FSAL_FAULT        (a NULL pointer was passed as mandatory argument)
 *      - Other error codes can be returned :
 *        ERR_FSAL_IO, ERR_FSAL_NOSPC, ERR_FSAL_DQUOT...
 */
fsal_status_t FSAL_write(fsal_file_t * file_descriptor, /* IN */
                         fsal_seek_t * seek_descriptor, /* IN */
                         fsal_size_t buffer_size,       /* IN */
                         caddr_t buffer,        /* IN */
                         fsal_size_t * write_amount     /* OUT */
    )
{
  int nb_written=0;
  int whence;
  fsal_off_t offset;

  /* sanity checks. */
  if(!file_descriptor || !buffer || !write_amount)
    Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_write);

  if (seek_descriptor)
    {
      if (seek_descriptor->whence == FSAL_SEEK_SET)
	whence = SEEK_SET;
      else if (seek_descriptor->whence == FSAL_SEEK_CUR)
	whence = SEEK_CUR;
      else if (seek_descriptor->whence == FSAL_SEEK_END)
	whence = SEEK_END;
    }

  TakeTokenFSCall();

  if (seek_descriptor)
    {
      offset=ceph_ll_lseek(*file_descriptor, seek_descriptor->offset,
			   whence);
    }
  else
    {
      offset=ceph_ll_lseek(*file_descriptor, 0, SEEK_CUR);
    }
      
  ReleaseTokenFSCall();

  if (offset < 0)
    Return(posix2fsal_error(offset), 0, INDEX_FSAL_read);

  TakeTokenFSCall();

  nb_written=ceph_ll_write(*file_descriptor, offset, buffer_size,
			   buffer);
  
  ReleaseTokenFSCall();

  if (nb_written < 0)
    Return(posix2fsal_error(nb_written), 0, INDEX_FSAL_read);

  *write_amount=nb_written;

  Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_write);

}

/**
 * FSAL_close:
 * Free the resources allocated by the FSAL_open call.
 *
 * \param file_descriptor (input):
 *        The file descriptor returned by FSAL_open.
 *
 * \return Major error codes:
 *        - ERR_FSAL_NO_ERROR     (no error)
 *        - ERR_FSAL_FAULT        (a NULL pointer was passed as mandatory argument)
 *        - Other error codes can be returned :
 *          ERR_FSAL_IO, ...
 */

fsal_status_t FSAL_close(fsal_file_t * file_descriptor  /* IN */
    )
{

  int rc;

  /* sanity checks. */
  if(!file_descriptor)
    Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_close);

  /* This is because of a bug in the cache layer which must be fixed
     later.  Right now, we work around it. */
  
  if(!(*file_descriptor))
    Return(ERR_FSAL_NOT_OPENED, 0, INDEX_FSAL_close);

  TakeTokenFSCall();

  rc=ceph_ll_close(*file_descriptor);

  ReleaseTokenFSCall();

  *file_descriptor=NULL;

  if (rc != 0)
    Return(posix2fsal_error(rc), 0, INDEX_FSAL_read);

  Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_close);

}

/* Some unsupported calls used in FSAL_PROXY, just for permit the ganeshell to compile */
fsal_status_t FSAL_open_by_fileid(fsal_handle_t * filehandle,   /* IN */
                                  fsal_u64_t fileid,    /* IN */
                                  fsal_op_context_t * p_context,        /* IN */
                                  fsal_openflags_t openflags,   /* IN */
                                  fsal_file_t * file_descriptor,        /* OUT */
                                  fsal_attrib_list_t * file_attributes /* [ IN/OUT ] */ )
{
  Return(ERR_FSAL_NOTSUPP, 0, INDEX_FSAL_open_by_fileid);
}

fsal_status_t FSAL_close_by_fileid(fsal_file_t * file_descriptor /* IN */ ,
                                   fsal_u64_t fileid)
{
  Return(ERR_FSAL_NOTSUPP, 0, INDEX_FSAL_open_by_fileid);
}
