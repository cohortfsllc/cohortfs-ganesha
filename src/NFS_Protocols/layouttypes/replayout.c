/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright (C) 2010 The Linux Box Corporation
 * All Rights Reserved
 *
 * Contributor: Adam C. Emerson
 */

/**
 * \file    filelayout.c
 * \brief   FSAL and encoding support for replication layouts
 *
 * filelayout.c: FSAL and encoding support for replication layouts
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
#include "layouttypes/replayout.h"

int encodereplayout(layouttype4 type,
		    layout_content4* dest,
		    size_t size,
		    void* source)
{
  fsal_replayout_t* lsrc = (fsal_replayout_t*) source;
  XDR xdrs;
  unsigned int beginning;
  
  dest->loc_type = type;
  xdrmem_create(&xdrs, dest->loc_body.loc_body_val, size, XDR_ENCODE);
  beginning = xdr_getpos(&xdrs);
  if (!xdr_deviceid4(&xdrs, lsrc->deviceid))
    return FALSE;
  if (!(xdr_array(&xdrs, (char**) &lsrc->fhs, &lsrc->fhn, UINT32_MAX,
		  sizeof(nfs_fh4), (xdrproc_t) xdr_nfs_fh4)))
    return FALSE;
  dest->loc_body.loc_body_len = xdr_getpos(&xdrs) - beginning;
  xdr_destroy(&xdrs);
  return TRUE;
}

int encoderepdevice(layouttype4 type,
		    device_addr4* dest,
		    size_t length,
		    caddr_t source)
{
  fsal_repdsaddr_t* lsrc = (fsal_repdsaddr_t*) source;
  XDR xdrs;

  unsigned int beginning;

  dest->da_layout_type = type;
  xdrmem_create(&xdrs, dest->da_addr_body.da_addr_body_val, length, XDR_ENCODE);
  beginning = xdr_getpos(&xdrs);
  if (!xdr_array
      (&xdrs, (char**) &lsrc->multipath_ds_list,
       (u_int*) &lsrc->num_multipath_ds_list, UINT32_MAX,
       sizeof(multipath_list4), (xdrproc_t) xdr_multipath_list4))
    return FALSE;
  dest->da_addr_body.da_addr_body_len = xdr_getpos(&xdrs) - beginning;
  xdr_destroy(&xdrs);
  return TRUE;
}
