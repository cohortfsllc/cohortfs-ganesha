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
    if (!init_entrytable())
	{
	    LogMajor(COMPONENT_STATES,
		     "state_init: could not initialise entry table.");
	    return ERR_STATE_FAIL;
	}
    if (!init_stateidtable())
	{
	    LogMajor(COMPONENT_STATES,
		     "state_init: could not initialise stateid table.");
	    return ERR_STATE_FAIL;
	}
    if (!init_openownertable())
	{
	    LogMajor(COMPONENT_STATES,
		     "state_init: could not initialise open owner table.");
	    return ERR_STATE_FAIL;
	}
    if (!init_lockownertable())
	{
	    LogMajor(COMPONENT_STATES,
		     "state_init: could not initialise lock owner table.");
	    return ERR_STATE_FAIL;
	}

#ifdef _USE_FSALMDS
    InitPool(&layoutentrypool, 100, locallayoutentry_t, NULL, NULL);
#endif
    
    InitPool(&statepool, 1000, state_t, NULL, NULL);
    InitPool(&entryheaderpool, 1000, entryheader_t, NULL, NULL);
    InitPool(&ownerpool, 1000, state_owner_t, NULL, NULL);

    return ERR_STATE_NO_ERROR;
}

/* stuff_alloc and HashTable don't have readymade destructors, so I
 * won't worry about it now.  it's unlikely that an SAL is likely to
 * get unloaded in the lifetime of the executable anyway (knock on
 * wood.)
 */

int localstate_shutdown(void)
{
    return ERR_STATE_NO_ERROR;
}

