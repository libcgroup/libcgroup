/*
 * Copyright IBM Corporation. 2007
 *
 * Authors:	Balbir Singh <balbir@linux.vnet.ibm.com>
 *		Dhaval Giani <dhaval@linux.vnet.ibm.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2.1 of the GNU Lesser General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * TODOs:
 *	1. Implement our own hashing scheme
 *	2. Add namespace support
 *	3. Add support for parsing cgroup filesystem and creating a
 *	   config out of it.
 *
 * Code initiated and designed by Balbir Singh. All faults are most likely
 * his mistake.
 *
 * Cleanup and changes to use the "official" structures and functions made
 * by Dhaval Giani. All faults will still be Balbir's mistake :)
 */

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <grp.h>
#include <libcgroup.h>
#include <libcgroup-internal.h>
#include <limits.h>
#include <pwd.h>
#include <pthread.h>
#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>

unsigned int MAX_CGROUPS = 64;	/* NOTE: This value changes dynamically */

extern FILE *yyin;
extern int yyparse(void);

static struct cgroup default_group;
static int default_group_set = 0;

/*
 * The basic global data structures.
 *
 * config_mount_table -> Where what controller is mounted
 * table_index -> Where in the table are we.
 * config_table_lock -> Serializing access to config_mount_table.
 * cgroup_table -> Which cgroups have to be created.
 * cgroup_table_index -> Where in the cgroup_table we are.
 */
static struct cg_mount_table_s config_mount_table[CG_CONTROLLER_MAX];
static struct cg_mount_table_s config_namespace_table[CG_CONTROLLER_MAX];
static int config_table_index;
static int namespace_table_index;
static pthread_rwlock_t config_table_lock = PTHREAD_RWLOCK_INITIALIZER;
static pthread_rwlock_t namespace_table_lock = PTHREAD_RWLOCK_INITIALIZER;
static struct cgroup *config_cgroup_table;
static int cgroup_table_index;

/*
 * Needed for the type while mounting cgroupfs.
 */
#define CGROUP_FILESYSTEM "cgroup"

/*
 * NOTE: All these functions return 1 on success
 * and not 0 as is the library convention
 */

/*
 * This call just sets the name of the cgroup. It will
 * always be called in the end, because the parser will
 * work bottom up.
 */
int cgroup_config_insert_cgroup(char *cg_name)
{
	struct cgroup *config_cgroup;

	if (cgroup_table_index >= MAX_CGROUPS - 1) {
		struct cgroup *newblk;
		unsigned int oldlen;

		if (MAX_CGROUPS >= INT_MAX) {
			last_errno = ENOMEM;
			return 0;
		}
		oldlen = MAX_CGROUPS;
		MAX_CGROUPS *= 2;
		newblk = realloc(config_cgroup_table, (MAX_CGROUPS *
					sizeof(struct cgroup)));
		if (!newblk) {
			last_errno = ENOMEM;
			return 0;
		}

		memset(newblk + oldlen, 0, (MAX_CGROUPS - oldlen) *
				sizeof(struct cgroup));
		init_cgroup_table(newblk + oldlen, MAX_CGROUPS - oldlen);
		config_cgroup_table = newblk;
		cgroup_dbg("MAX_CGROUPS %d\n", MAX_CGROUPS);
		cgroup_dbg("reallocated config_cgroup_table to %p\n", config_cgroup_table);
	}

	config_cgroup = &config_cgroup_table[cgroup_table_index];
	strncpy(config_cgroup->name, cg_name, FILENAME_MAX);

	/*
	 * Since this will be the last part to be parsed.
	 */
	cgroup_table_index++;
	free(cg_name);
	return 1;
}

/*
 * This function sets the various controller's control
 * files. It will always append values for cgroup_table_index
 * entry in the cgroup_table. The index is incremented in
 * cgroup_config_insert_cgroup
 */
int cgroup_config_parse_controller_options(char *controller,
	struct cgroup_dictionary *values)
{
	const char *name, *value;
	struct cgroup_controller *cgc;
	int error;
	struct cgroup *config_cgroup =
		&config_cgroup_table[cgroup_table_index];
	void *iter = NULL;

	cgroup_dbg("Adding controller %s\n", controller);
	cgc = cgroup_add_controller(config_cgroup, controller);

	if (!cgc)
		goto parse_error;

	/*
	 * Did we just specify the controller to create the correct
	 * set of directories, without setting any values?
	 */
	if (!values)
		goto done;

	error = cgroup_dictionary_iterator_begin(values, &iter, &name, &value);
	while (error == 0) {
		cgroup_dbg("[1] name value pair being processed is %s=%s\n",
				name, value);
		if (!name)
			goto parse_error;
		error = cgroup_add_value_string(cgc, name, value);
		if (error)
			goto parse_error;
		error = cgroup_dictionary_iterator_next(&iter, &name, &value);
	}
	cgroup_dictionary_iterator_end(&iter);
	iter = NULL;

	if (error != ECGEOF)
		goto parse_error;

done:
	free(controller);
	return 1;

parse_error:
	free(controller);
	cgroup_dictionary_iterator_end(&iter);
	cgroup_delete_cgroup(config_cgroup, 1);
	cgroup_table_index--;
	return 0;
}

/*
 * Sets the tasks file's uid and gid
 */
int cgroup_config_group_task_perm(char *perm_type, char *value)
{
	struct passwd *pw, *pw_buffer;
	struct group *group, *group_buffer;
	long val = atoi(value);
	char buffer[CGROUP_BUFFER_LEN];
	struct cgroup *config_cgroup =
			&config_cgroup_table[cgroup_table_index];

	if (!strcmp(perm_type, "uid")) {
		if (!val) {
			pw = (struct passwd *) malloc(sizeof(struct passwd));

			if (!pw)
				goto group_task_error;

			getpwnam_r(value, pw, buffer, CGROUP_BUFFER_LEN,
								&pw_buffer);
			if (pw_buffer == NULL) {
				free(pw);
				goto group_task_error;
			}

			val = pw->pw_uid;
			free(pw);
		}
		config_cgroup->tasks_uid = val;
	}

	if (!strcmp(perm_type, "gid")) {
		if (!val) {
			group = (struct group *) malloc(sizeof(struct group));

			if (!group)
				goto group_task_error;

			getgrnam_r(value, group, buffer,
					CGROUP_BUFFER_LEN, &group_buffer);

			if (group_buffer == NULL) {
				free(group);
				goto group_task_error;
			}

			val = group->gr_gid;
			free(group);
		}
		config_cgroup->tasks_gid = val;
	}

	if (!strcmp(perm_type, "fperm")) {
		char *endptr;
		val = strtol(value, &endptr, 8);
		if (*endptr)
			goto group_task_error;
		config_cgroup->task_fperm = val;
	}

	free(perm_type);
	free(value);
	return 1;

group_task_error:
	free(perm_type);
	free(value);
	cgroup_delete_cgroup(config_cgroup, 1);
	cgroup_table_index--;
	return 0;
}

/*
 * Set the control file's uid/gid
 */
int cgroup_config_group_admin_perm(char *perm_type, char *value)
{
	struct passwd *pw, *pw_buffer;
	struct group *group, *group_buffer;
	struct cgroup *config_cgroup =
				&config_cgroup_table[cgroup_table_index];
	long val = atoi(value);
	char buffer[CGROUP_BUFFER_LEN];

	if (!strcmp(perm_type, "uid")) {
		if (!val) {
			pw = (struct passwd *) malloc(sizeof(struct passwd));

			if (!pw)
				goto admin_error;

			getpwnam_r(value, pw, buffer, CGROUP_BUFFER_LEN,
								&pw_buffer);
			if (pw_buffer == NULL) {
				free(pw);
				goto admin_error;
			}

			val = pw->pw_uid;
			free(pw);
		}
		config_cgroup->control_uid = val;
	}

	if (!strcmp(perm_type, "gid")) {
		if (!val) {
			group = (struct group *) malloc(sizeof(struct group));

			if (!group)
				goto admin_error;

			getgrnam_r(value, group, buffer,
					CGROUP_BUFFER_LEN, &group_buffer);

			if (group_buffer == NULL) {
				free(group);
				goto admin_error;
			}

			val = group->gr_gid;
			free(group);
		}
		config_cgroup->control_gid = val;
	}

	if (!strcmp(perm_type, "fperm")) {
		char *endptr;
		val = strtol(value, &endptr, 8);
		if (*endptr)
			goto admin_error;
		config_cgroup->control_fperm = val;
	}

	if (!strcmp(perm_type, "dperm")) {
		char *endptr;
		val = strtol(value, &endptr, 8);
		if (*endptr)
			goto admin_error;
		config_cgroup->control_dperm = val;
	}

	free(perm_type);
	free(value);
	return 1;

admin_error:
	free(perm_type);
	free(value);
	cgroup_delete_cgroup(config_cgroup, 1);
	cgroup_table_index--;
	return 0;
}

/*
 * The moment we have found the controller's information
 * insert it into the config_mount_table.
 */
int cgroup_config_insert_into_mount_table(char *name, char *mount_point)
{
	int i;

	if (config_table_index >= CG_CONTROLLER_MAX)
		return 0;

	pthread_rwlock_wrlock(&config_table_lock);

	/*
	 * Merge controller names with the same mount point
	 */
	for (i = 0; i < config_table_index; i++) {
		if (strcmp(config_mount_table[i].mount.path,
				mount_point) == 0) {
			char *cname = config_mount_table[i].name;
			strncat(cname, ",", FILENAME_MAX - strlen(cname) - 1);
			strncat(cname, name,
					FILENAME_MAX - strlen(cname) - 1);
			goto done;
		}
	}

	strcpy(config_mount_table[config_table_index].name, name);
	strcpy(config_mount_table[config_table_index].mount.path, mount_point);
	config_mount_table[config_table_index].mount.next = NULL;
	config_table_index++;
done:
	pthread_rwlock_unlock(&config_table_lock);
	free(name);
	free(mount_point);
	return 1;
}

/*
 * Cleanup all the data from the config_mount_table
 */
void cgroup_config_cleanup_mount_table(void)
{
	memset(&config_mount_table, 0,
			sizeof(struct cg_mount_table_s) * CG_CONTROLLER_MAX);
}

/*
 * The moment we have found the controller's information
 * insert it into the config_mount_table.
 */
int cgroup_config_insert_into_namespace_table(char *name, char *nspath)
{
	if (namespace_table_index >= CG_CONTROLLER_MAX)
		return 0;

	pthread_rwlock_wrlock(&namespace_table_lock);

	strcpy(config_namespace_table[namespace_table_index].name, name);
	strcpy(config_namespace_table[namespace_table_index].mount.path,
			nspath);
	config_namespace_table[namespace_table_index].mount.next = NULL;
	namespace_table_index++;

	pthread_rwlock_unlock(&namespace_table_lock);
	free(name);
	free(nspath);
	return 1;
}

/*
 * Cleanup all the data from the config_mount_table
 */
void cgroup_config_cleanup_namespace_table(void)
{
	memset(&config_namespace_table, 0,
			sizeof(struct cg_mount_table_s) * CG_CONTROLLER_MAX);
}

/**
 * Add necessary options for mount. Currently only 'none' option is added
 * for mounts with only 'name=xxx' and without real controller.
 */
static int cgroup_config_ajdust_mount_options(struct cg_mount_table_s *mount)
{
	char *save = NULL;
	char *opts = strdup(mount->name);
	char *token;
	int name_only = 1;

	if (opts == NULL)
		return ECGFAIL;

	token = strtok_r(opts, ",", &save);
	while (token != NULL) {
		if (strncmp(token, "name=", 5) != 0) {
			name_only = 0;
			break;
		}
		token = strtok_r(NULL, ",", &save);
	}

	free(opts);
	if (name_only) {
		strncat(mount->name, ",", FILENAME_MAX - strlen(mount->name)-1);
		strncat(mount->name, "none",
				FILENAME_MAX - strlen(mount->name) - 1);
	}

	return 0;
}

/*
 * Start mounting the mount table.
 */
static int cgroup_config_mount_fs(void)
{
	int ret;
	struct stat buff;
	int i;
	int error;

	for (i = 0; i < config_table_index; i++) {
		struct cg_mount_table_s *curr =	&(config_mount_table[i]);

		ret = stat(curr->mount.path, &buff);

		if (ret < 0 && errno != ENOENT) {
			last_errno = errno;
			error = ECGOTHER;
			goto out_err;
		}

		if (errno == ENOENT) {
			ret = cg_mkdir_p(curr->mount.path);
			if (ret) {
				error = ret;
				goto out_err;
			}
		} else if (!S_ISDIR(buff.st_mode)) {
			errno = ENOTDIR;
			last_errno = errno;
			error = ECGOTHER;
			goto out_err;
		}

		error = cgroup_config_ajdust_mount_options(curr);
		if (error)
			goto out_err;

		ret = mount(CGROUP_FILESYSTEM, curr->mount.path,
				CGROUP_FILESYSTEM, 0, curr->name);

		if (ret < 0) {
			error = ECGMOUNTFAIL;
			goto out_err;
		}
	}
	return 0;
out_err:
	/*
	 * If we come here, we have failed. Since we have touched only
	 * mountpoints prior to i, we shall operate on only them now.
	 */
	config_table_index = i;
	return error;
}

/*
 * Actually create the groups once the parsing has been finished.
 */
static int cgroup_config_create_groups(void)
{
	int error = 0;
	int i;

	for (i = 0; i < cgroup_table_index; i++) {
		struct cgroup *cgroup = &config_cgroup_table[i];
		error = cgroup_create_cgroup(cgroup, 0);
		cgroup_dbg("creating group %s, error %d\n", cgroup->name,
			error);
		if (error)
			return error;
	}
	return error;
}

/*
 * Destroy the cgroups
 */
static int cgroup_config_destroy_groups(void)
{
	int error = 0, ret = 0;
	int i;

	for (i = 0; i < cgroup_table_index; i++) {
		struct cgroup *cgroup = &config_cgroup_table[i];
		error = cgroup_delete_cgroup_ext(cgroup,
				CGFLAG_DELETE_RECURSIVE
				| CGFLAG_DELETE_IGNORE_MIGRATION);
		if (error) {
			/* store the error, but continue deleting the rest */
			ret = error;
		}
	}
	return ret;
}

/*
 * Unmount the controllers
 */
static int cgroup_config_unmount_controllers(void)
{
	int i;
	int error;

	for (i = 0; i < config_table_index; i++) {
		/*
		 * We ignore failures and ensure that all mounted
		 * containers are unmounted
		 */
		error = umount(config_mount_table[i].mount.path);
		if (error < 0)
			cgroup_dbg("Unmount failed\n");
		error = rmdir(config_mount_table[i].mount.path);
		if (error < 0)
			cgroup_dbg("rmdir failed\n");
	}

	return 0;
}

static int config_validate_namespaces(void)
{
	int i;
	char *namespace = NULL;
	char *mount_path = NULL;
	int j, subsys_count;
	int error = 0;

	pthread_rwlock_wrlock(&cg_mount_table_lock);
	for (i = 0; cg_mount_table[i].name[0] != '\0'; i++) {
		/*
		 * If we get the path in the first run, then we
		 * are good, else we will need to go for two
		 * loops. This should be optimized in the future
		 */
		mount_path = cg_mount_table[i].mount.path;

		if (!mount_path) {
			last_errno = errno;
			error = ECGOTHER;
			goto out_error;
		}

		/*
		 * Setup the namespace for the subsystems having the same
		 * mount point.
		 */
		if (!cg_namespace_table[i]) {
			namespace = NULL;
		} else {
			namespace = cg_namespace_table[i];
			if (!namespace) {
				last_errno = errno;
				error = ECGOTHER;
				goto out_error;
			}
		}

		/*
		 * We want to handle all the subsytems that are mounted
		 * together. So initialize j to start from the next point in
		 * the mount table.
		 */

		j = i + 1;

		/*
		 * Search through the mount table to locate which subsystems
		 * are mounted together.
		 */
		while (!strncmp(cg_mount_table[j].mount.path, mount_path,
							FILENAME_MAX)) {
			if (!namespace && cg_namespace_table[j]) {
				/* In case namespace is not setup, set it up */
				namespace = cg_namespace_table[j];
				if (!namespace) {
					last_errno = errno;
					error = ECGOTHER;
					goto out_error;
				}
			}
			j++;
		}
		subsys_count = j;

		/*
		 * If there is no namespace, then continue on :)
		 */

		if (!namespace) {
			i = subsys_count -  1;
			continue;
		}

		/*
		 * Validate/setup the namespace
		 * If no namespace is specified, copy the namespace we have
		 * stored. If a namespace is specified, confirm if it is
		 * the same as we have stored. If not, we fail.
		 */
		for (j = i; j < subsys_count; j++) {
			if (!cg_namespace_table[j]) {
				cg_namespace_table[j] = strdup(namespace);
				if (!cg_namespace_table[j]) {
					last_errno = errno;
					error = ECGOTHER;
					goto out_error;
				}
			} else if (strcmp(namespace, cg_namespace_table[j])) {
				error = ECGNAMESPACEPATHS;
				goto out_error;
			}
		}
		/* i++ in the for loop will increment it */
		i = subsys_count - 1;
	}
out_error:
	pthread_rwlock_unlock(&cg_mount_table_lock);
	return error;
}

/*
 * Should always be called after cgroup_init() has been called
 *
 * NOT to be called outside the library. Is handled internally
 * when we are looking to  load namespace configurations.
 *
 * This function will order the namespace table in the same
 * fashion as how the mou table is setup.
 *
 * Also it will setup namespaces for all the controllers mounted.
 * In case a controller does not have a namespace assigned to it, it
 * will set it to null.
 */
static int config_order_namespace_table(void)
{
	int i = 0;
	int error = 0;

	pthread_rwlock_wrlock(&cg_mount_table_lock);
	/*
	 * Set everything to NULL
	 */
	for (i = 0; i < CG_CONTROLLER_MAX; i++)
		cg_namespace_table[i] = NULL;

	memset(cg_namespace_table, 0,
			CG_CONTROLLER_MAX * sizeof(cg_namespace_table[0]));

	/*
	 * Now fill up the namespace table looking at the table we have
	 * otherwise.
	 */

	for (i = 0; i < namespace_table_index; i++) {
		int j;
		int flag = 0;
		for (j = 0; cg_mount_table[j].name[0] != '\0'; j++) {
			if (strncmp(config_namespace_table[i].name,
				cg_mount_table[j].name, FILENAME_MAX) == 0) {

				flag = 1;

				if (cg_namespace_table[j]) {
					error = ECGNAMESPACEPATHS;
					goto error_out;
				}

				cg_namespace_table[j] = strdup(
					config_namespace_table[i].mount.path);
				if (!cg_namespace_table[j]) {
					last_errno = errno;
					error = ECGOTHER;
					goto error_out;
				}
			}
		}
		if (!flag)
			return ECGNAMESPACECONTROLLER;
	}
error_out:
	pthread_rwlock_unlock(&cg_mount_table_lock);
	return error;
}

/**
 * Free all memory allocated during cgroup_parse_config(), namely
 * config_cgroup_table.
 */
static void cgroup_free_config(void)
{
	int i;
	if (config_cgroup_table) {
		for (i = 0; i < cgroup_table_index; i++)
			cgroup_free_controllers(
					&config_cgroup_table[i]);
		free(config_cgroup_table);
		config_cgroup_table = NULL;
	}
	config_table_index = 0;
}

/**
 * Applies default permissions/uid/gid to all groups in config file.
 */
static void cgroup_config_apply_default()
{
	int i;
	if (config_cgroup_table) {
		for (i = 0; i < cgroup_table_index; i++) {
			struct cgroup *c = &config_cgroup_table[i];

			if (c->control_dperm == NO_PERMS)
				c->control_dperm = default_group.control_dperm;
			if (c->control_fperm == NO_PERMS)
				c->control_fperm = default_group.control_fperm;
			if (c->control_gid == NO_UID_GID)
				c->control_gid = default_group.control_gid;
			if (c->control_uid == NO_UID_GID)
				c->control_uid = default_group.control_uid;
			if (c->task_fperm == NO_PERMS)
				c->task_fperm = default_group.task_fperm;
			if (c->tasks_gid == NO_UID_GID)
				c->tasks_gid = default_group.tasks_gid;
			if (c->tasks_uid == NO_UID_GID)
				c->tasks_uid = default_group.tasks_uid;
		}
	}
}

static int cgroup_parse_config(const char *pathname)
{
	int ret;

	yyin = fopen(pathname, "re");

	if (!yyin) {
		cgroup_dbg("Failed to open file %s\n", pathname);
		last_errno = errno;
		return ECGOTHER;
	}

	config_cgroup_table = calloc(MAX_CGROUPS, sizeof(struct cgroup));
	if (!config_cgroup_table) {
		ret = ECGFAIL;
		goto err;
	}

	/* Clear all internal variables so this function can be called twice. */
	init_cgroup_table(config_cgroup_table, MAX_CGROUPS);
	memset(config_namespace_table, 0, sizeof(config_namespace_table));
	memset(config_mount_table, 0, sizeof(config_mount_table));
	config_table_index = 0;
	namespace_table_index = 0;
	cgroup_table_index = 0;

	if (!default_group_set) {
		/* init the default cgroup */
		init_cgroup_table(&default_group, 1);
	}

	/*
	 * Parser calls longjmp() on really fatal error (like out-of-memory).
	 */
	ret = setjmp(parser_error_env);
	if (!ret)
		ret = yyparse();
	if (ret) {
		/*
		 * Either yyparse failed or longjmp() was called.
		 */
		cgroup_dbg("Failed to parse file %s\n", pathname);
		ret = ECGCONFIGPARSEFAIL;
		goto err;
	}

err:
	if (yyin)
		fclose(yyin);
	if (ret) {
		cgroup_free_config();
	}
	return ret;
}

int _cgroup_config_compare_groups(const void *p1, const void *p2)
{
	const struct cgroup *g1 = p1;
	const struct cgroup *g2 = p2;

	return strcmp(g1->name, g2->name);
}

static void cgroup_config_sort_groups()
{
	qsort(config_cgroup_table, cgroup_table_index, sizeof(struct cgroup),
			_cgroup_config_compare_groups);
}

/*
 * The main function which does all the setup of the data structures
 * and finally creates the cgroups
 */
int cgroup_config_load_config(const char *pathname)
{
	int error;
	int namespace_enabled = 0;
	int mount_enabled = 0;
	int ret;

	ret = cgroup_parse_config(pathname);
	if (ret != 0)
		return ret;

	namespace_enabled = (config_namespace_table[0].name[0] != '\0');
	mount_enabled = (config_mount_table[0].name[0] != '\0');

	/*
	 * The configuration should have namespace or mount, not both.
	 */
	if (namespace_enabled && mount_enabled) {
		free(config_cgroup_table);
		return ECGMOUNTNAMESPACE;
	}

	error = cgroup_config_mount_fs();
	if (error)
		goto err_mnt;

	error = cgroup_init();
	if (error == ECGROUPNOTMOUNTED && cgroup_table_index == 0) {
		/*
		 * The config file seems to be empty.
		 */
		error = 0;
		goto err_mnt;
	}
	if (error)
		goto err_mnt;

	/*
	 * The very first thing is to sort the namespace table. If we fail
	 * we unmount everything and get out.
	 */

	error = config_order_namespace_table();
	if (error)
		goto err_mnt;

	error = config_validate_namespaces();
	if (error)
		goto err_mnt;

	cgroup_config_apply_default();
	error = cgroup_config_create_groups();
	cgroup_dbg("creating all cgroups now, error=%d\n", error);
	if (error)
		goto err_grp;

	cgroup_free_config();

	return 0;
err_grp:
	cgroup_config_destroy_groups();
err_mnt:
	cgroup_config_unmount_controllers();
	cgroup_free_config();
	return error;
}

/* unmounts given mount, but only if it is empty */
static int cgroup_config_try_unmount(struct cg_mount_table_s *mount_info)
{
	char *controller, *controller_list;
	struct cg_mount_point *mount = &(mount_info->mount);
	void *handle = NULL;
	int ret, lvl;
	struct cgroup_file_info info;
	char *saveptr = NULL;

	/* parse the first controller name from list of controllers */
	controller_list = strdup(mount_info->name);
	if (!controller_list) {
		last_errno = errno;
		return ECGOTHER;
	}
	controller = strtok_r(controller_list, ",", &saveptr);
	if (!controller) {
		free(controller_list);
		return ECGINVAL;
	}

	/* check if the hierarchy is empty */
	ret = cgroup_walk_tree_begin(controller, "/", 0, &handle, &info, &lvl);
	free(controller_list);
	if (ret == ECGCONTROLLEREXISTS)
		return 0;
	if (ret)
		return ret;
	/* skip the first found directory, it's '/' */
	ret = cgroup_walk_tree_next(0, &handle, &info, lvl);
	/* find any other subdirectory */
	while (ret == 0) {
		if (info.type == CGROUP_FILE_TYPE_DIR)
			break;
		ret = cgroup_walk_tree_next(0, &handle, &info, lvl);
	}
	cgroup_walk_tree_end(&handle);
	if (ret == 0) {
		cgroup_dbg("won't unmount %s: hieararchy is not empty\n",
				mount_info->name);
		return 0; /* the hieararchy is not empty */
	}
	if (ret != ECGEOF)
		return ret;


	/*
	 * ret must be ECGEOF now = there is only root group in the hierarchy
	 * -> unmount all mount points.
	 */
	ret = 0;
	while (mount) {
		int err;
		cgroup_dbg("unmounting %s at %s\n", mount_info->name,
				mount->path);
		err = umount(mount->path);

		if (err && !ret) {
			ret = ECGOTHER;
			last_errno = errno;
		}
		mount = mount->next;
	}
	return ret;
}

int cgroup_config_unload_config(const char *pathname, int flags)
{
	int ret, i, error;
	int namespace_enabled = 0;
	int mount_enabled = 0;

	cgroup_dbg("cgroup_config_unload_config: parsing %s\n", pathname);
	ret = cgroup_parse_config(pathname);
	if (ret)
		goto err;

	namespace_enabled = (config_namespace_table[0].name[0] != '\0');
	mount_enabled = (config_mount_table[0].name[0] != '\0');
	/*
	 * The configuration should have namespace or mount, not both.
	 */
	if (namespace_enabled && mount_enabled) {
		free(config_cgroup_table);
		return ECGMOUNTNAMESPACE;
	}

	ret = config_order_namespace_table();
	if (ret)
		goto err;

	ret = config_validate_namespaces();
	if (ret)
		goto err;

	/*
	 * Delete the groups in reverse order, i.e. subgroups first, then
	 * parents.
	 */
	cgroup_config_sort_groups();
	for (i = cgroup_table_index-1; i >= 0; i--) {
		struct cgroup *cgroup = &config_cgroup_table[i];
		cgroup_dbg("removing %s\n", pathname);
		error = cgroup_delete_cgroup_ext(cgroup, flags);
		if (error && !ret) {
			/* store the error, but continue deleting the rest */
			ret = error;
		}
	}

	if (mount_enabled) {
		for (i = 0; i < config_table_index; i++) {
			struct cg_mount_table_s *m = &(config_mount_table[i]);
			cgroup_dbg("unmounting %s\n", m->name);
			error = cgroup_config_try_unmount(m);
			if (error && !ret)
				ret = error;
		}
	}

err:
	cgroup_free_config();
	return ret;
}

static int cgroup_config_unload_controller(const struct cgroup_mount_point *mount_info)
{
	int ret, error;
	struct cgroup *cgroup = NULL;
	struct cgroup_controller *cgc = NULL;
	char path[FILENAME_MAX];
	void *handle;

	cgroup = cgroup_new_cgroup(".");
	if (cgroup == NULL)
		return ECGFAIL;

	cgc = cgroup_add_controller(cgroup, mount_info->name);
	if (cgc == NULL) {
		ret = ECGFAIL;
		goto out_error;
	}

	ret = cgroup_delete_cgroup_ext(cgroup, CGFLAG_DELETE_RECURSIVE);
	if (ret != 0)
		goto out_error;

	/* unmount everything */
	ret = cgroup_get_subsys_mount_point_begin(mount_info->name, &handle,
			path);
	while (ret == 0) {
		error = umount(path);
		if (error) {
			last_errno = errno;
			ret = ECGOTHER;
			goto out_error;
		}
		ret = cgroup_get_subsys_mount_point_next(&handle, path);
	}
	cgroup_get_subsys_mount_point_end(&handle);
	if (ret == ECGEOF)
		ret = 0;
out_error:
	if (cgroup)
		cgroup_free(&cgroup);
	return ret;
}

int cgroup_unload_cgroups(void)
{
	int error = 0;
	void *ctrl_handle;
	int ret = 0;
	char *curr_path = NULL;
	struct cgroup_mount_point info;

	error = cgroup_init();

	if (error) {
		ret = error;
		goto out_error;
	}

	error = cgroup_get_controller_begin(&ctrl_handle, &info);
	while (error == 0) {
		if (!curr_path || strcmp(info.path, curr_path) != 0) {
			if (curr_path)
				free(curr_path);

			curr_path = strdup(info.path);
			if (!curr_path)
				goto out_errno;

			error = cgroup_config_unload_controller(&info);
			if (error) {
				/* remember the error and continue unloading
				 * the rest */
				ret = error;
				error = 0;
			}
		}

		error = cgroup_get_controller_next(&ctrl_handle, &info);
	}
	if (error == ECGEOF)
		error = 0;
	if (error)
		ret = error;
out_error:
	if (curr_path)
		free(curr_path);
	cgroup_get_controller_end(&ctrl_handle);
	return ret;

out_errno:
	last_errno = errno;
	cgroup_get_controller_end(&ctrl_handle);
	return ECGOTHER;
}

/**
 * Defines the default group. The parser puts content of 'default { }' to
 * topmost group in config_cgroup_table. This function copies the permissions
 * from it to our default cgroup.
 */
int cgroup_config_define_default(void)
{
	struct cgroup *config_cgroup =
			&config_cgroup_table[cgroup_table_index];

	init_cgroup_table(&default_group, 1);
	if (config_cgroup->control_dperm != NO_PERMS)
		default_group.control_dperm = config_cgroup->control_dperm;
	if (config_cgroup->control_fperm != NO_PERMS)
		default_group.control_fperm = config_cgroup->control_fperm;
	if (config_cgroup->control_gid != NO_UID_GID)
		default_group.control_gid = config_cgroup->control_gid;
	if (config_cgroup->control_uid != NO_UID_GID)
		default_group.control_uid = config_cgroup->control_uid;
	if (config_cgroup->task_fperm != NO_PERMS)
		default_group.task_fperm = config_cgroup->task_fperm;
	if (config_cgroup->tasks_gid != NO_UID_GID)
		default_group.tasks_gid = config_cgroup->tasks_gid;
	if (config_cgroup->tasks_uid != NO_UID_GID)
		default_group.tasks_uid = config_cgroup->tasks_uid;

	/*
	 * Reset all changes made by 'default { }' to the topmost group so it
	 * can be used by following 'group { }'.
	 */
	init_cgroup_table(config_cgroup, 1);
	return 0;
}

int cgroup_config_set_default(struct cgroup *new_default)
{
	if (!new_default)
		return ECGINVAL;

	init_cgroup_table(&default_group, 1);

	default_group.control_dperm = new_default->control_dperm;
	default_group.control_fperm = new_default->control_fperm;
	default_group.control_gid = new_default->control_gid;
	default_group.control_uid = new_default->control_uid;
	default_group.task_fperm = new_default->task_fperm;
	default_group.tasks_gid = new_default->tasks_gid;
	default_group.tasks_uid = new_default->tasks_uid;
	default_group_set = 1;

	return 0;
}
