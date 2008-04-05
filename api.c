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

/*
 */
int cg_attach_task_pid(char *cgroup, pid_t tid)
{
	char path[FILENAME_MAX];
	FILE *tasks;

	if (cgroup == NULL) {
		cgroup = (char *) malloc(sizeof(char));
		cgroup = "\0";
	}

	strcpy(path, MOUNT_POINT);
	strcat(path, "/");
	strcat(path, cgroup);
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

/*
 * Used to attach the task to a control group.
 *
 * WARNING: Will change to use struct cgroup when it is implemented.
 */
int cg_attach_task(char *cgroup)
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
 * int modify_cgroup(struct cgroup *original, struct cgroup *final);
 *
 * where original is the cgroup which is to be modified and final is how it
 * should look.
 *
 * Also this version is still at one level since we do not have
 * multi-hierarchy support in kernel. The real alpha release should have this
 * issue sorted out as well.
 */

int cg_modify_cgroup(char *cgroup, struct control_value *values[], int n)
{
	char path[FILENAME_MAX], base[FILENAME_MAX];
	int i;
	int error;

	strcpy(base, MOUNT_POINT);
	strcat(base, "/");
	strcat(base, cgroup);
	strcat(base, "/");

	for (i = 0; i < n; i++, strcpy(path, base)) {
		strcat(path, values[i]->name);
		error = cg_set_control_value(path, values[i]->value);
		if (error)
			goto err;
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
int cg_create_cgroup(char *cgroup, struct control_value *values[], int n)
{
	char path[FILENAME_MAX], base[FILENAME_MAX];
	int i;
	int error;

	if (MOUNT_POINT == NULL)
		return ECGROUPNOTMOUNTED;

	strcpy(path, MOUNT_POINT);
	strcat(path, "/");
	strcat(path, cgroup);

	error = cg_create_control_group(path);
	strcat(path, "/");
	strcpy(base, path);

	for (i = 0; i < n; i++, strcpy(path, base)) {
		strcat(path, values[i]->name);
		error = cg_set_control_value(path, values[i]->value);
		if (!error)
			return error;
	}
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
 * int delete_cgroup(struct cgroup *group);
 *
 * where group is the group to be deleted.
 *
 * Also this version is still at one level since we do not have
 * multi-hierarchy support in kernel. The real alpha release should have this
 * issue sorted out as well.
 */
int cg_delete_cgroup(char *cgroup)
{
	FILE *delete_tasks, *base_tasks;
	int tids;
	char path[FILENAME_MAX];
	int error;

	strcpy(path, MOUNT_POINT);
	strcat(path,"/tasks");

	base_tasks = fopen(path, "w");

	strcpy(path, MOUNT_POINT);
	strcat(path, "/");
	strcat(path, cgroup);
	strcat(path,"/tasks");

	delete_tasks = fopen(path, "r");

	while (!feof(delete_tasks)) {
		fscanf(delete_tasks, "%d", &tids);
		fprintf(base_tasks, "%d", tids);
	}

	strcpy(path, MOUNT_POINT);
	strcat(path, "/");
	strcat(path, cgroup);

	error = rmdir(path);

	if (!error) {
			return ECGROUPNOTALLOWED;
		}

	return error;
}
