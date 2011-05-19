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
#ifdef _USE_FSALMDS
#include "layouttypes/filelayout.h"
#endif
#include "fsal_glue_const.h"

#define fsal_handle_t cephfsal_handle_t
#define fsal_op_context_t cephfsal_op_context_t
#define fsal_file_t cephfsal_file_t
#define fsal_dir_t cephfsal_dir_t
#define fsal_export_context_t cephfsal_export_context_t
#define fsal_cookie_t cephfsal_cookie_t
#define fs_specific_initinfo_t cephfsal_specific_initinfo_t
#define fsal_cred_t cephfsal_cred_t
#define fsal_layoutdata_t cephfsal_layoutdata_t
#define fsal_filelockinfo_t cephfsal_filelockinfo_t
#define fsal_lockpromise_t cephfsal_lockpromise_t

  /* In this section, you must define your own FSAL internal types.
   * Here are some template types :
   */

typedef union {
 struct
 {
   vinodeno_t vi;
   struct ceph_file_layout layout;
   uint64_t snapseq;
 } data;
#ifdef _BUILD_SHARED_FSAL
  char pad[FSAL_HANDLE_T_SIZE];
#endif
} cephfsal_handle_t;

#define VINODE(fh) (fh->data.vi)

/* Authentication context. */

typedef struct fsal_cred__
{
  int user;
  int group;
  int nbgroups;
  int alt_groups[FSAL_NGROUPS_MAX];
} cephfsal_cred_t;

typedef struct fsal_export_context__
{
  char mount[FSAL_MAX_PATH_LEN];
} cephfsal_export_context_t;

typedef struct fsal_op_context__
{
  fsal_cred_t credential;
  fsal_export_context_t *export_context;
} cephfsal_op_context_t;

#define FSAL_OP_CONTEXT_TO_UID( pcontext ) ( pcontext->credential.user )
#define FSAL_OP_CONTEXT_TO_GID( pcontext ) ( pcontext->credential.group )

typedef struct fs_specific_initinfo__
{
  char cephserver[FSAL_MAX_NAME_LEN];
} cephfsal_specific_initinfo_t;


typedef union {
  loff_t cookie;
  char pad[FSAL_COOKIE_T_SIZE];
} cephfsal_cookie_t;

#define COOKIE(c) (c.cookie)

typedef struct {
  DIR* dh;
  vinodeno_t vi;
  cephfsal_op_context_t ctx;
} cephfsal_dir_t;

#define DH(dir) (dir->dh)

typedef struct {
  Fh* fh;
  vinodeno_t vi;
  cephfsal_op_context_t ctx;
} cephfsal_file_t;

#define FH(file) (file->fh)

#ifdef _USE_FSALMDS

typedef struct __deviceaddrlink {
  uint64_t inode;
  uint64_t generation;
  fsal_file_dsaddr_t* addrinfo;
  size_t entry_size;
  struct __deviceaddrlink* next;
} deviceaddrinfo;


typedef deviceaddrinfo cephfsal_layoutdata_t;

#endif /* _USE_FSALMDS */

typedef void* cephfsal_filelockinfo_t;
typedef void* cephfsal_lockpromise_t;

#endif                          /* _FSAL_TYPES_SPECIFIC_H */
