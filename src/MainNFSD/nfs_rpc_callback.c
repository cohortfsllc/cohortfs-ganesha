/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright (C) 2010, The Linux Box Corporation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _SOLARIS
#include "solaris_port.h"
#endif                          /* _SOLARIS */

#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <time.h>
#include <pthread.h>
#include <assert.h>
#include "stuff_alloc.h"
#include "nlm_list.h"
#include "fsal.h"
#include "nfs_core.h"
#include "log.h"
#include "nfs_rpc_callback.h"

/**
 *
 * \file nfs_rpc_callback.c
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

/*
 * Initialize subsystem
 */
void nfs_rpc_cb_pkginit(void)
{
    return;
}

/*
 * Shutdown subsystem
 */
void nfs_rpc_cb_pkgshutdown(void)
{
    /* do nothing */
}

/* Create a channel for a new clientid (v4) or session, optionally
 * connecting it */
int nfs_rpc_create_chan_v40(nfs_client_id_t *client,
                            uint32_t flags)
{
    int code = 0;
    rpc_call_channel_t *chan = &client->cb.cb_u.v40.chan;

    assert(! chan->clnt);

    chan->type = RPC_CHAN_V40;
    chan->clnt = clnt_create(client->cb.client_r_addr,
                             client->cb.program,
                             1 /* Errata ID: 2291 */,
                             client->cb.client_r_netid);

    return (code);
}

/* Dispose a channel. */
void nfs_rpc_destroy_chan(rpc_call_channel_t *chan)
{
    assert (chan);

    /* XXX lock, wait for outstanding calls, etc */

    switch (chan->type) {
    case RPC_CHAN_V40:
        /* channel has a dedicated RPC client */
        if (chan->clnt)
            clnt_destroy(chan->clnt);        
        break;
    case RPC_CHAN_V41:
        /* XXX channel is shared */
        break;
    }

    chan->clnt = NULL;
    chan->last_called = 0;
}

/*
 * Call the NFSv4 client's CB_NULL procedure.
 */
enum clnt_stat
rpc_cb_null(rpc_call_channel_t *chan)
{
    struct timeval CB_TIMEOUT = {15, 0};

    return (clnt_call(chan->clnt, CB_NULL, (xdrproc_t) xdr_void, NULL,
		      (xdrproc_t) xdr_void, NULL, CB_TIMEOUT));
}

int32_t
nfs_rpc_submit_call(rpc_call_channel_t *chan, rpc_call_t *call, uint32_t flags)
{
    int32_t thrd_ix, code = 0;
    nfs_worker_data_t *worker = NULL;
    request_data_t *pnfsreq = NULL;

    /* select a thread from the general thread pool */
    thrd_ix = nfs_core_select_worker_queue();
    worker = &workers_data[thrd_ix];

    LogFullDebug(COMPONENT_NFS_CB,
                 "Use request from Worker Thread #%u's pool, thread has %d "
                 "pending requests",
                 thrd_ix,
                 worker->pending_request->nb_entry);

    pnfsreq = nfs_rpc_get_nfsreq(worker, 0 /* XXX flags */);

    pthread_mutex_lock(&call->we.mtx);
    call->states = NFS_RPC_CB_CALL_QUEUED;

    pnfsreq->rtype = NFS_CALL;
    pnfsreq->r_u.call = call;

    DispatchWorkNFS(pnfsreq, thrd_ix);

    pthread_mutex_unlock(&call->we.mtx);

    return (code);
}

int32_t
nfs_rpc_dispatch_call(rpc_call_t *call, uint32_t flags)
{
    int code = 0;
    struct timeval CB_TIMEOUT = {15, 0}; /* XXX */

    /* send the call, set states, wake waiters, etc */
    pthread_mutex_lock(&call->we.mtx);
    call->states = NFS_RPC_CB_CALL_DISPATCH;
    pthread_mutex_unlock(&call->we.mtx);

    /* do it */
    cb_compound_resinit(call->cbr);

    call->stat = clnt_call(call->chan->clnt,
                           CB_COMPOUND,
                           (xdrproc_t) xdr_CB_COMPOUND4args, call->cba,
                           (xdrproc_t) xdr_CB_COMPOUND4res, call->cbr,
                           CB_TIMEOUT);

    /* signal waiter(s) */
    pthread_mutex_lock(&call->we.mtx);
    call->states |= NFS_RPC_CB_CALL_FINISHED;

    /* broadcast will generally be inexpensive */
    pthread_cond_broadcast(&call->we.cv);
    pthread_mutex_unlock(&call->we.mtx);

    return (code);
}

int32_t
nfs_rpc_abort_call(rpc_call_t *call)
{
    return (0);
}
