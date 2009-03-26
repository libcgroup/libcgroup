/*
 * Copyright RedHat Inc. 2008
 *
 * Authors:	Vivek Goyal <vgoyal@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2.1 of the GNU Lesser General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#include <errno.h>
#include <grp.h>
#include <libcgroup.h>
#include <limits.h>
#include <pwd.h>
#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "tools-common.h"

int main(int argc, char *argv[])
{
	int ret = 0, i;
	int cg_specified = 0;
	uid_t euid;
	pid_t pid;
	gid_t egid;
	char c;
	struct cgroup_group_spec *cgroup_list[CG_HIER_MAX];

	if (argc < 2) {
		fprintf(stderr, "Usage is %s"
			" [-g <list of controllers>:<relative path to cgroup>]"
			" command [arguments]  \n",
			argv[0]);
		exit(2);
	}

	memset(cgroup_list, 0, sizeof(cgroup_list));

	while ((c = getopt(argc, argv, "+g:")) > 0) {
		switch (c) {
		case 'g':
			if (parse_cgroup_spec(cgroup_list, optarg)) {
				fprintf(stderr, "cgroup controller and path"
						"parsing failed\n");
				return -1;
			}
			cg_specified = 1;
			break;
		default:
			fprintf(stderr, "Invalid command line option\n");
			exit(1);
			break;
		}
	}

	/* Executable name */
	if (!argv[optind]) {
		fprintf(stderr, "No command specified\n");
		exit(1);
	}

	/* Initialize libcg */
	ret = cgroup_init();
	if (ret) {
		fprintf(stderr, "libcgroup initialization failed:%d", ret);
		return ret;
	}

	euid = geteuid();
	egid = getegid();
	pid = getpid();

	if (cg_specified) {
		/*
		 * User has specified the list of control group and
		 * controllers
		 * */
		for (i = 0; i < CG_HIER_MAX; i++) {
			if (!cgroup_list[i])
				break;

			ret = cgroup_change_cgroup_path(cgroup_list[i]->path,
							pid,
						cgroup_list[i]->controllers);
			if (ret) {
				fprintf(stderr,
					"cgroup change of group failed\n");
				return ret;
			}
		}
	} else {

		/* Change the cgroup by determining the rules based on euid */
		ret = cgroup_change_cgroup_uid_gid(euid, egid, pid);
		if (ret) {
			fprintf(stderr, "cgroup change of group failed\n");
			return ret;
		}
	}

	/* Now exec the new process */
	ret = execvp(argv[optind], &argv[optind]);
	if (ret == -1) {
		fprintf(stderr, "%s", strerror(errno));
		return -1;
	}
	return 0;
}
