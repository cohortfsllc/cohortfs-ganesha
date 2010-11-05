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
 * \file    filelayout.c
 * \brief   FSAL and encoding support for file layouts
 *
 * filelayout.c: FSAL and encoding support for file layouts
 *
 *
 */

#include "nfsv41.h"
#include "nfs23.h"
#include "fsal_types.h"
#include "nfs_exports.h"
#include "nfs_file_handle.h"
#include <rpc/xdr.h>
#include "fsal.h"
#include "layouttypes/fsal_layout.h"
#include "layouttypes/filelayout.h"

int encodefileslayout(layouttype4 type,
		      layout_content4* dest,
		      size_t size,
		      void* source)
{
  fsal_filelayout_t* lsrc=(fsal_filelayout_t*)source;
  XDR xdrs;
  unsigned int beginning;
  unsigned int end;
  
  dest->loc_type=type;
  xdrmem_create(&xdrs, dest->loc_body.loc_body_val, size, XDR_ENCODE);
  beginning=xdr_getpos(&xdrs);
  if (!xdr_deviceid4(&xdrs, lsrc->deviceid))
    return FALSE;
  end=xdr_getpos(&xdrs);
  if (!(xdr_u_long(&xdrs, (u_long*) &lsrc->util)))
    return FALSE;
  end=xdr_getpos(&xdrs);
  if (!(xdr_u_long(&xdrs, (u_long*) &(lsrc->first_stripe_index))))
    return FALSE;
  end=xdr_getpos(&xdrs);
  if (!(xdr_offset4(&xdrs, &lsrc->pattern_offset)))
    return FALSE;
  end=xdr_getpos(&xdrs);
  if (!(xdr_array(&xdrs, (char**) &lsrc->fhs, &lsrc->fhn, UINT32_MAX,
		  sizeof(fsal_dsfh_t), (xdrproc_t) xdr_nfs_fh4)))
    return FALSE;
  end=xdr_getpos(&xdrs);
  dest->loc_body.loc_body_len = (end-beginning);
  xdr_destroy(&xdrs);
  return TRUE;
}

int encodefilesdevice(layouttype4 type,
		      device_addr4* dest,
		      size_t length,
		      caddr_t source)
{
  nfsv4_1_file_layout_ds_addr4* lsrc=(nfsv4_1_file_layout_ds_addr4*) source;
  XDR xdrs;

  unsigned int beginning;

  dest->da_layout_type=type;
  xdrmem_create(&xdrs, dest->da_addr_body.da_addr_body_val, length, XDR_ENCODE);
  beginning=xdr_getpos(&xdrs);
  if (!(xdr_nfsv4_1_file_layout_ds_addr4(&xdrs, lsrc)))
    return FALSE;
  dest->da_addr_body.da_addr_body_len=xdr_getpos(&xdrs)-beginning;
  xdr_destroy(&xdrs);
  return TRUE;
}

