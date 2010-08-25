/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright (C) 2010 The Linux Box Corporation
 * Contributor: Adam C. Emerson
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
 *      - ERR_FSAL_NOT_OPENED   (tried to read in a non-opened fsal_file_t)
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
fsal_status_t CEPHFSAL_ds_write(cephfsal_handle_t * filehandle,      /* IN */
				fsal_seek_t * seek_descriptor,   /* IN */
				fsal_size_t buffer_size,         /* IN */
				caddr_t buffer,                  /* IN */
				fsal_size_t * write_amount,      /* OUT */
				fsal_boolean_t stable_flag       /* IN */
    );

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
}
