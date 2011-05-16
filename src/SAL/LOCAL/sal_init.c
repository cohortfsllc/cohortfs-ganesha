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

/************************************************************************
 * Initialisation/Shutdown
 *
 * These are top-level exported functions for initialisation/shutdown
 * of the State Realisation.  In this realisation, local data
 * structures are initialised.
 ***********************************************************************/

int localstate_init(void)
{
     if (!init_perfile_state_table()) {
	  LogMajor(COMPONENT_STATES,
		   "state_init: could not initialise per-file state table.");
	  return ERR_STATE_FAIL;
     }
     if (!init_stateid_table()) {
	  LogMajor(COMPONENT_STATES,
		   "state_init: could not initialise stateid table.");
	  return ERR_STATE_FAIL;
     }
     if (!init_open_owner_table()) {
	  LogMajor(COMPONENT_STATES,
		   "state_init: could not initialise open owner table.");
	  return ERR_STATE_FAIL;
     }
     if (!init_lock_owner_table()) {
	  LogMajor(COMPONENT_STATES,
		   "state_init: could not initialise lock owner table.");
	  return ERR_STATE_FAIL;
     }
     if (!init_share_state_table()) {
	  LogMajor(COMPONENT_STATES,
		   "state_init: could not initialise share state table.");
	  return ERR_STATE_FAIL;
     }
     if (!init_lock_state_table()) {
	  LogMajor(COMPONENT_STATES,
		   "state_init: could not initialise lock state table.");
	  return ERR_STATE_FAIL;
     }
     if (!init_openref_table()) {
	  LogMajor(COMPONENT_STATES,
		   "state_init: could not initialise openref table.");
	  return ERR_STATE_FAIL;
     }

     InitPool(&perfile_state_pool, 1000, perfile_state_t, NULL, NULL);
     InitPool(&state_pool, 1000, state_t, NULL, NULL);
     InitPool(&open_owner_pool, 1000, open_owner_t, NULL, NULL);
     InitPool(&lock_owner_pool, 1000, lock_owner_info_t, NULL, NULL);
     InitPool(&openref_pool, 1000, openref_t, NULL, NULL);
     InitPool(&lock_pool, 1000, lock_t, NULL, NULL);

     return ERR_STATE_NO_ERROR;
}

int localstate_shutdown(void)
{
    return ERR_STATE_NO_ERROR;
}
