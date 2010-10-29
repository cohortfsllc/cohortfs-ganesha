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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif                          /* HAVE_CONFIG_H */ 

#ifdef _SOLARIS
#include "solaris_port.h"
#endif                          /* _SOLARIS */

#ifdef _USE_SHARED_SAL
#include <stdlib.h>
#include <dlfcn.h>              /* For dlopen */
#endif


#include "sal.h"
#include "log_macros.h"
#include "fsal_types.h"

uint32_t allzeros[3] = {0, 0, 0};
uint32_t allones[3] = {0xffffffff, 0xffffffff, 0xffffffff};

uint32_t staterr2nfs4err(uint32_t staterr)
{
  switch (staterr)
      {
      case ERR_STATE_NO_ERROR:
	  return NFS4_OK;
	  break;
      case ERR_STATE_CONFLICT:
	  return NFS4ERR_DENIED;
	  break;
      case ERR_STATE_LOCKSHELD:
	  return NFS4ERR_LOCKS_HELD;
	  break;
      case ERR_STATE_OLDSEQ:
	  return NFS4ERR_OLD_STATEID;
	  break;
      case ERR_STATE_BADSEQ:
      case ERR_STATE_BAD:
      case ERR_STATE_NOENT:
	  return NFS4ERR_BAD_STATEID;
	  break;
      case ERR_STATE_STALE:
	  return NFS4ERR_STALE_STATEID;
	  break;
      case ERR_STATE_OBJTYPE:
	  return NFS4ERR_WRONG_TYPE;
	  break;
      case ERR_STATE_NOMUTATE:
      case ERR_STATE_PREEXISTS:
      case ERR_STATE_FAIL:
      default:
	  return NFS4ERR_SERVERFAULT;
	  break;
      }
}

bool_t state_anonymous_stateid(stateid4 stateid)
{
  return ((!memcmp(stateid.other, allzeros, 12) && !stateid.seqid) ||
	  (!memcmp(stateid.other, allones, 12) && !(~stateid.seqid)));
}

bool_t state_current_stateid(stateid4 stateid)
{
  return (!memcmp(stateid.other, allzeros, 12) &&
	  (stateid.seqid == 1));
}

bool_t state_invalid_stateid(stateid4 stateid)
{
  return (!memcmp(stateid.other, allzeros, 12) &&
	  (stateid.seqid == NFS4_UINT32_MAX));
}
