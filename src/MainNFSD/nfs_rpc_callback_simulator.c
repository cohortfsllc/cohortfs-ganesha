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
#include "nfs_rpc_callback_simulator.h"
#include "ganesha_dbus.h"

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

static DBusHandlerResult
nfs_rpc_cbsim_get_client_ids(DBusConnection *conn, DBusMessage *msg,
                      void *user_data)
{
  DBusMessage* reply;
  DBusMessageIter args;
  char *param;
  static uint32_t serial = 1;
  int i;

  hash_table_t *ht = ht_client_id;
  struct rbt_head *head_rbt;
  hash_data_t *pdata = NULL;
  struct rbt_node *pn;
  nfs_client_id_t *clientp;

  // read the arguments                                                                                                                                       
  if (!dbus_message_iter_init(msg, &args))
    LogDebug(COMPONENT_DBUS, "message has no arguments");
  else if (DBUS_TYPE_STRING != dbus_message_iter_get_arg_type(&args))
    LogDebug(COMPONENT_DBUS, "arg not string");
  else {
    dbus_message_iter_get_basic(&args, &param);
    LogDebug(COMPONENT_DBUS, "param: %s", param);
  }

  // create a reply from the message                                                                                                                          
  reply = dbus_message_new_method_return(msg);

  /* For each bucket of the hashtable */
  for(i = 0; i < ht->parameter.index_size; i++) {
    head_rbt = &(ht->array_rbt[i]);

    /* acquire mutex */
    P_w(&(ht->array_lock[i]));
    
    /* go through all entries in the red-black-tree*/
    RBT_LOOP(head_rbt, pn) {
      pdata = RBT_OPAQ(pn);
      
      clientp =
	(nfs_client_id_t *)pdata->buffval.pdata;
      char *clientid = clientp->clientid;

      dbus_message_append_args (reply,
				DBUS_TYPE_INT64, &clientid,
				DBUS_TYPE_INVALID);
      
      RBT_INCREMENT(pn);
    }
    V_w(&(ht->array_lock[i]));
  }

  // send the reply && flush the connection                                                                                                                   
  if (!dbus_connection_send(conn, reply, &serial)) {
    LogCrit(COMPONENT_DBUS, "reply failed");
  }
  dbus_connection_flush(conn);
  dbus_message_unref(reply);
  serial++;
}

/*
 * Initialize subsystem
 */
void nfs_rpc_cbsim_pkginit(void)
{
  LogEvent(COMPONENT_NFS_CB, "Callback Simulator Initialized");
 
    int32_t code;

    code = gsh_dbus_register_path("CBSIM", nfs_rpc_cbsim_get_client_ids);

    return;
}

/*
 * Shutdown subsystem
 */
void nfs_rpc_cbsim_pkgshutdown(void)
{

}
