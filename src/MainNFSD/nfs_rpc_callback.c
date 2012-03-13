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

/* XXX will need more sophisticated wait support, will fix */
static pthread_mutex_t cb_mtx;
static pthread_cond_t cb_cv;

static const uint32_t CB_SLEEPING = 0x00000001;
static const uint32_t CB_SHUTDOWN = 0x00000002;

static struct cb_thread_state
{
    pthread_t thread_id;
    uint32_t wait_ms;
    uint32_t flags;
} cb_thread_state;


#define S_NSECS 1000000000UL    /* nsecs in 1s */
#define MS_NSECS 1000000UL      /* nsecs in 1ms */

/* XXX Delay ms milliseconds.  Consolidate with LRU etc, along with
 * wait_entry and other support code.
 */
static void
cb_thread_delay_ms(unsigned long ms)
{
     time_t now;
     struct timespec then;
     unsigned long long nsecs;

     now = time(0);
     nsecs = (S_NSECS * now) + (MS_NSECS * ms);
     then.tv_sec = nsecs / S_NSECS;
     then.tv_nsec = nsecs % S_NSECS;

     pthread_mutex_lock(&cb_mtx);
     cb_thread_state.flags |= CB_SLEEPING;
     pthread_cond_timedwait(&cb_cv, &cb_mtx, &then);
     cb_thread_state.flags &= ~CB_SLEEPING;
     pthread_mutex_unlock(&cb_mtx);
}

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
     /* Post and wait for shutdown of LRU background thread */
     pthread_mutex_lock(&cb_mtx);
     cb_thread_state.flags |= CB_SHUTDOWN;
     cb_wake_thread(CB_FLAG_NONE);
     pthread_mutex_unlock(&cb_mtx);
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

struct rpc_call_sequence
{
    uint32_t states;
    struct glist_head calls;
    struct wait_entry we;
};

/*
 * Call the NFSv4 client's CB_NULL procedure.
 */
enum clnt_stat
rpc_cb_null(rpc_call_channel_t *chan, rpc_call_t *call)
{
    struct timeval CB_TIMEOUT = {15, 0};

    return (clnt_call(chan->clnt, CB_NULL, (xdrproc_t) xdr_void, NULL,
		      (xdrproc_t) xdr_void, NULL, CB_TIMEOUT));
}


/* Async thread to perform long-term reorganization, compaction,
 * other operations that cannot be performed in constant time. */
void *nfs_rpc_cb_thread(void *arg)
{

     SetNameFunction("nfs_rpc_cb_thread");

     /* Initialize BuddyMalloc (otherwise we crash whenever we call
        into the FSAL and it tries to update its calls stats) */
#ifndef _NO_BUDDY_SYSTEM
     if ((BuddyInit(&nfs_param.buddy_param_worker)) != BUDDY_SUCCESS) {
          /* Failed init */
          LogFatal(COMPONENT_NFS_CB,
                   "Memory manager could not be initialized");
     }
     LogFullDebug(COMPONENT_NFS_CB,
                  "Memory manager successfully initialized");
#endif

     while (1) {
         if (cb_thread_state.flags & CB_SHUTDOWN)
               break;

          LogFullDebug(COMPONENT_NFS_CB,
                       "top of poll loop");

          /* do stuff */

          cb_thread_delay_ms(cb_thread_state.wait_ms);
     }

     LogCrit(COMPONENT_NFS_CB,
             "shutdown");

     return (NULL);
}

void cb_wake_thread(uint32_t flags)
{
    if (cb_thread_state.flags & CB_SLEEPING)
        pthread_cond_signal(&cb_cv);
}
