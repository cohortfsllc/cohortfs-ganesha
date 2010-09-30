/*
 *
 * Copyright (C) 2010, The Linux Box, inc.
 *
 * Contributor: Adam C. Emerson
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
 * ---------------------------------------
 */

#include "sal.h"
#include "stuff_alloc.h"
#include "nfs_core.h"
#include "nfs_log.h"
#include "log_macros.h"
#include "sal_internal.h"

/************************************************************************
 * Initialisation/Shutdown
 *
 * These are top-level exported functions for initialisation/shutdown
 * of the State Realisation.  In this realisation, local data
 * structures are initialised.
 ***********************************************************************/

int state_init(void)
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
    if (!init_concattable())
	{
	    LogMajor(COMPONENT_STATES,
		     "state_init: could not initialise concatenated entry/clientid table.");
	    return ERR_STATE_FAIL;
	}

    STUFF_PREALLOC(lockentrypool, 100, locallockentry, next_alloc);
    if (!lockentrypool)
	{
	    LogMajor(COMPONENT_STATES,
		     "state_init: could not allocate lock entry pool.");
	    return ERR_STATE_FAIL;
	}
    STUFF_PREALLOC(layoutentrypool, 100, locallayoutentry, next_alloc);
    if (!layoutentrypool)
	{
	    LogMajor(COMPONENT_STATES,
		     "state_init: could not allocate layout entry pool.");
	    return ERR_STATE_FAIL;
	}
    STUFF_PREALLOC(entryheaderpool, 500, entryheader, next_alloc);
    if (!fsaldatapool)
	{
	    LogMajor(COMPONENT_STATES,
		     "state_init: could not allocate entry pool.");
	    return ERR_STATE_FAIL;
	}
    STUFF_PREALLOC(localstatepool, 1000, localstate, next_alloc);
    if (!fsaldatapool)
	{
	    LogMajor(COMPONENT_STATES,
		     "state_init: could not allocate state entry pool.");
	    return ERR_STATE_FAIL;
	}
    STUFF_PREALLOC(concatstates, 500, concatstatepool, next_alloc);
    if (!fsaldatapool)
	{
	    LogMajor(COMPONENT_STATES,
		     "state_init: could not allocate state pool.");
	    return ERR_STATE_FAIL;
	}
    return ERR_STATE_NO_ERROR;
}

/* stuff_alloc and HashTable don't have readymade destructors, so I
 * won't worry about it now.  it's unlikely that an SAL is likely to
 * get unloaded in the lifetime of the executable anyway (knock on
 * wood.)
 */

int state_shutdown(void)
{
    return ERR_STATE_NO_ERROR;
}

