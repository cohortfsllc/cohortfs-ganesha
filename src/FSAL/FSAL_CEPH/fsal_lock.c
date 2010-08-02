/*
 * Copyright (C) 2010 The Linx Box Corporation
 * Contributor : Adam C. Emerson
 *
 * Some Portions Copyright CEA/DAM/DIF  (2008)
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
 * ---------------------------------------
 */

/**
 *
 * \file    fsal_lock.c
 * \brief   Locking operations.
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "fsal.h"
#include "fsal_internal.h"

/**
 * FSAL_lock:
 * Lock an entry in the filesystem.
 */
fsal_status_t CEPHFSAL_lock(cephfsal_file_t * obj_handle,
			    cephfsal_lockdesc_t * ldesc,
			    fsal_boolean_t blocking)
{
  Return(ERR_FSAL_NOTSUPP, 0, INDEX_FSAL_lock);
}

/**
 * FSAL_changelock:
 * Not implemented.
 */
fsal_status_t CEPHFSAL_changelock(cephfsal_lockdesc_t * lock_descriptor, /* IN / OUT */
				  fsal_lockparam_t * lock_info      /* IN */
    )
{

  /* sanity checks. */
  if(!lock_descriptor)
    Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_changelock);

  Return(ERR_FSAL_NOTSUPP, 0, INDEX_FSAL_changelock);

}

/*
 * Unimplemented
 */

fsal_status_t CEPHFSAL_getlock(cephfsal_file_t * obj_handle, cephfsal_lockdesc_t * ldesc)
{

  /* sanity checks. */
  if(!ldesc || !obj_handle)
    Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_changelock);

  Return(ERR_FSAL_NOTSUPP, 0, INDEX_FSAL_changelock);

}

/**
 * FSAL_unlock:
 * Not implemented.
 */
fsal_status_t CEPHFSAL_unlock(cephfsal_file_t * obj_handle,
			      fsal_lockdesc_t * ldesc)
{

  Return(ERR_FSAL_NOTSUPP, 0, INDEX_FSAL_unlock);
}
