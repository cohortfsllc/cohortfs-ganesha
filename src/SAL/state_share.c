/*
 * vim:noexpandtab:shiftwidth=8:tabstop=8:
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 * ---------------------------------------
 */

/**
 * @defgroup SAL State abstraction layer
 * @{
 */

/**
 * @file state_share.c
 * @brief Share reservation management
 */

#include "config.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <assert.h>

#include "fsal.h"
#include "nfs_core.h"
#include "nfs4.h"
#include "sal_functions.h"
#include "nlm_util.h"
#include "cache_inode_lru.h"
#include "export_mgr.h"

static void state_share_update_counter(struct state_file *fstate,
				       int old_access, int old_deny,
				       int new_access, int new_deny, bool v4);

static unsigned int state_share_get_share_access(struct state_file *fstate);

static unsigned int state_share_get_share_deny(struct state_file *fstate);

/**
 * @brief Push share state down to FSAL
 *
 * Only the union of share states should be passed to this function.
 *
 * @param[in] obj   File to access
 * @param[in] owner Open owner
 * @param[in] share Share description
 *
 * @return State status.
 */
static state_status_t do_share_op(struct fsal_obj_handle *obj,
				  state_owner_t *owner,
				  fsal_share_param_t *share)
{
	fsal_status_t fsal_status;
	state_status_t status = STATE_SUCCESS;

	/* Quick exit if share reservation is not supported by FSAL */
	if (!op_ctx->fsal_export->exp_ops.
	    fs_supports(op_ctx->fsal_export, fso_share_support))
		return STATE_SUCCESS;

	fsal_status = obj->obj_ops.share_op(obj, NULL, *share);

	status = state_error_convert(fsal_status);

	LogFullDebug(COMPONENT_STATE, "FSAL_share_op returned %s",
		     state_err_str(status));

	return status;
}

/**
 * @brief Add new share state
 *
 * The state lock _must_ be held for this call.
 *
 * @param[in,out] obj   File on which to operate
 * @param[in]     owner Open owner
 * @param[in]     state State that holds the share bits to be added
 *
 * @return State status.
 */
state_status_t state_share_add(struct fsal_obj_handle *obj,
			       state_owner_t *owner,
			       state_t *state, bool reclaim)
{
	state_status_t status = STATE_SUCCESS;
	unsigned int old_entry_share_access = 0;
	unsigned int old_entry_share_deny = 0;
	unsigned int new_entry_share_access = 0;
	unsigned int new_entry_share_deny = 0;
	unsigned int new_share_access = 0;
	unsigned int new_share_deny = 0;
	fsal_share_param_t share_param;
	struct state_file *fstate;

	fstate = obj->obj_ops.get_file_state(obj);
	if (!fstate) {
		status = STATE_SERVERFAULT;
		LogFullDebug(COMPONENT_STATE, "Could not get file state");
		return status;
	}

	/* Get the current union of share states of this file. */
	old_entry_share_access = state_share_get_share_access(fstate);
	old_entry_share_deny = state_share_get_share_deny(fstate);

	/* Share state to be added. */
	new_share_access = state->state_data.share.share_access;
	new_share_deny = state->state_data.share.share_deny;

	/* Update the ref counted share state of this file. */
	state_share_update_counter(fstate, OPEN4_SHARE_ACCESS_NONE,
				   OPEN4_SHARE_DENY_NONE, new_share_access,
				   new_share_deny, true);

	/* Get the updated union of share states of this file. */
	new_entry_share_access = state_share_get_share_access(fstate);
	new_entry_share_deny = state_share_get_share_deny(fstate);

	/* If this file's share bits are different from the supposed value,
	 * update it.
	 */
	if ((new_entry_share_access != old_entry_share_access)
	    || (new_entry_share_deny != old_entry_share_deny)) {
		/* Try to push to FSAL. */
		share_param.share_access = new_entry_share_access;
		share_param.share_deny = new_entry_share_deny;
		share_param.share_reclaim = reclaim;

		status = do_share_op(obj, owner, &share_param);

		if (status != STATE_SUCCESS) {
			/* Revert the ref counted share state of this file. */
			state_share_update_counter(fstate, new_share_access,
						   new_share_deny,
						   OPEN4_SHARE_ACCESS_NONE,
						   OPEN4_SHARE_DENY_NONE, true);
			LogDebug(COMPONENT_STATE, "do_share_op failed");
			return status;
		}
	}

	if (isFullDebug(COMPONENT_NFS_V4_LOCK)) {
		char str[LOG_BUFF_LEN];
		struct display_buffer dspbuf = {sizeof(str), str, str};

		display_stateid(&dspbuf, state);

		LogFullDebug(COMPONENT_STATE,
			     "%s: added share_access %u, share_deny %u",
			     str, new_share_access, new_share_deny);
	}

	/* Update previously seen share state in the bitmap. */
	state_share_set_prev(state, &(state->state_data));

	return status;
}

/**
 * Remove a share state
 *
 * The state lock _must_ be held for this call.
 *
 * @param[in,out] obj   File to modify
 * @param[in]     owner Open owner
 * @param[in]     state State that holds the share bits to be removed
 *
 * @return State status.
 */
state_status_t state_share_remove(struct fsal_obj_handle *obj,
				  state_owner_t *owner,
				  state_t *state)
{
	state_status_t status = STATE_SUCCESS;
	unsigned int old_entry_share_access = 0;
	unsigned int old_entry_share_deny = 0;
	unsigned int new_entry_share_access = 0;
	unsigned int new_entry_share_deny = 0;
	unsigned int removed_share_access = 0;
	unsigned int removed_share_deny = 0;
	fsal_share_param_t share_param;
	struct state_file *fstate;

	fstate = obj->obj_ops.get_file_state(obj);
	if (!fstate) {
		status = STATE_SERVERFAULT;
		LogFullDebug(COMPONENT_STATE, "Could not get file state");
		return status;
	}


	/* Get the current union of share states of this file. */
	old_entry_share_access = state_share_get_share_access(fstate);
	old_entry_share_deny = state_share_get_share_deny(fstate);

	/* Share state to be removed. */
	removed_share_access = state->state_data.share.share_access;
	removed_share_deny = state->state_data.share.share_deny;

	/* Update the ref counted share state of this file. */
	state_share_update_counter(fstate, removed_share_access,
				   removed_share_deny, OPEN4_SHARE_ACCESS_NONE,
				   OPEN4_SHARE_DENY_NONE, true);

	/* Get the updated union of share states of this file. */
	new_entry_share_access = state_share_get_share_access(fstate);
	new_entry_share_deny = state_share_get_share_deny(fstate);

	/* If this file's share bits are different from the supposed value,
	 * update it.
	 */
	if ((new_entry_share_access != old_entry_share_access)
	    || (new_entry_share_deny != old_entry_share_deny)) {
		/* Try to push to FSAL. */
		share_param.share_access = new_entry_share_access;
		share_param.share_deny = new_entry_share_deny;
		share_param.share_reclaim = false;

		status = do_share_op(obj, owner, &share_param);

		if (status != STATE_SUCCESS) {
			/* Revert the ref counted share state of this file. */
			state_share_update_counter(fstate,
						   OPEN4_SHARE_ACCESS_NONE,
						   OPEN4_SHARE_DENY_NONE,
						   removed_share_access,
						   removed_share_deny, true);
			LogDebug(COMPONENT_STATE, "do_share_op failed");
			return status;
		}
	}

	if (isFullDebug(COMPONENT_NFS_V4_LOCK)) {
		char str[LOG_BUFF_LEN];
		struct display_buffer dspbuf = {sizeof(str), str, str};

		display_stateid(&dspbuf, state);

		LogFullDebug(COMPONENT_STATE,
			     "%s: removed share_access %u, share_deny %u",
			     str, removed_share_access, removed_share_deny);
	}

	return status;
}

/**
 * @brief Upgrade share modes
 *
 * The state lock _must_ be held for this call.
 *
 * @param[in,out] obj        File to modify
 * @param[in]     state_data New share bits
 * @param[in]     owner      Open owner
 * @param[in,out] state      State that holds current share bits
 *
 * @return State status.
 */
state_status_t state_share_upgrade(struct fsal_obj_handle *obj,
				   union state_data *state_data,
				   state_owner_t *owner, state_t *state,
				   bool reclaim)
{
	state_status_t status = STATE_SUCCESS;
	unsigned int old_entry_share_access = 0;
	unsigned int old_entry_share_deny = 0;
	unsigned int new_entry_share_access = 0;
	unsigned int new_entry_share_deny = 0;
	unsigned int old_share_access = 0;
	unsigned int old_share_deny = 0;
	unsigned int new_share_access = 0;
	unsigned int new_share_deny = 0;
	fsal_share_param_t share_param;
	struct state_file *fstate;

	fstate = obj->obj_ops.get_file_state(obj);
	if (!fstate) {
		status = STATE_SERVERFAULT;
		LogFullDebug(COMPONENT_STATE, "Could not get file state");
		return status;
	}


	/* Get the current union of share states of this file. */
	old_entry_share_access = state_share_get_share_access(fstate);
	old_entry_share_deny = state_share_get_share_deny(fstate);

	/* Old share state. */
	old_share_access = state->state_data.share.share_access;
	old_share_deny = state->state_data.share.share_deny;

	/* New share state. */
	new_share_access = state_data->share.share_access | old_share_access;
	new_share_deny = state_data->share.share_deny | old_share_deny;

	/* Update the ref counted share state of this file. */
	state_share_update_counter(fstate, old_share_access, old_share_deny,
				   new_share_access, new_share_deny, true);

	/* Get the updated union of share states of this file. */
	new_entry_share_access = state_share_get_share_access(fstate);
	new_entry_share_deny = state_share_get_share_deny(fstate);

	/* If this file's share bits are different from the supposed value,
	 * update it.
	 */
	if ((new_entry_share_access != old_entry_share_access)
	    || (new_entry_share_deny != old_entry_share_deny)) {
		/* Try to push to FSAL. */
		share_param.share_access = new_entry_share_access;
		share_param.share_deny = new_entry_share_deny;
		share_param.share_reclaim = reclaim;

		status = do_share_op(obj, owner, &share_param);

		if (status != STATE_SUCCESS) {
			/* Revert the ref counted share state of this file. */
			state_share_update_counter(fstate, new_share_access,
						   new_share_deny,
						   old_share_access,
						   old_share_deny, true);
			LogDebug(COMPONENT_STATE, "do_share_op failed");
			return status;
		}
	}

	/* Update share state. */
	state->state_data.share.share_access = new_share_access;
	state->state_data.share.share_deny = new_share_deny;

	if (isFullDebug(COMPONENT_NFS_V4_LOCK)) {
		char str[LOG_BUFF_LEN];
		struct display_buffer dspbuf = {sizeof(str), str, str};

		display_stateid(&dspbuf, state);

		LogFullDebug(COMPONENT_STATE,
			     "%s: upgraded share_access %u, share_deny %u",
			     str,
			     state->state_data.share.share_access,
			     state->state_data.share.share_deny);
	}

	/* Update previously seen share state. */
	state_share_set_prev(state, state_data);

	return status;
}

/**
 * @brief Downgrade share mode
 *
 * The state lock _must_ be held for this call.
 *
 * @param[in,out] obj        File to modify
 * @param[in]     state_data New share bits
 * @param[in]     owner      Open owner
 * @param[in]     state      State that holds current share bits
 *
 * @return State status.
 */
state_status_t state_share_downgrade(struct fsal_obj_handle *obj,
				     union state_data *state_data,
				     state_owner_t *owner, state_t *state)
{
	state_status_t status = STATE_SUCCESS;
	unsigned int old_entry_share_access = 0;
	unsigned int old_entry_share_deny = 0;
	unsigned int new_entry_share_access = 0;
	unsigned int new_entry_share_deny = 0;
	unsigned int old_share_access = 0;
	unsigned int old_share_deny = 0;
	unsigned int new_share_access = 0;
	unsigned int new_share_deny = 0;
	fsal_share_param_t share_param;
	struct state_file *fstate;

	fstate = obj->obj_ops.get_file_state(obj);
	if (!fstate) {
		status = STATE_SERVERFAULT;
		LogFullDebug(COMPONENT_STATE, "Could not get file state");
		return status;
	}


	/* Get the current union of share states of this file. */
	old_entry_share_access = state_share_get_share_access(fstate);
	old_entry_share_deny = state_share_get_share_deny(fstate);

	/* Old share state. */
	old_share_access = state->state_data.share.share_access;
	old_share_deny = state->state_data.share.share_deny;

	/* New share state. */
	new_share_access = state_data->share.share_access;
	new_share_deny = state_data->share.share_deny;

	/* Update the ref counted share state of this file. */
	state_share_update_counter(fstate, old_share_access, old_share_deny,
				   new_share_access, new_share_deny, true);

	/* Get the updated union of share states of this file. */
	new_entry_share_access = state_share_get_share_access(fstate);
	new_entry_share_deny = state_share_get_share_deny(fstate);

	/* If this file's share bits are different from the supposed value,
	 * update it.
	 */
	if ((new_entry_share_access != old_entry_share_access)
	    || (new_entry_share_deny != old_entry_share_deny)) {
		/* Try to push to FSAL. */
		share_param.share_access = new_entry_share_access;
		share_param.share_deny = new_entry_share_deny;
		share_param.share_reclaim = false;

		status = do_share_op(obj, owner, &share_param);

		if (status != STATE_SUCCESS) {
			/* Revert the ref counted share state of this file. */
			state_share_update_counter(fstate, new_share_access,
						   new_share_deny,
						   old_share_access,
						   old_share_deny, true);
			LogDebug(COMPONENT_STATE, "do_share_op failed");
			return status;
		}
	}

	/* Update share state. */
	state->state_data.share.share_access = new_share_access;
	state->state_data.share.share_deny = new_share_deny;

	if (isFullDebug(COMPONENT_NFS_V4_LOCK)) {
		char str[LOG_BUFF_LEN];
		struct display_buffer dspbuf = {sizeof(str), str, str};

		display_stateid(&dspbuf, state);

		LogFullDebug(COMPONENT_STATE,
			     "%s: downgraded share_access %u, share_deny %u",
			     str,
			     state->state_data.share.share_access,
			     state->state_data.share.share_deny);
	}

	return status;
}

/**
 * @brief Update the previously access and deny modes
 *
 * @param[in] state      State to update
 * @param[in] state_data Previous modes to add
 */
state_status_t state_share_set_prev(state_t *state,
				    union state_data *state_data)
{
	state_status_t status = STATE_SUCCESS;

	state->state_data.share.share_access_prev |=
	    (1 << state_data->share.share_access);

	state->state_data.share.share_deny_prev |=
	    (1 << state_data->share.share_deny);

	return status;
}

/**
 * @brief Check if the state has seen the share modes before
 *
 * This is needed when we check validity of open downgrade.
 *
 * @param[in] state      State to check
 * @param[in] state_data Alleged previous mode
 */
state_status_t state_share_check_prev(state_t *state,
				      union state_data *state_data)
{
	state_status_t status = STATE_SUCCESS;

	if ((state->state_data.share.
	     share_access_prev & (1 << state_data->share.share_access)) == 0)
		return STATE_STATE_ERROR;

	if ((state->state_data.share.
	     share_deny_prev & (1 << state_data->share.share_deny)) == 0)
		return STATE_STATE_ERROR;

	return status;
}

/**
 * @brief Check for share conflict
 *
 * The state lock _must_ be held for this call.
 *
 * @param[in] fstate       File state to query
 * @param[in] share_access Desired access mode
 * @param[in] share_deny   Desired deny mode
 * @param[in] bypass       Indicates if any bypass is to be used
 *
 * @return State status.
 */
state_status_t state_share_check_conflict(struct state_file *fstate,
					  int share_access,
					  int share_deny,
					  enum share_bypass_modes bypass)
{
	char *cause = "";

	if ((share_access & OPEN4_SHARE_ACCESS_READ) != 0
	    && fstate->share_state.share_deny_read > 0
	    && bypass != SHARE_BYPASS_READ) {
		cause = "access read denied by existing deny read";
		goto out_conflict;
	}

	if ((share_access & OPEN4_SHARE_ACCESS_WRITE) != 0
	    && (fstate->share_state.share_deny_write_v4 > 0 ||
		(bypass != SHARE_BYPASS_V3_WRITE &&
		 fstate->share_state.share_deny_write > 0))) {
		cause = "access write denied by existing deny write";
		goto out_conflict;
	}

	if ((share_deny & OPEN4_SHARE_DENY_READ) != 0
	    && fstate->share_state.share_access_read > 0) {
		cause = "deny read denied by existing access read";
		goto out_conflict;
	}

	if ((share_deny & OPEN4_SHARE_DENY_WRITE) != 0
	    && fstate->share_state.share_access_write > 0) {
		cause = "deny write denied by existing access write";
		goto out_conflict;
	}

	return STATE_SUCCESS;

 out_conflict:

	LogDebug(COMPONENT_STATE, "Share conflict detected: %s", cause);
	return STATE_STATE_CONFLICT;
}

/**
 * @brief Update the ref counter of share state
 *
 * This function should be called with the state lock held
 *
 * @param[in] fstate     File state to update
 * @param[in] old_access Previous access mode
 * @param[in] old_deny   Previous deny mode
 * @param[in] new_access Current access mode
 * @param[in] new_deny   Current deny mode
 * @param[in] v4         True if this is a v4 share/open
 */
static void state_share_update_counter(struct state_file *fstate, int
				       old_access, int old_deny, int new_access,
				       int new_deny, bool v4)
{
	int access_read_inc =
	    ((new_access & OPEN4_SHARE_ACCESS_READ) !=
	     0) - ((old_access & OPEN4_SHARE_ACCESS_READ) != 0);
	int access_write_inc =
	    ((new_access & OPEN4_SHARE_ACCESS_WRITE) !=
	     0) - ((old_access & OPEN4_SHARE_ACCESS_WRITE) != 0);
	int deny_read_inc =
	    ((new_deny & OPEN4_SHARE_DENY_READ) !=
	     0) - ((old_deny & OPEN4_SHARE_DENY_READ) != 0);
	int deny_write_inc =
	    ((new_deny & OPEN4_SHARE_DENY_WRITE) !=
	     0) - ((old_deny & OPEN4_SHARE_DENY_WRITE) != 0);

	fstate->share_state.share_access_read += access_read_inc;
	fstate->share_state.share_access_write += access_write_inc;
	fstate->share_state.share_deny_read += deny_read_inc;
	fstate->share_state.share_deny_write += deny_write_inc;
	if (v4)
		fstate->share_state.share_deny_write_v4 += deny_write_inc;

	LogFullDebug(COMPONENT_STATE,
		     "obj %p: share counter: access_read %u, access_write %u, deny_read %u, deny_write %u, deny_write_v4 %u",
		     fstate->obj,
		     fstate->share_state.share_access_read,
		     fstate->share_state.share_access_write,
		     fstate->share_state.share_deny_read,
		     fstate->share_state.share_deny_write,
		     fstate->share_state.share_deny_write_v4);
}

/**
 * @brief Calculate the union of share access of given file
 *
 * @param[in] fstate File state to check
 *
 * @return Calculated access.
 */
static unsigned int state_share_get_share_access(struct state_file *fstate)
{
	unsigned int share_access = 0;

	if (fstate->share_state.share_access_read > 0)
		share_access |= OPEN4_SHARE_ACCESS_READ;

	if (fstate->share_state.share_access_write > 0)
		share_access |= OPEN4_SHARE_ACCESS_WRITE;

	LogFullDebug(COMPONENT_STATE, "obj %p: union share access = %u",
		     fstate->obj, share_access);

	return share_access;
}

/**
 * @brief Calculate the union of share deny of given file
 *
 * @param[in] fstate File state to check
 *
 * @return Deny mode union.
 */
static unsigned int state_share_get_share_deny(struct state_file *fstate)
{
	unsigned int share_deny = 0;

	if (fstate->share_state.share_deny_read > 0)
		share_deny |= OPEN4_SHARE_DENY_READ;

	if (fstate->share_state.share_deny_write > 0)
		share_deny |= OPEN4_SHARE_DENY_WRITE;

	LogFullDebug(COMPONENT_STATE, "obj %p: union share deny = %u",
		     fstate->obj, share_deny);

	return share_deny;
}

/**
 * @brief Start I/O by an anonymous stateid
 *
 * This function checks for conflicts with existing deny modes and
 * marks the I/O as in process to conflicting shares won't be granted.
 *
 * @brief[in]     obj          File on which to operate
 * @brief[in]     share_access Access matching I/O done
 * @brief[in]     bypass       Indicates if any bypass is to be used
 *
 * @return State status.
 */
state_status_t state_share_anonymous_io_start(struct fsal_obj_handle *obj,
					      int share_access,
					      enum share_bypass_modes bypass)
{
	/** @todo FSF: This is currently unused, but I think there is
	 *             some additional work to make the conflict check
	 *             work for v3 and v4, and in fact, this function
	 *             should be called indicating v3 or v4...
	 */
	state_status_t status = 0;
	struct state_file *fstate;

	fstate = obj->obj_ops.get_file_state(obj);
	if (!fstate) {
		status = STATE_SERVERFAULT;
		LogFullDebug(COMPONENT_STATE, "Could not get file state");
		return status;
	}


	status = state_share_check_conflict(fstate,
					    share_access,
					    OPEN4_SHARE_DENY_NONE,
					    bypass);
	if (status != STATE_SUCCESS) {
		/* Need to convert the error from STATE_SHARE_CONFLICT */
		status = STATE_LOCKED;
		return status;
	}

	if (state_deleg_conflict(obj,
				 share_access & OPEN4_SHARE_ACCESS_WRITE)) {
		/* Delegations are being recalled. Delay client until that
		 * process finishes. */
		return STATE_FSAL_DELAY;
	}

	/* update a counter that says we are processing an anonymous
	 * request and can't currently grant a new delegation */
	atomic_inc_uint32_t(&fstate->anon_ops);

	/* Temporarily bump the access counters, v4 mode doesn't matter
	 * since there is no deny mode associated with anonymous I/O.
	 */
	state_share_update_counter(fstate, OPEN4_SHARE_ACCESS_NONE,
				   OPEN4_SHARE_DENY_NONE, share_access,
				   OPEN4_SHARE_DENY_NONE, false);

	return status;
}

/**
 * @brief Finish an anonymous I/O
 *
 * @param[in,out] obj          File on which to operate
 * @param[in]     share_access Access bits indicating I/O type
 */
void state_share_anonymous_io_done(struct fsal_obj_handle *obj, int share_access)
{
	struct state_file *fstate;

	fstate = obj->obj_ops.get_file_state(obj);
	if (!fstate) {
		return;
	}
	/* Undo the temporary bump to the access counters, v4 mode doesn't
	 * matter since there is no deny mode associated with anonymous I/O.
	 */
	state_share_update_counter(fstate, share_access, OPEN4_SHARE_DENY_NONE,
				   OPEN4_SHARE_ACCESS_NONE,
				   OPEN4_SHARE_DENY_NONE, false);

	/* If we are this far, then delegations weren't recalled and we
	 * incremented this variable. */
	atomic_dec_uint32_t(&fstate->anon_ops);
}

#ifdef _USE_NLM
/**
 * @brief Remove an NLM share
 *
 * @param[in]     state	The state_t describing the share to remove
 *
 */
void remove_nlm_share(state_t *state)
{
	state_owner_t *owner = state->state_owner;
	state_nlm_client_t *client = owner->so_owner.so_nlm_owner.so_client;

	/* Remove from share list for export */
	PTHREAD_RWLOCK_wrlock(&op_ctx->export->lock);
	glist_del(&state->state_export_list);
	PTHREAD_RWLOCK_unlock(&op_ctx->export->lock);

	/* Remove the share from the list for the file. */
	glist_del(&state->state_list);

	/* Remove the share from the NSM Client list */
	PTHREAD_MUTEX_lock(&client->slc_nsm_client->ssc_mutex);

	glist_del(&state->state_data.nlm_share.share_perclient);

	PTHREAD_MUTEX_unlock(&client->slc_nsm_client->ssc_mutex);

	dec_nsm_client_ref(client->slc_nsm_client);

	/* Remove the share from the NLM Owner list */
	PTHREAD_MUTEX_lock(&owner->so_mutex);

	glist_del(&state->state_owner_list);

	PTHREAD_MUTEX_unlock(&owner->so_mutex);

	/* Release the state_t reference for active share. */
	dec_state_t_ref(state);
}

/**
 * @brief Implement NLM share call
 *
 * @param[in,out] obj          File on which to operate
 * @param[in]     export       Export through which file is accessed
 * @param[in]     share_access Share mode requested
 * @param[in]     share_deny   Deny mode requested
 * @param[in]     owner        Share owner
 * @param[in]     state        state_t to manage the share
 * @param[in]     reclaim      Indicates if this is a reclaim
 *
 * @return State status.
 */
state_status_t state_nlm_share(struct fsal_obj_handle *obj,
			       int share_access,
			       int share_deny,
			       state_owner_t *owner,
			       state_t *state,
			       bool reclaim)
{
	unsigned int old_entry_share_access;
	unsigned int old_entry_share_deny;
	unsigned int new_entry_share_access;
	unsigned int new_entry_share_deny;
	unsigned int old_share_access;
	unsigned int old_share_deny;
	fsal_share_param_t share_param;
	fsal_openflags_t openflags;
	state_status_t status = 0;
	struct fsal_export *fsal_export = op_ctx->fsal_export;
	struct state_file *fstate;
	state_nlm_client_t *client = owner->so_owner.so_nlm_owner.so_client;

	if (share_access == OPEN4_SHARE_ACCESS_NONE) {
		/* An update to no access is considered the same as
		 * an unshare.
		 */
		return state_nlm_unshare(obj,
					 OPEN4_SHARE_ACCESS_BOTH,
					 OPEN4_SHARE_DENY_BOTH,
					 owner,
					 state);
	}

	fstate = obj->obj_ops.get_file_state(obj);
	if (!fstate) {
		status = STATE_SERVERFAULT;
		LogFullDebug(COMPONENT_STATE, "Could not get file state");
		return status;
	}

	/* If FSAL supports reopen method, we open read-only if the access
	 * needs read only. If not, a later request may need read-write
	 * open that needs closing and then opening the file again. The
	 * act of closing the file may remove shared lock state, so we
	 * open read-write now itself for all access needs.
	 */
	if (share_access == fsa_R &&
	    fsal_export->exp_ops.fs_supports(fsal_export, fso_reopen_method))
		openflags = FSAL_O_READ;
	else
		openflags = FSAL_O_RDWR;

	if (reclaim)
		openflags |= FSAL_O_RECLAIM;

	/* Check if new share state has conflicts. */
	status = state_share_check_conflict(fstate,
					    share_access,
					    share_deny,
					    SHARE_BYPASS_NONE);

	if (status != STATE_SUCCESS) {
		LogEvent(COMPONENT_STATE,
			 "Share conflicts detected during add");
		goto out_unlock;
	}

	/* Add share to list for NLM Owner */
	PTHREAD_MUTEX_lock(&owner->so_mutex);

	glist_add_tail(&owner->so_owner.so_nlm_owner.so_nlm_shares,
		       &state->state_owner_list);

	PTHREAD_MUTEX_unlock(&owner->so_mutex);

	/* Add share to list for NSM Client */
	inc_nsm_client_ref(client->slc_nsm_client);

	PTHREAD_MUTEX_lock(&client->slc_nsm_client->ssc_mutex);

	glist_add_tail(&client->slc_nsm_client->ssc_share_list,
		       &state->state_data.nlm_share.share_perclient);

	PTHREAD_MUTEX_unlock(&client->slc_nsm_client->ssc_mutex);

	/* Add share to list for file. */
	glist_add_tail(&fstate->nlm_share_list, &state->state_list);

	/* Add to share list for export */
	PTHREAD_RWLOCK_wrlock(&op_ctx->export->lock);
	glist_add_tail(&op_ctx->export->exp_nlm_share_list,
		       &state->state_export_list);
	PTHREAD_RWLOCK_unlock(&op_ctx->export->lock);

	/* Get the current union of share states of this file. */
	old_entry_share_access = state_share_get_share_access(fstate);
	old_entry_share_deny = state_share_get_share_deny(fstate);

	/* Get the old access/deny (it may be none if this is a new
	 * share reservation rather than an update).
	 */
	old_share_access = state->state_data.nlm_share.share_access;
	old_share_deny = state->state_data.nlm_share.share_deny;

	/* If we had never had a share, take a reference on the state_t
	 * to retain it.
	 */
	if (old_share_access != OPEN4_SHARE_ACCESS_NONE)
		inc_state_t_ref(state);

	/* Update the ref counted share state of this file. */
	state_share_update_counter(fstate,
				   old_share_access,
				   old_share_deny,
				   share_access,
				   share_deny,
				   true);

	/* Get the updated union of share states of this file. */
	new_entry_share_access = state_share_get_share_access(fstate);
	new_entry_share_deny = state_share_get_share_deny(fstate);

	/* If this file's share bits are different from the supposed value,
	 * update it.
	 */
	if ((new_entry_share_access != old_entry_share_access)
	    || (new_entry_share_deny != old_entry_share_deny)) {
		/* Try to push to FSAL. */
		share_param.share_access = new_entry_share_access;
		share_param.share_deny = new_entry_share_deny;
		share_param.share_reclaim = reclaim;

		status = do_share_op(obj, owner, &share_param);

		if (status != STATE_SUCCESS) {
			/* Revert the ref counted share state of this file. */
			state_share_update_counter(fstate,
						   share_access,
						   share_deny,
						   old_share_access,
						   old_share_deny,
						   true);

			remove_nlm_share(state);

			LogDebug(COMPONENT_STATE, "do_share_op failed");

			goto out_unlock;
		}
	}

	/* Update the current share type */
	state->state_data.nlm_share.share_access = share_access;
	state->state_data.nlm_share.share_deny = share_deny;

	LogFullDebug(COMPONENT_STATE, "added share_access %u, share_deny %u",
		     share_access, share_deny);

 out_unlock:

	return status;
}

/**
 * @brief Implement NLM unshare procedure
 *
 * @param[in,out] obj          File on which to operate
 * @param[in]     share_access Access mode to relinquish
 * @param[in]     share_deny   Deny mode to relinquish
 * @param[in]     owner        Share owner
 * @param[in]     state        The state object associated with this owner
 *
 * @return State status.
 */
state_status_t state_nlm_unshare(struct fsal_obj_handle *obj,
				 int share_access,
				 int share_deny,
				 state_owner_t *owner,
				 state_t *state)
{
	unsigned int old_entry_share_access;
	unsigned int old_entry_share_deny;
	unsigned int new_entry_share_access;
	unsigned int new_entry_share_deny;
	unsigned int old_share_access;
	unsigned int old_share_deny;
	unsigned int new_share_access;
	unsigned int new_share_deny;
	fsal_share_param_t share_param;
	state_status_t status = 0;
	struct state_file *fstate;

	fstate = obj->obj_ops.get_file_state(obj);
	if (!fstate) {
		status = STATE_SERVERFAULT;
		LogFullDebug(COMPONENT_STATE, "Could not get file state");
		return status;
	}

	/* Get the current union of share states of this file. */
	old_entry_share_access = state_share_get_share_access(fstate);
	old_entry_share_deny = state_share_get_share_deny(fstate);

	/* Old share state. */
	old_share_access = state->state_data.nlm_share.share_access;
	old_share_deny = state->state_data.nlm_share.share_deny;

	/* The removal might not remove everything. */
	new_share_access = old_share_access - (old_share_access & share_access);
	new_share_deny = old_share_deny - (old_share_deny & share_deny);

	/* Update the ref counted share state of this file. */
	state_share_update_counter(obj,
				   old_share_access,
				   old_share_deny,
				   new_share_access,
				   new_share_deny,
				   true);

	/* Get the updated union of share states of this file. */
	new_entry_share_access = state_share_get_share_access(fstate);
	new_entry_share_deny = state_share_get_share_deny(fstate);

	/* If this file's share bits are different from the supposed
	 * value, update it.
	 */
	if ((new_entry_share_access != old_entry_share_access)
	    || (new_entry_share_deny != old_entry_share_deny)) {
		/* Try to push to FSAL. */
		share_param.share_access = new_entry_share_access;
		share_param.share_deny = new_entry_share_deny;
		share_param.share_reclaim = false;

		status = do_share_op(obj, owner, &share_param);

		if (status != STATE_SUCCESS) {
			/* Revert the ref counted share state
			 * of this file.
			 */
			state_share_update_counter(entry,
						   new_share_access,
						   new_share_deny,
						   old_share_access,
						   old_share_deny,
						   true);

			LogDebug(COMPONENT_STATE, "do_share_op failed");
			goto out;
		}
	}

	LogFullDebug(COMPONENT_STATE,
		     "removed share_access %u, share_deny %u",
		     share_access, share_deny);

	if (new_share_access == OPEN4_SHARE_ACCESS_NONE &&
	    new_share_deny == OPEN4_SHARE_DENY_NONE) {
		/* The share is completely removed. */
		remove_nlm_share(state);
	}

 out:

	return status;
}

/**
 * @brief Remove all share state from a file
 *
 * @param[in] obj File to wipe
 */
void state_share_wipe(struct state_file *fstate)
{
	state_t *state;
	struct glist_head *glist;
	struct glist_head *glistn;

	glist_for_each_safe(glist, glistn, &fstate->nlm_share_list) {
		state = glist_entry(glist, state_t, state_list);

		remove_nlm_share(state, NULL);
	}
}

void state_export_unshare_all(void)
{
	int errcnt = 0;
	state_t *state;
	state_owner_t *owner;
	struct fsal_obj_handle *obj;
	state_status_t status;

	while (errcnt < STATE_ERR_MAX) {
		PTHREAD_RWLOCK_wrlock(&op_ctx->export->lock);

		state = glist_first_entry(&op_ctx->export->exp_nlm_share_list,
					  state_t,
					  state_export_list);

		if (state == NULL) {
			PTHREAD_RWLOCK_unlock(&op_ctx->export->lock);
			break;
		}

		obj = state->state_obj;
		owner = state->state_owner;

		/* Get a reference to the state_t */
		inc_state_t_ref(state);

		/* get a reference to the owner */
		inc_state_owner_ref(owner);

		/* Drop the export mutex to call unshare */
		PTHREAD_RWLOCK_unlock(&op_ctx->export->lock);

		/* Remove all shares held by this Owner on this export */
		status = state_nlm_unshare(obj,
					   OPEN4_SHARE_ACCESS_BOTH,
					   OPEN4_SHARE_DENY_BOTH,
					   owner,
					   state);

		/* Release references taken above. Should free the state_t. */
		dec_state_owner_ref(owner);
		dec_state_t_ref(state);

		if (!state_unlock_err_ok(status)) {
			/* Increment the error count and try the next share,
			 * with any luck the memory pressure which is causing
			 * the problem will resolve itself.
			 */
			LogCrit(COMPONENT_STATE,
				"state_unlock failed %s",
				state_err_str(status));
			errcnt++;
		}
	}

	if (errcnt == STATE_ERR_MAX) {
		LogFatal(COMPONENT_STATE,
			 "Could not complete cleanup of NLM shares for %s",
			 op_ctx->export->fullpath);
	}
}
#endif /* _USE_NLM */

/** @} */
