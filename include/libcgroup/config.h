#ifndef _LIBCGROUP_CONFIG_H
#define _LIBCGROUP_CONFIG_H

#ifndef _LIBCGROUP_H_INSIDE
#error "Only <libcgroup.h> should be included directly."
#endif

#ifndef SWIG
#include <features.h>
#endif

__BEGIN_DECLS

/**
 * @defgroup group_config 5. Configuration
 * @{
 *
 * @name Configuration file
 * @{
 *
 * @c libcgroup can mount and create control groups and set their parameters as
 * specified in a configuration file.
 *
 * @todo add this description?: These functions are mostly intended
 * to be used by internal @c libcgroup tools, however they are fully supported
 * and applications can benefit from them.
 */

/**
 * Load configuration file and mount and create control groups described there.
 * See cgconfig.conf man page for format of the file.
 * @param pathname Name of the configuration file to load.
 */
int cgroup_config_load_config(const char *pathname);

/**
 * Delete all control groups and unmount all hierarchies.
 */
int cgroup_unload_cgroups(void);

/**
 * Delete all cgroups and unmount all mount points defined in specified config
 * file.
 *
 * The groups are either removed recursively or only the empty ones, based
 * on given flags. Mount point are always umounted only if they are empty,
 * regardless of any flags.
 *
 * The groups are sorted before they are removed, so the removal of empty ones
 * actually works (i.e. subgroups are removed first).
 *
 * @param pathname Name of the configuration file to unload.
 * @param flags Combination of CGFLAG_DELETE_* flags, which indicate what and
 *	how to delete.
 */
int cgroup_config_unload_config(const char *pathname, int flags);

/**
 * Sets default permissions of groups created by subsequent
 * cgroup_config_load_config() calls. If a config file contains a 'default {}'
 * section, the default permissions from the config file is then used.
 *
 * Use cgroup_new_cgroup() to create a dummy group and cgroup_set_uid_gid() and
 * cgroup_set_permissions() to set its permissions. Use NO_UID_GID instead of
 * GID/UID and NO_PERMS instead of file/directory permissions to let kernel
 * decide the default permissions where you don't want specific user and/or
 * permissions. Kernel then uses current user/group and permissions from umask
 * then.
 *
 * @param new_default New default permissions from this group are copied to
 * libcgroup internal structures. I.e., this group can be freed immediatelly
 * after this function returns.
 */
int cgroup_config_set_default(struct cgroup *new_default);

/**
 * Initializes the templates cache and load it from file pathname.
 */
int cgroup_init_templates_cache(char *pathname);

/**
 * Reloads the templates list from file pathname.
 */
int cgroup_reload_cached_templates(char *pathname);

/**
 * Physically create a new control group in kernel, based on given control
 * group template and configuration file. If given template is not set in
 * configuration file, then the procedure works create the control group
 * using  cgroup_create_cgroup() function
 *
 * The flags can alter the behavior of this function:
 * CGFLAG_USE_TEMPLATE_CACHE: Use cached templates instead of
 * parsing the config file
 *
 * @param pathname Name of the configuration file with template definitions
 * @param cgroup Wanted control group - contains substitute name and wanted
 * controllers.
 * @param template_name Template name used for cgroup setting
 * @param flags Bit flags to change the behavior
 */
int cgroup_config_create_template_group(
	struct cgroup *cgroup, char *template_name,
	int flags);

/**
 * @}
 * @}
 */
__END_DECLS

#endif /*_LIBCGROUP_CONFIG_H*/
