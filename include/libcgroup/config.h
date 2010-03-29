#ifndef _LIBCGROUP_CONFIG_H
#define _LIBCGROUP_CONFIG_H

#ifndef _LIBCGROUP_H_INSIDE
#error "Only <libcgroup.h> should be included directly."
#endif

#include <features.h>

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
 * @}
 * @}
 */
__END_DECLS

#endif /*_LIBCGROUP_CONFIG_H*/
