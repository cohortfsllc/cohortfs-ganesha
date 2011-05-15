/*
 *
 * Copyright (C) 2010, The Linux Box, inc.
 * All Rights Reserved
 *
 * Contributor: Adam C. Emerson
 */

#include "sal.h"
#include "stuff_alloc.h"
#include "nfs_core.h"
#include "log_macros.h"
#include "sal_internal.h"

sal_functions_t
localsal_functions =
{
     .state_open_owner_begin41 = localstate_open_owner_begin41,
     .state_open_stateid_begin41 = localstate_open_stateid_begin41,
     .state_share_open = localstate_share_open,
     .state_share_close = localstate_share_close,
     .state_share_downgrade = localstate_share_downgrade,
     .state_share_commit = localstate_share_commit,
     .state_share_abort = localstate_share_abort,
     .state_share_dispose_transaction = localstate_share_dispose_transaction,
     .state_share_get_stateid = localstate_share_get_stateid,
     .state_share_get_nfs4err = localstate_share_get_nfs4err,
     .state_start_anonread = localstate_start_anonread,
     .state_start_anonwrite = localstate_start_anonwrite,
     .state_end_anonread = localstate_end_anonread,
     .state_end_anonwrite = localstate_end_anonwrite,
     .state_share_descriptor = localstate_share_descriptor,
     .state_init = localstate_init,
     .state_shutdown = localstate_shutdown
};

sal_functions_t
state_getfunctions(void)
{
     return localsal_functions;
}
