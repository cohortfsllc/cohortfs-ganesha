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
 * \file    nfs41_op_write.c
 * \author  $Author: deniel $
 * \date    $Date: 20heck
 * \version $Revision: 1.15 $
 * \brief   Routines used for managing the NFS4 COMPOUND functions.
 *
 * nfs41_op_write.c : Routines used for managing the NFS4 COMPOUND functions.
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
#include "cache_content_policy.h"
#include "nfs_exports.h"
#include "nfs_creds.h"
#include "nfs_proto_functions.h"
#include "nfs_tools.h"
#include "nfs_file_handle.h"

#include <openssl/sha.h>

extern nfs_parameter_t nfs_param;

/**
 * nfs41_op_rintegrity: The NFS4_OP_WRITE operation
 * 
 * This functions handles the NFS4_OP_WRITE operation in NFSv4. This function can be called only from nfs4_Compound.
 *
 * @param op    [IN]    pointer to nfs41_op arguments
 * @param data  [INOUT] Pointer to the compound request's data
 * @param resp  [IN]    Pointer to nfs41_op results
 *
 * @return NFS4_OK if successfull, other values show an error.  
 * 
 */

extern verifier4 NFS4_write_verifier;   /* NFS V4 write verifier from nfs_Main.c     */

#define arg_RINTEGRITY4 op->nfs_argop4_u.oprintegrity
#define res_RINTEGRITY4 resp->nfs_resop4_u.oprintegrity

int cohort_comparator(const void* x, const void* y)
{
  cohort_integrity_t* a = (cohort_integrity_t*) x;
  cohort_integrity_t* b = (cohort_integrity_t*) y;

  if (a->create)
    {
      if (!b->create)
	return -1;
      else
	{
	  if (a->inodeno < b->inodeno)
	    return -1;
	  else if (b->inodeno < a->inodeno)
	    return 1;
	  else
	    return strcmp(a->name, b->name);
	}
    }
  if (b->create)
    return 1;
  else
    {
      if (a->inodeno < b->inodeno)
	return -1;
      else
	return 1;
    }
}

int nfs41_op_rintegrity(struct nfs_argop4 *op, compound_data_t * data, struct nfs_resop4 *resp)
{
  char __attribute__ ((__unused__)) funcname[] = "nfs41_op_rintegrity";

  char* integrity;
  SHA_CTX context;
  int i = 0;
  const int SHA1_len = 20;
  nfs_client_id_t* nfs_clientid;
  
  resp->resop = NFS4_OP_RINTEGRITY;

  nfs_client_id_Get_Pointer(data->psession->clientid, &nfs_clientid);

  if (nfs_clientid->integrities == NULL)
    {
      res_RINTEGRITY4.rir_status = NFS4ERR_PNFS_NO_LAYOUT;
      return res_RINTEGRITY4.rir_status;
    }

  integrity = Mem_Alloc(SHA1_len);


  SHA1_Init(&context);

  pthread_mutex_lock(&nfs_clientid->int_mutex);
  qsort(nfs_clientid->integrities, nfs_clientid->num_integrities,
	sizeof(cohort_integrity_t), cohort_comparator);

  for (i=0; i < nfs_clientid->num_integrities; i++)
    {
      SHA1_Update(&context,
		  &(nfs_clientid->integrities[i]),
		  sizeof(cohort_integrity_t));
    }
  pthread_mutex_unlock(&nfs_clientid->int_mutex);

  SHA1_Final(integrity, &context);

  res_RINTEGRITY4.RINTEGRITY4res_u.rir_integrity.cohort_signed_integrity4_len
    = SHA1_len;

  res_RINTEGRITY4.RINTEGRITY4res_u.rir_integrity.cohort_signed_integrity4_val
    = integrity;

  res_RINTEGRITY4.rir_status = NFS4_OK;
  return res_RINTEGRITY4.rir_status;
} /* nfs41_op_rintegrity */

/**
 * nfs41_op_rintegrity_Free: frees what was allocared to handle nfs41_op_write.
 * 
 * Frees what was allocared to handle nfs41_op_write.
 *
 * @param resp  [INOUT]    Pointer to nfs41_op results
 *
 * @return nothing (void function )
 * 
 */
void nfs41_op_rintegrity_Free(RINTEGRITY4res * resp)
{
  Mem_Free(resp->RINTEGRITY4res_u.rir_integrity.cohort_signed_integrity4_val);
  return;
}                               /* nfs41_op_write_Free */
