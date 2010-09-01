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
 * \file    filelayout.h
 * \brief   FSAL support for file layouts
 *
 * filelayout.h: FSAL support for file layouts
 *
 *
 */

#ifndef __FILELAYOUT_H
#include "nfsv41.h"
#include "fsal_types.h"
#include "layouttypes/fsal_layout.h"

typedef nfsv4_1_file_layout_ds_addr4 fsal_file_dsaddr_t;

typedef struct _fsal_dsfh
{
  uint32_t len;
  char val[128];
} fsal_dsfh_t;

typedef struct __filelayout
{
  fsal_deviceid_t deviceid;
  uint32_t util;
  uint32_t first_stripe_index;
  fsal_layoutoffset_t pattern_offset;
  uint32_t fhn;
  fsal_dsfh_t* fhs;
} fsal_filelayout_t;

void FSALBACK_fh2dshandle(fsal_handle_t *fhin, fsaldsfh_t fhout,
			  void* cookie);
  
#endif /* __FILELAYOUT_H */
