/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright CEA/DAM/DIF  (2008)
 * contributeur : Philippe DENIEL   philippe.deniel@cea.fr
 *                Thomas LEIBOVICI  thomas.leibovici@cea.fr
 *
 * Portions Copyright (C) 2012, The Linux Box Corporation
 * Contributor : Matt Benjamin <matt@linuxbox.com>
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
 * \file    nfs4_cb_Compound.c
 * \author  $Author: deniel $
 * \date    $Date: 2008/03/11 13:25:44 $
 * \version $Revision: 1.24 $
 * \brief   Routines used for managing the NFS4/CB COMPOUND functions.
 *
 * nfs4_cb_Compound.c : Routines used for managing the NFS4/CB COMPOUND
 * functions.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _SOLARIS
#include "solaris_port.h"
#endif

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/file.h>           /* for having FNDELAY */
#include "HashData.h"
#include "HashTable.h"
#include "rpc.h"
#include "log.h"
#include "stuff_alloc.h"
#include "nfs23.h" /* XXX */
#include "nfs4.h"
#include "mount.h"
#include "nfs_core.h"
#include "cache_inode.h"
#include "cache_content.h"
#include "nfs_exports.h"
#include "nfs_creds.h"
#include "nfs_proto_functions.h"
#include "nfs_rpc_callback.h"

static const nfs4_cb_tag_t cbtagtab4[] = {
    {NFS4_CB_TAG_DEFAULT, "Ganesha CB Compound", 19},
};

/* Some CITI-inspired helper ideas */

void
cb_compound_init(nfs4_compound_t *cbt,
                 nfs_cb_argop4 *cba_un, uint32_t un_len,
                 char *tag, uint32_t tag_len)
{
    /* args */
    memset(cbt, 0, sizeof(nfs4_compound_t)); /* XDRS */

    cbt->v_u.v4.args.minorversion = 1;
    /* not un_len, see below */
    cbt->v_u.v4.args.argarray.argarray_len = 0;
    cbt->v_u.v4.args.argarray.argarray_val =  (nfs_cb_argop4 *) cba_un;

    if (tag) {
        /* sender must ensure tag is safe to queue */
        cbt->v_u.v4.args.tag.utf8string_val = tag;
        cbt->v_u.v4.args.tag.utf8string_len = tag_len;
    } else {
        cbt->v_u.v4.args.tag.utf8string_val =
            cbtagtab4[NFS4_CB_TAG_DEFAULT].val;
        cbt->v_u.v4.args.tag.utf8string_len =
            cbtagtab4[NFS4_CB_TAG_DEFAULT].len;
    }

} /* cb_compount_init */

void
cb_compound_add_op(nfs4_compound_t *cbt, nfs_cb_argop4 *src)
{
    uint32_t ix = /* old value */
        (cbt->v_u.v4.args.argarray.argarray_len)++;
    nfs_cb_argop4 *dst =
        cbt->v_u.v4.args.argarray.argarray_val + ix;
    *dst = *src;
}

