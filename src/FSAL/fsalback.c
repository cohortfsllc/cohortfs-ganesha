#include "nfs4.h"
#include "nfs23.h"
#include "fsal_types.h"
#include "nfs_exports.h"
#include "nfs_file_handle.h"
#include <rpc/xdr.h>
#include "fsal.h"
#include "nfs_core.h"

#ifdef _USE_FSALMDS
#include "layouttypes/fsal_layout.h"
#include "layouttypes/filelayout.h"


/**
 *
 * FSALBACK_fh2dshandle: converts an FSAL file handle to a DS handle
 *
 * Converts an FSAL file handle to a DS handle
 *
 * @param fhin   [IN]  pointer to FSAL file handle
 * @param fhout  [OUT] pointer to DS filehandle to write
 * @param cookie [IN]  cookie passed to layoutget
 *
 * @return 1 if successful, 0 otherwise
 *
 */

int FSALBACK_fh2dshandle(fsal_handle_t *fhin, fsal_dsfh_t* fhout,
			 void* opaque)
{
  compound_data_t* data = (compound_data_t*) opaque;
  nfs_fh4 fhk;
  int rc;
  file_handle_v4_t* fhs;

  fhk.nfs_fh4_val = fhout->nfs_fh4_val;
  
  rc = nfs4_FSALToFhandle(&fhk, fhin, data);
  fhout->nfs_fh4_len = fhk.nfs_fh4_len;
  fhs = (file_handle_v4_t *) fhout->nfs_fh4_val;
  fhs->ds_flag = 1;
  return rc;
}

int FSALBACK_fh2rhandle(fsal_handle_t *fhin, fsal_dsfh_t* fhout,
			void* opaque)
{
  compound_data_t* data = (compound_data_t*) opaque;
  nfs_fh4 fhk;
  int rc;
  file_handle_v4_t* fhs;

  fhk.nfs_fh4_val = fhout->nfs_fh4_val;
  
  rc = nfs4_FSALToFhandle(&fhk, fhin, data);
  fhout->nfs_fh4_len = fhk.nfs_fh4_len;
  fhs = (file_handle_v4_t *) fhout->nfs_fh4_val;
  return rc;
}

void FSALBACK_client_owner(void* opaque, client_owner4* client_owner)
{
  compound_data_t* data = (compound_data_t*) opaque;
  nfs_client_id_t* nfs_clientid;

  nfs_client_id_Get_Pointer(data->psession->clientid, &nfs_clientid);
  memcpy(client_owner->co_verifier, nfs_clientid->verifier,
	 NFS4_VERIFIER_SIZE);
  client_owner->co_ownerid.co_ownerid_len
    = strlen(nfs_clientid->client_name);
  client_owner->co_ownerid.co_ownerid_val
    = nfs_clientid->client_name;
}

#endif
