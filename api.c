/*
 * Copyright IBM Corporation. 2007
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
#include <libcg.h>
#include <mntent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

static char MOUNT_POINT[FILENAME_MAX];

int cg_init()
{
	FILE *proc_mount;
	struct mntent *ent;

	proc_mount = fopen("/proc/mounts", "r");
	ent = getmntent(proc_mount);

	while (strcmp(ent->mnt_fsname,"cgroup") != 0) {
		ent = getmntent(proc_mount);
		if (ent == NULL)
			return ECGROUPNOTMOUNTED;
	}
	strcpy(MOUNT_POINT, ent->mnt_dir);
	return 0;
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

	while (strcmp(ent->mnt_fsname, "cgroup") !=0) {
		ent = getmntent(proc_mount);
		if (ent == NULL)
			return 0;
	}
	return 1;
}

static inline pid_t cg_gettid()
{
	return syscall(__NR_gettid);
}

static char* cg_build_path(char *name, char *path)
{
	strcpy(path, MOUNT_POINT);
	strcat(path, name);
	strcat(path, "/");
	return path;
}

/** cg_attach_task_pid is used to assign tasks to a cgroup.
 *  struct cgroup *cgroup: The cgroup to assign the thread to.
 *  pid_t tid: The thread to be assigned to the cgroup.
 *
 *  returns 0 on success.
 *  returns ECGROUPNOTOWNER if the caller does not have access to the cgroup.
 *  returns ECGROUPNOTALLOWED for other causes of failure.
 */
int cg_attach_task_pid(struct cgroup *cgroup, pid_t tid)
{
	char path[FILENAME_MAX];
	FILE *tasks;

	if (cgroup == NULL)
		strcpy(path, MOUNT_POINT);
	else {
		cg_build_path(cgroup->name, path);
	}

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

	return 0;

}

/** cg_attach_task is used to attach the current thread to a cgroup.
 *  struct cgroup *cgroup: The cgroup to assign the current thread to.
 *
 *  See cg_attach_task_pid for return values.
 */
int cg_attach_task(struct cgroup *cgroup)
{
	pid_t tid = cg_gettid();
	int error;

	error = cg_attach_task_pid(cgroup, tid);

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
	error = mkdir(path, 0700);
	if (!error) {
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
 *
 * TODO:
 * At this point I am not sure what all values the control file can take. So
 * I put in an int arg. But this has to be made much more robust.
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
			return ECGROUPNOTALLOWED;
		}
	}

	fprintf(control_file, "%s", val);
	return 0;
}

/** cg_modify_cgroup modifies the cgroup control files.
 * struct cgroup *cgroup: The name will be the cgroup to be modified.
 * The values will be the values to be modified, those not mentioned
 * in the structure will not be modified.
 *
 * The uids cannot be modified yet.
 *
 * returns 0 on success.
 *
 */

int cg_modify_cgroup(struct cgroup *cgroup)
{
	char path[FILENAME_MAX], base[FILENAME_MAX];
	int i;
	int error;

	cg_build_path(cgroup->name, base);

	for (i = 0; i < CG_CONTROLLER_MAX && cgroup->controller[i];
						i++, strcpy(path, base)) {
		int j;
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

/*
 * WARNING: This API is not final. It WILL change format to use
 * struct cgroup. This API will then become internal and be called something
 * else.
 *
 * I am still not happy with how the data structure is looking at the moment,
 * plus there are a couple of additional details to be worked out. Please
 * do not rely on this API.
 *
 * Be prepared to change the implementation later once it shifts to
 * struct cgroup in the real alpha release.
 *
 * The final version is expected to be
 *
 * int create_cgroup(struct cgroup *group);
 *
 * where group is the group to be created
 *
 * Also this version is still at one level since we do not have
 * multi-hierarchy support in kernel. The real alpha release should have this
 * issue sorted out as well.
 */
int cg_create_cgroup(struct cgroup *cgroup)
{
	char path[FILENAME_MAX], base[FILENAME_MAX];
	int i;
	int error;

	if (MOUNT_POINT == NULL)
		return ECGROUPNOTMOUNTED;

	cg_build_path(cgroup->name, path);

	error = cg_create_control_group(path);

	strcpy(base, path);

	for (i = 0; i < CG_CONTROLLER_MAX && cgroup->controller[i];
						i++, strcpy(path, base)) {
		int j;
		for(j = 0; j < CG_NV_MAX && cgroup->controller[i]->values[j];
						j++, strcpy(path, base)) {
			strcat(path, cgroup->controller[i]->values[j]->name);
			error = cg_set_control_value(path,
					cgroup->controller[i]->values[j]->value);
			chown(path, cgroup->control_uid, cgroup->control_gid);
			if (!error)
				return error;
		}
	}
	strcpy(path, base);
	strcat(path, "tasks");
	chown(path, cgroup->tasks_uid, cgroup->tasks_gid);
	return error;
}

/** cg_delete cgroup deletes a control group.
 *  struct cgroup *cgroup takes the group which is to be deleted.
 *
 *  returns 0 on success.
 */
int cg_delete_cgroup(struct cgroup *cgroup)
{
	FILE *delete_tasks, *base_tasks;
	int tids;
	char path[FILENAME_MAX];
	int error;

	strcpy(path, MOUNT_POINT);
	strcat(path,"/tasks");

	base_tasks = fopen(path, "w");

	cg_build_path(cgroup->name, path);
	strcat(path,"tasks");

	delete_tasks = fopen(path, "r");

	while (!feof(delete_tasks)) {
		fscanf(delete_tasks, "%d", &tids);
		fprintf(base_tasks, "%d", tids);
	}

	cg_build_path(cgroup->name, path);

	error = rmdir(path);

	if (!error) {
			return ECGROUPNOTALLOWED;
		}

	return error;
}
