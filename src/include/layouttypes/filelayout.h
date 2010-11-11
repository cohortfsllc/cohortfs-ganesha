/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright (C) 2010 The Linux Box Corporation
 * All Rights Reserved
 *
 * Contributor: Adam C. Emerson
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
#define __FILELAYOUT_H

#include "nfsv41.h"
#include "fsal_types.h"
#include "layouttypes/fsal_layout.h"

typedef nfsv4_1_file_layout_ds_addr4 fsal_file_dsaddr_t;

typedef struct __filelayout
{
  fsal_deviceid_t deviceid;
  uint32_t util;
  uint32_t first_stripe_index;
  fsal_layoutoffset_t pattern_offset;
  uint32_t fhn;
  fsal_dsfh_t* fhs;
} fsal_filelayout_t;

#endif /* __FILELAYOUT_H */
