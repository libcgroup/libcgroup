
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
#include "tools-common.h"

static struct cgroup_string_list cfg_files;


static void usage(char *progname)
{
	printf("Usage: %s [-l FILE] ...\n", basename(progname));
	printf("Parse and load the specified cgroups configuration file\n");
	printf("\n");
	printf("  -h, --help			Display this help\n");
	printf("  -l, --load=FILE		Parse and load the cgroups"\
			" configuration file\n");
	printf("  -L, --load-directory=DIR	Parse and load the cgroups"\
			" configuration files from a directory\n");
	exit(2);
}

int main(int argc, char *argv[])
{
	int c, i;
	int ret, error = 0;
	static struct option options[] = {
		{"help", 0, 0, 'h'},
		{"load", 1, 0, 'l'},
		{"load-directory", 1, 0, 'L'},
		{0, 0, 0, 0}
	};

	if (argc < 2)
		usage(argv[0]); /* usage() exits */

	ret = cgroup_string_list_init(&cfg_files, argc/2);

	while ((c = getopt_long(argc, argv, "hl:L:", options, NULL)) > 0) {
		switch (c) {
		case 'h':
			usage(argv[0]);
			break;
		case 'l':
			ret = cgroup_string_list_add_item(&cfg_files, optarg);
			if (ret) {
				fprintf(stderr, "%s: cannot add file to list,"\
						" out of memory?\n", argv[0]);
				exit(1);
			}
			break;
		case 'L':
			cgroup_string_list_add_directory(&cfg_files, optarg,
					argv[0]);
			break;
		default:
			usage(argv[0]);
			break;
		}
	}

	for (i = 0; i < cfg_files.count; i++) {
		ret = cgroup_config_load_config(cfg_files.items[i]);
		if (ret) {
			fprintf(stderr, "%s; error loading %s: %s\n", argv[0],
					cfg_files.items[i],
					cgroup_strerror(ret));
			if (!error)
				error = ret;
		}
	}

	cgroup_string_list_free(&cfg_files);
	return error;
}
