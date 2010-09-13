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
typedef void *fsal_layoutcontent_t;
typedef void *fsal_devaddr_t;
typedef deviceid4 fsal_deviceid_t;
typedef fattr4_fs_layout_types fsal_layout_types;
typedef nfs_fh4 fsal_dsfh_t;
typedef layout4 fsal_layout_t;
  
#endif /* _FSAL_LAYOUT_H */
