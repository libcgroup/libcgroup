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
 * Description: This file contains the test code for testing libcgroup apis.
 */

#include "libcgrouptest.h"

int main(int argc, char *argv[])
{
	int fs_mounted, retval, i = 0, pass = 0;
	pid_t curr_tid, tid;
	struct cgroup *cgroup1, *nullcgroup = NULL;
	struct cgroup_controller *controller1;
	char controller_name[FILENAME_MAX], control_file[FILENAME_MAX],
			control_val[FILENAME_MAX], path_group[FILENAME_MAX];
	FILE *file;
	char mountpoint[FILENAME_MAX], tasksfile[FILENAME_MAX], group[FILENAME_MAX];

	if ((argc < 3) || (atoi(argv[1]) < 0)) {
		printf("ERROR: Wrong no of parameters recieved from script\n");
		printf("Exiting the libcgroup testset\n");
		exit(1);
	}
	fs_mounted = atoi(argv[1]);
	strcpy(mountpoint, argv[2]);
	dbg("C:DBG: fs_mounted as recieved from script=%d\n", fs_mounted);
	dbg("C:DBG: mountpoint as recieved from script=%s\n", mountpoint);

	/*
	 * get the list of supported controllers
	 */
	get_controllers("cpu", &cpu);
	get_controllers("memory", &memory);
	if (cpu == 0 && memory == 0) {
		fprintf(stderr, "controllers are not enabled in kernel\n");
		fprintf(stderr, "Exiting the libcgroup testcases......\n");
		exit(1);
	}

	/*
	 * Testsets: Testcases are broadly devided into 3 categories based on
	 * filesystem(fs) mount scenario. fs not mounted, fs mounted, fs multi
	 * mounted. Call different apis in these different scenarios.
	 */

	switch (fs_mounted) {

	case FS_NOT_MOUNTED:

		/*
		 * Test01: call cgroup_init() and check return values
		 * Exp outcome: error ECGROUPNOTMOUNTED
		 */

		retval = cgroup_init();
		if (retval == ECGROUPNOTMOUNTED)
			printf("Test[0:%2d]\tPASS: cgroup_init() retval= %d:\n",\
								 ++i, retval);
		else
			printf("Test[0:%2d]\tFAIL: cgroup_init() retval= %d:\n",\
								 ++i, retval);

		/*
		 * Test02: call cgroup_attach_task() with null group
		 * Exp outcome: error non zero return value
		 */
		retval = cgroup_attach_task(nullcgroup);
		if (retval != 0)
			printf("Test[0:%2d]\tPASS: cgroup_attach_task() ret: %d\n",\
								 ++i, retval);
		else
			printf("Test[0:%2d]\tFAIL: cgroup_attach_task() ret: %d\n",\
								 ++i, retval);

		/*
		 * Test03: Create a valid cgroup and check all return values
		 * Exp outcome: no error. 0 return value
		 */

		strncpy(group, "group1", sizeof(group));
		retval = set_controller(MEMORY, controller_name,
				 control_file, control_val, "40960000");
		if (retval)
			fprintf(stderr, "Setting controller failled\n");

		cgroup1 = cgroup_new_cgroup(group, 0, 0, 0, 0);
		if (cgroup1) {
			controller1 = cgroup_add_controller(cgroup1, controller_name);
			if (controller1) {
				retval = cgroup_add_value_string(controller1,
						 control_file, control_val);
				if (!retval)
					printf("Test[0:%2d]\tPASS: cgroup_new_cgroup() success\n", ++i);
				else
					printf("Test[0:%2d]\tFAIL: cgroup_add_value_string()\n", ++i);
				}
			else
				printf("Test[0:%2d]\tFAIL: cgroup_add_controller()\n", ++i);
			}
		else
			printf("Test[0:%2d]\tFAIL: cgroup_new_cgroup() fails\n", ++i);

		/*
		 * Test04: Then Call cgroup_create_cgroup() with this valid group
		 * Exp outcome: non zero return value
		 */
		retval = cgroup_create_cgroup(cgroup1, 1);
		if (retval)
			printf("Test[0:%2d]\tPASS: cgroup_create_cgroup() retval=%d\n", ++i, retval);
		else
			printf("Test[0:%2d]\tFAIL: cgroup_create_cgroup() retval=%d\n", ++i, retval);

		/*
		 * Test05: delete cgroup
		 * Exp outcome: non zero return value but what ?
		 */
		retval = cgroup_delete_cgroup(cgroup1, 1);
		if (retval)
			printf("Test[0:%2d]\tPASS: cgroup_delete_cgroup() retval=%d\n", ++i, retval);
		else
			printf("Test[0:%2d]\tFAIL: cgroup_delete_cgroup() retval=%d\n", ++i, retval);

		cgroup_free(&nullcgroup);
		cgroup_free(&cgroup1);

		break;

	case FS_MOUNTED:

		/*
		 * Test01: call cgroup_attach_task() with null group
		 * without calling cgroup_inti(). We can check other apis too.
		 * Exp outcome: error ECGROUPNOTINITIALIZED
		 */
		retval = cgroup_attach_task(nullcgroup);
		if (retval == ECGROUPNOTINITIALIZED)
			printf("Test[1:%2d]\tPASS: cgroup_attach_task() ret: %d\n",\
								 ++i, retval);
		else
			printf("Test[1:%2d]\tFAIL: cgroup_attach_task() ret: %d\n",\
								 ++i, retval);


		/*
		 * Test02: call cgroup_init() and check return values
		 * Exp outcome:  no error. return value 0
		 */

		retval = cgroup_init();
		if (retval == 0)
			printf("Test[1:%2d]\tPASS: cgroup_init() retval= %d:\n",\
								 ++i, retval);
		else
			printf("Test[1:%2d]\tFAIL: cgroup_init() retval= %d:\n",\
								 ++i, retval);

		/*
		 * Test03: Call cgroup_attach_task() with null group and check if
		 * return values are correct. If yes check if task exists in
		 * root group tasks file
		 * TODO: This test needs some modification in script
		 * Exp outcome: current task should be attached to root group
		 */
		retval = cgroup_attach_task(nullcgroup);
		if (retval == 0) {
			strncpy(tasksfile, mountpoint, sizeof(mountpoint));
			strcat(tasksfile, "/tasks");
			file = fopen(tasksfile, "r");
			if (!file) {
				printf("ERROR: in opening %s\n", tasksfile);
				return -1;
			}

			curr_tid = cgrouptest_gettid();
			while (!feof(file)) {
				fscanf(file, "%u", &tid);
				if (tid == curr_tid) {
					pass = 1;
					break;
				}
			}
			if (pass)
				printf("Test[1:%2d]\tPASS: Task found in %s\n",\
							 ++i, tasksfile);
			else
				printf("Test[1:%2d]\tFAIL: Task not found in %s\n",\
								 ++i, tasksfile);
		} else {
			printf("Test[1:%2d]\tFAIL: cgroup_attach_task() ret: %d\n",\
								 ++i, retval);
		}

		/*
		 * Test04: Call cgroup_attach_task_pid() with null group
		 * and invalid pid
		 * Exp outcome: error
		 */
		retval = cgroup_attach_task_pid(nullcgroup, 0);
		if (retval != 0)
			printf("Test[1:%2d]\tPASS: cgroup_attach_task_pid() ret: %d\n",\
								 ++i, retval);
		else
			printf("Test[1:%2d]\tFAIL: cgroup_attach_task_pid() ret: %d\n",\
								 ++i, retval);

		/*
		 * Test05: Create a valid cgroup structure
		 * Exp outcome: no error. 0 return value
		 */
		strncpy(group, "group1", sizeof(group));
		retval = set_controller(MEMORY, controller_name,
				 control_file, control_val, "40960000");
		if (retval)
			fprintf(stderr, "Setting controller failled\n");

		cgroup1 = cgroup_new_cgroup(group, 0, 0, 0, 0);
		if (cgroup1) {
			controller1 = cgroup_add_controller(cgroup1, controller_name);
			if (controller1) {
				retval = cgroup_add_value_string(controller1,
						 control_file, control_val);
				if (!retval)
					printf("Test[1:%2d]\tPASS: cgroup_new_cgroup() success\n", ++i);
				else
					printf("Test[1:%2d]\tFAIL: cgroup_add_value_string()\n", ++i);
				}
			else
				printf("Test[1:%2d]\tFAIL: cgroup_add_controller()\n", ++i);
			}
		else
			printf("Test[1:%2d]\tFAIL: cgroup_new_cgroup() fails\n", ++i);

		/*
		 * Test06: Then Call cgroup_create_cgroup() with this valid group
		 * Exp outcome: zero return value
		 */
		retval = cgroup_create_cgroup(cgroup1, 1);
		if (!retval) {
			/* Check if the group exists in the dir tree */
			strncpy(path_group, mountpoint, sizeof(mountpoint));
			strncat(path_group, "/group1", sizeof("group1"));
			if (group_exist(path_group) == 0)
				printf("Test[1:%2d]\tPASS: cgroup_create_cgroup() retval=%d\n",
										 ++i, retval);
		} else
			printf("Test[1:%2d]\tFAIL: cgroup_create_cgroup() retval=%d\n", ++i, retval);

		/*
		 * Test07: delete cgroup
		 * Exp outcome: zero return value
		 */
		retval = cgroup_delete_cgroup(cgroup1, 1);
		if (!retval) {
			/* Check if the group is deleted from the dir tree */
			strncpy(path_group, mountpoint, sizeof(mountpoint));
			strncat(path_group, "/group1", sizeof("group1"));
			if (group_exist(path_group) == -1)
				printf("Test[1:%2d]\tPASS: cgroup_delete_cgroup() retval=%d\n",
										 ++i, retval);
		} else
			printf("Test[1:%2d]\tFAIL: cgroup_delete_cgroup() retval=%d\n", ++i, retval);

		cgroup_free(&nullcgroup);
		cgroup_free(&cgroup1);

		break;

	case FS_MULTI_MOUNTED:
		/*
		 * Test01: call apis and check return values
		 * Exp outcome:
		 */
		/*
		 * Scenario 1: cgroup fs is multi mounted
		 * Exp outcome: no error. 0 return value
		 */

		retval = cgroup_init();
		if (retval == 0)
			printf("Test[2:%2d]\tPASS: cgroup_init() retval= %d:\n",\
								 ++i, retval);
		else
			printf("Test[2:%2d]\tFAIL: cgroup_init() retval= %d:\n",\
								 ++i, retval);

		/*
		 * Will add further testcases in separate patchset
		 */

		break;

	default:
		fprintf(stderr, "ERROR: Wrong parameters recieved from script. \
						Exiting tests\n");
		exit(1);
		break;
	}
	return 0;
}

void get_controllers(char *name, int *exist)
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
}

static int group_exist(char *path_group)
{
	int ret;
	ret = open(path_group, O_DIRECTORY);
	if (ret == -1)
		return ret;
	return 0;
}

static int set_controller(int controller, char *controller_name,
			 char *control_file, char *control_val, char *value)
{
	switch (controller) {
	case MEMORY:
		if (memory == 0)
			return 1;

		strncpy(controller_name, "memory", FILENAME_MAX);
		strncpy(control_file, "memory.limit_in_bytes", FILENAME_MAX);
		strncpy(control_val, value, FILENAME_MAX);
		return 0;
		break;

	case CPU:
		if (cpu == 0)
			return 1;

		strncpy(controller_name, "cpu", FILENAME_MAX);
		strncpy(control_file, "cpu.shares", FILENAME_MAX);
		strncpy(control_val, value, FILENAME_MAX);
		return 0;
		break;
		/* Future controllers can be added here */

	default:
		return 1;
		break;
	}
}
