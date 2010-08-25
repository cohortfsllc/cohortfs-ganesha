/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright CEA/DAM/DIF  (2008)
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
 * \file    nfs41_op_getdevicelist.c
 * \author  $Author: deniel $
 * \date    $Date: 2009/08/19 16:02:52 $
 * \brief   Routines used for managing the NFS4_OP_GETDEVICELIST operation.
 *
 * nfs41_op_getdevicelist.c :  Routines used for managing the GETDEVICELIST operation.
 * 
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _SOLARIS
#include "solaris_port.h"
#endif

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/file.h>           /* for having FNDELAY */
#include "HashData.h"
#include "HashTable.h"
#ifdef _USE_GSSRPC
#include <gssrpc/types.h>
#include <gssrpc/rpc.h>
#include <gssrpc/auth.h>
#include <gssrpc/pmap_clnt.h>
#else
#include <rpc/types.h>
#include <rpc/rpc.h>
#include <rpc/auth.h>
#include <rpc/pmap_clnt.h>
#endif

#include "log_macros.h"
#include "stuff_alloc.h"
#include "nfs23.h"
#include "nfs4.h"
#include "mount.h"
#include "nfs_core.h"
#include "cache_inode.h"
#include "cache_content.h"
#include "nfs_exports.h"
#include "nfs_creds.h"
#include "nfs_proto_functions.h"
#include "nfs_file_handle.h"
#include "nfs_tools.h"

/**
 *
 * nfs41_op_getdevicelist:  The NFS4_OP_GETDEVICELIST operation.
 *
 * Gets the list of pNFS devices
 *
 * @param op    [IN]    pointer to nfs4_op arguments
 * @param data  [INOUT] Pointer to the compound request's data
 * @param resp  [IN]    Pointer to nfs4_op results
 *
 * @return NFS4_OK if successfull, other values show an error. 
 *
 * @see nfs4_Compound
 *
 */
#define arg_GETDEVICELIST4  op->nfs_argop4_u.opgetdevicelist
#define res_GETDEVICELIST4  resp->nfs_resop4_u.opgetdevicelist

int nfs41_op_getdevicelist(struct nfs_argop4 *op,
                           compound_data_t * data, struct nfs_resop4 *resp)
{
  char __attribute__ ((__unused__)) funcname[] = "nfs4_op_getdevicelist";
#if defined(_USE_PNFS)

  resp->resop = NFS4_OP_GETDEVICELIST;
  res_GETDEVICELIST4.gdlr_status = NFS4_OK;

  return res_GETDEVICELIST4.gdlr_status;

#elidf defined(_USE_FSALMDS)
  fsal_status_t status;
  size_t bufflen=10240;
  deviceid4* buff;
  char* xdrbuff;
  uint64_t cookie=arg_GETDEVICELIST4.gdla_cookie;
  fsal_boolean_t eof=FALSE;
  uint32_t count=arg_GETDEVICELIST4.gdla_maxdevices;
  fsal_handle_t fsalh;
  
  resp->resop = NFS4_OP_GETDEVICELIST;

  /* If there is no FH */
  if(nfs4_Is_Fh_Empty(&(data->currentFH)))
    {
      res_GETDEVICELIST4.gdlr_status = NFS4ERR_NOFILEHANDLE;
      return res_GETDEVICELIST4.gdlr_status;
    }

  /* If the filehandle is invalid */
  if(nfs4_Is_Fh_Invalid(&(data->currentFH)))
    {
      res_GETDEVICELIST4.gdlr_status = NFS4ERR_BADHANDLE;
      return res_GETDEVICELIST4.gdlr_status;
    }

  /* Tests if the Filehandle is expired (for volatile filehandle) */
  if(nfs4_Is_Fh_Expired(&(data->currentFH)))
    {
      res_GETDEVICELIST4.gdlr_status = NFS4ERR_FHEXPIRED;
      return res_GETDEVICELIST4.gdlr_status;
    }

#ifdef _USE_FSALDS
  if(nfs4_Is_Fh_DSHandle(data->currentFH))
    {
      res_GETDEVICELIST4.status = NFS4ERR_NOTSUPP;
      return res_GETDEVICELIST4.status;
    }
#endif /* _USE_FSALDS */

  if ((buff = (deviceid4*) Mem_Alloc(bufflen))==NULL)
    {
      res_GETDEVICELIST4.gdlr_status = NFS4ERR_SERVERFAULT;
      return res_GETDEVICELIST4.gdlr_status;
    }

  nfs4_FhandletoFSAL(data->currentFH, &fsalh, data->pcontext);

  status = FSAL_getdevicelist(&fsalh,
			      arg_GETDEVICELIST4.gdla_layout_type,
			      &count,
			      &cookie,
			      &eof,
			      buff,
			      &bufflen);

  if (FSAL_IS_ERROR(status))
    {
      Mem_Free(buff);
      res_GETDEVICELIST4.gdlr_status = status.major;
      return res_GETDEVICELIST4.gdlr_status;
    }

  res_GETDEVICELIST4.GETDEVICELIST4res_u.gdlr_resok4.gdlr_cookie=cookie;
  res_GETDEVICELIST4.GETDEVICELIST4res_u.gdlr_resok4
    .gdlr_deviceid_list.gdlr_deviceid_list_val=buff;
  res_GETDEVICELIST4.GETDEVICELIST4res_u.gdlr_resok4
    .gdlr_deviceid_list.gdlr_deviceid_list_len=bufflen;
  res_GETDEVICELIST4.GETDEVICELIST4res_u.gdlr_resok4.gdlr_eof=eof;
  
  res_GETDEVICELIST4.gdlr_status = NFS4_OK;
  return res_GETDEVICELIST4.gdlr_status;
			      
#endif                          /* _USE_PNFS */
}                               /* nfs41_op_exchange_id */

/**
 * nfs4_op_getdevicelist_Free: frees what was allocared to handle nfs4_op_getdevicelist.
 * 
 * Frees what was allocared to handle nfs4_op_getdevicelist.
 *
 * @param resp  [INOUT]    Pointer to nfs4_op results
 *
 * @return nothing (void function )
 * 
 */
void nfs41_op_getdevicelist_Free(GETDEVICELIST4res * resp)
{
#ifdef _USE_FSALMDS
  if (resp->gdlr_status == NFS4_OK)
    Mem_Free(resp->GETDEVICELIST4res_u.gdlr_resok4
	     .gdlr_deviceid_list.gdlr_deviceid_list_val);
#endif                          /* _USE_FSALMDS */
  return;
}                               /* nfs41_op_exchange_id_Free */
