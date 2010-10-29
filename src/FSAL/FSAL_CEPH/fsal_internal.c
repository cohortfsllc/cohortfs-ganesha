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
 * \file    fsal_internal.c
 * \brief   Defines the datas that are to be
 *          accessed as extern by the fsal modules
 *
 */
#define FSAL_INTERNAL_C
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include  "fsal.h"
#include "fsal_internal.h"
#include "stuff_alloc.h"
#include "SemN.h"
#include "nfsv41.h"
#include "HashTable.h"

#include <pthread.h>

/* static filesystem info.
 * The access is thread-safe because
 * it is read-only, except during initialization.
 */
fsal_staticfsinfo_t global_fs_info;
fs_specific_initinfo_t global_spec_info;
extern hash_table_t* deviceidtable;

#define POSIX_SUPPORTED_ATTRIBUTES (                                       \
          FSAL_ATTR_SUPPATTR | FSAL_ATTR_TYPE     | FSAL_ATTR_SIZE      | \
          FSAL_ATTR_FSID     | FSAL_ATTR_FILEID   | \
          FSAL_ATTR_MODE     | FSAL_ATTR_NUMLINKS | FSAL_ATTR_OWNER     | \
          FSAL_ATTR_GROUP    | FSAL_ATTR_ATIME    | FSAL_ATTR_RAWDEV    | \
          FSAL_ATTR_CTIME    | FSAL_ATTR_MTIME    | FSAL_ATTR_SPACEUSED | \
          FSAL_ATTR_CHGTIME  )



/* filesystem info for your filesystem */
static fsal_staticfsinfo_t default_hpss_info = {
  0xFFFFFFFFFFFFFFFFLL,         /* max file size (64bits) */
  1024,                         /* max links for an object of your filesystem */
  FSAL_MAX_NAME_LEN,            /* max filename */
  FSAL_MAX_PATH_LEN,            /* min filename */
  TRUE,                         /* no_trunc */
  TRUE,                         /* chown restricted */
  FALSE,                        /* case insensitivity */
  TRUE,                         /* case preserving */
  FSAL_EXPTYPE_PERSISTENT,      /* FH expire type */
  TRUE,                         /* hard link support */
  TRUE,                         /* sym link support */
  FALSE,                        /* lock support */
  TRUE,                         /* named attributes */
  TRUE,                         /* handles are unique and persistent */
  {10, 0},                      /* Duration of lease at FS in seconds */
  FSAL_ACLSUPPORT_DENY,        /* ACL support */
  TRUE,                         /* can change times */
  TRUE,                         /* homogenous */
  POSIX_SUPPORTED_ATTRIBUTES,    /* supported attributes */
  (4096 * 1024),                /* maxread size */
  (4096 * 1024),                /* maxwrite size */
  0,                            /* default umask */
  0,                            /* don't allow cross fileset export path */
  0400                         /* default access rights for xattrs: root=RW, owner=R */
};

layouttype4 supportedlayouttypes[]={LAYOUT4_NFSV4_1_FILES};

/*
 *  Log Descriptor
 */

/* variables for limiting the calls to the filesystem */
static int limit_calls = FALSE;
semaphore_t sem_fs_calls;

/* threads keys for stats */
static pthread_key_t key_stats;
static pthread_once_t once_key = PTHREAD_ONCE_INIT;

/* init keys */
static void init_keys(void)
{

  return;
}                               /* init_keys */

/**
 * fsal_increment_nbcall:
 * Updates fonction call statistics.
 *
 * \param function_index (input):
 *        Index of the function whom number of call is to be incremented.
 * \param status (input):
 *        Status the function returned.
 *
 * \return Nothing.
 */
void fsal_increment_nbcall(int function_index, fsal_status_t status)
{
}

/**
 * fsal_internal_getstats:
 * (For internal use in the FSAL).
 * Retrieve call statistics for current thread.
 *
 * \param output_stats (output):
 *        Pointer to the call statistics structure.
 *
 * \return Nothing.
 */
void fsal_internal_getstats(fsal_statistics_t * output_stats)
{
}

/**
 *  Used to limit the number of simultaneous calls to Filesystem.
 */
void TakeTokenFSCall()
{
  /* no limits */
  if(limit_calls == FALSE)
    return;

  /* there is a limit */
  semaphore_P(&sem_fs_calls);

}

void ReleaseTokenFSCall()
{
  /* no limits */
  if(limit_calls == FALSE)
    return;

  /* there is a limit */
  semaphore_V(&sem_fs_calls);

}

#define SET_INTEGER_PARAM( cfg, p_init_info, _field )             \
    switch( (p_init_info)->behaviors._field ){                    \
    case FSAL_INIT_FORCE_VALUE :                                  \
      /* force the value in any case */                           \
      cfg._field = (p_init_info)->values._field;                  \
      break;                                                      \
    case FSAL_INIT_MAX_LIMIT :                                    \
      /* check the higher limit */                                \
      if ( cfg._field > (p_init_info)->values._field )            \
        cfg._field = (p_init_info)->values._field ;               \
      break;                                                      \
    case FSAL_INIT_MIN_LIMIT :                                    \
      /* check the lower limit */                                 \
      if ( cfg._field < (p_init_info)->values._field )            \
        cfg._field = (p_init_info)->values._field ;               \
      break;                                                      \
    /* In the other cases, we keep the default value. */          \
    }

#define SET_BITMAP_PARAM( cfg, p_init_info, _field )              \
    switch( (p_init_info)->behaviors._field ){                    \
    case FSAL_INIT_FORCE_VALUE :                                  \
        /* force the value in any case */                         \
        cfg._field = (p_init_info)->values._field;                \
        break;                                                    \
    case FSAL_INIT_MAX_LIMIT :                                    \
      /* proceed a bit AND */                                     \
      cfg._field &= (p_init_info)->values._field ;                \
      break;                                                      \
    case FSAL_INIT_MIN_LIMIT :                                    \
      /* proceed a bit OR */                                      \
      cfg._field |= (p_init_info)->values._field ;                \
      break;                                                      \
    /* In the other cases, we keep the default value. */          \
    }

#define SET_BOOLEAN_PARAM( cfg, p_init_info, _field )             \
    switch( (p_init_info)->behaviors._field ){                    \
    case FSAL_INIT_FORCE_VALUE :                                  \
        /* force the value in any case */                         \
        cfg._field = (p_init_info)->values._field;                \
        break;                                                    \
    case FSAL_INIT_MAX_LIMIT :                                    \
      /* proceed a boolean AND */                                 \
      cfg._field = cfg._field && (p_init_info)->values._field ;   \
      break;                                                      \
    case FSAL_INIT_MIN_LIMIT :                                    \
      /* proceed a boolean OR */                                  \
      cfg._field = cfg._field && (p_init_info)->values._field ;   \
      break;                                                      \
    /* In the other cases, we keep the default value. */          \
    }


unsigned long hash_inode_rbt(p_hash_parameter_t params,
			     hash_buffer_t* keybuff)
{
  uint64_t inode=*((uint64_t* )keybuff->pdata);

  return (unsigned long) (((0xaaaaaaaa00000000 & inode) >> 0x20) |
			  ((0x0000000055555555 & inode)));
}

int compare_inode_key(hash_buffer_t* keybuff1,
		      hash_buffer_t* keybuff2)
{
  uint64_t inode1=*((uint64_t* )keybuff1->pdata);
  uint64_t inode2=*((uint64_t* )keybuff2->pdata);

  return !(inode1 == inode2);
}

int dummy2str(hash_buffer_t* buff, char* string)
{
  strcpy(string, "dummy");
}

unsigned long simple_hash_func(hash_parameter_t * p_hparam,
			       hash_buffer_t * buffclef);

hash_parameter_t hashparams = {
  .index_size=17,
  .alphabet_length=8,
  .nb_node_prealloc=100,
  .hash_func_key=simple_hash_func,
  .hash_func_rbt=hash_inode_rbt,
  .compare_key=compare_inode_key,
  .key_to_str=dummy2str,
  .val_to_str=dummy2str
};

/*
 *  This function initializes shared variables of the fsal.
 */
fsal_status_t fsal_internal_init_global(fsal_init_info_t * fsal_info,
                                        fs_common_initinfo_t * fs_common_info)
{

#ifdef _USE_FSALMDS
  deviceidtable=HashTable_Init(hashparams);
#endif
  /* sanity check */
  if(!fsal_info || !fs_common_info)
    ReturnCode(ERR_FSAL_FAULT, 0);

  /* inits FS call semaphore */
  if(fsal_info->max_fs_calls > 0)
    {
      int rc;

      limit_calls = TRUE;

      rc = semaphore_init(&sem_fs_calls, fsal_info->max_fs_calls);

      if(rc != 0)
        ReturnCode(ERR_FSAL_SERVERFAULT, rc);


    }

  /* setting default values. */
  global_fs_info = default_hpss_info;

#ifdef _USE_FSALMODS
  global_fs_info.fs_layout_types.fattr4_fs_layout_types_len=1;
  global_fs_info.fs_layout_types.fattr4_fs_layout_types_val=supportedlayouttypes;
#endif

  /* Analyzing fs_common_info struct */

  if((fs_common_info->behaviors.maxfilesize != FSAL_INIT_FS_DEFAULT) ||
     (fs_common_info->behaviors.maxlink != FSAL_INIT_FS_DEFAULT) ||
     (fs_common_info->behaviors.maxnamelen != FSAL_INIT_FS_DEFAULT) ||
     (fs_common_info->behaviors.maxpathlen != FSAL_INIT_FS_DEFAULT) ||
     (fs_common_info->behaviors.no_trunc != FSAL_INIT_FS_DEFAULT) ||
     (fs_common_info->behaviors.case_insensitive != FSAL_INIT_FS_DEFAULT) ||
     (fs_common_info->behaviors.case_preserving != FSAL_INIT_FS_DEFAULT) ||
     (fs_common_info->behaviors.named_attr != FSAL_INIT_FS_DEFAULT) ||
     (fs_common_info->behaviors.lease_time != FSAL_INIT_FS_DEFAULT) ||
     (fs_common_info->behaviors.supported_attrs != FSAL_INIT_FS_DEFAULT) ||
     (fs_common_info->behaviors.homogenous != FSAL_INIT_FS_DEFAULT) ||
     (fs_common_info->behaviors.symlink_support != FSAL_INIT_FS_DEFAULT) ||
     (fs_common_info->behaviors.link_support != FSAL_INIT_FS_DEFAULT) ||
     (fs_common_info->behaviors.lock_support != FSAL_INIT_FS_DEFAULT) ||
     (fs_common_info->behaviors.cansettime != FSAL_INIT_FS_DEFAULT) ||
     (fs_common_info->behaviors.maxread != FSAL_INIT_FS_DEFAULT) ||
     (fs_common_info->behaviors.maxwrite != FSAL_INIT_FS_DEFAULT) ||
     (fs_common_info->behaviors.auth_exportpath_xdev != FSAL_INIT_FS_DEFAULT))
    ReturnCode(ERR_FSAL_NOTSUPP, 0);

  SET_BITMAP_PARAM(global_fs_info, fs_common_info, umask);

  SET_BOOLEAN_PARAM(global_fs_info, fs_common_info, auth_exportpath_xdev);

  SET_BITMAP_PARAM(global_fs_info, fs_common_info, xattr_access_rights);

  ReturnCode(ERR_FSAL_NO_ERROR, 0);
}

