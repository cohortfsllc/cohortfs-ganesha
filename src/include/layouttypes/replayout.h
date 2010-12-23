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
 * \brief   FSAL support for replication layouts
 *
 * replayout.h: FSAL support for replication layouts
 *
 *
 */

#ifndef __REPLAYOUT_H
#define __REPLAYOUT_H

#include "nfsv41.h"
#include "fsal_types.h"
#include "layouttypes/fsal_layout.h"

#define LBX_REPLICATION 0x87654321


typedef struct __replayout
{
  deviceid4 deviceid;
  uint32_t fhn;
  nfs_fh4* fhs;
} fsal_replayout_t;

typedef struct __repdsaddr
{
  uint32_t num_multipath_ds_list;
  multipath_list4* multipath_ds_list;
} fsal_repdsaddr_t;

typedef struct __signed_integrity
{
  uint32_t len;
  char* signed_integrity;
} fsal_signed_integrity_t;

typedef struct __rep_layoutupdate
{
  uint32_t num_integrities;
  fsal_signed_integrity_t*  integrities;
} fsal_replayoutupdate;

int FSALBACK_fh2rhandle(fsal_handle_t *fhin, fsal_dsfh_t* fhout,
			void* opaque);

#endif /* __REPLAYOUT_H */
