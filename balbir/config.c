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
 * 	1. Implement our own hashing scheme
 * 	2. Add namespace support
 * 	3. Add support for parsing cgroup filesystem and creating a
 * 	   config out of it.
 *
 * Code initiated and designed by Balbir Singh. All faults are most likely
 * his mistake.
 */

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <grp.h>
#include <libcg.h>
#include <limits.h>
#include <pwd.h>
#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>

extern FILE *yyin;
extern int yyparse(void);
extern int yydebug;
extern int line_no;
extern int verbose;

struct hsearch_data group_hash;
struct list_of_names *group_list;
struct mount_table *mount_table;

const char library_ver[] = "0.01";
const char cg_filesystem[] = "cgroup";

struct cg_group *current_group;

const char *cg_controller_names[] = {
	"cpu",
	NULL,
};

/*
 * File traversal routines require the maximum number of open file
 * descriptors to be specified
 */
const int cg_max_openfd = 20;

/*
 * Insert the group into the list of group names we maintain. This helps
 * us cleanup nicely
 */
int cg_insert_into_group_list(const char *name)
{
	struct list_of_names *tmp, *curr;

	tmp = malloc(sizeof(*tmp));
	if (!tmp)
		return 0;
	tmp->next = NULL;
	tmp->name = (char *)name;

	if (!group_list) {
		group_list = tmp;
		return 1;
	}
	curr = group_list;
	while (curr->next)
		curr = curr->next;

	curr->next = tmp;
	return 1;
}

/*
 * Cleanup the group list. We walk the group list and free the entries in the
 * hash tables and controller specific entries.
 */
int cg_cleanup_group_list(void)
{
	struct list_of_names *curr = group_list, *tmp;
	ENTRY item, *found_item;
	int ret;
	struct cg_group *cg_group;

	while (curr) {
		tmp = curr;
		curr = curr->next;
		item.key = tmp->name;
		ret = hsearch_r(item, FIND, &found_item, &group_hash);
		if (!ret) {
			printf("Most likely a bug in the code\n");
			continue;
		}
		/*
		 * Free the name and it's value
		 */
		free(tmp->name);
		cg_group = (struct cg_group *)found_item->data;
		/*
		 * Controller specific cleanup
		 */
		if (cg_group->cpu_config.shares)
			free(cg_group->cpu_config.shares);

		free(found_item->data);
	}

	return 1;
}

/*
 * Find and walk the mount_table structures to find the specified controller
 * name. This routine is *NOT* thread safe.
 */
struct mount_table *cg_find_mount_info(const char *controller_name)
{
	struct mount_table *curr = mount_table;
	char *str;

	while (curr) {
		str = curr->options;
		if (!str)
			return NULL;

		str = strtok(curr->options, ",");
		do {
			if (!strncmp(str, controller_name, strlen(str)))
				return curr;
			str = strtok(NULL, ",");
		} while(str);
		curr = curr->next;
	}
	return NULL;
}

int cg_cpu_controller_settings(struct cg_group *cg_group,
				const char *group_path)
{
	int ret = 1;
	char *shares_file;

	shares_file = malloc(strlen(group_path) + strlen("/cpu.shares") + 1);
	if (!shares_file)
		return 0;

	strncpy(shares_file, group_path, strlen(group_path));
	shares_file = strncat(shares_file, "/cpu.shares",
					strlen("/cpu.shares"));
	dbg("shares file is %s\n", shares_file);
	if (cg_group->cpu_config.shares) {
		FILE *fd = fopen(shares_file, "rw+");
		if (!fd)
			goto cleanup_shares;
		/*
		 * Assume that one write will do it for us
		 */
		fwrite(cg_group->cpu_config.shares,
			strlen(cg_group->cpu_config.shares), 1, fd);
		fclose(fd);
	}
cleanup_shares:
	free(shares_file);
	return ret;
}

int cg_controller_handle_option(struct cg_group *cg_group,
				const char *cg_controller_name,
				const char *group_path)
{
	int ret = 0;
	if (!strncmp(cg_controller_name, "cpu", strlen("cpu"))) {
		ret = cg_cpu_controller_settings(cg_group, group_path);
	} else
		assert(0);
	return ret;
}

int cg_create_group(struct cg_group *cg_group)
{
	int i, ret;
	struct mount_table *mount_info;
	char *group_path, *tasks_file, *shares_file;

	dbg("found group %s\n", cg_group->name);

	for (i = 0; cg_controller_names[i]; i++) {

		/*
		 * Find the mount point related information
		 */
		mount_info = cg_find_mount_info(cg_controller_names[i]);
		dbg("mount_info for controller %s:%s\n",
			mount_info->mount_point, cg_controller_names[i]);
		if (!mount_info)
			return 0;

		/*
		 * TODO: Namespace support is most likely going to be
		 * plugged in here
		 */

		/*
		 * Find the path to the group directory
		 */
		group_path = cg_build_group_path(cg_group, mount_info);
		if (!group_path)
			goto cleanup_group;

		/*
		 * Create the specified directory. Errors are ignored.
		 * If the directory already exists, we are most likely
		 * OK
		 */
		ret = cg_make_directory(cg_group, group_path);
		if (!ret && (errno != EEXIST))
			goto cleanup_dir;

		/*
		 * Find the tasks file, should probably be encapsulated
		 * like we encapsulate cg_build_group_path
		 */
		tasks_file = malloc(strlen(group_path) + strlen("/tasks") + 1);
		if (!tasks_file)
			goto cleanup_dir;
		strncpy(tasks_file, group_path, strlen(group_path));
		tasks_file = strncat(tasks_file, "/tasks", strlen("/tasks"));
		dbg("tasks file is %s\n", tasks_file);

		/*
		 * Assign task file ownership
		 */
		ret = chown(tasks_file, cg_group->tasks_uid,
				cg_group->tasks_gid);
		if (ret < 0)
			goto cleanup_perm;

		/*
		 * Controller specific work, errors are not fatal.
		 */
		cg_controller_handle_option(cg_group, cg_controller_names[i],
						group_path);
		free(tasks_file);
		free(group_path);
	}
	return 1;
cleanup_perm:
	rmdir(group_path);
cleanup_dir:
	free(group_path);
cleanup_group:
	return 0;
}

/*
 * Go ahead and create the groups in the filesystem. This routine will need
 * to be revisited everytime new controllers are added.
 */
int cg_create_groups(void)
{
	struct list_of_names *curr = group_list, *tmp;
	ENTRY item, *found_item;
	struct cg_group *cg_group;
	int ret = 1;

	while (curr) {
		tmp = curr;
		curr = curr->next;
		item.key = tmp->name;
		ret = hsearch_r(item, FIND, &found_item, &group_hash);
		if (!ret)
			return 0;
		cg_group = (struct cg_group *)found_item->data;
		ret = cg_create_group(cg_group);
		if (!ret)
			break;
	}

	return ret;
}

/*
 * Go ahead and create the groups in the filesystem. This routine will need
 * to be revisited everytime new controllers are added.
 */
int cg_destroy_groups(void)
{
	struct list_of_names *curr = group_list, *tmp;
	ENTRY item, *found_item;
	struct cg_group *cg_group;
	int ret;
	struct mount_table *mount_info;
	char *group_path;

	while (curr) {
		tmp = curr;
		curr = curr->next;
		item.key = tmp->name;
		ret = hsearch_r(item, FIND, &found_item, &group_hash);
		if (!ret)
			return 0;
		cg_group = (struct cg_group *)found_item->data;
		mount_info = cg_find_mount_info("cpu");
		dbg("mount_info for cpu %s\n", mount_info->mount_point);
		if (!mount_info)
			return 0;
		group_path = malloc(strlen(mount_info->mount_point) +
				strlen(cg_group->name) + 2);
		if (!group_path)
			return 0;
		strncpy(group_path, mount_info->mount_point,
					strlen(mount_info->mount_point) + 1);
		dbg("group_path is %s\n", group_path);
		strncat(group_path, "/", strlen("/"));
		strncat(group_path, cg_group->name, strlen(cg_group->name));
		dbg("group_path is %s\n", group_path);
		/*
		 * We might strategically add migrate tasks here, so that
		 * rmdir is successful. TODO: Later
		 */
		ret = rmdir(group_path);
		if (ret < 0)
			goto err;
	}

	return 1;
err:
	free(group_path);
	return 0;
}
/*
 * The cg_get_current_group routine is used by the parser to figure out
 * the current group that is being built and fill it in with the information
 * as it parses through the configuration file
 */
struct cg_group *cg_get_current_group(void)
{
	if (!current_group)
		current_group = calloc(1, sizeof(*current_group));

	return current_group;
}

/*
 * This routine should be invoked when the current group being parsed is
 * completely parsed
 */
void cg_put_current_group(void)
{
	/*
	 * NOTE: we do not free the group, the group is installed into the
	 * hash table, the cleanup routine will do the freeing of the group
	 */
	current_group = NULL;
}

int cg_insert_group(const char *group_name)
{
	struct cg_group *cg_group = cg_get_current_group();
	ENTRY item, *found_item;
	int ret;

	if (!cg_group)
		return 0;
	/*
	 * Dont' copy the name over, just point to it
	 */
	cg_group->name = (char *)group_name;
	item.key = (char *)group_name;
	item.data = cg_group;
	dbg("Inserting %s into hash table\n", group_name);
	ret = hsearch_r(item, ENTER, &found_item, &group_hash);
	if (!ret) {
		if (found_item)
			errno = EEXIST;
		errno = ENOMEM;
		goto err;
	}
	ret = cg_insert_into_group_list(group_name);
	if (!ret)
		goto err;
	dbg("Inserted %s into hash and list\n", group_name);
	cg_put_current_group();
	return 1;
err:
	cg_cleanup_group_list();
	return 0;
}

/*
 * Because of the way parsing works (bottom-up, shift-reduce), we don't
 * know the name of the controller yet. Compilers build an AST and use
 * a symbol table to deal with this problem. This code does simple things
 * like concatinating strings and passing them upwards. This routine is
 * *NOT* thread safe.
 *
 * This code will need modification everytime new controller support is
 * added.
 */
int cg_parse_controller_options(char *controller, char *name_value)
{
	struct cg_group *cg_group = cg_get_current_group();
	char *name, *value;

	if (!cg_group)
		return 0;

	if (!strncmp(controller, "cpu", strlen("cpu"))) {
		name = strtok(name_value, " ");
		value = strtok(NULL, " ");
		if (!strncmp(name, "cpu.shares", strlen("cpu.shares")))
			cg_group->cpu_config.shares = strdup(value);
		else {
			free(controller);
			free(name_value);
			return 0;
		}
		dbg("cpu name %s value %s\n", name, value);
	} else {
		return 0;
	}
	free(controller);
	free(name_value);
	return 1;
}

/*
 * Convert the uid/gid field and supplied value to appropriate task
 * permissions. This routine is *NOT* thread safe.
 */
int cg_group_task_perm(char *perm_type, char *value)
{
	struct cg_group *cg_group = cg_get_current_group();
	struct passwd *pw;
	struct group *group;
	long val = atoi(value);
	if (!strncmp(perm_type, "uid", strlen("uid"))) {
		if (!val) {	/* We got the identifier as a name */
			pw = getpwnam(value);
			if (!pw) {
				free(perm_type);
				free(value);
				return 0;
			} else {
				cg_group->tasks_uid = pw->pw_uid;
			}
		} else {
			cg_group->tasks_uid = val;
		}
		dbg("TASKS %s: %d\n", perm_type, cg_group->tasks_uid);
	}
	if (!strncmp(perm_type, "gid", strlen("gid"))) {
		if (!val) {	/* We got the identifier as a name */
			group = getgrnam(value);
			if (!group) {
				free(perm_type);
				free(value);
				return 0;
			} else {
				cg_group->tasks_gid = group->gr_gid;
			}
		} else {
			cg_group->tasks_gid = val;
		}
		dbg("TASKS %s: %d\n", perm_type, cg_group->tasks_gid);
	}
	free(perm_type);
	free(value);
	return 1;
}

int cg_group_admin_perm(char *perm_type, char *value)
{
	struct cg_group *cg_group = cg_get_current_group();
	struct passwd *pw;
	struct group *group;
	long val = atoi(value);
	if (!strncmp(perm_type, "uid", strlen("uid"))) {
		if (!val) {	/* We got the identifier as a name */
			pw = getpwnam(value);
			if (!pw) {
				free(perm_type);
				free(value);
				return 0;
			} else {
				cg_group->admin_uid = pw->pw_uid;
			}
		} else {
			cg_group->admin_uid = val;
		}
		dbg("ADMIN %s: %d\n", perm_type, cg_group->admin_uid);
	}
	if (!strncmp(perm_type, "gid", strlen("gid"))) {
		if (!val) {	/* We got the identifier as a name */
			group = getgrnam(value);
			if (!group) {
				free(perm_type);
				free(value);
				return 0;
			} else {
				cg_group->admin_gid = group->gr_gid;
			}
		} else {
			cg_group->admin_gid = val;
		}
		dbg("ADMIN %s: %d\n", perm_type,
							cg_group->admin_gid);
	}
	free(perm_type);
	free(value);
	return 1;
}

/*
 * We maintain a hash table. The group hash table maintains the parameters for
 * each group, including the parameters for each controller
 *
 * TODO: Make the initialization a run time configuration parameter
 */
int cg_init_group_and_mount_info(void)
{
	int ret;

	group_list = NULL;
	mount_table = NULL;
	current_group = NULL;

	ret = hcreate_r(MAX_GROUP_ELEMENTS, &group_hash);
	if (!ret)
		return 0;
	return 1;
}

/*
 * This routine should be called only once all elements of the hash table have
 * been freed. Otherwise, we'll end up with a memory leak.
 */
void cg_destroy_group_and_mount_info(void)
{
	hdestroy_r(&group_hash);
	group_list = NULL;
	mount_table = NULL;
	current_group = NULL;
}

/*
 * Insert a name, mount_point pair into the mount_table data structure
 * TODO: Validate names and mount points
 */
int cg_insert_into_mount_table(const char *name, const char *mount_point)
{
	struct mount_table *tmp, *prev = mount_table;

	while (prev && prev->next != NULL &&
		(strncmp(mount_point, prev->mount_point, strlen(mount_point))))
		prev = prev->next;

	if (prev &&
		!(strncmp(mount_point, prev->mount_point, strlen(mount_point)))) {
		prev->options = realloc(prev->options, strlen(prev->options)
							+ strlen(name) + 2);
		if (!prev->options)
			return 0;
		strncat(prev->options, ",", strlen(","));
		strncat(prev->options, name, strlen(name));
		dbg("options %s: mount_point %s\n", prev->options,
							prev->mount_point);
		return 1;
	}

	tmp = malloc(sizeof(*tmp));
	if (!tmp)
		return 0;

	tmp->next = NULL;
	tmp->mount_point = (char *)mount_point;
	tmp->options = (char *)name;
	dbg("options %s: mount_point %s\n", tmp->options, tmp->mount_point);

	if (!prev) {
		mount_table = tmp;
		return 1;
	} else {
		prev->next = tmp;
	}

	return 1;
}

void cg_cleanup_mount_table(void)
{
	struct mount_table *curr = mount_table, *tmp;

	while (curr) {
		tmp = curr;
		curr = curr->next;
		tmp->next = NULL;

		/*
		 * Some of this data might have been allocated by the lexer
		 * while parsing tokens
		 */
		free(tmp->mount_point);
		free(tmp->options);

		free(tmp);
	}
}

int cg_load_config(const char *pathname)
{
	yyin = fopen(pathname, "rw");
	if (!yyin) {
		dbg("Failed to open file %s\n", pathname);
		return 0;
	}

	if (!cg_init_group_and_mount_info())
		return 0;

	if (yyparse() != 0) {
		dbg("Failed to parse file %s\n", pathname);
		return 0;
	}

	if (!cg_mount_controllers())
		goto err_mnt;
	if (!cg_create_groups())
		goto err_grp;

	fclose(yyin);
	return 1;
err_grp:
	cg_destroy_groups();
	cg_cleanup_group_list();
err_mnt:
	cg_unmount_controllers();
	cg_cleanup_mount_table();
	fclose(yyin);
	return 0;
}

void cg_unload_current_config(void)
{
	cg_destroy_groups();
	cg_cleanup_group_list();
	cg_unmount_controllers();
	cg_cleanup_mount_table();
	cg_destroy_group_and_mount_info();
}

int main(int argc, char *argv[])
{
	int c;
	char filename[PATH_MAX];
	int ret;

	if (argc < 2) {
		fprintf(stderr, "usage is %s <option> <config file>\n",
			argv[0]);
		exit(2);
	}

	while ((c = getopt(argc, argv, "l:ur:")) > 0) {
		switch (c) {
		case 'u':
			cg_unload_current_config();
			break;
		case 'r':
			cg_unload_current_config();
			/* FALLTHROUGH */
		case 'l':
			strncpy(filename, optarg, PATH_MAX);
			ret = cg_load_config(filename);
			if (!ret)
				exit(3);
			break;
		default:
			fprintf(stderr, "Invalid command line option\n");
			break;
		}
	}

	cg_destroy_group_and_mount_info();
}
