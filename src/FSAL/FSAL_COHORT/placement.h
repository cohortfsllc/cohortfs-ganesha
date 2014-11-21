/*
 * Copyright © 2014 CohortFS LLC
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
 * -------------
 */

#include "nfsv41.h"

	typedef uint32_t nfl_util4;

/* Encoded in the loh_body field of data type layouthint4:
 *  Nothing, zero bytes */

/*
 * Encoded in the da_addr_body field of
 * data type device_addr4:
 */

	struct placement_layout_ds_addr4 {
		struct {
			u_int nflda_stripe_indices_len;
			uint32_t *nflda_stripe_indices_val;
		} nflda_stripe_indices;
		struct {
			u_int nflda_multipath_ds_list_len;
			netaddr4 *nflda_multipath_ds_val;
		} nflda_multipath_ds_list;
	};
	typedef struct placement_layout_ds_addr4
	    placement_layout_ds_addr4;

/*
 * Encoded in the loc_body field of
 * data type layout_content4:
 */

	struct placement_layout4 {
		deviceid4 nfl_deviceid;
		nfl_util4 nfl_util;
		struct {
			u_int nfl_fh_list_len;
			nfs_fh4 *nfl_fh_list_val;
		} nfl_fh_list;
	};
	typedef struct placement_layout4 placement_layout4;

/*
 * Encoded in the da_addr_body field of
 * data type device_addr4:
 */

	static inline bool xdr_placement_layout_ds_addr4(
					XDR *xdrs,
					placement_layout_ds_addr4 * objp)
	{
		if (!xdr_array
		    (xdrs,
		     (char **)&objp->nflda_stripe_indices.
		     nflda_stripe_indices_val,
		     (u_int *) &objp->nflda_stripe_indices.
		     nflda_stripe_indices_len, ~0, sizeof(uint32_t),
		     (xdrproc_t) xdr_uint32_t))
			return false;
		if (!xdr_array
		    (xdrs,
		     (char **)&objp->nflda_multipath_ds_list.
		     nflda_multipath_ds_val,
		     (u_int *) &objp->nflda_multipath_ds_list.
		     nflda_multipath_ds_list_len, ~0,
		     sizeof(netaddr4), (xdrproc_t) xdr_netaddr4))
			return false;
		return true;
	}

/*
 * Encoded in the loc_body field of
 * data type layout_content4:
 */

	static inline bool xdr_placement_layout4(XDR *xdrs,
						    placement_layout4 * objp)
	{
		if (!xdr_deviceid4(xdrs, objp->nfl_deviceid))
			return false;
		if (!xdr_nfl_util4(xdrs, &objp->nfl_util))
			return false;
		if (!xdr_array
		    (xdrs, (char **)&objp->nfl_fh_list.nfl_fh_list_val,
		     (u_int *) &objp->nfl_fh_list.nfl_fh_list_len, ~0,
		     sizeof(nfs_fh4), (xdrproc_t) xdr_nfs_fh4))
			return false;
		return true;
	}

/*
 * Encoded in the lou_body field of data type layoutupdate4:
 *      Nothing. lou_body is a zero length array of bytes.
 */

/*
 * Encoded in the lrf_body field of
 * data type layoutreturn_file4:
 *      Nothing. lrf_body is a zero length array of bytes.
 */


nfsstat4 FSAL_encode_placement_layout(XDR *xdrs,
				 const struct pnfs_deviceid *deviceid,
				 nfl_util4 util,
				 const unsigned int export_id,
				 const uint32_t num_fhs,
				 const struct gsh_buffdesc *fhs);

nfsstat4 FSAL_encode_placement_devices(XDR *xdrs,
				 int num_indices,
				 uint32_t *indices,
				 int num_dss,
				 fsal_multipath_member_t *dss);

/**
 * The wire content of a DS (data server) handle
 */

struct pl_ds_wire {
	uint32_t foo;	/*< Placeholder */
};
