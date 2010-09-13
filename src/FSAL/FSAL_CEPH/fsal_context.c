/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright (C) 2010, The Linux Box Corporation
 * Contributor : Adam C. Emerson <aemerson@linuxbox.com>
 *
 * Some portions Copyright CEA/DAM/DIF  (2008)
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
 * ------------- 
 */

/**
 *
 * \file    fsal_creds.c
 * \brief   FSAL credentials handling functions.
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "fsal.h"
#include "fsal_internal.h"
#include <pwd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/**
 * @defgroup FSALCredFunctions Credential handling functions.
 *
 * Those functions handle security contexts (credentials).
 * 
 * @{
 */

/**
 * Parse FS specific option string
 * to build the export entry option.
 */
fsal_status_t CEPHFSAL_BuildExportContext(cephfsal_export_context_t * p_export_context, /* OUT */
					  fsal_path_t * p_export_path,      /* IN */
					  char *fs_specific_options /* IN */
    )
{
  char *argv[2];
  int argc=1;
  int rc;

  char procname[]="FSAL_CEPH";

  if((fs_specific_options != NULL) && (fs_specific_options[0] != '\0'))
    {
      DisplayLog
	("FSAL BUILD CONTEXT: ERROR: found an EXPORT::FS_Specific item whereas it is not supported for this filesystem.");

    }

  /* The mountspec we pass to Ceph's init */
 
  if (snprintf(p_export_context->mount, FSAL_MAX_PATH_LEN, "%s:%s",
	       global_spec_info.cephserver, p_export_path->path) >=
      FSAL_MAX_PATH_LEN) {
    DisplayLog ("FSAL BUILD CONTEXT: ERROR: Combined server name and path too long.");
    Return(ERR_FSAL_NAMETOOLONG, 0, INDEX_FSAL_BuildExportContext);
  }

  /* This sucks, do something better */

  argv[0]=procname;
  argv[1]=p_export_context->mount;

  if (rc=ceph_initialize(argc, (const char **)argv)) {
    Return(ERR_FSAL_SERVERFAULT, 0, INDEX_FSAL_InitClientContext);
  }

  if (ceph_mount()) {
    Return(ERR_FSAL_SERVERFAULT, 0, INDEX_FSAL_InitClientContext);
  }


  Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_BuildExportContext);
}

/**
 * FSAL_CleanUpExportContext :
 * this will clean up and state in an export that was created during
 * the BuildExportContext phase.  For many FSALs this may be a noop.
 *
 * \param p_export_context (in, gpfsfsal_export_context_t)
 */

fsal_status_t CEPHFSAL_CleanUpExportContext(cephfsal_export_context_t * p_export_context) 
{
  if (ceph_umount()) {
    Return(ERR_FSAL_SERVERFAULT, 0, INDEX_FSAL_InitClientContext);
  }


  ceph_deinitialize();

  Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_CleanUpExportContext);
}

fsal_status_t CEPHFSAL_InitClientContext(cephfsal_op_context_t * p_thr_context)
{

  int rc, i;

  /* sanity check */
  if(!p_thr_context)
    Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_InitClientContext);

  /* initialy set the export entry to none */
  p_thr_context->export_context = NULL;

  Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_InitClientContext);

}

 /**
 * FSAL_GetClientContext :
 * Get a user credential from its uid.
 * 
 * \param p_cred (in out, fsal_cred_t *)
 *        Initialized credential to be changed
 *        for representing user.
 * \param uid (in, fsal_uid_t)
 *        user identifier.
 * \param gid (in, fsal_gid_t)
 *        group identifier.
 * \param alt_groups (in, fsal_gid_t *)
 *        list of alternative groups.
 * \param nb_alt_groups (in, fsal_count_t)
 *        number of alternative groups.
 *
 * \return major codes :
 *      - ERR_FSAL_PERM : the current user cannot
 *                        get credentials for this uid.
 *      - ERR_FSAL_FAULT : Bad adress parameter.
 *      - ERR_FSAL_SERVERFAULT : unexpected error.
 */

fsal_status_t CEPHFSAL_GetClientContext(cephfsal_op_context_t * p_thr_context,  /* IN/OUT  */
					cephfsal_export_context_t * p_export_context,   /* IN */
					fsal_uid_t uid,     /* IN */
					fsal_gid_t gid,     /* IN */
					fsal_gid_t * alt_groups,    /* IN */
					fsal_count_t nb_alt_groups  /* IN */
    )
{

  fsal_status_t st;
  fsal_count_t ng = nb_alt_groups;
  unsigned int i;

  /* sanity check */
  if(!p_thr_context || !p_export_context)
    Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_GetClientContext);

  /* set the specific export context */
  p_thr_context->export_context = p_export_context;

  /* We believe what we're told */
  if(ng > FSAL_NGROUPS_MAX)
    ng = FSAL_NGROUPS_MAX;
  if((ng > 0) && (alt_groups == NULL))
    Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_GetClientContext);

  p_thr_context->credential.nbgroups = ng;

  for(i = 0; i < ng; i++)
    p_thr_context->credential.alt_groups[i] = alt_groups[i];
#if defined( _DEBUG_FSAL )

  /* traces: prints p_credential structure */

  DisplayLogJdLevel(fsal_log, NIV_FULL_DEBUG, "credential modified:");
  DisplayLogJdLevel(fsal_log, NIV_FULL_DEBUG, "\tuid = %d, gid = %d",
                    p_thr_context->credential.user, p_thr_context->credential.group);

  for(i = 0; i < p_thr_context->credential.nbgroups; i++)
    DisplayLogJdLevel(fsal_log, NIV_FULL_DEBUG, "\tAlt grp: %d",
                      p_thr_context->credential.alt_groups[i]);
#endif

  Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_GetClientContext);

}

/* @} */

