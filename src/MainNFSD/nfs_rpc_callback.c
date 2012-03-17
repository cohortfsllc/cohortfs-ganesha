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
#include "nfs4.h"

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

static struct prealloc_pool rpc_call_pool;

/*
 * Initialize subsystem
 */
void nfs_rpc_cb_pkginit(void)
{
    /* Create a pool of rpc_call_t */
    MakePool(&rpc_call_pool,
             nfs_param.worker_param.nb_pending_prealloc, /* XXX */
             rpc_call_t, nfs_rpc_init_call, NULL);
    NamePool(&rpc_call_pool, "RPC Call Pool");
               
    if(!IsPoolPreallocated(&rpc_call_pool)) {
        LogCrit(COMPONENT_INIT,
                "Error while allocating rpc call pool");
        LogError(COMPONENT_INIT, ERR_SYS, ERR_MALLOC, errno);
        Fatal();
    }

    return;
}

/*
 * Shutdown subsystem
 */
void nfs_rpc_cb_pkgshutdown(void)
{
    /* do nothing */
}

/* XXXX this is automatically redundant, but in fact upstream TI-RPC is
 * not up-to-date with RFC 5665, will fix (Matt)
 *
 * (c) 2012, Linux Box Corp
 */

nc_type nfs_netid_to_nc(const char *netid)
{
    if (! strncmp(netid, netid_nc_table[_NC_TCP].netid,
                  netid_nc_table[_NC_TCP].netid_len))
        return(_NC_TCP);

    if (! strncmp(netid, netid_nc_table[_NC_TCP6].netid,
                  netid_nc_table[_NC_TCP6].netid_len))
        return(_NC_TCP6);

    if (! strncmp(netid, netid_nc_table[_NC_UDP].netid,
                  netid_nc_table[_NC_UDP].netid_len))
        return (_NC_UDP);

    if (! strncmp(netid, netid_nc_table[_NC_UDP6].netid,
                  netid_nc_table[_NC_UDP6].netid_len))
        return (_NC_UDP6);

    if (! strncmp(netid, netid_nc_table[_NC_RDMA].netid,
                  netid_nc_table[_NC_RDMA].netid_len))
        return (_NC_RDMA);

    if (! strncmp(netid, netid_nc_table[_NC_RDMA6].netid,
                 netid_nc_table[_NC_RDMA6].netid_len))
        return (_NC_RDMA6);

    if (! strncmp(netid, netid_nc_table[_NC_SCTP].netid,
                  netid_nc_table[_NC_SCTP].netid_len))
        return (_NC_SCTP);

    if (! strncmp(netid, netid_nc_table[_NC_SCTP6].netid,
                  netid_nc_table[_NC_SCTP6].netid_len))
        return (_NC_SCTP6);

    return (_NC_ERR);
}

#ifdef _USE_NFS4_1
void nfs_set_client_addr(nfs_client_id_t *clid, const netaddr4 *addr4)
{
    clid->cb.addr.nc = nfs_netid_to_nc(addr4->na_r_netid);
    memcpy(&clid->cb.addr.ss, addr4->na_r_addr,
           sizeof(struct sockaddr_storage));
}
#else
void nfs_set_client_addr(nfs_client_id_t *clid, const clientaddr4 *addr4)
{

    clid->cb.addr.nc = nfs_netid_to_nc(addr4->r_netid);
    memcpy(&clid->cb.addr.ss, addr4->r_addr,
           sizeof(struct sockaddr_storage));
}
#endif

/* end TI-RPC */

/* Create a channel for a new clientid (v4) or session, optionally
 * connecting it */
int nfs_rpc_create_chan_v40(nfs_client_id_t *client,
                            uint32_t flags)
{
    int code = 0;
    rpc_call_channel_t *chan = &client->cb.cb_u.v40.chan;

    assert(! chan->clnt);

    chan->type = RPC_CHAN_V40;
    chan->clnt = clnt_create((struct sockaddr *) &client->cb.addr.ss,
                             client->cb.program,
                             1 /* Errata ID: 2291 */,
                             netid_nc_table[client->cb.addr.nc].netid);

    return (code);
}

rpc_call_channel_t *
nfs_rpc_get_chan(nfs_client_id_t *client, uint32_t flags)
{
    /* XXX v41 */
    rpc_call_channel_t *chan = &client->cb.cb_u.v40.chan;

    if (! chan->clnt) {
        nfs_rpc_create_chan_v40(client, flags);
    }

    return (chan);
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
rpc_cb_null(rpc_call_channel_t *chan, struct timeval timeout)
{
    return (clnt_call(chan->clnt, CB_NULL, (xdrproc_t) xdr_void, NULL,
		      (xdrproc_t) xdr_void, NULL,
                      timeout));
}

static inline void free_argop(nfs_cb_argop4 *op)
{
    Mem_Free(op);
}

static inline void free_resop(nfs_cb_resop4 *op)
{
    Mem_Free(op);
}

rpc_call_t *alloc_rpc_call()
{
    rpc_call_t *call;

    GetFromPool(call, &rpc_call_pool, rpc_call_t);

    return (call);
}

void free_rpc_call(rpc_call_t *call)
{
    free_argop(call->cbt.v_u.v4.args.argarray.argarray_val);
    free_resop(call->cbt.v_u.v4.res.resarray.resarray_val);
    ReleaseToPool(call, &rpc_call_pool);
}

static inline void RPC_CALL_HOOK(rpc_call_t *call, rpc_call_hook hook,
                                 void* arg, uint32_t flags)
{
    if (call)
        (void) call->call_hook(call, hook, arg, flags);
}

int32_t
nfs_rpc_submit_call(rpc_call_channel_t *chan, rpc_call_t *call, uint32_t flags)
{
    int32_t code = 0;
    request_data_t *pnfsreq = NULL;

    if (call->flags & NFS_RPC_CALL_INLINE) {
        code = nfs_rpc_dispatch_call(call, NFS_RPC_CALL_NONE);
    }
    else {
        /* select a thread from the general thread pool */
        int32_t thrd_ix;
        nfs_worker_data_t *worker;

        thrd_ix = nfs_core_select_worker_queue();
        worker = &workers_data[thrd_ix];

        LogFullDebug(COMPONENT_NFS_CB,
                     "Use request from Worker Thread #%u's pool, thread has %d "
                     "pending requests",
                     thrd_ix,
                     worker->pending_request->nb_entry);

        pnfsreq = nfs_rpc_get_nfsreq(worker, 0 /* flags */);
        pthread_mutex_lock(&call->we.mtx);
        call->states = NFS_CB_CALL_QUEUED;
        pnfsreq->rtype = NFS_CALL;
        pnfsreq->r_u.call = call;
        DispatchWorkNFS(pnfsreq, thrd_ix);
        pthread_mutex_unlock(&call->we.mtx);
    }

    return (code);
}

int32_t
nfs_rpc_dispatch_call(rpc_call_t *call, uint32_t flags)
{
    int code = 0;
    struct timeval CB_TIMEOUT = {15, 0}; /* XXX */

    /* send the call, set states, wake waiters, etc */
    pthread_mutex_lock(&call->we.mtx);
    call->states = NFS_CB_CALL_DISPATCH;
    pthread_mutex_unlock(&call->we.mtx);

    call->stat = clnt_call(call->chan->clnt,
                           CB_COMPOUND,
                           (xdrproc_t) xdr_CB_COMPOUND4args,
                           &call->cbt.v_u.v4.args,
                           (xdrproc_t) xdr_CB_COMPOUND4res,
                           &call->cbt.v_u.v4.res,
                           CB_TIMEOUT);

    /* signal waiter(s) */
    pthread_mutex_lock(&call->we.mtx);
    call->states |= NFS_CB_CALL_FINISHED;

    /* broadcast will generally be inexpensive */
    if (call->flags & NFS_RPC_CALL_BROADCAST)
        pthread_cond_broadcast(&call->we.cv);
    pthread_mutex_unlock(&call->we.mtx);

    /* call completion hook */
    RPC_CALL_HOOK(call, RPC_CALL_COMPLETE, NULL, NFS_RPC_CALL_NONE);

    return (code);
}

int32_t
nfs_rpc_abort_call(rpc_call_t *call)
{
    return (0);
}

