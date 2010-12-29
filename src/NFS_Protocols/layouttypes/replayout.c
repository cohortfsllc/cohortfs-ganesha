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
  if (!xdr_nfs_fh4(&xdrs, &lsrc->fh))
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
  fsal_reprsaddr_t* lsrc = (fsal_reprsaddr_t*) source;
  XDR xdrs;

  unsigned int beginning;

  dest->da_layout_type = type;
  xdrmem_create(&xdrs, dest->da_addr_body.da_addr_body_val, length, XDR_ENCODE);
  beginning = xdr_getpos(&xdrs);
  if (!xdr_nfs_fh4(&xdrs, &lsrc->multipath_rs))
    return FALSE;
  dest->da_addr_body.da_addr_body_len = xdr_getpos(&xdrs) - beginning;
  xdr_destroy(&xdrs);
  return TRUE;
}
