
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

static void usage(int status, char *progname)
{
	if (status != 0) {
		fprintf(stderr, "Wrong input parameters, "\
			"try %s -h' for more information.\n", progname);
		return;
	}
	printf("Usage: %s [-h] [-f mode] [-d mode] [-s mode] "\
		"[-t <tuid>:<tgid>] [-a <agid>:<auid>] [-l FILE] "\
		"[-L DIR] ...\n", progname);
	printf("Parse and load the specified cgroups configuration file\n");
	printf("  -a <tuid>:<tgid>		Default owner of groups files "\
		"and directories\n");
	printf("  -d, --dperm=mode		Default group directory "\
		"permissions\n");
	printf("  -f, --fperm=mode		Default group file "\
		"permissions\n");
	printf("  -h, --help			Display this help\n");
	printf("  -l, --load=FILE		Parse and load the cgroups "\
		"configuration file\n");
	printf("  -L, --load-directory=DIR	Parse and load the cgroups "\
		"configuration files from a directory\n");
	printf("  -s, --tperm=mode		Default tasks file "\
		"permissions\n");
	printf("  -t <tuid>:<tgid>		Default owner of the tasks "\
		"file\n");
}

int main(int argc, char *argv[])
{
	int c, i;
	int ret, error = 0;
	static struct option options[] = {
		{"help", 0, 0, 'h'},
		{"load", 1, 0, 'l'},
		{"load-directory", 1, 0, 'L'},
		{"task", required_argument, NULL, 't'},
		{"admin", required_argument, NULL, 'a'},
		{"dperm", required_argument, NULL, 'd'},
		{"fperm", required_argument, NULL, 'f' },
		{"tperm", required_argument, NULL, 's' },
		{0, 0, 0, 0}
	};
	uid_t tuid = NO_UID_GID, auid = NO_UID_GID;
	gid_t tgid = NO_UID_GID, agid = NO_UID_GID;
	mode_t dir_mode = NO_PERMS;
	mode_t file_mode = NO_PERMS;
	mode_t tasks_mode = NO_PERMS;
	int dirm_change = 0;
	int filem_change = 0;
	struct cgroup *default_group = NULL;

	cgroup_set_default_logger(-1);

	if (argc < 2) {
		usage(1, argv[0]);
		return -1;
	}

	ret = cgroup_init();
	if (ret) {
		fprintf(stderr, "%s: libcgroup initialization failed: %s\n",
			argv[0], cgroup_strerror(ret));
		goto err;
	}

	error = cgroup_string_list_init(&cfg_files, argc/2);
	if (error)
		goto err;

	while ((c = getopt_long(argc, argv, "hl:L:t:a:d:f:s:", options,
			NULL)) > 0) {
		switch (c) {
		case 'h':
			usage(0, argv[0]);
			error = 0;
			goto err;
		case 'l':
			error = cgroup_string_list_add_item(&cfg_files, optarg);
			if (error) {
				fprintf(stderr, "%s: cannot add file to list,"\
						" out of memory?\n", argv[0]);
				goto err;
			}
			break;
		case 'L':
			cgroup_string_list_add_directory(&cfg_files, optarg,
					argv[0]);
			break;
		case 'a':
			/* set admin uid/gid */
			error = parse_uid_gid(optarg, &auid, &agid, argv[0]);
			if (error)
				goto err;
			break;
		case 't':
			/* set task uid/gid */
			error = parse_uid_gid(optarg, &tuid, &tgid, argv[0]);
			if (error)
				goto err;
			break;
		case 'd':
			dirm_change = 1;
			error = parse_mode(optarg, &dir_mode, argv[0]);
			if (error)
				goto err;
			break;
		case 'f':
			filem_change = 1;
			error = parse_mode(optarg, &file_mode, argv[0]);
			if (error)
				goto err;
			break;
		case 's':
			filem_change = 1;
			error = parse_mode(optarg, &tasks_mode, argv[0]);
			if (error)
				goto err;
			break;
		default:
			usage(1, argv[0]);
			error = -1;
			goto err;
		}
	}

	if (argv[optind]) {
		usage(1, argv[0]);
		error = -1;
		goto err;
	}

	/* set default permissions */
	default_group = cgroup_new_cgroup("default");
	if (!default_group) {
		error = -1;
		fprintf(stderr, "%s: cannot create default cgroup\n", argv[0]);
		goto err;
	}

	error = cgroup_set_uid_gid(default_group, tuid, tgid, auid, agid);
	if (error) {
		fprintf(stderr, "%s: cannot set default UID and GID: %s\n",
				argv[0], cgroup_strerror(error));
		goto free_cgroup;
	}

	if (dirm_change | filem_change) {
		cgroup_set_permissions(default_group, dir_mode, file_mode,
				tasks_mode);
	}

	error = cgroup_config_set_default(default_group);
	if (error) {
		fprintf(stderr, "%s: cannot set config parser defaults: %s\n",
				argv[0], cgroup_strerror(error));
		goto free_cgroup;
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

free_cgroup:
	cgroup_free(&default_group);
err:
	cgroup_string_list_free(&cfg_files);
	return error;
}
