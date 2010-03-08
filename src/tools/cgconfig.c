
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

/* For basename() */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <string.h>
#include <libgen.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <getopt.h>


static void usage(char *progname)
{
	printf("Usage: %s [OPTION] [FILE]\n", basename(progname));
	printf("Parse and load the specified cgroups configuration file\n");
	printf("\n");
	printf("  -h, --help		Display this help\n");
	printf("  -l, --load=FILE	Parse and load the cgroups configuration file\n");
	exit(2);
}

int main(int argc, char *argv[])
{
	int c;
	char filename[PATH_MAX];
	int ret;
	static struct option options[] = {
		{"help", 0, 0, 'h'},
		{"load", 1, 0, 'l'},
		{0, 0, 0, 0}
	};

	if (argc < 2)
		usage(argv[0]); /* usage() exits */

	while ((c = getopt_long(argc, argv, "hl:", options, NULL)) > 0) {
		switch (c) {
		case 'h':
			usage(argv[0]);
			break;
		case 'l':
			strncpy(filename, optarg, PATH_MAX);
			ret = cgroup_config_load_config(filename);
			if (ret) {
				printf("Loading configuration file %s "
					"failed\n%s\n", filename,
					cgroup_strerror(ret));
				exit(3);
			}
			return 0;
		default:
			usage(argv[0]);
			break;
		}
	}
	return 0;
}
