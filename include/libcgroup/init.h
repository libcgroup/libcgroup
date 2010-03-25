#ifndef _LIBCGROUP_INIT_H
#define _LIBCGROUP_INIT_H

#ifndef _LIBCGROUP_H_INSIDE
#error "Only <libcgroup.h> should be included directly."
#endif

#include <features.h>

__BEGIN_DECLS

/* Functions and structures that can be used by the application*/
int cgroup_init(void);

/*
 * Reads the mount to table to give the mount point of a controller
 * @controller: Name of the controller
 * @mount_point: The string where the mount point is stored. Please note,
 * the caller must free mount_point.
 */
int cgroup_get_subsys_mount_point(char *controller, char **mount_point);

__END_DECLS

#endif /* _LIBCGROUP_INIT_H */
