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
#if defined(USE_DBUS)
#include <dbus/dbus.h>
#endif /* USE_DBUS */
#include "stuff_alloc.h"
#include "nlm_list.h"
#include "fsal.h"
#include "nfs_core.h"
#include "log.h"
#include "nfs_rpc_callback.h"
#include "ganesha_dbus.h"

/**
 *
 * \file dbus_server.c
 * \author Matt Benjamin
 * \brief Low-level DBUS message server and callout framework.
 *
 * \section DESCRIPTION
 *
 * This module implements a (somewhat) generic service handler, initially to
 * support a planned callback simulator.  Developers are encouraged to expand
 * on this interface to support other use cases.
 *
 * This module should be initialized before any service provider module
 * calls gsh_dbus_register_msg();
 *
 */

#define GSH_DBUS_NONE      0x0000
#define GSH_DBUS_SHUTDOWN  0x0001
#define GSH_DBUS_SLEEPING  0x0002

struct ganesha_dbus_handler
{
    char *name;
    struct avltree_node node_k;
    gsh_dbus_method_t method;
};
typedef struct ganesha_dbus_handler ganesha_dbus_handler_t;

struct _dbus_thread_state
{
    int initialized;
    pthread_t thread_id;
    wait_entry_t we;
    DBusConnection* dbus_conn;
    DBusError dbus_err;
    uint32_t dbus_serial;
    struct avltree callouts;
    uint32_t flags;
};

static struct _dbus_thread_state thread_state;

static inline int
dbus_callout_cmpf(const struct avltree_node *lhs,
                  const struct avltree_node *rhs)
{
    ganesha_dbus_handler_t *lk, *rk;

    lk = avltree_container_of(lhs, ganesha_dbus_handler_t, node_k);
    rk = avltree_container_of(rhs, ganesha_dbus_handler_t, node_k);

    return (strcmp(lk->name, rk->name));
}

void gsh_dbus_pkginit(void)
{
    LogDebug(COMPONENT_DBUS, "init");

    avltree_init(&thread_state.callouts, dbus_callout_cmpf, 0 /* must be 0 */);

    dbus_error_init(&thread_state.dbus_err); /* sigh */
    thread_state.dbus_conn = dbus_bus_get(DBUS_BUS_SESSION,
                                          &thread_state.dbus_err);
    if (dbus_error_is_set(&thread_state.dbus_err)) {
        LogCrit(COMPONENT_DBUS,
                "dbus_bus_get failed (%s)", thread_state.dbus_err.message);
        dbus_error_free(&thread_state.dbus_err);
    }

    thread_state.initialized = TRUE;

    return;
}

int32_t gsh_dbus_register_method(const char *name, gsh_dbus_method_t method)
{
    ganesha_dbus_handler_t *handler;
    struct avltree_node *node;
    char regbuf[512];
    int code = 0;

    snprintf(regbuf, 512, "ganesha.method.%s", name);

    code = dbus_bus_request_name(thread_state.dbus_conn, regbuf,
                                 DBUS_NAME_FLAG_REPLACE_EXISTING ,
                                 &thread_state.dbus_err);
    if (dbus_error_is_set(&thread_state.dbus_err)) { 
        LogCrit(COMPONENT_DBUS, "server reg failed (%s, %s)", regbuf,
                thread_state.dbus_err.message); 
        dbus_error_free(&thread_state.dbus_err);
        if (! code)
            code = EINVAL;
        goto out;
    }
    if (code != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        LogCrit(COMPONENT_DBUS, "server failed becoming primary owner (%s, %d)",
                regbuf, code);
        goto out;
    }

    handler = (ganesha_dbus_handler_t *)
        Mem_Alloc(sizeof(ganesha_dbus_handler_t));
    handler->name = Str_Dup(regbuf);
    handler->method = method;

    node = avltree_insert(&handler->node_k, &thread_state.callouts);
    if (node) {
        LogCrit(COMPONENT_DBUS, "failed inserting method %s", regbuf);
        code = EINVAL;
    }

out:
    return (code);
}

void gsh_dbus_pkgshutdown(void)
{
    LogDebug(COMPONENT_DBUS, "shutdown");

    if (thread_state.dbus_conn)
        dbus_connection_close(thread_state.dbus_conn);
}

void *gsh_dbus_thread(void *arg)
{
    struct avltree_node *node;
    ganesha_dbus_handler_t hk, *handler;
    DBusMessage* msg;

    SetNameFunction("gsh_dbus_thread");
 
     /* Initialize BuddyMalloc */
#ifndef _NO_BUDDY_SYSTEM
    if ((BuddyInit(&nfs_param.buddy_param_worker)) != BUDDY_SUCCESS) {
        /* Failed init */
        LogFatal(COMPONENT_DBUS,
                 "Memory manager could not be initialized");
    }
    LogFullDebug(COMPONENT_DBUS,
                 "Memory manager successfully initialized");
#endif

    while (1) {
        if (thread_state.flags & GSH_DBUS_SHUTDOWN)
            break;
         
        LogFullDebug(COMPONENT_DBUS, "top of poll loop");

        /* do stuff */
        dbus_connection_read_write(thread_state.dbus_conn, 30);
        msg = dbus_connection_pop_message(thread_state.dbus_conn);
        if (! msg) {
            LogDebug(COMPONENT_DBUS, "dbus null msg");
            continue;
        }

        hk.name = dbus_message_get_path(msg); /* XXXX msg */
        node = avltree_lookup(&hk.node_k, &thread_state.callouts);
        if (node) {
            handler = avltree_container_of(node, ganesha_dbus_handler_t,
                                           node_k);
            /* we are serialized by the bus */
            handler->method(thread_state.dbus_conn, msg);
        } else {
            LogDebug(COMPONENT_DBUS, "msg for unknown handler %s",
                     hk.name);
        }
    } /* 1 */

    LogCrit(COMPONENT_DBUS, "shutdown");

    return (NULL);
}

void gsh_dbus_wake_thread(uint32_t flags)
{
    if (thread_state.flags & GSH_DBUS_SLEEPING)
        pthread_cond_signal(&thread_state.we.cv);
}
