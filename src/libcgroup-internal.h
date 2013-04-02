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
#include <setjmp.h>

/* Maximum number of mount points/controllers */
#define MAX_MNT_ELEMENTS	8
/* Estimated number of groups created */
#define MAX_GROUP_ELEMENTS	128

#define CG_NV_MAX 100
#define CG_CONTROLLER_MAX 100
/* Max number of mounted hierarchies. Event if one controller is mounted per
 * hier, it can not exceed CG_CONTROLLER_MAX
 */
#define CG_HIER_MAX  CG_CONTROLLER_MAX

/* Definitions for the uid and gid members of a cgroup_rules */
/* FIXME: These really should not be negative values */
#define CGRULE_INVALID ((uid_t) -1)
#define CGRULE_WILD ((uid_t) -2)

#define CGRULE_SUCCESS_STORE_PID	"SUCCESS_STORE_PID"


#define CGCONFIG_CONF_FILE		"/etc/cgconfig.conf"

#define CGRULES_CONF_FILE       "/etc/cgrules.conf"
#define CGRULES_MAX_FIELDS_PER_LINE		3

#define CGROUP_BUFFER_LEN (5 * FILENAME_MAX)

/* Maximum length of a key(<user>:<process name>) in the daemon config file */
#define CGROUP_RULE_MAXKEY	(LOGIN_NAME_MAX + FILENAME_MAX + 1)

/* Maximum length of a line in the daemon config file */
#define CGROUP_RULE_MAXLINE	(FILENAME_MAX + CGROUP_RULE_MAXKEY + \
	CG_CONTROLLER_MAX + 3)

#define cgroup_err(x...) cgroup_log(CGROUP_LOG_ERROR, x)
#define cgroup_warn(x...) cgroup_log(CGROUP_LOG_WARNING, x)
#define cgroup_info(x...) cgroup_log(CGROUP_LOG_INFO, x)
#define cgroup_dbg(x...) cgroup_log(CGROUP_LOG_DEBUG, x)

#define CGROUP_DEFAULT_LOGLEVEL CGROUP_LOG_ERROR

#define max(x,y) ((y)<(x)?(x):(y))
#define min(x,y) ((y)>(x)?(x):(y))

struct control_value {
	char name[FILENAME_MAX];
	char value[CG_VALUE_MAX];
	bool dirty;
};

struct cgroup_controller {
	char name[FILENAME_MAX];
	struct control_value *values[CG_NV_MAX];
	struct cgroup *cgroup;
	int index;
};

struct cgroup {
	char name[FILENAME_MAX];
	struct cgroup_controller *controller[CG_CONTROLLER_MAX];
	int index;
	uid_t tasks_uid;
	gid_t tasks_gid;
	mode_t task_fperm;
	uid_t control_uid;
	gid_t control_gid;
	mode_t control_fperm;
	mode_t control_dperm;
};

struct cg_mount_point {
	char path[FILENAME_MAX];
	struct cg_mount_point *next;
};

struct cg_mount_table_s {
	/** Controller name. */
	char name[FILENAME_MAX];
	/**
	 * List of mount points, at least one mount point is there for sure.
	 */
	struct cg_mount_point mount;
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

/**
 * Internal item of dictionary. Linked list is sufficient for now - we need
 * only 'add' operation and simple iterator. In future, this might be easily
 * rewritten to dynamic array when random access is needed,
 * just keep in mind that the order is important and the iterator should
 * return the items in the order they were added there.
 */
struct cgroup_dictionary_item {
	const char *name;
	const char *value;
	struct cgroup_dictionary_item *next;
};

/* Flags for cgroup_dictionary_create */
/**
 * All items (i.e. both name and value strings) stored in the dictionary
 * should *NOT* be free()d on cgroup_dictionary_free(),
 * only the  dictionary helper structures (i.e. underlying linked list)
 * should be freed.
 */
#define CG_DICT_DONT_FREE_ITEMS		1

/**
 * Dictionary of (name, value) items.
 * The dictionary keeps its order, iterator iterates in the same order
 * as the items were added there. It is *not* hash-style structure,
 * it does not provide random access to its items nor quick search.
 * This structure should be opaque to users of the dictionary, underlying data
 * structure might change anytime and without warnings.
 */
struct cgroup_dictionary {
	struct cgroup_dictionary_item *head;
	struct cgroup_dictionary_item *tail;
	int flags;
};

/** Opaque iterator of an dictionary. */
struct cgroup_dictionary_iterator {
	struct cgroup_dictionary_item *item;
};

/**
 * per thread errno variable, to be used when return code is ECGOTHER
 */
extern __thread int last_errno;

/**
 * 'Exception handler' for lex parser.
 */
extern jmp_buf parser_error_env;

/* Internal API */
char *cg_build_path(const char *name, char *path, const char *type);
int cgroup_get_uid_gid_from_procfs(pid_t pid, uid_t *euid, gid_t *egid);
int cgroup_get_procname_from_procfs(pid_t pid, char **procname);
int cg_mkdir_p(const char *path);
struct cgroup *create_cgroup_from_name_value_pairs(const char *name,
		struct control_value *name_value, int nv_number);
void init_cgroup_table(struct cgroup *cgroups, size_t count);

/*
 * Main mounting structures
 */
extern struct cg_mount_table_s cg_mount_table[CG_CONTROLLER_MAX];
extern pthread_rwlock_t cg_mount_table_lock;

/*
 * config related structures
 */

extern __thread char *cg_namespace_table[CG_CONTROLLER_MAX];

/*
 * config related API
 */
int cgroup_config_insert_cgroup(char *cg_name);
int cgroup_config_parse_controller_options(char *controller,
		struct cgroup_dictionary *values);
int template_config_insert_cgroup(char *cg_name);
int template_config_parse_controller_options(char *controller,
		struct cgroup_dictionary *values);
int template_config_group_task_perm(char *perm_type, char *value);
int template_config_group_admin_perm(char *perm_type, char *value);
int cgroup_config_group_task_perm(char *perm_type, char *value);
int cgroup_config_group_admin_perm(char *perm_type, char *value);
int cgroup_config_insert_into_mount_table(char *name, char *mount_point);
int cgroup_config_insert_into_namespace_table(char *name, char *mount_point);
void cgroup_config_cleanup_mount_table(void);
void cgroup_config_cleanup_namespace_table(void);
int cgroup_config_define_default(void);

/**
 * Create an empty dictionary.
 */
extern int cgroup_dictionary_create(struct cgroup_dictionary **dict,
		int flags);
/**
 * Add an item to existing dictionary.
 */
extern int cgroup_dictionary_add(struct cgroup_dictionary *dict,
		const char *name, const char *value);
/**
 * Fully destroy existing dictionary. Depending on flags passed to
 * cgroup_dictionary_create(), names and values might get destroyed too.
 */
extern int cgroup_dictionary_free(struct cgroup_dictionary *dict);

/**
 * Start iterating through a dictionary. The items are returned in the same
 * order as they were added using cgroup_dictionary_add().
 */
extern int cgroup_dictionary_iterator_begin(struct cgroup_dictionary *dict,
		void **handle, const char **name, const char **value);
/**
 * Continue iterating through the dictionary.
 */
extern int cgroup_dictionary_iterator_next(void **handle,
		const char **name, const char **value);
/**
 * Finish iteration through the dictionary.
 */
extern void cgroup_dictionary_iterator_end(void **handle);

/**
 * Changes permissions for given path. If owner_is_umask is specified
 * then it uses owner permissions as a mask for group and others permissions.
 *
 * @param path Patch to chmod.
 * @param mode File permissions to set.
 * @param owner_is_umask Flag whether path owner permissions should be used
 * as a mask for group and others permissions.
 */
int cg_chmod_path(const char *path, mode_t mode, int owner_is_umask);

__END_DECLS

#endif
