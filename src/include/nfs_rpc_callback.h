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

typedef struct wait_entry
{
    pthread_mutex_t mtx;
    pthread_cond_t cv;
} wait_entry_t;

/* thread wait queue */
typedef struct wait_q_entry
{
    uint32_t lflags;
    uint32_t rflags;
    wait_entry_t lwe; /* initial waiter */
    wait_entry_t rwe; /* reciprocal waiter */
    struct wait_q_entry *tail;
    struct wait_q_entry *next;
} wait_queue_entry_t;

typedef struct _rpc_call
{
    uint32_t states;
    struct wait_entry we;
    void *rpc;
    void *arg1;
    void *arg2;
} rpc_call_t;

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

/* XXX Submit rpc to be called on chan, optionally waiting for completion,
 * need wait machinery. */
int nfs_rpc_call(rpc_call_channel_t chan, void *rpc /* XXX */,
                 rpc_call_t **call /* OUT */, uint32_t flags);

#endif /* _NFS_RPC_CALLBACK_H */
