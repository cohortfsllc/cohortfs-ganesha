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

#define allzeros  {0x00, 0x00, 0x00, 0x00, \
                   0x00, 0x00, 0x00, 0x00, \
		   0x00, 0x00, 0x00, 0x00}
#define allones {0xff, 0xff, 0xff, 0xff, \
                 0xff, 0xff, 0xff, 0xff,			\
		 0xff, 0xff, 0xff, 0xff}

stateid4 state_anonymous_stateid =
  {
    .seqid = 0,
    .other = allzeros
  };

stateid4 state_bypass_stateid =
  {
    .seqid = NFS4_UINT32_MAX,
    .other = allones
  };

stateid4 state_current_stateid =
  {
    .seqid = 1,
    .other = allzeros
  };

stateid4 state_invalid_stateid =
  {
    .seqid = NFS4_UINT32_MAX,
    .other = allzeros
  };
  
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
#ifdef _USE_NFS4_1
	  return NFS4ERR_WRONG_TYPE;
#else
	  return NFS4ERR_INVAL;
#endif
	  break;
      case ERR_STATE_NOMUTATE:
      case ERR_STATE_PREEXISTS:
      case ERR_STATE_FAIL:
      default:
	  return NFS4ERR_SERVERFAULT;
	  break;
      }
}

bool_t state_anonymous_check(stateid4 stateid)
{
  return (state_anonymous_exact_check(stateid) ||
	  state_bypass_check(stateid));
}

bool_t state_anonymous_exact_check(stateid4 stateid)
{
  return !memcmp(&stateid, &state_anonymous_stateid, sizeof(stateid4));
}

bool_t state_bypass_check(stateid4 stateid)
{
  return !memcmp(&stateid, &state_bypass_stateid, sizeof(stateid4));
}

bool_t state_current_check(stateid4 stateid)
{
  return !memcmp(&stateid, &state_current_stateid, sizeof(stateid4));
}

bool_t state_invalid_check(stateid4 stateid)
{
  return !memcmp(&stateid, &state_invalid_stateid, sizeof(stateid4));
}
