/*
 * Copyright IBM Corporation. 2007
 *
 * Author:	Balbir Singh <balbir@linux.vnet.ibm.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2.1 of the GNU Lesser General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#ifndef _LIBCG_H
#define _LIBCG_H

#include <features.h>

__BEGIN_DECLS

#include <grp.h>
#include <linux/types.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef __USE_GNU
#define __USE_GNU
#endif

/* Maximum number of mount points/controllers */
#define MAX_MNT_ELEMENTS	8
/* Estimated number of groups created */
#define MAX_GROUP_ELEMENTS	128

#ifdef DEBUG
#define dbg(x...) printf(x)
#else
#define dbg(x...) do {} while(0)
#endif

/*
 * NOTE: Wide characters are not supported at the moment. Wide character support
 * would require us to use a scanner/parser that can parse beyond ASCII
 */

/*
 * These data structures are heavily controller dependent, which means
 * any changes (additions/removal) of configuration items would have to
 *  be reflected in this library. We might implement a plugin
 *  infrastructure, so that we can deal with such changes with ease.
 */

struct cpu_controller {
	/*TODO: Add the cpu.usage file here, also need to automate this.*/
	char *shares;	/* Having strings helps us write them to files */
	/*
	 * XX: No it does not make a difference. It requires a fprintf anyway
	 * so it needs the qualifier.
	 */
};

struct cg_group {
	char *name;
	uid_t tasks_uid;
	gid_t tasks_gid;
	uid_t admin_uid;
	gid_t admin_gid;
	struct cpu_controller cpu_config;
};

/*
 * A singly linked list suffices since we don't expect too many mount points
 */
struct mount_table {
	char *options;		/* Name(s) of the controller */
	char *mount_point;	/* The place where the controller is mounted */
	struct mount_table *next;
};

/*
 * Maintain a list of all group names. These will be used during cleanup
 */
struct list_of_names {
	char *name;
	struct list_of_names *next;
};

enum cg_msg_type {
	CG_MSG_LOAD_FILE,
	CG_MSG_UNLOAD_FILE,
	CG_MSG_ERR,
	CG_MSG_DONE,
};

enum cgroup_errors {
	ECGROUPNOTCOMPILED=50000,
	ECGROUPNOTMOUNTED,
	ECGROUPNOTEXIST,
	ECGROUPNOTCREATED,
	ECGROUPSUBSYSNOTMOUNTED,
	ECGROUPNOTOWNER,
	ECGROUPMULTIMOUNTED,/* Controllers bound to different mount points */
	ECGROUPNOTALLOWED,  /* This is the stock error. Default error. */
	ECGMAXVALUESEXCEEDED,
	ECGCONTROLLEREXISTS,
	ECGVALUEEXISTS,
	ECGINVAL,
	ECGCONTROLLERCREATEFAILED,
	ECGFAIL,
	ECGROUPNOTINITIALIZED,
	ECGROUPVALUENOTEXIST,
	/* Represents error coming from other libraries like glibc. libcgroup
	 * users need to check errno upon encoutering ECGOTHER.
	 */
	ECGOTHER,
	ECGROUPNOTEQUAL,
	ECGCONTROLLERNOTEQUAL,
};

#define CG_MAX_MSG_SIZE		256
#define CG_SERVER_MSG_PATH	"/tmp/control_group"
#define CG_BACKLOG		5

/* Message's exchanged between server and client */
struct cg_msg {
	enum cg_msg_type type;
	char buf[CG_MAX_MSG_SIZE];
};

/* Function Prototypes start here */
int cg_init_group_and_mount_info(void);
int cg_insert_into_mount_table(const char *name, const char *mount_point);
void cg_cleanup_mount_table(void);
int cg_group_admin_perm(char *perm_type, char *value);
int cg_group_task_perm(char *perm_type, char *value);
int cg_parse_controller_options(char *controller, char *name_value);
int cg_insert_group(const char *group_name);
int chown_recursive(const char* path, uid_t owner, gid_t group);
int cg_make_directory(struct cg_group *cg_group, const char *group_path);
char *cg_build_group_path(struct cg_group *cg_group,
					struct mount_table *mount_info);
int cg_mount_controllers(void);
int cg_unmount_controllers(void);
int cg_load_config(const char *pathname);
void cg_unload_current_config(void);

#define CG_NV_MAX 100
#define CG_CONTROLLER_MAX 100
#define CG_VALUE_MAX 100
/* Functions and structures that can be used by the application*/
struct cgroup;
struct cgroup_controller;

int cgroup_init(void);
int cgroup_attach_task(struct cgroup *cgroup);
int cgroup_modify_cgroup(struct cgroup *cgroup);
int cgroup_create_cgroup(struct cgroup *cgroup, int ignore_ownership);
int cgroup_delete_cgroup(struct cgroup *cgroup, int ignore_migration);
int cgroup_attach_task_pid(struct cgroup *cgroup, pid_t tid);
struct cgroup *cgroup_get_cgroup(struct cgroup *cgroup);

/* The wrappers for filling libcg structures */

struct cgroup *cgroup_new_cgroup(const char *name, uid_t tasks_uid,
		gid_t tasks_gid, uid_t control_uid, gid_t control_gid);
struct cgroup_controller *cgroup_add_controller(struct cgroup *cgroup,
						const char *name);
void cgroup_free(struct cgroup **cgroup);
int cgroup_add_value_string(struct cgroup_controller *controller,
				const char *name, const char *value);
int cgroup_add_value_int64(struct cgroup_controller *controller,
				const char *name, int64_t value);
int cgroup_add_value_uint64(struct cgroup_controller *controller,
				const char *name, u_int64_t value);
int cgroup_add_value_bool(struct cgroup_controller *controller,
				const char *name, bool value);
int cgroup_compare_cgroup(struct cgroup *cgroup_a, struct cgroup *cgroup_b);
int cgroup_compare_controllers(struct cgroup_controller *cgca,
					struct cgroup_controller *cgcb);

__END_DECLS

#endif /* _LIBCG_H  */
