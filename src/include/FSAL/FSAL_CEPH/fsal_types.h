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
 * \file    fsal_types.h
 * \author  $Author: leibovic $
 * \date    $Date: 2006/02/08 12:45:27 $
 * \version $Revision: 1.19 $
 * \brief   File System Abstraction Layer types and constants.
 *
 */

#ifndef _FSAL_TYPES_SPECIFIC_H
#define _FSAL_TYPES_SPECIFIC_H

# define CONF_LABEL_FS_SPECIFIC   "CEPH"

#include <sys/types.h>
#include <sys/param.h>
#include "config_parsing.h"
#include "err_fsal.h"
#include <ceph/libceph.h>
#include <pthread.h>

  /* In this section, you must define your own FSAL internal types.
   * Here are some template types :
   */
# define FSAL_MAX_NAME_LEN  256
# define FSAL_MAX_PATH_LEN  1024

/* prefered readdir size */
#define FSAL_READDIR_SIZE 2048

/** object name.  */

typedef struct fsal_name__
{
  char name[FSAL_MAX_NAME_LEN];
  unsigned int len;
} fsal_name_t;

/** object path.  */

typedef struct fsal_path__
{
  char path[FSAL_MAX_PATH_LEN];
  unsigned int len;
} fsal_path_t;

# define FSAL_NAME_INITIALIZER {"",0}
# define FSAL_PATH_INITIALIZER {"",0}

static fsal_name_t FSAL_DOT = { ".", 1 };
static fsal_name_t FSAL_DOT_DOT = { "..", 2 };

  /* some void types for this template... */

typedef uint64_t volume_id_t;

typedef struct fsal_handle__
{
  vinodeno_t vi;
  volume_id_t volid;
  fsal_nodetype_t object_type_reminder;

} fsal_handle_t;

#define FSAL_NGROUPS_MAX 32

typedef struct fsal_cred__
{
  int user;
  int group;
  int nbgroups;
  int alt_groups[FSAL_NGROUPS_MAX];
} fsal_cred_t;

typedef struct fsal_export_context__
{
  char mount[FSAL_MAX_PATH_LEN];
} fsal_export_context_t;

#define FSAL_EXPORT_CONTEXT_SPECIFIC( pexport_context ) (uint64_t)(FSAL_Handle_to_RBTIndex( &(pexport_context->root_handle), 0 ) )

typedef struct fsal_op_context__
{
  fsal_cred_t user_credential;
  fsal_cred_t credential;
  fsal_export_context_t *export_context;
} fsal_op_context_t;

#define FSAL_OP_CONTEXT_TO_UID( pcontext ) ( pcontext->credential.user )
#define FSAL_OP_CONTEXT_TO_GID( pcontext ) ( pcontext->credential.group )

typedef uintptr_t fsal_dir_t;
/* typedef uintptr_t fsal_file_t; */

typedef struct __fsal_file
{
  char guard[12];
  Fh *desc;
} fsal_file_t;

# define FSAL_FILENO(_f) fileno(_f)

typedef loff_t fsal_cookie_t;

#define FSAL_READDIR_FROM_BEGINNING 0

typedef struct fs_specific_initinfo__
{
  char cephserver[FSAL_MAX_NAME_LEN];
} fs_specific_initinfo_t;

typedef void *fsal_lockdesc_t;

#endif                          /* _FSAL_TYPES_SPECIFIC_H */
