/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright (C) 2010 The Linux Box Corporation
 * All Rights Reserved
 *
 * Contributor: Adam C. Emerson
 */

#ifndef _FSAL_LAYOUT_H
#define _FSAL_LAYOUT_H
#include <nfsv41.h>

/**
 * \file    fsal_layout.h
 * \brief   FSAL layout types and functions
 *
 * fsal_layout.h: FSAL layout types and functions
 *
 *
 */

typedef layouttype4 fsal_layouttype_t;
typedef offset4 fsal_layoutoffset_t;
typedef length4 fsal_layoutlength_t;
typedef layoutiomode4 fsal_layoutiomode_t;
typedef deviceid4 fsal_deviceid_t;
typedef fattr4_fs_layout_types fsal_layout_types;
typedef nfs_fh4 fsal_dsfh_t;
typedef layout4 fsal_layout_t;
  
#endif /* _FSAL_LAYOUT_H */
