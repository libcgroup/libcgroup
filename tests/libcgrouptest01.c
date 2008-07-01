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
	int fs_mounted;

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
		 * Test01: call apis and check return values
		 * Exp outcome:
		 */
		printf("First set\n");

		break;

	case FS_MOUNTED:
		/*
		 * Test01: call apis and check return values
		 * Exp outcome:
		 */
		printf("Second set\n");

		break;

	case FS_MULTI_MOUNTED:
		/*
		 * Test01: call apis and check return values
		 * Exp outcome:
		 */
		printf("Third set\n");
		/*
		 * Will add testcases once multiple mount patch is in
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
