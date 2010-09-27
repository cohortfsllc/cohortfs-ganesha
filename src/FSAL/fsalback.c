#include "nfsv41.h"
#include "nfs23.h"
#include "fsal_types.h"
#include "nfs_exports.h"
#include "nfs_file_handle.h"
#include <rpc/xdr.h>
#include "fsal.h"
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
			 void* cookie)
{
  struct lg_cbc* cbc=(struct lg_cbc*) cookie;
  nfs_fh4 fhk;
  int rc;
  file_handle_v4_t* fhs;

  fhk.nfs_fh4_val=fhout->nfs_fh4_val;
  
  rc=nfs4_FSALToFhandle(&fhk, fhin, (compound_data_t*) cbc->data);
  fhout->nfs_fh4_len=fhk.nfs_fh4_len;
  fhs=(file_handle_v4_t *) fhout->nfs_fh4_val;
  fhs->ds_flag=1;
  return rc;
}
