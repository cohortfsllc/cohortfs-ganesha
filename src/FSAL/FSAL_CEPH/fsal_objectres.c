/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright (C) 2010 The Linux Box, Inc.
 * Contributor : Adam C. Emerson <aemerson@linuxbox.com>
 *
 * Portions copyright CEA/DAM/DIF  (2008)
 * contributeur : Philippe DENIEL   philippe.deniel@cea.fr
 *                Thomas LEIBOVICI  thomas.leibovici@cea.fr
 *
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
 * ------------- 
 */

/**
 * \file    fsal_objectres.c
 * \brief   FSAL remanent resources management routines.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "fsal.h"
#include "fsal_internal.h"

/**
 * FSAL_CleanObjectResources:
 * This function cleans remanent internal resources
 * that are kept for a given FSAL handle.
 *
 * \param in_fsal_handle (input):
 *        The handle whose the resources are to be cleaned.
 */

fsal_status_t CEPHFSAL_CleanObjectResources(cephfsal_handle_t * in_fsal_handle)
{

  ceph_ll_forget(VINODE(in_fsal_handle), 1);
  
  Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_CleanObjectResources);
}
