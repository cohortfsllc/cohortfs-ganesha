/*
 *
 * Copyright (C) 2010, The Linux Box, inc.
 * All Rights Reserved
 *
 * Contributor: Adam C. Emerson
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

static bool_t
compare_nfs3_lockowner(state_lockowner_t* owner1,
		       state_lockowner_t* owner2)
{
    LogMajor(COMPONENT_STATES,
	     "NFS3 owner comparison is not implemented.  Please go to line %d in file %s and implement it.",
	     __LINE__, __FILE__);
    return FALSE;
}

static bool_t
compare_nfs4_lockowner(state_lockowner_t* owner1,
		       state_lockowner_t* owner2)
{
    return ((owner1->u.nfs4_owner.clientid ==
	     owner2->u.nfs4_owner.clientid) &&
	    (owner1->u.nfs4_owner.owner.owner_len ==
	     owner2->u.nfs4_owner.owner.owner_len) &&
	    (memcmp((void*) owner1->u.nfs4_owner.owner.owner_val,
		    (void*) owner2->u.nfs4_owner.owner.owner_val,
		    owner1->u.nfs4_owner.owner.owner_len) == 0));
}

bool_t
state_compare_lockowner(state_lockowner_t* owner1,
			state_lockowner_t* owner2)
{
    switch (owner1->owner_type) {
    case LOCKOWNER_NFS3:
	if (owner2->owner_type == LOCKOWNER_NFS3) {
	    return compare_nfs3_lockowner(owner1, owner2);
	}

    case LOCKOWNER_NFS4:
	if (owner2->owner_type == LOCKOWNER_NFS4) {
	    return compare_nfs4_lockowner(owner1, owner2);
	}

    default:
	return FALSE;
    }
}
