/*
 * Copyright IBM Corporation. 2007
 *
 * Author:	Dhaval Giani <dhaval@linux.vnet.ibm.com>
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
 * TODOs:
 *	1. Convert comments to Docbook style.
 *	2. Add more APIs for the control groups.
 *	3. Handle the configuration related APIs.
 *	4. Error handling.
 *
 * Code initiated and designed by Dhaval Giani. All faults are most likely
 * his mistake.
 */

#include <errno.h>
#include <libcgroup.h>
#include <libcgroup-internal.h>
#include <mntent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <fts.h>

#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION 0.01
#endif

#define VERSION(ver)	#ver

/*
 * Remember to bump this up for major API changes.
 */
const static char cg_version[] = VERSION(PACKAGE_VERSION);

struct cg_mount_table_s cg_mount_table[CG_CONTROLLER_MAX];

static int cg_chown_file(FTS *fts, FTSENT *ent, uid_t owner, gid_t group)
{
	int ret = 0;
	const char *filename = fts->fts_path;
	dbg("seeing file %s\n", filename);
	switch (ent->fts_info) {
	case FTS_ERR:
		errno = ent->fts_errno;
		break;
	case FTS_D:
	case FTS_DC:
	case FTS_NSOK:
	case FTS_NS:
	case FTS_DNR:
	case FTS_DP:
	case FTS_F:
	case FTS_DEFAULT:
		ret = chown(filename, owner, group);
		break;
	}
	return ret;
}

/*
 * TODO: Need to decide a better place to put this function.
 */
static int cg_chown_recursive(char **path, uid_t owner, gid_t group)
{
	int ret = 0;
	dbg("path is %s\n", *path);
	FTS *fts = fts_open(path, FTS_PHYSICAL | FTS_NOCHDIR |
				FTS_NOSTAT, NULL);
	while (1) {
		FTSENT *ent;
		ent = fts_read(fts);
		if (!ent) {
			dbg("fts_read failed\n");
			break;
		}
		ret = cg_chown_file(fts, ent, owner, group);
	}
	fts_close(fts);
	return ret;
}

/**
 * cgroup_init(), initializes the MOUNT_POINT.
 * This code is not currently thread safe (hint: getmntent is not thread safe).
 * This API is likely to change in the future to push state back to the caller
 * to achieve thread safety. The code currently supports just one mount point.
 * Complain if the cgroup filesystem controllers are bound to different mount
 * points.
 */
int cgroup_init()
{
	FILE *proc_mount;
	struct mntent *ent, *found_ent = NULL;
	int found_mnt = 0;
	int ret = 0;
	char *mntent_tok;
	static char *controllers[CG_CONTROLLER_MAX];
	FILE *proc_cgroup;
	char subsys_name[FILENAME_MAX];
	int hierarchy, num_cgroups, enabled;
	int i=0;
	char *mntopt;
	int err;

	proc_cgroup = fopen("/proc/cgroups", "r");

	if (!proc_cgroup)
		return EIO;

	/*
	 * The first line of the file has stuff we are not interested in.
	 * So just read it and discard the information.
	 *
	 * XX: fix the size for fgets
	 */
	fgets(subsys_name, FILENAME_MAX, proc_cgroup);
	while (!feof(proc_cgroup)) {
		err = fscanf(proc_cgroup, "%s %d %d %d", subsys_name,
				&hierarchy, &num_cgroups, &enabled);
		if (err < 0)
			break;
		controllers[i] = (char *)malloc(strlen(subsys_name));
		strcpy(controllers[i], subsys_name);
		i++;
	}
	controllers[i] = NULL;
	fclose(proc_cgroup);

	proc_mount = fopen("/proc/mounts", "r");
	if (proc_mount == NULL) {
		return EIO;
	}

	while ((ent = getmntent(proc_mount)) != NULL) {
		if (!strncmp(ent->mnt_type, "cgroup", strlen("cgroup"))) {
			for (i = 0; controllers[i] != NULL; i++) {
				mntopt = hasmntopt(ent, controllers[i]);
				if (mntopt &&
					strcmp(mntopt, controllers[i]) == 0) {
					dbg("matched %s:%s\n", mntopt,
						controllers[i]);
					strcpy(cg_mount_table[found_mnt].name,
						controllers[i]);
					strcpy(cg_mount_table[found_mnt].path,
						ent->mnt_dir);
					dbg("Found cgroup option %s, "
						" count %d\n",
						ent->mnt_opts, found_mnt);
					found_mnt++;
				}
			}
		}
	}

	if (!found_mnt) {
		cg_mount_table[0].name[0] = '\0';
		return ECGROUPNOTMOUNTED;
	}

	found_mnt++;
	cg_mount_table[found_mnt].name[0] = '\0';


	fclose(proc_mount);
	return ret;
}

static char **get_mounted_controllers(char *mountpoint)
{
	char **controllers;
	int i, j;

	i = 0;
	j = 0;

	controllers = (char **) malloc(sizeof(char *) * CG_CONTROLLER_MAX);

	for (i = 0; i < CG_CONTROLLER_MAX && cg_mount_table[i].name != NULL;
									i++) {
		if (strcmp(cg_mount_table[i].name, mountpoint) == 0) {
			controllers[j] = (char *)malloc(sizeof(char) *
								FILENAME_MAX);
			strcpy(controllers[j], cg_mount_table[i].name);
			j++;
		}
	}
	controllers[j] = (char *)malloc(sizeof(char) * FILENAME_MAX);
	controllers[j][0] = '\0';

	return controllers;
}

static int cg_test_mounted_fs()
{
	FILE *proc_mount;
	struct mntent *ent;

	proc_mount = fopen("/proc/mounts", "r");
	if (proc_mount == NULL) {
		return -1;
	}
	ent = getmntent(proc_mount);

	while (strcmp(ent->mnt_type, "cgroup") !=0) {
		ent = getmntent(proc_mount);
		if (ent == NULL)
			return 0;
	}
	fclose(proc_mount);
	return 1;
}

static inline pid_t cg_gettid()
{
	return syscall(__NR_gettid);
}

static char* cg_build_path(char *name, char *path, char *type)
{
	int i;
	for (i = 0; cg_mount_table[i].name[0] != '\0'; i++) {
		if (strcmp(cg_mount_table[i].name, type) == 0) {
			strcpy(path, cg_mount_table[i].path);
			strcat(path, "/");
			strcat(path, name);
			strcat(path, "/");
			return path;
		}
	}
	return NULL;
}

/** cgroup_attach_task_pid is used to assign tasks to a cgroup.
 *  struct cgroup *cgroup: The cgroup to assign the thread to.
 *  pid_t tid: The thread to be assigned to the cgroup.
 *
 *  returns 0 on success.
 *  returns ECGROUPNOTOWNER if the caller does not have access to the cgroup.
 *  returns ECGROUPNOTALLOWED for other causes of failure.
 */
int cgroup_attach_task_pid(struct cgroup *cgroup, pid_t tid)
{
	char path[FILENAME_MAX];
	FILE *tasks;
	int i;

	if(!cgroup)
	{
		for(i = 0; i < CG_CONTROLLER_MAX &&
				cg_mount_table[i].name[0]!='\0'; i++) {
			if (!cg_build_path(cgroup->name, path, NULL))
				continue;
			strcat(path, "/tasks");

			tasks = fopen(path, "w");
			if (!tasks) {
				switch (errno) {
				case EPERM:
					return ECGROUPNOTOWNER;
				default:
					return ECGROUPNOTALLOWED;
				}
			}
			fprintf(tasks, "%d", tid);
			fclose(tasks);
		}
	} else {
		for( i = 0; i <= CG_CONTROLLER_MAX &&
				cgroup->controller[i] != NULL ; i++) {
			if (!cg_build_path(cgroup->name, path,
					cgroup->controller[i]->name))
				continue;

			strcat(path, "/tasks");

			tasks = fopen(path, "w");
			if (!tasks) {
				switch (errno) {
				case EPERM:
					return ECGROUPNOTOWNER;
				default:
					return ECGROUPNOTALLOWED;
				}
			}
			fprintf(tasks, "%d", tid);
			fclose(tasks);
		}
	}
	return 0;

}

/** cgroup_attach_task is used to attach the current thread to a cgroup.
 *  struct cgroup *cgroup: The cgroup to assign the current thread to.
 *
 *  See cg_attach_task_pid for return values.
 */
int cgroup_attach_task(struct cgroup *cgroup)
{
	pid_t tid = cg_gettid();
	int error;

	error = cgroup_attach_task_pid(cgroup, tid);

	return error;
}

/*
 * create_control_group()
 * This is the basic function used to create the control group. This function
 * just makes the group. It does not set any permissions, or any control values.
 * The argument path is the fully qualified path name to make it generic.
 */
static int cg_create_control_group(char *path)
{
	int error;
	if (!cg_test_mounted_fs())
		return ECGROUPNOTMOUNTED;
	error = mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	if (error) {
		switch(errno) {
		case EPERM:
			return ECGROUPNOTOWNER;
		default:
			return ECGROUPNOTALLOWED;
		}
	}
	return error;
}

/*
 * set_control_value()
 * This is the low level function for putting in a value in a control file.
 * This function takes in the complete path and sets the value in val in that
 * file.
 */
static int cg_set_control_value(char *path, char *val)
{
	int error;
	FILE *control_file;
	if (!cg_test_mounted_fs())
		return ECGROUPNOTMOUNTED;

	control_file = fopen(path, "a");

	if (!control_file) {
		if (errno == EPERM) {
			/*
			 * We need to set the correct error value, does the
			 * group exist but we don't have the subsystem
			 * mounted at that point, or is it that the group
			 * does not exist. So we check if the tasks file
			 * exist. Before that, we need to extract the path.
			 */
			int len = strlen(path);

			while (*(path+len) != '/')
				len--;
			*(path+len+1) = '\0';
			strcat(path, "tasks");
			control_file = fopen(path, "r");
			if (!control_file) {
				if (errno == ENOENT)
					return ECGROUPSUBSYSNOTMOUNTED;
			}
			fclose(control_file);
			return ECGROUPNOTALLOWED;
		}
		return errno;
	}

	fprintf(control_file, "%s", val);
	fclose(control_file);
	return 0;
}

/** cgroup_modify_cgroup modifies the cgroup control files.
 * struct cgroup *cgroup: The name will be the cgroup to be modified.
 * The values will be the values to be modified, those not mentioned
 * in the structure will not be modified.
 *
 * The uids cannot be modified yet.
 *
 * returns 0 on success.
 *
 */

int cgroup_modify_cgroup(struct cgroup *cgroup)
{
	char path[FILENAME_MAX], base[FILENAME_MAX];
	int i;
	int error;

	for (i = 0; i < CG_CONTROLLER_MAX && cgroup->controller[i];
						i++, strcpy(path, base)) {
		int j;
		if (!cg_build_path(cgroup->name, base,
			cgroup->controller[i]->name))
			continue;
		for(j = 0; j < CG_NV_MAX &&
			cgroup->controller[i]->values[j];
			j++, strcpy(path, base)) {
			strcat(path, cgroup->controller[i]->values[j]->name);
			error = cg_set_control_value(path,
				cgroup->controller[i]->values[j]->value);
			if (error)
				goto err;
		}
	}
	return 0;
err:
	return error;

}

/** cgroup_create_cgroup creates a new control group.
 * struct cgroup *cgroup: The control group to be created
 *
 * returns 0 on success. We recommend calling cg_delete_cgroup
 * if this routine fails. That should do the cleanup operation.
 */
int cgroup_create_cgroup(struct cgroup *cgroup, int ignore_ownership)
{
	char *fts_path[2], base[FILENAME_MAX], *path;
	int i, j, k;
	int error = 0;

	fts_path[0] = (char *)malloc(FILENAME_MAX);
	if (!fts_path[0])
		return ENOMEM;
	fts_path[1] = NULL;
	path = fts_path[0];

	/*
	 * XX: One important test to be done is to check, if you have multiple
	 * subsystems mounted at one point, all of them *have* be on the cgroup
	 * data structure. If not, we fail.
	 */
	for (k = 0; k < CG_CONTROLLER_MAX && cgroup->controller[k]; k++) {
		path[0] = '\0';

		if (!cg_build_path(cgroup->name, path,
				cgroup->controller[k]->name))
			continue;

		dbg("path is %s\n", path);
		error = cg_create_control_group(path);
		if (error)
			goto err;

		strcpy(base, path);

		if (!ignore_ownership)
			error = cg_chown_recursive(fts_path,
				cgroup->control_uid, cgroup->control_gid);

		if (error)
			goto err;

		for (j = 0; j < CG_NV_MAX && cgroup->controller[k]->values[j];
					j++, strcpy(path, base)) {
			strcat(path, cgroup->controller[k]->values[j]->name);
			error = cg_set_control_value(path,
				cgroup->controller[k]->values[j]->value);
			/*
			 * Should we undo, what we've done in the loops above?
			 */
			if (error)
				goto err;
		}

		if (!ignore_ownership) {
			strcpy(path, base);
			strcat(path, "/tasks");
			chown(path, cgroup->tasks_uid, cgroup->tasks_gid);
		}
	}

err:
	free(path);
	return error;
}

/** cgroup_delete cgroup deletes a control group.
 *  struct cgroup *cgroup takes the group which is to be deleted.
 *
 *  returns 0 on success.
 */
int cgroup_delete_cgroup(struct cgroup *cgroup, int ignore_migration)
{
	FILE *delete_tasks, *base_tasks = NULL;
	int tids;
	char path[FILENAME_MAX];
	int error = ECGROUPNOTALLOWED;
	int i;

	for (i = 0; i < CG_CONTROLLER_MAX && cgroup->controller; i++) {
		if (!cg_build_path(cgroup->name, path,
					cgroup->controller[i]->name))
			continue;
		strcat(path, "../tasks");

		base_tasks = fopen(path, "w");
		if (!base_tasks)
			goto base_open_err;

		if (!cg_build_path(cgroup->name, path,
					cgroup->controller[i]->name))
			continue;

		strcat(path, "tasks");

		delete_tasks = fopen(path, "r");
		if (!delete_tasks)
			goto del_open_err;

		while (!feof(delete_tasks)) {
			fscanf(delete_tasks, "%d", &tids);
			fprintf(base_tasks, "%d", tids);
		}

		if (!cg_build_path(cgroup->name, path,
					cgroup->controller[i]->name))
			continue;
		error = rmdir(path);

		fclose(delete_tasks);
	}
del_open_err:
	if (base_tasks)
		fclose(base_tasks);
base_open_err:
	if (ignore_migration) {
		for (i = 0; cgroup->controller[i] != NULL; i++) {
			if (!cg_build_path(cgroup->name, path,
						cgroup->controller[i]->name))
				continue;
			error = rmdir(path);
		}
	}
	return error;
}
