/*
 * vim:noexpandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright (C) 2014 CohortFS LLC
 * Author: Daniel Gryniewicz <dang@cohortfs.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 * ---------------------------------------
 */

/**
 * @addtogroup FSAL_COHORT
 * @{
 */

#include "config.h"
#include "fsal_types.h"
#include "fsal_api.h"
#include "pnfs_utils.h"
#include "export_mgr.h"
#include "nfs_file_handle.h"

#include "placement.h"

#ifdef COHORT_PNFS
/**
 * @brief Convenience function to encode da_addr_body for Placement Layout
 *
 * This function allows the FSAL to encode an placement_layout_ds_addr4
 * without having to allocate and construct all the components of the
 * structure, including addresses.
 *
 * To encode a completed placement_layout_ds_addr4 structure, call
 * xdr_placement_layout_ds_addr4.
 *
 * @param[out] xdrs      XDR stream
 * @param[in]  deviceid  The deviceid for the layout
 * @param[in]  util      Stripe width and flags for the layout
 * @param[in]  first_idx First stripe index
 * @param[in]  ptrn_ofst Pattern offset
 * @param[in]  export_id Export ID (export on Data Server)
 * @param[in]  num_fhs   Number of file handles in array
 * @param[in]  fhs       Array if buffer descriptors holding opaque DS
 *                       handles
 * @return NFS status codes.
 */
nfsstat4 FSAL_encode_placement_devices(XDR *xdrs,
				 int num_indices,
				 uint32_t *indices,
				 int num_dss,
				 fsal_multipath_member_t *dss)
{
	/* Index for traversing arrays */
	size_t i = 0;
	/* NFS status code */
	nfsstat4 nfs_status = 0;

	LogEvent(COMPONENT_PNFS,
			"num_indices=%d indices[0]=%u num_dss=%d dss[0]=%u",
			num_indices, indices[0], num_dss, dss[0].addr);

	if (!inline_xdr_u_int32_t(xdrs, &num_indices)) {
		LogCrit(COMPONENT_PNFS,
			"Failed to encode length of " "stripe_indices array: %"
			PRIu32 ".", num_indices);
		return NFS4ERR_SERVERFAULT;
	}

	for (i = 0; i < num_indices; i++) {
		LogEvent(COMPONENT_PNFS, "    index %lu", i);
		if (!inline_xdr_u_int32_t(xdrs, &indices[i])) {
			LogCrit(COMPONENT_PNFS,
				"Failed to encode OSD for index %lu.", i);
			return NFS4ERR_SERVERFAULT;
		}
	}

	if (!inline_xdr_u_int32_t(xdrs, &num_dss)) {
		LogCrit(COMPONENT_PNFS,
			"Failed to encode length of "
			"multipath_ds_list array: %u", num_dss);
		return NFS4ERR_SERVERFAULT;
	}

	for (i = 0; i < num_dss; i++) {
		LogEvent(COMPONENT_PNFS, "    dss %lu", i);
		nfs_status = FSAL_encode_v4_multipath(xdrs, 1, &dss[i]);
		if (nfs_status != NFS4_OK)
			return nfs_status;
	}


	return NFS4_OK;
}
#endif /* COHORT_PNFS */
