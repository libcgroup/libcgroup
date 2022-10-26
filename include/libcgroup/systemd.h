/* SPDX-License-Identifier: LGPL-2.1-only */
#ifndef _LIBCGROUP_SYSTEMD_H
#define _LIBCGROUP_SYSTEMD_H

#ifndef _LIBCGROUP_H_INSIDE
#error "Only <libcgroup.h> should be included directly."
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum cgroup_systemd_mode_t {
	CGROUP_SYSTEMD_MODE_FAIL = 0,
	CGROUP_SYSTEMD_MODE_REPLACE,
	CGROUP_SYSTEMD_MODE_ISOLATE,
	CGROUP_SYSTEMD_MODE_IGNORE_DEPS,
	CGROUP_SYSTEMD_MODE_IGNORE_REQS,

	CGROUP_SYSTEMD_MODE_CNT,
	CGROUP_SYSTEMD_MODE_DFLT = CGROUP_SYSTEMD_MODE_REPLACE
};

/**
 * Options associated with creating a systemd scope
 */
struct cgroup_systemd_scope_opts {
	/** should systemd delegate this cgroup or not.  1 == yes, 0 == no */
	int delegated;
	/** systemd behavior when the scope already exists */
	enum cgroup_systemd_mode_t mode;
	/** pid to be placed in the cgroup.  if 0, libcgroup will create a dummy process */
	pid_t pid;
};

/**
 * Populate the scope options structure with default values
 *
 * @param opts Scope creation options structure instance.  Must already be allocated
 *
 * @return 0 on success and > 0 on error
 */
int cgroup_set_default_scope_opts(struct cgroup_systemd_scope_opts * const opts);

/**
 * Create a systemd scope under the specified slice
 *
 * @param scope_name Name of the scope, must end in .scope
 * @param slice_name Name of the slice, must end in .slice
 * @param opts Scope creation options structure instance
 *
 * @return 0 on success and > 0 on error
 */
int cgroup_create_scope(const char * const scope_name, const char * const slice_name,
			const struct cgroup_systemd_scope_opts * const opts);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _LIBCGROUP_SYSTEMD_H */
