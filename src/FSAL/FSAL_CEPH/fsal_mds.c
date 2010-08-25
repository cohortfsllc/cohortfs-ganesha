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
 * \file    mdsf.c
 * \brief   MDS realisation for the filesystem abstraction
 *
 * filelayout.c: MDS realisation for the filesystem abstraction
 *               Obviously, all of these functions should dispatch
 *               on type if more than one layout type is supported.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "fsal.h"
#include "fsal_internal.h"
#include "fsal_convert.h"

/**
 *
 * FSAL_layoutget: The NFSv4.1 LAYOUTGET operation
 *
 * Return a layout for the requested range on the given filehandle.
 *
 * \param filehandle (input):
 *        Handle of the file on which the layout is requested.
 * \param type (input):
 *        The type of layout requested
 * \param iomode (input):
 *        The iomode requested
 * \param offset (input):
 *        The beginning requested
 * \param length (input):
 *        The length requested
 * \param minlength (input):
 *        The minimum length required
 * \param layouts (output):
 *        Pointer to a buffer allocated by this function beginning
 *        with numlayouts layouts.  The following space is to hold
 *        variable-sized structures referenced in the layouts.
 * \param numlayouts (output):
 *        The number of layouts returned
 * \param stateid (input):
 *        The stateid of the open file (perhaps to be communicated to
 *        the DS using some control protocol.)
 * \param return_on_close (output):
 *        Return on close flag.  To make this profitable, FSAL_close
 *        would need a means to trigger a layoutrecall.
 * \param fsal_op_context (input):
 *        Credential information
 * \param cbcookie (input):
 *        Opaque, passed to callbacks.
 *
 * \return Error codes or ERR_FSAL_NO_ERROR
 */

fsal_status_t CEPHFSAL_layoutget(cephfsal_handle_t filehandle,
				 fsal_layouttype_t type,
				 fsal_layoutiomode_t iomode,
				 fsal_off_t offset, fsal_size_t length,
				 fsal_size_t minlength,
				 fsal_layout_t** layouts,
				 int *numlayouts,
				 const char* stateid,
				 fsal_boolean_t *return_on_close,
				 cephfsal_op_context_t *context,
				 void* cbcookie)
{
  fsal_dsfh_t rethandle;

  /* Determine actual dimensions of layout.  e.g. grant layout to
     whole file, expand layout to be aligned with OSD block structure,
     etc. */

  if (FSALBACK_layout_add_state(type, iomode, offset, length,
				cbcookie) != 0)
    {
      Return(ERR_FSAL_DELAY, 0, INDEX_FSAL_getlayout);
    }
      
  /* Do FSAL specific allocation for the layout */

  /* Allocate and populate layout vector */

  /* This function converts an FSAL filehandle that can be included in
     a layout and that the client can pass to a Ganesha-based DS. */
  
  FSALBACK_fh2dshandle(filehandle, &rethandle, cbcookie);

  /* Obviously, change this when real code is added */

  Return(ERR_FSAL_NOTSUPP, 0, INDEX_FSAL_getlayout);
}

/**
 *
 * FSAL_layoutreturn: The NFSv4.1 LAYOUTRETURN operation
 *
 * Free a layout on a given file.  This routine will always be
 * passed exactly a layout granted by LAYOUTGET (retrieved from the
 * state table.)
 *
 * \param filehandle (input):
 *        The handle upon which the layout was granted
 * \param type (input):
 *        The layout type
 * \param iomode (input):
 *        The iomode granted
 * \param offset (input):
 *        The offset
 * \param length (input):
 *        The length
 * \param context (input)
 *        The authentication context
 *
 * \return Error codes or ERR_FSAL_NO_ERROR
 *
 */

fsal_status_t CEPHFSAL_layoutreturn(fsal_handle_t* filehandle,
				    fsal_layouttype_t type,
				    fsal_layoutiomode_t iomode,
				    fsal_off_t offset,
				    fsal_size_t length,
				    fsal_op_context_t* context)
{
  /* Perform FSAL specific tasks to release the layout */

  /* Obviously, change this when real code is added */

  Return(ERR_FSAL_NOTSUPP, 0, INDEX_FSAL_getlayout);
}

/**
 *
 * FSAL_layoutcommit: The NFSv4.1 LAYOUTCOMMIT operation
 *
 * Commit changes made on the DSs to the MDS
 *
 * \param filehandle (input):
 *        The filehandle in question
 * \param type (input):
 *        The layout type
 * \param layout (input):
 *        The layout itself
 * \param layout_length (input):
 *        Length of the layout
 * \param offset (input):
 *        Offset into the file of the changed portion
 * \param length (input):
 *        Length of the changed portion
 * \param newoff (input/output):
 *        Client suggested offset for the length of the file (NULL if
 *        none)/
 *        FSAL supplied offset for the length of the file (if changed
 *        true)
 * \param changed (output)
 *        True if MDS returns a new file length
 * \param newtime (input)
 *        Client suggested modification time
 *
 * \return Error codes or ERR_FSAL_NO_ERROR
 *
 */

fsal_status_t CEPHFSAL_layoutcommit(fsal_handle_t* filehandle,
				    fsal_layouttype_t type,
				    char* layout,
				    size_t layout_length,
				    fsal_off_t offset,
				    fsal_size_t length,
				    fsal_off_t* newoff,
				    fsal_boolean_t* changed,
				    fsal_time_t* newtime)
{
  /* Commit data */
  if (newtime)
    {
      /* Check that time does not run backward, follow suggestion if
	 we wish */
    }

  if (newoff)
    {
      /* File may only grow, follow suggestion if we wish */
    }

  if (0) /* Some test for a new filesize */
    {
      *changed=1;
    }
  else
    {
      *changed=0;
    }

  /* Obviously, change this when real code is added */

  Return(ERR_FSAL_NOTSUPP, 0, INDEX_FSAL_getlayout);
}

/**
 *
 * FSAL_getdeviceinfo: The NFSv4.1 GETDEVICEINFO operation
 *
 * Look up the address for a given deviceid
 *
 * \param type (input):
 *        The layout type
 * \param deviceid (input):
 *        The device ID to look up
 * \param buff (output):
 *        The buffer to be filled with the device address
 * \param len (input/output):
 *        The avaialble length of buff/The length of the data
 *        returned
 *
 * \return Error codes or ERR_FSAL_NO_ERROR
 *
 */

fsal_status_t CEPHFSAL_getdeviceinfo(fsal_layouttype_t* type,
				     fsal_deviceid_t* deviceid,
				     char* buff,
				     size_t* len)
{
  /* Look up the data and fill the buffer.  Cast to the appropriate
     address type. */

  /* Obviously, change this when real code is added */

  Return(ERR_FSAL_NOTSUPP, 0, INDEX_FSAL_getlayout);
}

/**
 *
 * FSAL_getdevicelist: The NFSv4.1 GETDEVICELIST operation
 *
 * Return all deviceids for a given filesystem
 *
 * \param filehandle (input):
 *        Handle of a file on the filesystem in question
 * \param type (input):
 *        The layout type
 * \param numdevices (input/output):
 *        Number of devices requested/returned
 * \param cookie (input/output):
 *        Cookie passed by client/returned by MDS.  Will be zero to
 *        start from the beginning.
 * \param eof (output):
 *        True if all devices have been returned
 * \param buff (output):
 *        Buffer to hold device list (as an array)
 * \param size_t (intput/output):
 *        Amount of memory allocated/used for buff
 *
 * \return Error codes or ERR_FSAL_NO_ERROR
 *
 */

fsal_status_t FSAL_getdevicelist(fsal_handle_t* filehandle,
				 fsal_layouttype_t type,
				 int* numdevices,
				 uint64* cookie,
				 fsal_boolean* eof,
				 void* buff,
				 size_t* bufflen)
{
  /* Populate buff with devices */

  /* Obviously, change this when real code is added */

  Return(ERR_FSAL_NOTSUPP, 0, INDEX_FSAL_getlayout);
}
