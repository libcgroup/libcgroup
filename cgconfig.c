
/*
 * Copyright IBM Corporation. 2007
 *
 * Authors:	Dhaval Giani <dhaval@linux.vnet.ibm.com>
 * 		Balbir Singh <balbir@linux.vnet.ibm.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2.1 of the GNU Lesser General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Code initiated and designed by Dhaval Giani. All faults are most likely
 * his mistake.
 */

#include <libcgroup.h>
#include <libcgroup-internal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

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

	while ((c = getopt(argc, argv, "l:")) > 0) {
		switch (c) {
		case 'l':
			strncpy(filename, optarg, PATH_MAX);
			ret = cgroup_config_load_config(filename);
			if (ret) {
				printf("Loading configuration file %s "
					"failed, error: %s\n", filename,
					strerror(errno));
				printf("return code = %d\n", ret);
				exit(3);
			}
			return 0;
		default:
			fprintf(stderr, "Invalid command line option\n");
			break;
		}
	}
	fprintf(stderr, "usage is %s <option> <config file>\n",
		argv[0]);
	return 0;
}
