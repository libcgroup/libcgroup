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

#include <dirent.h>
#include <errno.h>
#include <libcgroup.h>
#include <libcgroup-internal.h>
#include <mntent.h>
#include <pthread.h>
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
static pthread_rwlock_t cg_mount_table_lock = PTHREAD_RWLOCK_INITIALIZER;

/* Check if cgroup_init has been called or not. */
static int cgroup_initialized;

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

static int cgroup_test_subsys_mounted(const char *name)
{
	int i;

	pthread_rwlock_rdlock(&cg_mount_table_lock);

	for (i = 0; cg_mount_table[i].name[0] != '\0'; i++) {
		if (strncmp(cg_mount_table[i].name, name,
				sizeof(cg_mount_table[i].name)) == 0) {
			pthread_rwlock_unlock(&cg_mount_table_lock);
			return 1;
		}
	}
	pthread_rwlock_unlock(&cg_mount_table_lock);
	return 0;
}

/**
 * cgroup_init(), initializes the MOUNT_POINT.
 *
 * This code is theoretically thread safe now. Its not really tested
 * so it can blow up. If does for you, please let us know with your
 * test case and we can really make it thread safe.
 *
 */
int cgroup_init()
{
	FILE *proc_mount;
	struct mntent *ent, *temp_ent;
	int found_mnt = 0;
	int ret = 0;
	static char *controllers[CG_CONTROLLER_MAX];
	FILE *proc_cgroup;
	char subsys_name[FILENAME_MAX];
	int hierarchy, num_cgroups, enabled;
	int i=0;
	char *mntopt;
	int err;
	char *buf;
	char mntent_buffer[4 * FILENAME_MAX];
	char *strtok_buffer;

	pthread_rwlock_wrlock(&cg_mount_table_lock);

	proc_cgroup = fopen("/proc/cgroups", "r");

	if (!proc_cgroup) {
		ret = EIO;
		goto unlock_exit;
	}

	/*
	 * The first line of the file has stuff we are not interested in.
	 * So just read it and discard the information.
	 *
	 * XX: fix the size for fgets
	 */
	buf = fgets(subsys_name, FILENAME_MAX, proc_cgroup);
	if (!buf) {
		ret = EIO;
		goto unlock_exit;
	}

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
		ret = ECGFAIL;
		goto unlock_exit;
	}

	temp_ent = (struct mntent *) malloc(sizeof(struct mntent));

	if (!temp_ent) {
		ret = ECGFAIL;
		goto unlock_exit;
	}

	while ((ent = getmntent_r(proc_mount, temp_ent,
					mntent_buffer,
					sizeof(mntent_buffer))) != NULL) {
		if (!strcmp(ent->mnt_type, "cgroup")) {
			for (i = 0; controllers[i] != NULL; i++) {
				mntopt = hasmntopt(ent, controllers[i]);

				if (!mntopt)
					continue;

				mntopt = strtok_r(mntopt, ",", &strtok_buffer);

				if (strcmp(mntopt, controllers[i]) == 0) {
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
		ret = ECGROUPNOTMOUNTED;
		goto unlock_exit;
	}

	found_mnt++;
	cg_mount_table[found_mnt].name[0] = '\0';

	fclose(proc_mount);
	cgroup_initialized = 1;

unlock_exit:
	pthread_rwlock_unlock(&cg_mount_table_lock);
	return ret;
}

static int cg_test_mounted_fs()
{
	FILE *proc_mount;
	struct mntent *ent, *temp_ent;
	char mntent_buff[4 * FILENAME_MAX];

	proc_mount = fopen("/proc/mounts", "r");
	if (proc_mount == NULL) {
		return -1;
	}

	temp_ent = (struct mntent *) malloc(sizeof(struct mntent));
	if (!temp_ent) {
		/* We just fail at the moment. */
		return 0;
	}

	ent = getmntent_r(proc_mount, temp_ent, mntent_buff,
						sizeof(mntent_buff));

	if (!ent)
		return 0;

	while (strcmp(ent->mnt_type, "cgroup") !=0) {
		ent = getmntent_r(proc_mount, temp_ent, mntent_buff,
						sizeof(mntent_buff));
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


/* Call with cg_mount_table_lock taken */
static char *cg_build_path_locked(char *name, char *path, char *type)
{
	int i;
	for (i = 0; cg_mount_table[i].name[0] != '\0'; i++) {
		if (strcmp(cg_mount_table[i].name, type) == 0) {
			strcpy(path, cg_mount_table[i].path);
			strcat(path, "/");
			if (name) {
				strcat(path, name);
				strcat(path, "/");
			}
			return path;
		}
	}
	return NULL;
}

static char *cg_build_path(char *name, char *path, char *type)
{
	pthread_rwlock_rdlock(&cg_mount_table_lock);
	path = cg_build_path_locked(name, path, type);
	pthread_rwlock_unlock(&cg_mount_table_lock);

	return path;
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

	if (!cgroup_initialized) {
		dbg ("libcgroup is not initialized\n");
		return ECGROUPNOTINITIALIZED;
	}
	if(!cgroup)
	{
		pthread_rwlock_rdlock(&cg_mount_table_lock);
		for(i = 0; i < CG_CONTROLLER_MAX &&
				cg_mount_table[i].name[0]!='\0'; i++) {
			if (!cg_build_path_locked(NULL, path,
						cg_mount_table[i].name))
				continue;
			strcat(path, "/tasks");

			tasks = fopen(path, "w");
			if (!tasks) {
				pthread_rwlock_unlock(&cg_mount_table_lock);
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
		pthread_rwlock_unlock(&cg_mount_table_lock);
	} else {
		for (i = 0; i < cgroup->index; i++) {
			if (!cgroup_test_subsys_mounted(cgroup->controller[i]->name)) {
				dbg("subsystem %s is not mounted\n",
					cgroup->controller[i]->name);
				return ECGROUPSUBSYSNOTMOUNTED;
			}
		}
		for (i = 0; i < cgroup->index; i++) {
			if (!cg_build_path(cgroup->name, path,
					cgroup->controller[i]->name))
				continue;

			strcat(path, "/tasks");

			tasks = fopen(path, "w");
			if (!tasks) {
				dbg("fopen failed for %s:%s", path,
							strerror(errno));

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
		return ECGROUPVALUENOTEXIST;
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

	if (!cgroup_initialized)
		return ECGROUPNOTINITIALIZED;

	if (!cgroup)
		return ECGROUPNOTALLOWED;

	for (i = 0; i < cgroup->index; i++) {
		if (!cgroup_test_subsys_mounted(cgroup->controller[i]->name)) {
			dbg("subsystem %s is not mounted\n",
				cgroup->controller[i]->name);
			return ECGROUPSUBSYSNOTMOUNTED;
		}
	}

	for (i = 0; i < cgroup->index; i++, strcpy(path, base)) {
		int j;
		if (!cg_build_path(cgroup->name, base,
			cgroup->controller[i]->name))
			continue;
		for (j = 0; j < cgroup->controller[i]->index; j++,
						strcpy(path, base)) {
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

	if (!cgroup_initialized)
		return ECGROUPNOTINITIALIZED;

	if (!cgroup)
		return ECGROUPNOTALLOWED;

	for (i = 0; i < cgroup->index;	i++) {
		if (!cgroup_test_subsys_mounted(cgroup->controller[i]->name))
			return ECGROUPSUBSYSNOTMOUNTED;
	}

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
	for (k = 0; k < cgroup->index; k++) {
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

		for (j = 0; j < cgroup->controller[k]->index; j++,
							strcpy(path, base)) {
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
			error = chown(path, cgroup->tasks_uid,
							cgroup->tasks_gid);
			if (!error) {
				error = ECGFAIL;
				goto err;
			}
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
	int i, ret;

	if (!cgroup_initialized)
		return ECGROUPNOTINITIALIZED;

	if (!cgroup)
		return ECGROUPNOTALLOWED;

	for (i = 0; i < cgroup->index; i++) {
		if (!cgroup_test_subsys_mounted(cgroup->controller[i]->name))
			return ECGROUPSUBSYSNOTMOUNTED;
	}

	for (i = 0; i < cgroup->index; i++) {
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
			ret = fscanf(delete_tasks, "%d", &tids);
			/*
			 * Don't know how to handle EOF yet, so
			 * ignore it
			 */
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
		for (i = 0; i < cgroup->index; i++) {
			if (!cg_build_path(cgroup->name, path,
						cgroup->controller[i]->name))
				continue;
			error = rmdir(path);
		}
	}
	return error;
}

/*
 * This function should really have more checks, but this version
 * will assume that the callers have taken care of everything.
 * Including the locking.
 */
static char *cg_rd_ctrl_file(char *subsys, char *cgroup, char *file)
{
	char *value;
	char path[FILENAME_MAX];
	FILE *ctrl_file;

	if (!cg_build_path_locked(cgroup, path, subsys))
		return NULL;

	strcat(path, file);
	ctrl_file = fopen(path, "r");
	if (!ctrl_file)
		return NULL;

	fscanf(ctrl_file, "%as", &value);

	fclose(ctrl_file);

	return value;
}

/*
 * Call this function with required locks taken.
 */
static int cgroup_fill_cgc(struct dirent *ctrl_dir, struct cgroup *cgroup,
			struct cgroup_controller *cgc, int index)
{
	char *ctrl_name;
	char *ctrl_file;
	char *ctrl_value;
	char *d_name;
	char *buffer;
	int error = 0;

	d_name = strdup(ctrl_dir->d_name);

	ctrl_name = strtok_r(d_name, ".", &buffer);

	if (!ctrl_name) {
		error = ECGFAIL;
		goto fill_error;
	}

	ctrl_file = strtok_r(NULL, ".", &buffer);

	if (!ctrl_file) {
		error = ECGFAIL;
		goto fill_error;
	}

	if (strcmp(ctrl_name, cg_mount_table[index].name) == 0) {
		ctrl_value = cg_rd_ctrl_file(cg_mount_table[index].name,
				cgroup->name, d_name);
		if (!ctrl_value) {
			error = ECGFAIL;
			goto fill_error;
		}

		if (cgroup_add_value_string(cgc, ctrl_dir->d_name,
				ctrl_value)) {
			error = ECGFAIL;
			goto fill_error;
		}
	}
fill_error:
	free(ctrl_value);
	free(d_name);
	return error;
}

/*
 * cgroup_get_cgroup returns the cgroup data from the filesystem.
 * struct cgroup has the name of the group to be populated
 *
 * return succesfully filled cgroup data structure on success.
 */
struct cgroup *cgroup_get_cgroup(struct cgroup *cgroup)
{
	int i;
	char path[FILENAME_MAX];
	DIR *dir;
	struct dirent *ctrl_dir;

	if (!cgroup_initialized) {
		/* ECGROUPNOTINITIALIZED */
		return NULL;
	}

	if (!cgroup) {
		/* ECGROUPNOTALLOWED */
		return NULL;
	}

	pthread_rwlock_rdlock(&cg_mount_table_lock);
	for (i = 0; i < CG_CONTROLLER_MAX &&
			cg_mount_table[i].name[0] != '\0'; i++) {
		/*
		 * cgc will not leak, since it has to be freed using
		 * cgroup_free_cgroup
		 */
		struct cgroup_controller *cgc;
		if (!cg_build_path_locked(NULL, path,
					cg_mount_table[i].name))
			continue;

		strncat(path, cgroup->name, sizeof(path));

		if (access(path, F_OK))
			continue;

		if (!cg_build_path_locked(cgroup->name, path,
					cg_mount_table[i].name)) {
			/*
			 * This fails when the cgroup does not exist
			 * for that controller.
			 */
			continue;
		}

		cgc = cgroup_add_controller(cgroup,
				cg_mount_table[i].name);
		if (!cgc)
			goto unlock_error;

		dir = opendir(path);
		if (!dir) {
			/* error = ECGROUPSTRUCTERROR; */
			goto unlock_error;
		}
		while ((ctrl_dir = readdir(dir)) != NULL) {
			if (cgroup_fill_cgc(ctrl_dir, cgroup, cgc, i)) {
				closedir(dir);
				goto unlock_error;
			}

		}
		closedir(dir);
	}
	pthread_rwlock_unlock(&cg_mount_table_lock);
	return cgroup;

unlock_error:
	pthread_rwlock_unlock(&cg_mount_table_lock);
	cgroup = NULL;
	return NULL;
}
