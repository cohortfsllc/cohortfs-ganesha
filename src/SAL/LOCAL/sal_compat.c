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

sal_functions_t localsal_functions =
    {
      .state_create_share = localstate_create_share,
      .state_upgrade_share = localstate_upgrade_share,
      .state_downgrade_share = localstate_downgrade_share,
      .state_delete_share = localstate_delete_share,
      .state_query_share = localstate_query_share,
      .state_check_share = localstate_check_share,
      .state_start_32read = localstate_start_32read,
      .state_start_32write = localstate_start_32write,
      .state_end_32read = localstate_end_32read,
      .state_end_32write = localstate_end_32write,
      .state_create_delegation = localstate_create_delegation,
      .state_delete_delegation = localstate_delete_delegation,
      .state_query_delegation = localstate_query_delegation,
      .state_check_delegation = localstate_check_delegation,
#ifdef _USE_NFS4_1
      .state_create_dir_delegation = localstate_create_dir_delegation,
      .state_delete_dir_delegation = localstate_delete_dir_delegation,
      .state_query_dir_delegation = localstate_query_dir_delegation,
#endif
      .state_check_delegation = localstate_check_delegation,
#if 0
      .state_create_lock_state = localstate_create_lock_state,
      .state_delete_lock_state = localstate_delete_lock_state,
      .state_query_lock_state = localstate_query_lock_state,
      .state_lock_inc_state = localstate_lock_inc_state,
#endif /* 0 */
#ifdef _USE_FSALMDS
      .state_create_layout_state = localstate_create_layout_state,
      .state_delete_layout_state = localstate_delete_layout_state,
      .state_query_layout_state = localstate_query_layout_state,
      .state_add_layout_segment = localstate_add_layout_segment,
      .state_mod_layout_segment = localstate_mod_layout_segment,
      .state_free_layout_segment = localstate_free_layout_segment,
      .state_layout_inc_state = localstate_layout_inc_state,
      .state_iter_layout_entries = localstate_iter_layout_entries,
#endif
      .state_lock_filehandle = localstate_lock_filehandle,
      .state_unlock_filehandle = localstate_unlock_filehandle,
      .state_iterate_by_filehandle = localstate_iterate_by_filehandle,
      .state_iterate_by_clientid = localstate_iterate_by_clientid,
      .state_retrieve_state = localstate_retrieve_state,
      .state_lock_state_owner = localstate_lock_state_owner,
      .state_unlock_state_owner = localstate_unlock_state_owner,
      .state_save_response = localstate_save_response,
      .state_init = localstate_init,
      .state_shutdown = localstate_shutdown
    };

sal_functions_t state_getfunctions(void)
{
    return localsal_functions;
}
