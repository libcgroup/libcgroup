/*
 * Copyright IBM Corporation. 2009
 *
 * Authors:	Dhaval Giani <dhaval@linux.vnet.ibm.com>
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
#include <getopt.h>
#include "tools-common.h"

static struct cgroup_string_list cfg_files;

static void usage(int status, const char *program_name)
{
	if (status != 0) {
		fprintf(stderr, "Wrong input parameters,"
			" try %s -h' for more information.\n",
			program_name);
		return;
	}
	printf("Usage: %s [-h] [-l FILE] [-L DIR] [-e]\n",
		program_name);
	printf("Unload the cgroup filesystem\n");
	printf("  -e, --empty			Remove only empty cgroups\n");
	printf("  -h, --help			Display this help\n");
	printf("  -l, --load=FILE		Parse and load the cgroups "\
		"configuration file\n");
	printf("  -L, --load-directory=DIR	Parse and load the cgroups "\
		"configuration files from a directory\n");
}

static void report_error(int error, const char *program_name)
{
	/* Don't spit an error when there is nothing to clear. */
	if (error == ECGROUPNOTMOUNTED)
		error = 0;
	if (error) {
		printf("%s failed with %s\n", program_name,
				cgroup_strerror(error));
	}
}


int main(int argc, char *argv[])
{
	int error = 0, ret;
	int c;
	int unload_all = 1;
	int flags = CGFLAG_DELETE_RECURSIVE;

	struct option longopts[] = {
			{"load", required_argument, 0,  'l' },
			{"load-directory", required_argument, 0,  'L' },
			{"only-empty", no_argument, 0,  'e' },
			{"help", no_argument, 0, 'h'},
			{ 0, 0, 0, 0}
	};

	ret = cgroup_string_list_init(&cfg_files, argc/2);
	if (ret) {
		fprintf(stderr, "%s: cannot initialize list of files,"
				" out of memory?\n",
				argv[0]);
		exit(1);
	}

	while ((c = getopt_long(argc, argv, "hl:L:e", longopts, NULL)) > 0) {
		switch (c) {
		case 'e':
			flags = CGFLAG_DELETE_EMPTY_ONLY;
			break;

		case 'l':
			unload_all = 0;
			ret = cgroup_string_list_add_item(&cfg_files, optarg);
			if (ret) {
				fprintf(stderr, "%s: cannot add file to list,"\
						" out of memory?\n", argv[0]);
				exit(1);
			}
			break;

		case 'L':
			unload_all = 0;
			cgroup_string_list_add_directory(&cfg_files, optarg,
					argv[0]);
			break;

		case 'h':
			usage(0, argv[0]);
			exit(0);
		default:
			usage(1, argv[0]);
			exit(1);
		}
	}

	if (unload_all) {
		error = cgroup_unload_cgroups();
		if (error)
			report_error(error, argv[0]);
	} else {
		int i;

		ret = cgroup_init();
		if (ret) {
			report_error(ret, argv[0]);
			exit(4);
		}
		/* process the config files in reverse order */
		for (i = cfg_files.count-1; i >= 0 ; i--) {
			ret = cgroup_config_unload_config(cfg_files.items[i],
					flags);
			if (ret && ret != ECGNONEMPTY) {
				report_error(ret, argv[0]);
				if (!error)
					error = ret;
			}
		}
	}
	cgroup_string_list_free(&cfg_files);
	if (error)
		exit(3);

	return 0;
}
