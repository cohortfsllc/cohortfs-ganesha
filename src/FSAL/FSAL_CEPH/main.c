/*
 * Copyright © 2012, CohortFS, LLC.
 * Author: Adam C. Emerson <aemerson@linuxbox.com>
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

/**
 * @file FSAL_CEPH/main.c
 * @author Adam C. Emerson <aemerson@linuxbox.com>
 * @date Thu Jul  5 14:48:33 2012
 *
 * @brief Implementation of FSAL module functions for Ceph
 *
 * This file implements the module functions for the Ceph FSAL, for
 * initialization, teardown, configuration, and creation of exports.
 */

#include <stdlib.h>
#include <assert.h>
#include "internal.h"
#include "FSAL/fsal_init.h"
#include "FSAL/fsal_commonlib.h"

/**
 * A local copy of the handle for this module, so it can be disposed
 * of.
 */
static struct fsal_module *module = NULL;

/**
 * The name of this module.
 */
static const char *module_name = "Ceph";

/* Shared mount indirection */
struct shared_ceph_mount *sm;

/**
 * There is no defined mechanism for Ceph clients to interact with
 * multiple, distinct clusters yet (but presumably there will be).
 * For now, every object in a given Ceph env is by definition in the
 * same cluster, so sharing a single Ceph mount between exports is
 * the only case--but it should be the unmarked case when a way to
 * disambiguate Ceph clusters is added.  Then each export should have a
 * refcounted shared_ceph_mount object, and create_export (and later
 * destroy_export, etc) must do housekeeping accordingly.
 */

/**
 * @brief Allocate a shared Ceph mount
 *
 * This function uses (currently non-existent) parameters to allocate
 * and initialize a new shared Ceph mount.
 *
 * @return the new object.
 */
static struct shared_ceph_mount *
new_ceph_mount(const char *path, fsal_status_t *status)
{
	struct shared_ceph_mount *sm = NULL;
	/* A fake argument list for Ceph */
	const char *argv[] = {"FSAL_CEPH", path};
	int ceph_status = 0;

	sm = gsh_calloc(1, sizeof(struct shared_ceph_mount));
	if (sm == NULL) {
		status->major = ERR_FSAL_NOMEM;
		LogCrit(COMPONENT_FSAL,
			"Unable to allocate shared mount object for "
			"%s.", path);
		goto error;
	}

	/* allocates ceph_mount_info */
	ceph_status = ceph_create(&sm->cmount, NULL);
	if (ceph_status != 0) {
		status->major = ERR_FSAL_SERVERFAULT;
		LogCrit(COMPONENT_FSAL,
			"Unable to create Ceph handle");
		goto error;
	}

	ceph_status = ceph_conf_read_file(sm->cmount, NULL);
	if (ceph_status == 0) {
		ceph_status = ceph_conf_parse_argv(sm->cmount, 2, argv);
	}

	if (ceph_status != 0) {
		status->major = ERR_FSAL_SERVERFAULT;
		LogCrit(COMPONENT_FSAL,
			"Unable to read Ceph configuration");
		goto error;
	}

	ceph_status = ceph_mount(sm->cmount, NULL);
	if (ceph_status != 0) {
		status->major = ERR_FSAL_SERVERFAULT;
		LogCrit(COMPONENT_FSAL,
			"Unable to mount Ceph cluster.");
		goto error;
	}

	/* ds.osd holds OSD number for this machine (if applicable) */
	sm->ds.osd = ceph_get_local_osd(sm->cmount);

	return (sm);

error:
	if (sm) {
		ceph_shutdown(sm->cmount);
		sm->cmount = NULL;
		gsh_free(sm);
	}

	return (NULL);
}

/**
 * @brief Create a new export under this FSAL
 *
 * This function creates a new export object for the Ceph FSAL.
 *
 * @param[in]     module_in  The supplied module handle
 * @param[in]     path       The path to export
 * @param[in]     options    Export specific options for the FSAL
 * @param[in,out] list_entry Our entry in the export list
 * @param[in]     next_fsal  Next stacked FSAL
 * @param[out]    pub_export Newly created FSAL export object
 *
 * @return FSAL status.
 */
static fsal_status_t create_export(struct fsal_module *module,
				   const char *path,
				   const char *options,
				   struct exportlist *list_entry,
				   struct fsal_module *next_fsal,
				   const struct fsal_up_vector *up_ops,
				   struct fsal_export **pub_export)
{
	/* The status code to return */
	fsal_status_t status = {ERR_FSAL_NO_ERROR, 0};
	/* True if we have called fsal_export_init */
	bool initialized = false;
	/* The internal export object */
	struct export *export = NULL;

	if ((path == NULL) ||
	    (strlen(path) == 0)) {
		status.major = ERR_FSAL_INVAL;
		LogCrit(COMPONENT_FSAL,
			"No path to export.");
		goto error;
	}

	if (next_fsal != NULL) {
		status.major = ERR_FSAL_INVAL;
		LogCrit(COMPONENT_FSAL,
			"Stacked FSALs unsupported.");
		goto error;
	}

	export = gsh_calloc(1, sizeof(struct export));
	if (export == NULL) {
		status.major = ERR_FSAL_NOMEM;
		LogCrit(COMPONENT_FSAL,
			"Unable to allocate export object for %s.",
			path);
		goto error;
	}

	if (! sm) {
		sm = new_ceph_mount(path, &status);
		if (! sm)
			goto error;
	}

	if (fsal_export_init(&export->export,
			     list_entry) != 0) {
		status.major =  ERR_FSAL_NOMEM;
		LogCrit(COMPONENT_FSAL,
			"Unable to allocate export ops vectors for %s.",
			path);
		goto error;
	}

	export->sm = sm;
	atomic_inc_int32_t(&sm->refcnt);
	export_ops_init(export->export.ops);
	handle_ops_init(export->export.obj_ops);
	ds_ops_init(export->export.ds_ops);
	export->export.up_ops = up_ops;

	initialized = true;

	if (fsal_attach_export(module,
			       &export->export.exports) != 0) {
		status.major = ERR_FSAL_SERVERFAULT;
		LogCrit(COMPONENT_FSAL,
			"Unable to attach export.");
		goto error;
	}

	export->export.fsal = module;
	export->export.fsal = module;

	*pub_export = &export->export;
	return status;

error:
	if (initialized) {
		pthread_mutex_destroy(&export->export.lock);
		initialized = false;
	}

	if (export != NULL) {
		gsh_free(export);
		export = NULL;
	}

	return status;
}

/**
 * @brief Initialize and register the FSAL
 *
 * This function initializes the FSAL module handle, being called
 * before any configuration or even mounting of a Ceph cluster.  It
 * exists solely to produce a properly constructed FSAL module
 * handle.  Currently, we have no private, per-module data or
 * initialization.
 */

MODULE_INIT void init(void)
{
	/* register_fsal seems to expect zeroed memory. */
	module = gsh_calloc(1, sizeof(struct fsal_module));
	if (module == NULL) {
		LogCrit(COMPONENT_FSAL,
			"Unable to allocate memory for Ceph FSAL module.");
		return;
	}

	if (register_fsal(module, module_name, FSAL_MAJOR_VERSION,
			  FSAL_MINOR_VERSION) != 0) {
		/* The register_fsal function prints its own log
                   message if it fails*/
		gsh_free(module);
		LogCrit(COMPONENT_FSAL,
			"Ceph module failed to register.");
	}

	/* Set up module operations */
	module->ops->create_export = create_export;

	/* Null mounts */
	sm = NULL;

        /* Init reservation cache */
        ds_cache_pkginit();
}

/**
 * @brief Release FSAL resources
 *
 * This function unregisters the FSAL and frees its module handle.
 * The Ceph FSAL has no other resources to release on the per-FSAL
 * level.
 */

MODULE_FINI void finish(void)
{
	/* clean up reservation cache */
	ds_cache_pkgshutdown();

	if (unregister_fsal(module) != 0) {
		LogCrit(COMPONENT_FSAL,
			"Unable to unload FSAL.  Dying with extreme "
			"prejudice.");
		abort();
	}

	gsh_free(module);
	module = NULL;
}

