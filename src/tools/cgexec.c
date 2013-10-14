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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

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

static struct option longopts[] = {
	{"sticky", no_argument, NULL, 's'},
	{"help", no_argument, NULL, 'h'},
	{0, 0, 0, 0}
};

static void usage(int status, const char *program_name)
{
	if (status != 0) {
		fprintf(stderr, "Wrong input parameters,"
			" try %s --help' for more information.\n",
			program_name);
		return;
	}
	printf("Usage: %s [-h] [-g <controllers>:<path>] [--sticky] "\
		"command [arguments] ...\n", program_name);
	printf("Run the task in given control group(s)\n");
	printf("  -g <controllers>:<path>	Control group which "\
		"should be added\n");
	printf("  -h, --help			Display this help\n");
	printf("  --sticky			cgred daemon does not "\
		"change pidlist and children tasks\n");
}


int main(int argc, char *argv[])
{
	int ret = 0, i;
	int cg_specified = 0;
	int flag_child = 0;
	uid_t uid;
	gid_t gid;
	pid_t pid;
	int c;
	struct cgroup_group_spec *cgroup_list[CG_HIER_MAX];

	memset(cgroup_list, 0, sizeof(cgroup_list));

	while ((c = getopt_long(argc, argv, "+g:sh", longopts, NULL)) > 0) {
		switch (c) {
		case 'g':
			ret = parse_cgroup_spec(cgroup_list, optarg,
					CG_HIER_MAX);
			if (ret) {
				fprintf(stderr, "cgroup controller and path"
						"parsing failed\n");
				return -1;
			}
			cg_specified = 1;
			break;
		case 's':
			flag_child |= CGROUP_DAEMON_UNCHANGE_CHILDREN;
			break;
		case 'h':
			usage(0, argv[0]);
			exit(0);
		default:
			usage(1, argv[0]);
			exit(1);
		}
	}

	/* Executable name */
	if (!argv[optind]) {
		usage(1, argv[0]);
		exit(1);
	}

	/* Initialize libcg */
	ret = cgroup_init();
	if (ret) {
		fprintf(stderr, "libcgroup initialization failed: %s\n",
			cgroup_strerror(ret));
		return ret;
	}

	/* Just for debugging purposes. */
	uid = geteuid();
	gid = getegid();
	cgroup_dbg("My euid and egid is: %d,%d\n", (int) uid, (int) gid);

	uid = getuid();
	gid = getgid();
	pid = getpid();

	ret = cgroup_register_unchanged_process(pid, flag_child);
	if (ret) {
		fprintf(stderr, "registration of process failed\n");
		return ret;
	}

	/*
	 * 'cgexec' command file needs the root privilege for executing
	 * a cgroup_register_unchanged_process() by using unix domain
	 * socket, and an euid/egid should be changed to the executing user
	 * from a root user.
	 */
	if (setresuid(uid, uid, uid)) {
		fprintf(stderr, "%s", strerror(errno));
		return -1;
	}
	if (setresgid(gid, gid, gid)) {
		fprintf(stderr, "%s", strerror(errno));
		return -1;
	}
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
                                                        (const char*const*) cgroup_list[i]->controllers);
			if (ret) {
				fprintf(stderr,
					"cgroup change of group failed\n");
				return ret;
			}
		}
	} else {

		/* Change the cgroup by determining the rules based on uid */
		ret = cgroup_change_cgroup_flags(uid, gid,
						 argv[optind], pid, 0);
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
