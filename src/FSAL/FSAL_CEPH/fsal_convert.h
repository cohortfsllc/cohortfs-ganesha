/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright (C) 2010, The Linux Box Corporation
 * Contributor : Adam C. Emerson <aemerson@linuxbox.com>
 *
 * Some portions Copyright CEA/DAM/DIF  (2008)
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
 *
 * \file    fsal_convert.h
 * \brief   Your FS to FSAL type converting function.
 *
 */
#ifndef _FSAL_CONVERTION_H
#define _FSAL_CONVERTION_H

#include "fsal.h"

int posix2fsal_error(int posix_errorcode);
fsal_status_t posix2fsal_attributes(struct stat_precise * p_buffstat,
                                    fsal_attrib_list_t * p_fsalattr_out);

fsal_nodetype_t posix2fsal_type(mode_t posix_type_in);
fsal_time_t ceph2fsal_time(time_t tsec, time_t tmicro);
fsal_fsid_t posix2fsal_fsid(dev_t posix_devid);
fsal_dev_t posix2fsal_devt(dev_t posix_devid);
void stat2fsal_fh(struct stat_precise *st, fsal_handle_t *fh);
fsal_accessmode_t unix2fsal_mode(mode_t unix_mode);

#endif
