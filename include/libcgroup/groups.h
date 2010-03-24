#ifndef _LIBCGROUP_GROUPS_H
#define _LIBCGROUP_GROUPS_H

#ifndef _LIBCGROUP_H_INSIDE
#error "Only <libcgroup.h> should be included directly."
#endif

#include <features.h>
#include <sys/types.h>
#include <stdbool.h>

__BEGIN_DECLS

/**
 * Flags for cgroup_delete_cgroup_ext
 */
enum cgroup_delete_flag {
	/**
	 * Ignore errors caused by migration of tasks to parent group.
	 */
	CGFLAG_DELETE_IGNORE_MIGRATION = 1,

	/**
	 * Recursively delete all child groups.
	 */
	CGFLAG_DELETE_RECURSIVE	= 2,
};

struct cgroup;
struct cgroup_controller;

int cgroup_modify_cgroup(struct cgroup *cgroup);
int cgroup_create_cgroup(struct cgroup *cgroup, int ignore_ownership);
int cgroup_delete_cgroup(struct cgroup *cgroup, int ignore_migration);
int cgroup_get_cgroup(struct cgroup *cgroup);
int cgroup_create_cgroup_from_parent(struct cgroup *cgroup,
		int ignore_ownership);
int cgroup_copy_cgroup(struct cgroup *dst, struct cgroup *src);

/**
 * Delete control group.
 * All tasks are automatically moved to parent group.
 * If CGFLAG_DELETE_IGNORE_MIGRATION flag is used, the errors that occurred
 * during the task movement are ignored.
 * CGFLAG_DELETE_RECURSIVE flag specifies that all subgroups should be removed
 * too. If root group is being removed with this flag specified, all subgroups
 * are removed but the root group itself is left undeleted.
 *
 * @param cgroup Group to delete.
 * @param flags  Combination of CGFLAG_DELETE_* flags, which indicate what and
 *	how to delete.
 */
int cgroup_delete_cgroup_ext(struct cgroup *cgroup, int flags);

struct cgroup *cgroup_new_cgroup(const char *name);
struct cgroup_controller *cgroup_add_controller(struct cgroup *cgroup,
						const char *name);
void cgroup_free(struct cgroup **cgroup);
void cgroup_free_controllers(struct cgroup *cgroup);
int cgroup_compare_cgroup(struct cgroup *cgroup_a, struct cgroup *cgroup_b);
int cgroup_compare_controllers(struct cgroup_controller *cgca,
					struct cgroup_controller *cgcb);
struct cgroup_controller *cgroup_get_controller(struct cgroup *cgroup,
							const char *name);

int cgroup_add_value_string(struct cgroup_controller *controller,
				const char *name, const char *value);
int cgroup_add_value_int64(struct cgroup_controller *controller,
				const char *name, int64_t value);
int cgroup_add_value_uint64(struct cgroup_controller *controller,
				const char *name, u_int64_t value);
int cgroup_add_value_bool(struct cgroup_controller *controller,
				const char *name, bool value);
int cgroup_set_uid_gid(struct cgroup *cgroup, uid_t tasks_uid, gid_t tasks_gid,
					uid_t control_uid, gid_t control_gid);
int cgroup_get_uid_gid(struct cgroup *cgroup, uid_t *tasks_uid,
		gid_t *tasks_gid, uid_t *control_uid, gid_t *control_gid);
int cgroup_get_value_string(struct cgroup_controller *controller,
					const char *name, char **value);
int cgroup_set_value_string(struct cgroup_controller *controller,
					const char *name, const char *value);
int cgroup_get_value_int64(struct cgroup_controller *controller,
					const char *name, int64_t *value);
int cgroup_set_value_int64(struct cgroup_controller *controller,
					const char *name, int64_t value);
int cgroup_get_value_uint64(struct cgroup_controller *controller,
					const char *name, u_int64_t *value);
int cgroup_set_value_uint64(struct cgroup_controller *controller,
					const char *name, u_int64_t value);
int cgroup_get_value_bool(struct cgroup_controller *controller,
						const char *name, bool *value);
int cgroup_set_value_bool(struct cgroup_controller *controller,
						const char *name, bool value);
/**
 * Return the number of variables for the specified controller, if the
 * structure does not exist -1 is returned
 * @param controller Name of the controller for which stats are requested.
 */
int cgroup_get_value_name_count(struct cgroup_controller *controller);

/**
 * Return the "index" variable for the specified controller,
 * the return value is the pointer to the internal structure so
 * don't dealocate it, or change the content of the memory space.
 * @param controller Name of the controller for which stats are requested.
 * @param index number of the variable.
 */
char *cgroup_get_value_name(struct cgroup_controller *controller, int index);


__END_DECLS

#endif /* _LIBCGROUP_GROUPS_H */
