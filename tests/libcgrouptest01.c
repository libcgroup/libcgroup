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
	struct cgroup *nullcgroup = NULL;
	FILE *file;
	char mountpoint[FILENAME_MAX], tasksfile[FILENAME_MAX];

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
