/*
 * Copyright IBM Corporation. 2008
 *
 * Author:	Sudhir Kumar <skumar@linux.vnet.ibm.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2.1 of the GNU Lesser General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Description: This file contains the functions for testing libcgroup apis.
 */

#include "libcgrouptest.h"

/* The messages that may be useful to the user */
char info[][SIZE] = {
	" Parameter nullcgroup\n", 			/* NULLGRP */
	" Parameter commoncgroup\n",			/* COMMONGRP */
	" Parameter not created group\n",		/* NOTCRTDGRP */
	" Parameter same cgroup\n",			/* SAMEGRP */
	" Task found in group/s\n",			/* TASKINGRP */
	" Task not found in group/s\n",			/* TASKNOTINGRP */
	" Task not found in all groups\n",		/* TASKNOTINANYGRP */
	" group found in filesystem\n",			/* GRPINFS */
	" group not found in filesystem\n",		/* GRPNOTINFS */
	" group found under both controllers\n",	/* GRPINBOTHCTLS */
	" group not found under second controller\n",	/* GRPNOTIN2NDCTL */
	" group not found under first controller\n",	/* GRPNOTIN1STCTL */
	" group modified under both controllers\n",	/* GRPMODINBOTHCTLS */
	" group not modified under second controller\n",/* GRPNOTMODIN2NDCTL */
	" group not modified under any controller\n",	/* GRPNOTMODINANYCTL */
	" Group deleted from filesystem\n",		/* GRPDELETEDINFS */
	" Group not deleted from filesystem\n",		/* GRPNOTDELETEDINFS */
	" Group not deleted globally\n",		/* GRPNOTDELETEDGLOBALY */
	/* In case there is no extra info messages to be printed */
	"\n",						/* NOMESSAGE */
};

/**
 * Tests the cgroup_init_cgroup() api under different scenarios
 * @param retcode error code in case any error is expected from api
 * @param i the test number
 */
void test_cgroup_init(int retcode, int i)
{
	int retval;

	retval = cgroup_init();
	if (retval == retcode)
		message(i, PASS, "init()\t", retval, info[NOMESSAGE]);
	else
		message(i, FAIL, "init()",  retval, info[NOMESSAGE]);
}

/**
 * Tests the cgroup_attach_cgroup() api under different scenarios
 * @param retcode error code in case any error is expected from api
 * @param cgrp the group to assign the task to
 * @param group1 the name of the group under first (single) mountpoint
 * @param group2 the name of the group under 2nd moutpoint for multimount
 * @param i the test number
 * @param k the message enum number to print the useful message
 */
void test_cgroup_attach_task(int retcode, struct cgroup *cgrp,
	 const char *group1, const char *group2, pid_t pid, int k, int i)
{
	int retval;
	char tasksfile[FILENAME_MAX], tasksfile2[FILENAME_MAX];
	/* Check, In case some error is expected due to a negative scenario */
	if (retcode) {
		if (pid)
			retval = cgroup_attach_task_pid(cgrp, pid);
		else
			retval = cgroup_attach_task(cgrp);

		if (retval == retcode)
			message(i, PASS, "attach_task()", retval, info[k]);
		else
			message(i, FAIL, "attach_task()", retval, info[k]);

		return;
	}

	/* Now there is no error and it is a genuine call */
	if (pid)
		retval = cgroup_attach_task_pid(cgrp, pid);
	else
		retval = cgroup_attach_task(cgrp);

	/* API returned success, so perform check */
	if (retval == 0) {
		build_path(tasksfile, mountpoint,
					 group1, "tasks");

		if (check_task(tasksfile, 0)) {
			if (fs_mounted == 2) {
				/* multiple mounts */
				build_path(tasksfile2, mountpoint2,
							 group2, "tasks");
				if (check_task(tasksfile2, 0)) {
					message(i, PASS, "attach_task()",
						 retval, info[TASKINGRP]);
				} else {
					message(i, FAIL, "attach_task()",
						 retval, info[TASKNOTINANYGRP]);
				}
			} else {
				/* single mount */
				message(i, PASS, "attach_task()",
						 retval, info[TASKINGRP]);
			}
		} else {
			message(i, FAIL, "attach_task()", retval,
							 info[TASKNOTINGRP]);
		}
	} else {
		message(i, FAIL, "attach_task()", retval, (char *)"\n");
	}
}

/**
 * This function creates a cgroup data structure
 * This function is a bit ugly for now and need to be changed
 * @param ctl the controller under which group is to be created
 * @param grpname the name of the group
 * @param value_type which value out of four types
 * @param struct cval the control value structure
 * @param struct ids the permissions struct
 * @param the test number
 */
struct cgroup *create_new_cgroup_ds(int ctl, const char *grpname,
	 int value_type, struct cntl_val_t cval, struct uid_gid_t ids, int i)
{
	int retval;
	char group[FILENAME_MAX];
	char controller_name[FILENAME_MAX], control_file[FILENAME_MAX];

	strncpy(group, grpname, sizeof(group) - 1);
	retval = set_controller(ctl, controller_name, control_file);
	if (retval) {
		fprintf(stderr, "Setting controller failled\n");
		return NULL;
	}

	switch (ctl) {
		/* control values are controller specific, so will be set
		 * accordingly from the config file */
	case CPU:
		strncpy(cval.val_string, "260000", sizeof(cval.val_string));
		break;

	case MEMORY:
		strncpy(cval.val_string, "7000064", sizeof(cval.val_string));
		break;

	/* To be added for other controllers */
	default:
		printf("Invalid controller name passed. Setting control value"
					" failed. Dependent tests may fail\n");
		return NULL;
		break;
	}

	return new_cgroup(group, controller_name, control_file,
						 value_type, cval, ids, i);
}

/**
 * Tests the cgroup_create_cgroup() api under different scenarios
 * @param retcode error code in case any error is expected from api
 * @param cgrp the group to be created
 * @param name the name of the group
 * @param common to test if group will be created under one or both mountpoints
 * @param mpnt to test if group under mountpoint or mountpoint2
 * @param ign parameter for api if to ignore the ownership
 * @param the test number
 */
void test_cgroup_create_cgroup(int retcode, struct cgroup *cgrp,
			 const char *name, int common, int mpnt, int ign, int i)
{
	int retval;
	char path1_group[FILENAME_MAX], path2_group[FILENAME_MAX];
	/* Check, In case some error is expected due to a negative scenario */
	if (retcode) {
		retval = cgroup_create_cgroup(cgrp, ign);
		if (retval == retcode)
			message(i, PASS, "create_cgroup()", retval,
							 info[NOMESSAGE]);
		else
			message(i, FAIL, "create_cgroup()", retval,
							 info[NOMESSAGE]);

		return;
	}

	/* Now there is no error and it is a genuine call */
	retval = cgroup_create_cgroup(cgrp, ign);
	if (retval) {
		message(i, FAIL, "create_cgroup()", retval,  info[NOMESSAGE]);
		return;
	}

	/* Let us now check if the group exist in file system */
	if (!common) {
		/* group only under one mountpoint */
		if (mpnt == 1)
			/* group under mountpoint */
			build_path(path1_group, mountpoint, name, NULL);
		else
			/* group under mountpoint2 */
			build_path(path1_group, mountpoint2, name, NULL);

		if (group_exist(path1_group) == 0)
			message(i, PASS, "create_cgroup()", retval,
							 info[GRPINFS]);
		else
			message(i, FAIL, "create_cgroup()", retval,
							 info[GRPNOTINFS]);

	 /* group under both mountpoints */
	} else {
		/* check if the group exists under both controllers */
		build_path(path1_group, mountpoint, name, NULL);
		if (group_exist(path1_group) == 0) {
			build_path(path2_group, mountpoint2, name, NULL);

			if (group_exist(path2_group) == 0)
				message(i, PASS, "create_cgroup()",
						 retval, info[GRPINBOTHCTLS]);
			else
				message(i, FAIL, "create_cgroup()",
						 retval, info[GRPNOTIN2NDCTL]);
		} else {
			message(i, FAIL, "create_cgroup()", retval,
							 info[GRPNOTIN1STCTL]);
		}
	}

	return;
}

/**
 * Tests the cgroup_delete_cgroup() api under different scenarios
 * @param retcode error code in case any error is expected from api
 * @param cgrp the group to be deleted
 * @param name the name of the group
 * @param common to test if group was created under one or both mountpoints
 * @param mpnt to test if group under mountpoint or mountpoint2
 * @param ign parameter for api if to ignore the ownership
 * @param the test number
 */
void test_cgroup_delete_cgroup(int retcode, struct cgroup *cgrp,
			 const char *name, int common, int mpnt, int ign, int i)
{
	int retval;
	char path1_group[FILENAME_MAX], path2_group[FILENAME_MAX];
	/* Check, In case some error is expected due to a negative scenario */
	if (retcode) {
		retval = cgroup_delete_cgroup(cgrp, ign);
		if (retval == retcode)
			message(i, PASS, "delete_cgroup()", retval,
							 info[NOMESSAGE]);
		else
			message(i, FAIL, "delete_cgroup()", retval,
							 info[NOMESSAGE]);

		return;
	}

	/* Now there is no error and it is a genuine call */
	retval = cgroup_delete_cgroup(cgrp, ign);
	if (retval) {
		message(i, FAIL, "delete_cgroup()", retval,  info[NOMESSAGE]);
		return;
	}

	/* Let us now check if the group has been deleted from file system */
	if (!common) {
		/* check only under one mountpoint */
		if (mpnt == 1)
			/* check group under mountpoint */
			build_path(path1_group, mountpoint, name, NULL);
		else
			/* check group under mountpoint2 */
			build_path(path1_group, mountpoint2, name, NULL);

		if (group_exist(path1_group) == ENOENT)
			message(i, PASS, "delete_cgroup()", retval,
						 info[GRPDELETEDINFS]);
		else
			message(i, FAIL, "delete_cgroup()", retval,
						 info[GRPNOTDELETEDINFS]);

	} else {
		/* check group under both mountpoints */
		/* Check if the group deleted under both mountpoints */
		build_path(path1_group, mountpoint, name, NULL);
		if (group_exist(path1_group) == ENOENT) {
			build_path(path2_group, mountpoint2, name, NULL);

			if (group_exist(path2_group) == ENOENT)
				message(i, PASS, "delete_cgroup()",
						 retval, info[GRPDELETEDINFS]);
			else
				message(i, FAIL, "delete_cgroup()",
					 retval, info[GRPNOTDELETEDGLOBALY]);
		} else {
			message(i, FAIL, "delete_cgroup()", retval,
						 info[GRPNOTDELETEDINFS]);
		}
	}

}

/**
 * The function tests if the given controller is enabled in kernel
 * @param name the name of the controller to be checked
 * @param exist set to 1 if the controller exists
 */
void is_subsystem_enabled(const char *name, int *exist)
{
	int hierarchy, num_cgroups, enabled;
	FILE *fd;
	char subsys_name[FILENAME_MAX];

	fd = fopen("/proc/cgroups", "r");
	if (!fd)
		return;

	while (!feof(fd)) {
		fscanf(fd, "%s, %d, %d, %d", subsys_name,
					 &hierarchy, &num_cgroups, &enabled);
		if (strncmp(name, subsys_name, sizeof(*name)) == 0)
			*exist = 1;
	}

	fclose(fd);
}

/**
 * This function tests if the given group exists in filesystem
 * @param path_group path to the group to be tested for existence
 */
int group_exist(char *path_group)
{
	struct stat statbuf;
	if (stat(path_group, &statbuf) == -1) {
		/* Group deleted. OK */
		if (errno == ENOENT)
			return ENOENT;
		/* There is some other failure */
		printf("stat failed, return code is %d\n", errno);
		return -1;
	}

	if (S_ISDIR(statbuf.st_mode))
		return 0;
	else
		return -1;
}

/**
 * Sets the controller name and control file name
 * @param controller the enum for the name of the controller
 * @param controller_name name of the controller
 * @param control_file corresponding control file
 */
int set_controller(int controller, char *controller_name, char *control_file)
{
	switch (controller) {
	case MEMORY:
		if (memory == 0)
			return 1;

		strncpy(controller_name, "memory", FILENAME_MAX);
		strncpy(control_file, "memory.limit_in_bytes", FILENAME_MAX);
		return 0;
		break;

	case CPU:
		if (cpu == 0)
			return 1;

		strncpy(controller_name, "cpu", FILENAME_MAX);
		strncpy(control_file, "cpu.shares", FILENAME_MAX);
		return 0;
		break;

	case CPUSET:
		strncpy(controller_name, "cpuset", FILENAME_MAX);
		strncpy(control_file, "cpuset.cpus", FILENAME_MAX);
		return 0;
		break;
		/* Future controllers can be added here */

	default:
		printf("Invalid controller name passed. Setting controller"
					" failed. Dependent tests may fail\n");
		return 1;
		break;
	}
}

/**
 * Tests if a group has been modified
 * @param path_control_file path to the control file of the controller
 * @param value_type which value out of four types
 * @param struct cval the control value structure
 */
int group_modified(char *path_control_file, int value_type,
						 struct cntl_val_t cval)
{
	bool bool_val;
	int64_t int64_val;
	u_int64_t uint64_val;
	/* 100 char looks ok for a control value as string */
	char string_val[100];
	FILE *fd;
	int error = 1;
	int aux;

	fd = fopen(path_control_file, "r");
	if (!fd) {
		fprintf(stderr, "Error in opening %s\n", path_control_file);
		fprintf(stderr, "Skipping modified values check....\n");
		return 1;
	}

	switch (value_type) {

	case BOOL:
		fscanf(fd, "%d", &aux);
		bool_val = aux;
		if (bool_val == cval.val_bool)
			error = 0;
		break;
	case INT64:
		fscanf(fd, "%" SCNi64, &int64_val);
		if (int64_val == cval.val_int64)
			error = 0;
		break;
	case UINT64:
		fscanf(fd, "%" SCNu64, &uint64_val);
		if (uint64_val == cval.val_uint64)
			error = 0;
		break;
	case STRING:
		fscanf(fd, "%s", string_val);
		if (!strncmp(string_val, cval.val_string, strlen(string_val)))
			error = 0;
		break;
	default:
		fprintf(stderr, "Wrong value_type passed "
						"in group_modified()\n");
		fprintf(stderr, "Skipping modified values check....\n");
		/* Can not report test result as failure */
		error = 0;
		break;
	}

	fclose(fd);
	return error;
}

/**
 * Adds the control value to a controller using wrapper apis
 * @param newcontroller the controller to be added the value to
 * @param control_file name of the control file of the controller
 * @param wr the name of wrapper api
 * @param value_type which value out of four types
 * @param struct cval the control value structure
 */
int add_control_value(struct cgroup_controller *newcontroller,
	 char *control_file, char *wr, int value_type, struct cntl_val_t cval)
{
	int retval;

	switch (value_type) {

	case BOOL:
		retval = cgroup_add_value_bool(newcontroller,
					 control_file, cval.val_bool);
		snprintf(wr, SIZE, "add_value_bool()");
		break;
	case INT64:
		retval = cgroup_add_value_int64(newcontroller,
					 control_file, cval.val_int64);
		snprintf(wr, SIZE, "add_value_int64()");
		break;
	case UINT64:
		retval = cgroup_add_value_uint64(newcontroller,
					 control_file, cval.val_uint64);
		snprintf(wr, SIZE, "add_value_uint64()");
		break;
	case STRING:
		retval = cgroup_add_value_string(newcontroller,
					 control_file, cval.val_string);
		snprintf(wr, SIZE, "add_value_string()");
		break;
	default:
		printf("ERROR: wrong value in add_control_value()\n");
		return 1;
		break;
	}
	return retval;
}

/**
 * This function creates and returns a cgroup data structure
 * @param group the name of the group
 * @param controller_name the name of the controller to be added to the group
 * @param control_file name of the control file of the controller
 * @param value_type which value out of four types
 * @param struct cval the control value structure
 * @param struct ids the permissions struct
 * @param the test number
 */
struct cgroup *new_cgroup(char *group, char *controller_name,
			 char *control_file, int value_type,
			 struct cntl_val_t cval, struct uid_gid_t ids, int i)
{
	int retval;
	/* Names of wrapper apis */
	char wr[SIZE];
	struct cgroup *newcgroup;
	struct cgroup_controller *newcontroller;

	newcgroup = cgroup_new_cgroup(group);

	if (newcgroup) {
		retval = cgroup_set_uid_gid(newcgroup, ids.tasks_uid,
			 ids.tasks_gid,	ids.control_uid, ids.control_gid);

		if (retval) {
			snprintf(wr, SIZE, "set_uid_gid()");
			message(i++, FAIL, wr, retval, info[NOMESSAGE]);
		}

		newcontroller = cgroup_add_controller(newcgroup,
							 controller_name);
		if (newcontroller) {
			retval =  add_control_value(newcontroller,
					 control_file, wr, value_type, cval);

			if (!retval) {
				message(i++, PASS, "new_cgroup()",
						 retval, info[NOMESSAGE]);
			} else {
				message(i++, FAIL, wr, retval ,
							 info[NOMESSAGE]);
				cgroup_free(&newcgroup);
				return NULL;
			}
		 } else {
			/* Since these wrappers do not return an int so -1 */
			message(i++, FAIL, "add_controller", -1,
							 info[NOMESSAGE]);
			cgroup_free(&newcgroup);
			return NULL;
		}
	} else {
		message(i++, FAIL, "new_cgroup", -1, info[NOMESSAGE]);
		return NULL;
	}
	return newcgroup;
}

/**
 * Checks if the cgroup filesystem has been mounted
 * @param multimnt to decide if check is for single mount or multimount
 */
int check_fsmounted(int multimnt)
{
	int count = 0;
	int ret = 1;
	struct mntent *entry = NULL, *tmp_entry = NULL;
	/* Need a better mechanism to decide memory allocation size here */
	char entry_buffer[FILENAME_MAX * 4];
	FILE *proc_file = NULL;

	tmp_entry = (struct mntent *) malloc(sizeof(struct mntent));
	if (!tmp_entry) {
		perror("Error: failled to mallloc for mntent\n");
		ret = errno;
		goto error;
	}

	proc_file = fopen("/proc/mounts", "r");
	if (!proc_file) {
		printf("Error in opening /proc/mounts.\n");
		ret = errno;
		goto error;
	}
	while ((entry = getmntent_r(proc_file, tmp_entry, entry_buffer,
						 FILENAME_MAX*4)) != NULL) {
		if (!strncmp(entry->mnt_type, "cgroup", strlen("cgroup"))) {
			count++;
			if (multimnt) {
				if (count >= 2) {
					printf("sanity check pass. %s\n",
							 entry->mnt_type);
					ret = 0;
					goto error;
				}
			} else {
				printf("sanity check pass. %s\n",
							 entry->mnt_type);
				ret = 0;
				goto error;
			}
		}
	}
error:
	if (tmp_entry)
		free(tmp_entry);
	if (proc_file)
		fclose(proc_file);
	return ret;
}

/**
 * Checks if the current task belongs to the given tasks file
 * @param tasksfile the task file to be tested for the task
 */
int check_task(char *tasksfile, pid_t pid)
{
	FILE *file;
	pid_t curr_tid, tid;
	int pass = 0;

	file = fopen(tasksfile, "r");
	if (!file) {
		printf("ERROR: in opening %s\n", tasksfile);
		printf("Exiting without running other testcases in this set\n");
		exit(1);
	}

	if (pid)
		curr_tid = pid;
	else
		curr_tid = cgrouptest_gettid();

	while (!feof(file)) {
		fscanf(file, "%u", &tid);
		if (tid == curr_tid) {
			pass = 1;
			break;
		}
	}
	fclose(file);

	return pass;
}

/**
 * Prints the test result in a readable format with some verbose messages
 * @param num the test number
 * @param pass test passed or failed
 * @param api the name of the api tested
 * @param retval the return value of the api
 * @param extra the extra message to the user about the scenario tested
 */
void message(int num, int pass, const char *api, int retval, char *extra)
{
	char res[10];
	char buf[2*SIZE];
	if (pass)
		strncpy(res, "PASS :", 10);
	else
		strncpy(res, "FAIL :", 10);

	/* Populate message buffer for the api */
	snprintf(buf, sizeof(buf), "cgroup_%s\t\t Ret Value = ", api);
	fprintf(stdout, "TEST%2d:%s %s%d\t%s", num, res, buf, retval, extra);
}

/**
 * Builds the path to target file/group
 * @param target to write the built path to
 * @param mountpoint for which mountpoint the path to be built
 * @param group the name of the group (directory)
 * @param file what file under the group
 */
void
build_path(char *target, char *mountpoint, const char *group, const char *file)
{
	if (!target)
		return;

	strncpy(target, mountpoint, FILENAME_MAX);

	if (group) {
		strncat(target, "/", FILENAME_MAX - strlen(target));
		strncat(target, group, FILENAME_MAX - strlen(target));
	}

	if (file) {
		strncat(target, "/", FILENAME_MAX - strlen(target));
		strncat(target, file, FILENAME_MAX - strlen(target));
	}
}

/**
 * Tests the cgroup_compare_cgroup() api under different scenarios
 * @param ctl1 controller 1 to be used for testing
 * @param ctl2 controller 1 to be used for testing
 * @param the test number
 */
void test_cgroup_compare_cgroup(int ctl1, int ctl2, int i)
{
	int retval;

	struct cntl_val_t cval;
	cval.val_int64 = 0;
	cval.val_uint64 = 0;
	cval.val_bool = 0;
	strcpy(cval.val_string, "5000");

	struct cgroup *cgroup1 = NULL, *cgroup2 = NULL;
	struct cgroup_controller *controller = NULL;
	char controller_name[FILENAME_MAX], control_file[FILENAME_MAX];
	char wr[SIZE], extra[] = "in cgroup_compare_cgroup";

	retval = cgroup_compare_cgroup(NULL, NULL);
	if (retval)
		message(i++, PASS, "compare_cgroup()", retval, info[NULLGRP]);
	else
		message(i++, FAIL, "compare_cgroup()", retval, info[NULLGRP]);

	cgroup1 = cgroup_new_cgroup("testgroup");
	cgroup2 = cgroup_new_cgroup("testgroup");
	cgroup_set_uid_gid(cgroup1, 0, 0, 0, 0);
	cgroup_set_uid_gid(cgroup2, 0, 0, 0, 0);

	retval = set_controller(ctl1, controller_name, control_file);

	controller = cgroup_add_controller(cgroup1, controller_name);
	if (controller) {
		retval =  add_control_value(controller,
					 control_file, wr, STRING, cval);
		if (retval)
			message(i++, FAIL, wr, retval, extra);
	}

	controller = cgroup_add_controller(cgroup2, controller_name);
	if (controller) {
		retval =  add_control_value(controller,
					 control_file, wr, STRING, cval);
		if (retval)
			message(i++, FAIL, wr, retval, extra);
	}

	retval = cgroup_compare_cgroup(cgroup1, cgroup2);
	if (retval)
		message(i++, FAIL, "compare_cgroup()", retval, info[NOMESSAGE]);
	else
		message(i++, PASS, "compare_cgroup()", retval, info[NOMESSAGE]);

	/* Test the api by putting diff number of controllers in cgroups */
	retval = set_controller(ctl2, controller_name, control_file);
	controller = cgroup_add_controller(cgroup2, controller_name);
	if (controller) {
		retval =  add_control_value(controller,
					 control_file, wr, STRING, cval);
		if (retval)
			message(i++, FAIL, wr, retval, extra);
	}

	retval = cgroup_compare_cgroup(cgroup1, cgroup2);
	if (retval == ECGROUPNOTEQUAL)
		message(i++, PASS, "compare_cgroup()", retval, info[NOMESSAGE]);
	else
		message(i++, FAIL, "compare_cgroup()", retval, info[NOMESSAGE]);

	cgroup_free(&cgroup1);
	cgroup_free(&cgroup2);
}

/**
 * Tests the cgroup_get_cgroup() api under different scenarios
 * @param ctl1 controller 1 to be used for testing
 * @param ctl2 controller 1 to be used for testing
 * @param struct ids the permissions struct
 * @param the test number
 */
void test_cgroup_get_cgroup(int ctl1, int ctl2, struct uid_gid_t ids, int i)
{
	struct cgroup *cgroup_filled = NULL, *cgroup_a = NULL, *cgroup_b = NULL;
	struct cgroup_controller *controller = NULL;
	char controller_name[FILENAME_MAX], control_file[FILENAME_MAX];
	struct cntl_val_t cval = {0, 0, 0, "5000"};
	int ret;

	/*
	 * No need to test the next 3 scenarios separately for Multimnt
	 * so testing them only under single mount
	 */
	if (fs_mounted == FS_MOUNTED) {
		/* 1. Test with nullcgroup first */
		ret = cgroup_get_cgroup(NULL);
		if (ret == ECGROUPNOTALLOWED)
			message(i++, PASS, "get_cgroup()", ret, info[NULLGRP]);
		else
			message(i++, FAIL, "get_cgroup()", ret, info[NULLGRP]);

		/* 2. Test with invalid name filled cgroup(non existing) */
		cgroup_filled = cgroup_new_cgroup("nogroup");
		if (!cgroup_filled)
			message(i++, FAIL, "new_cgroup()", 0, info[NOMESSAGE]);

		ret = cgroup_get_cgroup(cgroup_filled);
		if (ret)
			message(i++, PASS, "get_cgroup()", ret,
							 info[NOTCRTDGRP]);
		else
			message(i++, FAIL, "get_cgroup()", ret,
							 info[NOTCRTDGRP]);
		/* Free the allocated cgroup before reallocation */
		cgroup_free(&cgroup_filled);

		/* 3.
		 * Test with name filled cgroup. Ensure the group group1 exists
		 * in the filesystem before calling this test function
		 */
		cgroup_filled = cgroup_new_cgroup("group1");
		if (!cgroup_filled)
			message(i++, FAIL, "new_cgroup()", 0, info[NOMESSAGE]);

		ret = cgroup_get_cgroup(cgroup_filled);
		if (!ret)
			message(i++, PASS, "get_cgroup()", ret,
							 info[NOMESSAGE]);
		else
			message(i++, FAIL, "get_cgroup()", ret,
							 info[NOMESSAGE]);
	}

	/* SINGLE & MULTI MOUNT: Create, get and compare a cgroup */

	/* get cgroup_a ds and create group_a in filesystem */
	cgroup_a = create_new_cgroup_ds(ctl1, "group_a", STRING, cval, ids, 0);
	if (fs_mounted == FS_MULTI_MOUNTED) {
		/* Create under another controller also */
		ret = set_controller(ctl2, controller_name, control_file);
		controller = cgroup_add_controller(cgroup_a, controller_name);
		if (controller)
			message(i++, PASS, "cgroup_add_controller()",
					0, info[NOMESSAGE]);
		else
			message(i++, FAIL, "cgroup_add_controller()",
					-1, info[NOMESSAGE]);
	}
	test_cgroup_create_cgroup(0, cgroup_a, "group_a", 0, 1, 1, 00);

	/* create group_b ds to be filled by cgroup_get_cgroup */
	cgroup_b = cgroup_new_cgroup("group_a");
	if (!cgroup_b)
		message(i++, FAIL, "new_cgroup()", 0, info[NOMESSAGE]);
	/* Fill the ds and compare the two */
	ret = cgroup_get_cgroup(cgroup_b);
	if (!ret) {
		ret = cgroup_compare_cgroup(cgroup_a, cgroup_b);
		if (ret == 0)
			message(i++, PASS, "get_cgroup()", ret, info[SAMEGRP]);
		else
			message(i++, FAIL, "get_cgroup()", ret,
							 info[NOMESSAGE]);
	} else {
		message(i++, FAIL, "get_cgroup()", ret, info[NOMESSAGE]);
	}

	/* Delete this created group from fs to leave fs clean */
	if (fs_mounted == FS_MULTI_MOUNTED)
		test_cgroup_delete_cgroup(0, cgroup_a, "group_a", 1, 1, 0, 0);
	else
		test_cgroup_delete_cgroup(0, cgroup_a, "group_a", 0, 1, 0, 0);

	cgroup_free(&cgroup_a);
	cgroup_free(&cgroup_b);
	cgroup_free(&cgroup_filled);
}

/**
 * Tests the cgroup_add_controller() and cgroup_free_controller() wrapper
 * apis under different scenarios
 * @param the test number
 */
void test_cgroup_add_free_controller(int i)
{
	struct cgroup *cgroup1 = NULL, *cgroup2 = NULL;
	struct cgroup_controller *cgctl1, *cgctl2;

	/* Test with a Null cgroup */
	cgctl1 = cgroup_add_controller(cgroup1, "cpu");
	if (!cgctl1)
		message(i++, PASS, "add_controller()", 0, info[NOMESSAGE]);
	else
		message(i++, FAIL, "add_controller()", -1, info[NOMESSAGE]);

	cgroup1 = cgroup_new_cgroup("testgroup");
	cgctl1 = cgroup_add_controller(cgroup1, "cpuset");
	if (cgctl1)
		message(i++, PASS, "add_controller()", 0, info[NOMESSAGE]);
	else
		message(i++, FAIL, "add_controller()", -1, info[NOMESSAGE]);

	cgctl2 = cgroup_add_controller(cgroup1, "cpu");
	if (cgctl2)
		message(i++, PASS, "add_controller()", 0, info[NOMESSAGE]);
	else
		message(i++, FAIL, "add_controller()", -1, info[NOMESSAGE]);

	cgroup_free(&cgroup1);
	cgroup_free_controllers(cgroup2);
}

/**
 * Returns the tid of the current thread
 */
pid_t cgrouptest_gettid()
{
	return syscall(__NR_gettid);
}
