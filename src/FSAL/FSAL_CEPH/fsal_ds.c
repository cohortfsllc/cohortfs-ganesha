/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright (C) 2010 The Linux Box Corporation
 * All Rights Reserved
 *
 * Contributor: Adam C. Emerson
 *
 */

/**
 * \file    fsal_ds.c
 * \brief   DS realisation for the filesystem abstraction
 *
 * filelayout.c: DS realisation for the filesystem abstraction
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "fsal.h"
#include "fsal_internal.h"
#include "fsal_convert.h"
#include "nfsv41.h"
#include <ceph/libceph.h>
#include <fcntl.h>
#include <pthread.h>
#include "stuff_alloc.h"
#include "fsal_types.h"

#define min(a,b)	  \
  ({ typeof (a) _a = (a); \
    typeof (b) _b = (b);  \
    _a < _b ? _a : _b; })

/**
 * FSAL_ds_read:
 * Read for file-based layouts
 *
 * \param filehandle (input):
 *        The file handle provided in the layout
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
 *      - ERR_FSAL_FAULT        (a NULL pointer was passed as mandatory argument)
 *      - Other error codes can be returned :
 *        ERR_FSAL_IO, ...
 */

fsal_status_t CEPHFSAL_ds_read(cephfsal_handle_t * filehandle,     /*  IN  */
			       fsal_seek_t * seek_descriptor,  /* [IN] */
			       fsal_size_t buffer_size,        /*  IN  */
			       caddr_t buffer,                 /* OUT  */
			       fsal_size_t * read_amount,      /* OUT  */
			       fsal_boolean_t * end_of_file    /* OUT  */
    )
{
  int me_the_OSD;
  struct stat_precise st;
  int rc;
  uint32_t su;
  off_t filesize;
  fsal_off_t read_start;
  fsal_off_t block_start;
  fsal_size_t length = buffer_size;
  uint32_t stripe;
  uint64_t internal_offset;
  uint64_t left, pos, read;

  me_the_OSD=ceph_get_local_osd();
  
  /* Find the stripe being read */

  read_start = seek_descriptor->offset;
  
  su=ceph_ll_stripe_unit(VINODE(filehandle));

  if (su==(uint32_t) -ESTALE)
    {
      Return(ERR_FSAL_STALE, 0, INDEX_FSAL_ds_read);
    }
  
  block_start = read_start - read_start % su;
  stripe = block_start/su;

  rc=ceph_ll_getattr_precise(VINODE(filehandle), &st, -1, -1);

  if (rc < 0)
    Return(posix2fsal_error(rc), 0, INDEX_FSAL_ds_read);
  
  filesize=st.st_size;

  internal_offset=block_start - read_start;

  if (internal_offset==(uint32_t) -ESTALE)
    Return(ERR_FSAL_STALE, 0, INDEX_FSAL_ds_read);

  left = length;
  pos = read_start;
  read = -1;

  while ((left != 0) && (pos <= filesize) && (read != 0))
    {
      if (me_the_OSD != ceph_ll_get_stripe_osd(VINODE(filehandle),
					       stripe,
					       &(filehandle->data.layout)))

	  Return(ERR_FSAL_PNFS_IO_HOLE, 0, INDEX_FSAL_ds_read);

      read = ceph_ll_read_block(VINODE(filehandle), stripe,
				buffer, internal_offset,
				min((su - internal_offset),
				    (left - internal_offset)),
				&(filehandle->data.layout));
      if (read < 0)
	  Return(posix2fsal_error(rc), 0, INDEX_FSAL_ds_read);

      internal_offset=0;
      left -= read;
      pos += read;
      *read_amount += read;
      ++stripe;
    }

  if (pos == filesize)
    *end_of_file=true;
  else
    *end_of_file=false;

  Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_ds_read);
}

/**
 * FSAL_ds_write:
 * Perform a DS write on a layoutgotten filehandle
 *
 * \param filehandle (input):
 *        The filehandle returned as part of the layout
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
 * \param stable_flag
 *        Whether write should be committed to stable storage before
 *        return.
 *
 * \return Major error codes:
 *      - ERR_FSAL_NO_ERROR     (no error)
 *      - ERR_FSAL_INVAL        (invalid parameter)
 *      - ERR_FSAL_NOT_OPENED   (tried to write in a non-opened fsal_file_t)
 *      - ERR_FSAL_FAULT        (a NULL pointer was passed as mandatory argument)
 *      - Other error codes can be returned :
 *        ERR_FSAL_IO, ERR_FSAL_NOSPC, ERR_FSAL_DQUOT...
 */
fsal_status_t CEPHFSAL_ds_write(cephfsal_handle_t * filehandle,  /* IN */
				fsal_seek_t * seek_descriptor,   /* IN */
				fsal_size_t buffer_size,         /* IN */
				caddr_t buffer,                  /* IN */
				fsal_size_t * write_amount,      /* OUT */
				fsal_boolean_t stable_flag       /* IN */
    )
{
  int me_the_OSD;
  int rc;
  uint32_t su;
  fsal_off_t write_start;
  fsal_off_t block_start;
  fsal_size_t length = buffer_size;
  uint32_t stripe;
  uint64_t internal_offset;
  uint64_t left, pos, written, resp;

  me_the_OSD=ceph_get_local_osd();
  
  /* Find the stripe being read */

  write_start = seek_descriptor->offset;
  
  su = filehandle->data.layout.fl_stripe_unit;

  block_start = write_start - write_start % su;
  stripe = block_start/su;

  internal_offset = block_start - write_start;

  left = length;
  pos = write_start;
  resp = 0;
  written = 0;

  while ((left != 0)  && (resp == 0))
    {
      struct stat_precise st;

      uint64_t towrite = min((su - internal_offset),
			     (left - internal_offset));
      
      if (me_the_OSD != ceph_ll_get_stripe_osd(VINODE(filehandle),
					       stripe,
					       &(filehandle->data.layout)))
	  Return(ERR_FSAL_PNFS_IO_HOLE, 0, INDEX_FSAL_ds_write);

      resp = ceph_ll_write_block(VINODE(filehandle), stripe,
				 buffer, internal_offset,
				 towrite, &(filehandle->data.layout),
				 filehandle->data.snapseq);
      if (resp == 0)
	{
	  written += towrite;
	  internal_offset=0;
	  left -= written;
	  pos += written;
	  ++stripe;
	}
      *write_amount=written;
    }
  
  if (resp)
    Return(posix2fsal_error(rc), 0, INDEX_FSAL_ds_write);

  Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_ds_read);
}

/**
 * FSAL_ds_commit:
 * Perform a DS commit on a layoutgotten filehandle
 *
 * \param filehandle (input):
 *        The filehandle returned as part of the layout
 * \param offset (input):
 *        Beginning of window to commit
 * \param length (input):
 *        Size of window to commit
 * \return Major error codes:
 *      - ERR_FSAL_NO_ERROR     (no error)
 *      - ERR_FSAL_INVAL        (invalid parameter)
 *      - ERR_FSAL_NOT_OPENED   (tried to write in a non-opened fsal_file_t)
 *      - ERR_FSAL_FAULT        (a NULL pointer was passed as mandatory argument)
 *      - Other error codes can be returned :
 *        ERR_FSAL_IO, ERR_FSAL_NOSPC, ERR_FSAL_DQUOT...
 */
 
fsal_status_t CEPHFSAL_ds_commit(cephfsal_handle_t * filehandle,     /* IN */
				 fsal_off_t offset,
				 fsal_size_t length)
{
  /* We are writing everything synchronously on the assumption that
     we'll be moved uner Cache_Inode, so this is a no-op. */
  
  Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_ds_commit);
}
