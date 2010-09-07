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

bool_t xdr_fsal_dsfh_t(XDR* xdrs, fsal_dsfh_t* fh)
{
  return (xdr_opaque(xdrs, (caddr_t) fh->nfs_fh4_val,
		     fh->nfs_fh4_len));
}

int encodefileslayout(layouttype4 type,
		      layout_content4* dest,
		      size_t size,
		      fsal_layoutcontent_t* source)
{
  fsal_filelayout_t* lsrc=(fsal_filelayout_t*)source;
  XDR xdrs;
  unsigned int beginning;
  
  dest->loc_type=type;
  xdrmem_create(&xdrs, dest->loc_body.loc_body_val, size, XDR_ENCODE);
  beginning=xdr_getpos(&xdrs);
  if (!xdr_deviceid4(&xdrs, lsrc->deviceid))
    return FALSE;
  if (!(xdr_u_long(&xdrs, (u_long*) &lsrc->util)))
    return FALSE;
  if (!(xdr_u_long(&xdrs, (u_long*) &(lsrc->first_stripe_index))))
    return FALSE;
  if (!(xdr_offset4(&xdrs, &lsrc->pattern_offset)))
    return FALSE;
  if (!(xdr_array(&xdrs, (char**) &lsrc->fhs, &lsrc->fhn, UINT32_MAX,
		  sizeof(fsal_dsfh_t), (xdrproc_t) xdr_fsal_dsfh_t)))
    return FALSE;
  dest->loc_body.loc_body_len=beginning-xdr_getpos(&xdrs);
  xdr_destroy(&xdrs);
  return TRUE;
}

int encodefilesdevice(layouttype4 type,
		      device_addr4* dest,
		      size_t destsize,
		      fsal_devaddr_t source)
{
  fsal_file_dsaddr_t* lsrc=(fsal_file_dsaddr_t*) source;
  XDR xdrs;
  unsigned int beginning;

  dest->da_layout_type=type;
  xdrmem_create(&xdrs, dest->da_addr_body.da_addr_body_val, destsize, XDR_ENCODE);
  beginning=xdr_getpos(&xdrs);
  if (!(xdr_nfsv4_1_file_layout_ds_addr4(&xdrs, lsrc)))
    return FALSE;
  dest->da_addr_body.da_addr_body_len=beginning-xdr_getpos(&xdrs);
  xdr_destroy(&xdrs);
  return TRUE;
}

