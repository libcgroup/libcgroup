#ifndef _LIBCGROUP_TASKS_H
#define _LIBCGROUP_TASKS_H

#ifndef _LIBCGROUP_H_INSIDE
#error "Only <libcgroup.h> should be included directly."
#endif

#include <libcgroup/groups.h>

#include <features.h>
#include <stdbool.h>

__BEGIN_DECLS

/* Flags for cgroup_change_cgroup_uid_gid() */
enum cgflags {
	CGFLAG_USECACHE = 0x01,
};

enum cgroup_daemon_type {
	CGROUP_DAEMON_UNCHANGE_CHILDREN = 0x1,
};

int cgroup_attach_task(struct cgroup *cgroup);
int cgroup_attach_task_pid(struct cgroup *cgroup, pid_t tid);

/**
 * Changes the cgroup of a program based on the rules in the config file.
 * If a rule exists for the given UID, GID or PROCESS NAME, then the given
 * PID is placed into the correct group.  By default, this function parses
 * the configuration file each time it is called.
 *
 * The flags can alter the behavior of this function:
 * 	CGFLAG_USECACHE: Use cached rules instead of parsing the config file
 *
 * This function may NOT be thread safe.
 * 	@param uid The UID to match
 * 	@param gid The GID to match
 * 	@param procname The PROCESS NAME to match
 * 	@param pid The PID of the process to move
 * 	@param flags Bit flags to change the behavior, as defined above
 * 	@return 0 on success, > 0 on error
 * TODO: Determine thread-safeness and fix of not safe.
 */
int cgroup_change_cgroup_flags(const uid_t uid, const gid_t gid,
		char *procname, const pid_t pid, const int flags);

/**
 * Changes the cgroup of a program based on the rules in the config file.  If a
 * rule exists for the given UID or GID, then the given PID is placed into the
 * correct group.  By default, this function parses the configuration file each
 * time it is called.
 *
 * The flags can alter the behavior of this function:
 *      CGFLAG_USECACHE: Use cached rules instead of parsing the config file
 *
 * This function may NOT be thread safe.
 * 	@param uid The UID to match
 * 	@param gid The GID to match
 * 	@param pid The PID of the process to move
 * 	@param flags Bit flags to change the behavior, as defined above
 * 	@return 0 on success, > 0 on error
 * TODO: Determine thread-safeness and fix if not safe.
 */
int cgroup_change_cgroup_uid_gid_flags(const uid_t uid, const gid_t gid,
				const pid_t pid, const int flags);

/**
 * Provides backwards-compatibility with older versions of the API.  This
 * function is deprecated, and cgroup_change_cgroup_uid_gid_flags() should be
 * used instead.  In fact, this function simply calls the newer one with flags
 * set to 0 (none).
 * 	@param uid The UID to match
 * 	@param gid The GID to match
 * 	@param pid The PID of the process to move
 * 	@return 0 on success, > 0 on error
 */
int cgroup_change_cgroup_uid_gid(uid_t uid, gid_t gid, pid_t pid);

/**
 * Changes the cgroup of a program based on the path provided.  In this case,
 * the user must already know into which cgroup the task should be placed and
 * no rules will be parsed.
 *
 *  returns 0 on success.
 */
int cgroup_change_cgroup_path(char *path, pid_t pid, char *controllers[]);

/**
 * Print the cached rules table.  This function should be called only after
 * first calling cgroup_parse_config(), but it will work with an empty rule
 * list.
 * 	@param fp The file stream to print to
 */
void cgroup_print_rules_config(FILE *fp);

/**
 * Reloads the rules list, using the given configuration file.  This function
 * is probably NOT thread safe (calls cgroup_parse_rules_config()).
 * 	@return 0 on success, > 0 on failure
 */
int cgroup_reload_cached_rules(void);

/**
 * Initializes the rules cache.
 * 	@return 0 on success, > 0 on failure
 */
int cgroup_init_rules_cache(void);

/**
 * Get the current cgroup path where the task specified by pid_t pid
 * has been classified
 */
int cgroup_get_current_controller_path(pid_t pid, const char *controller,
					char **current_path);

/**
 * Register the unchanged process to a cgrulesengd daemon.
 * If the daemon does not work, this function returns 0 as success.
 * @param pid: The process id
 * @param flags Bit flags to change the behavior, as defined above
 * @return 0 on success, > 0 on error.
 */
int cgroup_register_unchanged_process(pid_t pid, int flags);

__END_DECLS

#endif /* _LIBCGROUP_TASKS_H */
