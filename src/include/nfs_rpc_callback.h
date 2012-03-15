/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright (C) 2012, The Linux Box Corporation
 * Contributor : Matt Benjamin <matt@linuxbox.com>
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

#ifndef _NFS_RPC_CALLBACK_H
#define _NFS_RPC_CALLBACK_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif                          /* HAVE_CONFIG_H */

#ifdef _SOLARIS
#include "solaris_port.h"
#endif                          /* _SOLARIS */

#include "log.h"
#include "cache_inode.h"

/**
 *
 * \file nfs_rpc_callback.h
 * \author Matt Benjamin and Lee Dobryden
 * \brief RPC callback dispatch package
 *
 * \section DESCRIPTION
 *
 * This module implements APIs for submission, and dispatch of NFSv4.0
 * and (soon) NFSv4.1 format callbacks.
 *
 * Planned strategy is to deal with all backchannels from a small number of
 * service threads, initially 1, using non-blocking socket operations.  This
 * may change, as NFSv4.1 bi-directional support is integrated.
 *
 */

#define CB_FLAG_NONE          0x0000

#define NFS_RPC_CB_CALL_NONE         0x0000
#define NFS_RPC_CB_CALL_QUEUED       0x0001
#define NFS_RPC_CB_CALL_DISPATCH     0x0002
#define NFS_RPC_CB_CALL_FINISHED     0x0003

static inline void init_wait_entry(wait_entry_t *we)
{
   pthread_mutex_init(&we->mtx, NULL);
   pthread_cond_init(&we->cv, NULL);
}

static inline void nfs_rpc_init_call(rpc_call_t *call)
{
    memset(call, 0, sizeof(rpc_call_t));
    init_wait_entry(&call->we);
}

void nfs_rpc_cb_pkginit(void);
void nfs_rpc_cb_pkgshutdown(void);
void *nfs_rpc_cb_thread(void *arg);
void cb_wake_thread();

/* Create a channel for a new clientid (v4) or session, optionally
 * connecting it */
int nfs_rpc_create_chan_v40(nfs_client_id_t *client,
                            uint32_t flags);

/* Create a channel for a new clientid (v4) or session, optionally
 * connecting it */
int nfs_rpc_create_chan_v41(nfs41_session_t *session,
                            uint32_t flags);

/* Dispose a channel. */
void nfs_rpc_destroy_chan(rpc_call_channel_t *chan);

int
nfs_rpc_call_init(rpc_call_t call, uint32_t flags);

/* Submit rpc to be called on chan, optionally waiting for completion. */
int32_t nfs_rpc_submit_call(rpc_call_channel_t *chan, rpc_call_t *call,
                            uint32_t flags);

/* Dispatch method to process a (queued) call */
int32_t nfs_rpc_dispatch_call(rpc_call_t *call, uint32_t flags);

#endif /* _NFS_RPC_CALLBACK_H */
