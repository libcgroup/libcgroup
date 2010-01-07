/*
 * Copyright IBM Corporation. 2008
 *
 * Author:	Dhaval Giani <dhaval@linux.vnet.ibm.com>
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
#ifndef __LIBCG_INTERNAL

#define __LIBCG_INTERNAL

__BEGIN_DECLS

#include "config.h"
#include <fts.h>
#include <libcgroup.h>
#include <limits.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>

#define CGRULES_CONF_FILE       "/etc/cgrules.conf"
#define CGRULES_MAX_FIELDS_PER_LINE		3

#define CGROUP_BUFFER_LEN (5 * FILENAME_MAX)

/* Maximum length of a key(<user>:<process name>) in the daemon config file */
#define CGROUP_RULE_MAXKEY	(LOGIN_NAME_MAX + FILENAME_MAX + 1)

/* Maximum length of a line in the daemon config file */
#define CGROUP_RULE_MAXLINE	(FILENAME_MAX + CGROUP_RULE_MAXKEY + \
	CG_CONTROLLER_MAX + 3)

#ifdef CGROUP_DEBUG
#define cgroup_dbg(x...) printf(x)
#else
#define cgroup_dbg(x...) do {} while (0)
#endif

#define max(x,y) ((y)<(x)?(x):(y))
#define min(x,y) ((y)>(x)?(x):(y))

struct control_value {
	char name[FILENAME_MAX];
	char value[CG_VALUE_MAX];
};

struct cgroup_controller {
	char name[FILENAME_MAX];
	struct control_value *values[CG_NV_MAX];
	int index;
};

struct cgroup {
	char name[FILENAME_MAX];
	struct cgroup_controller *controller[CG_CONTROLLER_MAX];
	int index;
	uid_t tasks_uid;
	gid_t tasks_gid;
	uid_t control_uid;
	gid_t control_gid;
};


struct cg_mount_table_s {
	char name[FILENAME_MAX];
	char path[FILENAME_MAX];
	int index;
};

struct cgroup_rules_data {
	pid_t	pid; /* pid of the process which needs to change group */

	/* Details of user under consideration for destination cgroup */
	struct passwd	*pw;
	/* Gid of the process */
	gid_t	gid;
};

/* A rule that maps UID/GID to a cgroup */
struct cgroup_rule {
	uid_t uid;
	gid_t gid;
	char *procname;
	char username[LOGIN_NAME_MAX];
	char destination[FILENAME_MAX];
	char *controllers[MAX_MNT_ELEMENTS];
	struct cgroup_rule *next;
};

/* Container for a list of rules */
struct cgroup_rule_list {
	struct cgroup_rule *head;
	struct cgroup_rule *tail;
	int len;
};

/*The walk_tree handle */
struct cgroup_tree_handle {
	FTS *fts;
	int flags;
};


/* Internal API */
char *cg_build_path(char *name, char *path, char *type);
int cgroup_get_uid_gid_from_procfs(pid_t pid, uid_t *euid, gid_t *egid);
int cgroup_get_procname_from_procfs(pid_t pid, char **procname);
int cg_mkdir_p(const char *path);
struct cgroup *create_cgroup_from_name_value_pairs(const char *name,
		struct control_value *name_value, int nv_number);

/*
 * Main mounting structures
 */
struct cg_mount_table_s cg_mount_table[CG_CONTROLLER_MAX];
static pthread_rwlock_t cg_mount_table_lock = PTHREAD_RWLOCK_INITIALIZER;

/*
 * config related structures
 */

extern __thread char *cg_namespace_table[CG_CONTROLLER_MAX];

/*
 * config related API
 */
int cgroup_config_insert_cgroup(char *cg_name);
int cgroup_config_parse_controller_options(char *controller, char *name_value);
int cgroup_config_group_task_perm(char *perm_type, char *value);
int cgroup_config_group_admin_perm(char *perm_type, char *value);
int cgroup_config_insert_into_mount_table(char *name, char *mount_point);
int cgroup_config_insert_into_namespace_table(char *name, char *mount_point);
void cgroup_config_cleanup_mount_table(void);
__END_DECLS

#endif
