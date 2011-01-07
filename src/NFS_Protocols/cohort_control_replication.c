/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright (C) 2011, The Linux Box Corporation
 * Contributor: Adam C. Emerson <aemerson@linuxbox.com>
 */

/**
 * \file    cohort_control_replication.c
 *
 * cohort_control_replication.c : Handle the Replication Control
 * operation in CohortFS
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
 * cohort_control_replication: Slave replication for cohort
 *
 * This function implements the COHORT_CONTROL_REPLICATION operation.
 *
 * @param op    [IN]    pointer to nfs4_op arguments
 * @param data  [INOUT] Pointer to the compound request's data
 * @param resp  [IN]    Pointer to nfs4_op results
 * 
 * @return NFS4_OK if successfull, other values show an error. 
 *
 * @see all the nfs41_op_<*> function
 * @see nfs4_Compound
 *
 */

#define arg_COHORT_CONTROL_REPLICATION op->nfs_argop4_u.cohort_control_replication
#define res_COHORT_CONTROL_REPLICATION resp->nfs_resop4_u.cohort_control_replication

int cohort_control_replication(struct nfs_argop4 *op, compound_data_t * data,
			       struct nfs_resop4 *resp)
{
  char __attribute__ ((__unused__)) funcname[] = "cohort_control_replication";
  clientid4 clientid;
  nfs_client_id_t nfs_clientid;
  nfs_worker_data_t *pworker = NULL;

  resp->resop = COHORT_CONTROL_REPLICATION;

  pworker = (nfs_worker_data_t *) data->pclient->pworker;

  if (arg_COHORT_CONTROL_REPLICATION.ccra_operation == COHORT_BEGIN)
    {
      strncpy(nfs_clientid.client_name,
	      arg_COHORT_CONTROL_REPLICATION.ccra_client_owner.co_ownerid.co_ownerid_val,
	      arg_COHORT_CONTROL_REPLICATION.ccra_client_owner.co_ownerid.co_ownerid_len);
      nfs_clientid.client_name[arg_COHORT_CONTROL_REPLICATION.ccra_client_owner.co_ownerid.co_ownerid_len]
	= '\0';
      if(nfs_client_id_basic_compute(nfs_clientid.client_name, &clientid) != CLIENT_ID_SUCCESS)
	{
	  res_COHORT_CONTROL_REPLICATION.ccrr_status = NFS4ERR_SERVERFAULT;
	  return res_COHORT_CONTROL_REPLICATION.ccrr_status;
	}
      memset(&nfs_clientid, 0, sizeof(nfs_clientid));
      nfs_clientid.clientid = clientid;
      nfs_clientid.num_integrities = 0;
      nfs_clientid.integrities = Mem_Alloc(MAX_COHORT_INTEGRITIES *
					   sizeof(cohort_integrity_t));
      nfs_clientid.repstate
	= arg_COHORT_CONTROL_REPLICATION.ccra_stateid;
      if (nfs_clientid.integrities == NULL)
	{
	  res_COHORT_CONTROL_REPLICATION.ccrr_status
	    = NFS4ERR_SERVERFAULT;
	  return res_COHORT_CONTROL_REPLICATION.ccrr_status;
	}

      if(nfs_client_id_add(clientid, nfs_clientid, &pworker->clientid_pool) !=
         CLIENT_ID_SUCCESS)
        {
	  res_COHORT_CONTROL_REPLICATION.ccrr_status
	    = NFS4ERR_SERVERFAULT;
	  return res_COHORT_CONTROL_REPLICATION.ccrr_status;
        }
    }
  else if (arg_COHORT_CONTROL_REPLICATION.ccra_operation ==
	   COHORT_END)
    {
      char client_name[MAXNAMLEN];

      strncpy(client_name,
	      arg_COHORT_CONTROL_REPLICATION.ccra_client_owner.co_ownerid.co_ownerid_val,
	      arg_COHORT_CONTROL_REPLICATION.ccra_client_owner.co_ownerid.co_ownerid_len);
      client_name[arg_COHORT_CONTROL_REPLICATION.ccra_client_owner.co_ownerid.co_ownerid_len]
	= '\0';
      if (nfs_client_id_get_reverse(client_name, &nfs_clientid) !=
	  CLIENT_ID_SUCCESS)
	{
	  res_COHORT_CONTROL_REPLICATION.ccrr_status =
	    NFS4ERR_NOENT;
	  return res_COHORT_CONTROL_REPLICATION.ccrr_status;
	}
      if (nfs_clientid.integrities == NULL)
	{
	  res_COHORT_CONTROL_REPLICATION.ccrr_status =
	    NFS4ERR_NOENT;
	  return res_COHORT_CONTROL_REPLICATION.ccrr_status;
	}
      Mem_Free(nfs_clientid.integrities);
      nfs_clientid.integrities = NULL;
      nfs_clientid.num_integrities = 0;
      memset(&nfs_clientid.repstate, 0, sizeof(stateid4));
      if (nfs_client_id_set(nfs_clientid.clientid,
			    nfs_clientid,
			    &pworker->clientid_pool)
	  != CLIENT_ID_SUCCESS)
	{
	  res_COHORT_CONTROL_REPLICATION.ccrr_status =
	    NFS4ERR_SERVERFAULT;
	  return res_COHORT_CONTROL_REPLICATION.ccrr_status;
	}
    }
  else
    {
      res_COHORT_CONTROL_REPLICATION.ccrr_status = NFS4ERR_INVAL;
      return res_COHORT_CONTROL_REPLICATION.ccrr_status;
    }

  res_COHORT_CONTROL_REPLICATION.ccrr_status = NFS4_OK;
  return res_COHORT_CONTROL_REPLICATION.ccrr_status;
}                               /* cohort_control_replication */
