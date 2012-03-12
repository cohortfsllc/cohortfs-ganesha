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
#include "log_macros.h"
#include "nfs_rpc_callback.h"

/**
 *
 * \file nfs_rpc_callback_simulator.c
 * \author Matt Benjamin and Lee Dobryden
 * \brief RPC callback dispatch package
 *
 * \section DESCRIPTION
 *
 * This module implements a stocastic dispatcher for callbacks, which works
 * by traversing the list of connected clients and, dispatching a callback
 * at random in consideration of state.
 *
 * This concept is inspired by the upcall simulator, though necessarily less
 * fully satisfactory until delegation and layout state are available.
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
void nfs_rpc_cbsim_pkginit(void)
{
    return;
}

/*
 * Shutdown subsystem
 */
void nfs_rpc_cbsim_pkgshutdown(void)
{
     /* Post and wait for shutdown of background thread */
     pthread_mutex_lock(&cb_mtx);
     cb_thread_state.flags |= CB_SHUTDOWN;
     cb_wake_thread(CB_FLAG_NONE);
     pthread_mutex_unlock(&cb_mtx);
}

/* Async thread to perform long-term reorganization, compaction,
 * other operations that cannot be performed in constant time. */
static void *nfs_rpc_cbsim_thread(void *arg)
{

    SetNameFunction("nfs_rpc_cbsim_thread");

    /* Initialize BuddyMalloc (otherwise we crash whenever we call
       into the FSAL and it tries to update its calls stats) */
#ifndef _NO_BUDDY_SYSTEM
    if ((BuddyInit(&nfs_param.buddy_param_worker)) != BUDDY_SUCCESS) {
        /* Failed init */
        LogFatal(COMPONENT_NFS_RPC_CB,
                 "Memory manager could not be initialized");
    }
    LogFullDebug(COMPONENT_NFS_RPC_CB,
                 "Memory manager successfully initialized");
#endif

    while (1) {
        if (cb_thread_state.flags & CB_SHUTDOWN)
            break;

        LogFullDebug(COMPONENT_NFS_RPC_CB,
                     "top of poll loop");

        /* do stuff */

        cb_thread_delay_ms(cb_thread_state.wait_ms);
    }

    LogCrit(COMPONENT_NFS_RPC_CB,
            "shutdown");

    return (NULL);
}

void cb_wake_thread(uint32_t flags)
{
    if (cb_thread_state.flags & CB_SLEEPING)
        pthread_cond_signal(&cb_cv);
}
