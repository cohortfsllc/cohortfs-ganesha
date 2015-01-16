/*
 * vim:noexpandtab:shiftwidth=8:tabstop=8:tw=80:
 *
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

#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <cephfs/libcephfs.h>

#include "fsal_types.h"
#include "fsal_api.h"
#include "pnfs_utils.h"
#include "export_mgr.h"
#include "internal.h"
#include "placement.h"

struct ceph_file_layout {
	/* file -> object mapping */
	uint32_t fl_stripe_unit;   /* stripe unit, in bytes.  must be multiple
				      of page size. */
	uint32_t fl_stripe_count;   /* over this many objects */
	uint32_t fl_object_size;   /* until objects are this big, then move to
				      new objects */
	uint8_t fl_uuid[16];
};
#ifdef COHORT_PNFS

/**
 * @file   FSAL_COHORT/mds.c
 * @author Daniel Gryniewicz <dang@cohortfs.com>
 * @author Adam C. Emerson <aemerson@linuxbox.com>
 * @author Marcus Watts <mdw@cohortfs.com>
 *
 * @brief pNFS Metadata Server Operations for the Cohort FSAL
 *
 * This file implements the layoutget, layoutreturn, layoutcommit,
 * getdeviceinfo, and getdevicelist operations and export query
 * support for the Cohort FSAL.
 */

static uint32_t _get_local_address(void)
{
	uint32_t addr;
	struct ifaddrs *ifaddr, *ifa;
	if (getifaddrs(&ifaddr) == -1)
		return 0;

	/* Walk through linked list, maintaining head pointer so we can free
	 * list later */

	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL)
			continue;

		if (ifa->ifa_addr->sa_family != AF_INET)
			continue;

		if (strncmp(ifa->ifa_name, "eth0", 4))
			continue;

		addr = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr;
		break;
	}
	freeifaddrs(ifaddr);
	return htonl(addr);
}

/*================================= fsal ops ===============================*/
/**
 * @brief Size of the buffer needed for a ds_addr
 *
 * This one is huge, due to the striping pattern.
 *
 * @param[in] export_pub Public export handle
 *
 * @return Size of the buffer needed for a ds_addr
 */
static
size_t pl_fsal_fs_da_addr_size(struct fsal_module *fsal_hdl)
{
	LogFullDebug(COMPONENT_FSAL, "Ret => ~0UL");
	return ~0UL;
	/*return 0x1400;*/
}

#define PL_MAX_DEVS 1
/**
 * @brief Get the devices in a Cohort Placement
 *
 * At present, we support a files based layout only.
 *
 * @param[in]  fsal_hdl     Module handle
 * @param[out] da_addr_body Stream we write the result to
 * @param[in]  type         Type of layout that gave the device
 * @param[in]  deviceid     The device to look up
 *
 * @return Valid error codes in RFC 5661, p. 365.
 */
static
nfsstat4 pl_fsal_getdeviceinfo(struct fsal_module *fsal_hdl, XDR *da_addr_body,
		       const layouttype4 type,
		       const struct pnfs_deviceid *deviceid)
{
	uint32_t indices[PL_MAX_DEVS];
	fsal_multipath_member_t dss[PL_MAX_DEVS];

	/* Sanity check on type */
	if (type != LAYOUT4_PLACEMENT) {
		LogCrit(COMPONENT_PNFS, "Unsupported layout type: %x", type);
		return NFS4ERR_UNKNOWN_LAYOUTTYPE;
	}

	memset(dss, 0, sizeof(dss));
	/* Currently a placeholder; get from actual Volume placement */
	indices[0] = 0;
	dss[0].proto = 6; /* Means TCP, not IPv6 */
	dss[0].port = 2049;
	dss[0].addr = _get_local_address();
	if (dss[0].addr == 0) {
		LogCrit(COMPONENT_PNFS,
				"Unable to get IP address for OSD %lu.", 0LU);
		return NFS4ERR_SERVERFAULT;
	}

	return FSAL_encode_placement_devices(da_addr_body, 1, indices, 1, dss);

}

/*================================= export ops ===============================*/
/**
 * @brief Get list of available devices
 *
 * We do not support listing devices and just set EOF without doing
 * anything.
 *
 * @param[in]     export_pub Export handle
 * @param[in]     type      Type of layout to get devices for
 * @param[in]     cb        Function taking device ID halves
 * @param[in,out] res       In/out and output arguments of the function
 *
 * @return Valid error codes in RFC 5661, pp. 365-6.
 */

static
nfsstat4 pl_exp_getdevicelist(struct fsal_export *exp_hdl, layouttype4 type,
		       void *opaque, bool(*cb) (void *opaque,
						const uint64_t id),
		       struct fsal_getdevicelist_res *res)
{
	res->eof = true;
	LogFullDebug(COMPONENT_FSAL, "ret => %d", NFS4_OK);
	return NFS4_OK;
}

/**
 * @brief Get layout types supported by export
 *
 * We just return a pointer to the single type and set the count to 1.
 *
 * @param[in]  export_pub Public export handle
 * @param[out] count      Number of layout types in array
 * @param[out] types      Static array of layout types that must not be
 *                        freed or modified and must not be dereferenced
 *                        after export reference is relinquished
 */

static
void pl_exp_layouttypes(struct fsal_export *exp_hdl, int32_t *count,
		    const layouttype4 **types)
{
	static const layouttype4 supported_layout_type = LAYOUT4_PLACEMENT;

	*types = &supported_layout_type;
	*count = 1;
	LogFullDebug(COMPONENT_FSAL, "count = 1");
}

/**
 * @brief Get layout block size for export
 *
 * This function just returns the Cohort default.
 *
 * @param[in] export_pub Public export handle
 *
 * @return 4 MB.
 */

uint32_t pl_exp_layout_blocksize(struct fsal_export *exp_hdl)
{
	LogFullDebug(COMPONENT_FSAL, "ret => 0x1000000");
	return 0x1000000;
}

/**
 * @brief Maximum number of segments we will use
 *
 * Since current clients only support 1, that's what we'll use.
 *
 * @param[in] export_pub Public export handle
 *
 * @return 1
 */
static
uint32_t pl_exp_maximum_segments(struct fsal_export *exp_hdl)
{
	LogFullDebug(COMPONENT_FSAL, "ret => 1");
	return 1;
}

/**
 * @brief Size of the buffer needed for a loc_body
 *
 * Just a handle plus a bit.
 * Note: ~0UL means client's maximum
 *
 * @param[in] export_pub Public export handle
 *
 * @return Size of the buffer needed for a loc_body
 */
static
size_t pl_exp_loc_body_size(struct fsal_export *exp_hdl)
{
	LogFullDebug(COMPONENT_FSAL, "ret => 0x100");
	return 0x100;
}

/*================================= handle ops ===============================*/
/**
 * @brief Grant a layout segment.
 *
 * Grant a layout on a subset of a file requested.  As a special case,
 * lie and grant a whole-file layout if requested, because Linux will
 * ignore it otherwise.
 *
 * @param[in]     obj_pub  Public object handle
 * @param[in]     req_ctx  Request context
 * @param[out]    loc_body An XDR stream to which the FSAL must encode
 *                         the layout specific portion of the granted
 *                         layout segment.
 * @param[in]     arg      Input arguments of the function
 * @param[in,out] res      In/out and output arguments of the function
 *
 * @return Valid error codes in RFC 5661, pp. 366-7.
 */

static
nfsstat4 pl_hdl_layoutget(struct fsal_obj_handle *obj_hdl,
		   struct req_op_context *req_ctx, XDR *loc_body,
		   const struct fsal_layoutget_arg *arg,
		   struct fsal_layoutget_res *res)
{
	struct cohort_handle *myself;
	/* The private 'full' export */
	struct cohort_export *export;
	/* Size of each stripe unit */
	uint32_t stripe_unit = 0;
	/* Utility parameter */
	nfl_util4 util = 0;
	/* The deviceid for this layout */
	struct pnfs_deviceid deviceid = DEVICE_ID_INIT_ZERO(FSAL_ID_COHORT);
	/* Ganesha server ID for DS */
	uint16_t ds_id = 0;
	/* NFS Status */
	nfsstat4 nfs_status = 0;
	/* DS wire handle */
	struct cohort_ds_wire ds_wire;
	/* Descriptor for DS handle */
	struct gsh_buffdesc ds_desc = {.addr = &ds_wire,
		.len = sizeof(struct cohort_ds_wire)
	};
	struct ceph_file_layout ceph_layout;
	int rv;

	myself = container_of(obj_hdl, struct cohort_handle, handle);
	export = container_of(op_ctx->fsal_export, struct cohort_export,
	    export);

	LogDebug(COMPONENT_PNFS, "begin");
	/* We support only LAYOUT4_PLACEMENT layouts */

	if (arg->type != LAYOUT4_PLACEMENT) {
		LogCrit(COMPONENT_PNFS, "Unsupported layout type: %x",
			arg->type);
		return NFS4ERR_UNKNOWN_LAYOUTTYPE;
	}

	rv = ceph_ll_file_layout(export->cmount, myself->i, &ceph_layout);
	if (rv != 0) {
		LogCrit(COMPONENT_PNFS, "Failed to get Cohort layout");
		return NFS4ERR_LAYOUTUNAVAILABLE;
	}

	stripe_unit = ceph_layout.fl_stripe_unit;
	if ((stripe_unit & ~NFL4_UFLG_STRIPE_UNIT_SIZE_MASK) != 0) {
		LogCrit(COMPONENT_PNFS,
		    "Cohort returned stripe width that is disallowed by "
		    "NFS: %" PRIu32 ".", stripe_unit);
		return NFS4ERR_SERVERFAULT;
	}
	util = stripe_unit;

	/* For now, fake the device ID, since we'll have one device.  Once
	 * FSAL_COHORT exists, use inode number in the low quad of the device
	 * ID */
	/*deviceid.devid = handle->wire.vi.ino.val;*/
	deviceid.devid = 1;
	/* For now we fake the DS ID, since we only have one.  Eventually, we'll
	 * need to look it up on the FSAL */
	ds_id = 0;

	/* We return exactly one filehandle, filling in the necessary
	   information for the DS server to speak to the Cohort OSD
	   directly. */
	memcpy(&ds_wire.vi, &myself->vi, sizeof(ds_wire.vi));

	memcpy(&ds_wire.volume, ceph_layout.fl_uuid, sizeof(ds_wire.volume));

	rv = ceph_ll_file_key(export->cmount, myself->i, ds_wire.object_key,
			sizeof(ds_wire.object_key));
	if (rv < 0) {
		LogCrit(COMPONENT_PNFS, "Failed to get Cohort object key");
		return NFS4ERR_LAYOUTUNAVAILABLE;
	}

	LogDebug(COMPONENT_PNFS,
		"encoding fsal_id=%#hhx devid=%#lx util=%#x first_idx=%#x export_id=%#x num_fhs=%#x fh_len=%#Zx key=%s",
			deviceid.fsal_id, deviceid.devid, util, 0,
			ds_id, 1, ds_desc.len, ds_wire.object_key);
	nfs_status = FSAL_encode_file_layout(loc_body, &deviceid, util, 0, 0,
					     ds_id, 1, &ds_desc);
	if (nfs_status != NFS4_OK) {
		LogCrit(COMPONENT_PNFS,
			"Failed to encode nfsv4_1_file_layout.");
		goto relinquish;
	}

	/* We grant only one segment, and we want it back when the file
	   is closed. */

	res->return_on_close = true;
	res->last_segment = true;

	return NFS4_OK;

 relinquish:

	/* If we failed in encoding the lo_content, relinquish what we
	   reserved for it. */

	return nfs_status;
}

/**
 * @brief Potentially return one layout segment
 *
 * Since we don't make any reservations, in this version, or get any
 * pins to release, always succeed
 *
 * @param[in] obj_pub  Public object handle
 * @param[in] req_ctx  Request context
 * @param[in] lrf_body Nothing for us
 * @param[in] arg      Input arguments of the function
 *
 * @return Valid error codes in RFC 5661, p. 367.
 */

static
nfsstat4 pl_hdl_layoutreturn(struct fsal_obj_handle *obj_hdl,
		      struct req_op_context *req_ctx, XDR *lrf_body,
		      const struct fsal_layoutreturn_arg *arg)
{
	LogDebug(COMPONENT_PNFS, "begin");
	LogDebug(COMPONENT_FSAL,
		 "reclaim=%d return_type=%d fsal_seg_data=%p dispose=%d last_segment=%d ncookies=%zu",
		 arg->circumstance, arg->return_type, arg->fsal_seg_data,
		 arg->dispose, arg->last_segment, arg->ncookies);

	/* Sanity check on type */
	if (arg->lo_type != LAYOUT4_PLACEMENT) {
		LogCrit(COMPONENT_PNFS, "Unsupported layout type: %x",
			arg->lo_type);
		return NFS4ERR_UNKNOWN_LAYOUTTYPE;
	}

	/* XXX Handle release of layout from before restart */

	return NFS4_OK;
}

/**
 * @brief Commit a segment of a layout
 *
 * Update the size and time for a file accessed through a layout.
 *
 * @param[in]     obj_pub  Public object handle
 * @param[in]     req_ctx  Request context
 * @param[in]     lou_body An XDR stream containing the layout
 *                         type-specific portion of the LAYOUTCOMMIT
 *                         arguments.
 * @param[in]     arg      Input arguments of the function
 * @param[in,out] res      In/out and output arguments of the function
 *
 * @return Valid error codes in RFC 5661, p. 366.
 */

static
nfsstat4 pl_hdl_layoutcommit(struct fsal_obj_handle *obj_hdl,
		      struct req_op_context *req_ctx, XDR *lou_body,
		      const struct fsal_layoutcommit_arg *arg,
		      struct fsal_layoutcommit_res *res)
{
	/* Attributes used to set new values */
	struct attrlist attrs;
	/* Return status from FSAL calls */
	fsal_status_t fsal_stat;

	LogDebug(COMPONENT_PNFS, "begin");
	/* Sanity check on type */
	if (arg->type != LAYOUT4_PLACEMENT) {
		LogCrit(COMPONENT_PNFS, "Unsupported layout type: %x",
			arg->type);
		return NFS4ERR_UNKNOWN_LAYOUTTYPE;
	}

	memset(&attrs, 0, sizeof(attrs));
	/* Get old attrs for comparison */
	obj_hdl->obj_ops.getattrs(obj_hdl);

	if (arg->new_offset) {
		/* File size changed.  This can only grow the file */
		if (obj_hdl->attributes.filesize < arg->last_write + 1) {
			attrs.filesize = arg->last_write + 1;
			FSAL_SET_MASK(attrs.mask, ATTR_SIZE);
		}
	}

	if (arg->time_changed && (arg->new_time.seconds >
				obj_hdl->attributes.mtime.tv_sec)) {
		attrs.mtime.tv_sec = arg->new_time.seconds;
		attrs.mtime.tv_nsec = 0;
		FSAL_SET_MASK(attrs.mask, ATTR_MTIME);
	}

	fsal_stat = obj_hdl->obj_ops.setattrs(obj_hdl, &attrs);
	if (FSAL_IS_ERROR(fsal_stat))
		return posix2nfs4_error(fsal_stat.minor);

	/* This is likely universal for files. */

	res->commit_done = true;

	return NFS4_OK;
}

/*============================== initialization ==============================*/
void export_ops_pnfs(struct export_ops *ops)
{
	ops->getdevicelist = pl_exp_getdevicelist;
	ops->fs_layouttypes = pl_exp_layouttypes;
	ops->fs_layout_blocksize = pl_exp_layout_blocksize;
	ops->fs_maximum_segments = pl_exp_maximum_segments;
	ops->fs_loc_body_size = pl_exp_loc_body_size;
	LogFullDebug(COMPONENT_FSAL, "Init'd export vector");
}

void handle_ops_pnfs(struct fsal_obj_ops *ops)
{
	ops->layoutget = pl_hdl_layoutget;
	ops->layoutreturn = pl_hdl_layoutreturn;
	ops->layoutcommit = pl_hdl_layoutcommit;
	LogDebug(COMPONENT_FSAL, "Init'd handle vector");
}

void fsal_ops_pnfs(struct fsal_ops *ops)
{
	ops->getdeviceinfo = pl_fsal_getdeviceinfo;
	ops->fs_da_addr_size = pl_fsal_fs_da_addr_size;
	LogDebug(COMPONENT_FSAL, "Init'd fsal vector");
}

#endif /* COHORT_PNFS */
