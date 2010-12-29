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

#define LAYOUT4_COHORT_REPLICATION 0x87654001

typedef struct __replayout
{
  deviceid4 deviceid;
  nfs_fh4 fh;
} fsal_replayout_t;

typedef struct __repdsaddr
{
  multipath_list4 multipath_rs;
} fsal_reprsaddr_t;

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
