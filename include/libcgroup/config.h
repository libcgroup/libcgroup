#ifndef _LIBCGROUP_CONFIG_H
#define _LIBCGROUP_CONFIG_H

#ifndef _LIBCGROUP_H_INSIDE
#error "Only <libcgroup.h> should be included directly."
#endif

#include <features.h>

__BEGIN_DECLS

/*
 * Config related stuff
 */
int cgroup_config_load_config(const char *pathname);
int cgroup_unload_cgroups(void);

__END_DECLS

#endif /*_LIBCGROUP_CONFIG_H*/
