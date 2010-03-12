#ifndef _LIBCGROUP_ITERATORS_H
#define _LIBCGROUP_ITERATORS_H

#include <sys/types.h>
#include <stdio.h>
#include <features.h>

__BEGIN_DECLS

/*
 * Don't use CGROUP_WALK_TYPE_FILE right now. It is added here for
 * later refactoring and better implementation. Most users *should*
 * use CGROUP_WALK_TYPE_PRE_DIR.
 */
enum cgroup_walk_type {
	CGROUP_WALK_TYPE_PRE_DIR = 0x1,	/* Pre Order Directory */
	CGROUP_WALK_TYPE_POST_DIR = 0x2,	/* Post Order Directory */
};

enum cgroup_file_type {
	CGROUP_FILE_TYPE_FILE,		/* File */
	CGROUP_FILE_TYPE_DIR,		/* Directory */
	CGROUP_FILE_TYPE_OTHER,		/* Directory */
};
struct cgroup_file_info {
	enum cgroup_file_type type;
	const char *path;
	const char *parent;
	const char *full_path;
	short depth;
};

#define CG_VALUE_MAX 100
struct cgroup_stat {
	char name[FILENAME_MAX];
	char value[CG_VALUE_MAX];
};

struct cgroup_mount_point {
	char name[FILENAME_MAX];
	char path[FILENAME_MAX];
};

/*
 * Detailed information about available controller.
 */

struct controller_data {
/** Controller name. */
	char name[FILENAME_MAX];
/**
 * Hierarchy ID. Controllers with the same hierarchy ID
 * are mounted together as one hierarchy. Controllers with
 * ID 0 are not currently used.
 */
	int hierarchy;
/** Number of groups. */
	int num_cgroups;
/** Enabled flag */
	int enabled;
};

/**
 * Walk through the directory tree for the specified controller.
 * @controller: Name of the controller, for which we want to walk
 * the directory tree
 * @base_path: Begin walking from this path
 * @depth: The maximum depth to which the function should walk, 0
 * implies all the way down
 * @handle: Handle to be used during iteration
 * @info: info filled and returned about directory information
 */
int cgroup_walk_tree_begin(char *controller, char *base_path, const int depth,
				void **handle, struct cgroup_file_info *info,
				int *base_level);
/**
 * Get the next element during the walk
 * @depth: The maximum depth to which the function should walk, 0
 * implies all the way down
 * @handle: Handle to be used during iteration
 * @info: info filled and returned about directory information
 *
 * Returns ECGEOF when we are done walking through the nodes.
 */
int cgroup_walk_tree_next(const int depth, void **handle,
				struct cgroup_file_info *info, int base_level);
int cgroup_walk_tree_end(void **handle);

/**
 * This API is used to set the flags for walk_tree API. Currently availble
 *  flags are
 *
 *  CGROUP_WALK_TYPE_PRE_DIR
 *  CGROUP_WALK_TYPE_POST_DIR
 *
 */
int cgroup_walk_tree_set_flags(void **handle, int flags);

/**
 * Read the statistics values for the specified controller
 * @controller: Name of the controller for which stats are requested.
 * @path: cgroup path.
 * @handle: Handle to be used during iteration.
 * @stat: Stats values will be filled and returned here.
 */
int cgroup_read_stats_begin(char *controller, char *path, void **handle,
				struct cgroup_stat *stat);

/**
 * Read the next stat value.
 * @handle: Handle to be used during iteration.
 * @stat: Stats values will be filled and returned here.
 */
int cgroup_read_stats_next(void **handle, struct cgroup_stat *stat);

int cgroup_read_stats_end(void **handle);

/**
 * Read the tasks file to get the list of tasks in a cgroup
 * @cgroup: Name of the cgroup
 * @controller: Name of the cgroup subsystem
 * @handle: Handle to be used in the iteration
 * @pid: The pid read from the tasks file. Will be filled in by the API
 */
int cgroup_get_task_begin(char *cgroup, char *controller, void **handle,
								pid_t *pid);

/**
 * Read the next task value
 * @handle: The handle used for iterating
 * @pid: The variable where the value will be stored
 *
 * return ECGEOF when the iterator finishes getting the list of tasks.
 */
int cgroup_get_task_next(void **handle, pid_t *pid);
int cgroup_get_task_end(void **handle);

/**
 * Read the mount table to give a list where each controller is
 * mounted
 * @handle: Handle to be used for iteration.
 * @name: The variable where the name is stored. Should be freed by caller.
 * @path: Te variable where the path to the controller is stored. Should be
 * freed by the caller.
 */
int cgroup_get_controller_begin(void **handle, struct cgroup_mount_point *info);
/*
 * While walking through the mount table, the controllers will be
 * returned in order of their mount points.
 */
int cgroup_get_controller_next(void **handle, struct cgroup_mount_point *info);
int cgroup_get_controller_end(void **handle);

/**
 * Read the list of controllers from /proc/cgroups (not mounted included)
 * @param handle: Handle to be used for iteration.
 * @param info: The structure which contains all controller data
 */
int cgroup_get_all_controller_begin(void **handle,
	struct controller_data *info);
/*
 * While walking through the mount table, the controllers will be
 * returned in the same order as is in /proc/cgroups file
 */
int cgroup_get_all_controller_next(void **handle, struct controller_data *info);
int cgroup_get_all_controller_end(void **handle);

__END_DECLS

#endif /* _LIBCGROUP_ITERATORS_H */
