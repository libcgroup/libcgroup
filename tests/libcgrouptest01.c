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
	int fs_mounted, retval, i = 0;

	if ((argc < 3) || (atoi(argv[1]) < 0)) {
		printf("ERROR: Wrong no of parameters recieved from script\n");
		printf("Exiting the libcgroup testset\n");
		exit(1);
	}
	fs_mounted = atoi(argv[1]);

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

		break;

	case FS_MOUNTED:

		/*
		 * Test01: call cgroup_init() and check return values
		 * Exp outcome:  no error. return value 0
		 */

		retval = cgroup_init();
		if (retval == 0)
			printf("Test[1:%2d]\tPASS: cgroup_init() retval= %d:\n",\
								 ++i, retval);
		else
			printf("Test[1:%2d]\tFAIL: cgroup_init() retval= %d:\n",\
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
