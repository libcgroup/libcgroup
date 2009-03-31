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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libcgroup.h>
#include <limits.h>
#include <pwd.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "tools-common.h"

#define TEMP_BUF	81

/*
 * Go through /proc/<pid>/status file to determine the euid of the
 * process.
 * It returns 0 on success and negative values on failure.
 */

int euid_of_pid(pid_t pid)
{
	FILE *fp;
	char path[FILENAME_MAX];
	char buf[TEMP_BUF];
	uid_t ruid, euid, suid, fsuid;

	sprintf(path, "/proc/%d/status", pid);
	fp = fopen(path, "r");
	if (!fp) {
		cgroup_dbg("Error in opening file %s:%s\n", path,
				strerror(errno));
		return -1;
	}

	while (fgets(buf, TEMP_BUF, fp)) {
		if (!strncmp(buf, "Uid:", 4)) {
			sscanf((buf + 5), "%d%d%d%d", (int *)&ruid,
				(int *)&euid, (int *)&suid, (int *)&fsuid);
			cgroup_dbg("Scanned proc values are %d %d %d %d\n",
				ruid, euid, suid, fsuid);
			fclose(fp);
			return euid;
		}
	}
	fclose(fp);

	/* If we are here, we could not find euid. Return error. */
	return -1;
}

/*
 * Go through /proc/<pid>/status file to determine the egid of the
 * process.
 * It returns 0 on success and negative values on failure.
 */

int egid_of_pid(pid_t pid)
{
	FILE *fp;
	char path[FILENAME_MAX];
	char buf[TEMP_BUF];
	gid_t rgid, egid, sgid, fsgid;

	sprintf(path, "/proc/%d/status", pid);
	fp = fopen(path, "r");
	if (!fp) {
		cgroup_dbg("Error in opening file %s:%s\n", path,
				strerror(errno));
		return -1;
	}

	while (fgets(buf, TEMP_BUF, fp)) {
		if (!strncmp(buf, "Gid:", 4)) {
			sscanf((buf + 5), "%d%d%d%d", (int *)&rgid,
				(int *)&egid, (int *)&sgid, (int *)&fsgid);
			cgroup_dbg("Scanned proc values are %d %d %d %d\n",
				rgid, egid, sgid, fsgid);
			return egid;
		}
	}

	/* If we are here, we could not find egid. Return error. */
	return -1;
}

/*
 * Change process group as specified on command line.
 */
int change_group_path(pid_t pid, struct cgroup_group_spec *cgroup_list[])
{
	int i;
	int ret = 0;

	for (i = 0; i < CG_HIER_MAX; i++) {
		if (!cgroup_list[i])
			break;

		ret = cgroup_change_cgroup_path(cgroup_list[i]->path, pid,
			cgroup_list[i]->controllers);
		if (ret)
			fprintf(stderr, "Error changing group of pid %d: %s\n",
				pid, cgroup_strerror(ret));
			return -1;
	}

	return 0;
}

/*
 * Change process group as specified in cgrules.conf.
 */
int change_group_uid_gid(pid_t pid)
{
	uid_t euid;
	gid_t egid;
	int ret;

	/* Put pid into right cgroup as per rules in /etc/cgrules.conf */
	euid = euid_of_pid(pid);
	if (euid == -1) {
		fprintf(stderr, "Error in determining euid of"
		" pid %d\n", pid);
		return -1;
	}

	egid = egid_of_pid(pid);
	if (egid == -1) {
		fprintf(stderr, "Error in determining egid of"
		" pid %d\n", pid);
		return -1;
	}

	/* Change the cgroup by determining the rules based on uid */
	ret = cgroup_change_cgroup_uid_gid(euid, egid, pid);
	if (ret) {
		fprintf(stderr, "Error: change of cgroup failed for"
		" pid %d: %s\n",
		pid, cgroup_strerror(ret));
		return -1;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int ret = 0, i, exit_code = 0;
	pid_t pid;
	int cg_specified = 0;
	struct cgroup_group_spec *cgroup_list[CG_HIER_MAX];
	int c;


	if (argc < 2) {
		fprintf(stderr, "usage is %s "
			"[-g <list of controllers>:<relative path to cgroup>] "
			"<list of pids>  \n",
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
			exit(2);
			break;
		}
	}


	/* Initialize libcg */
	ret = cgroup_init();
	if (ret) {
		fprintf(stderr, "libcgroup initialization failed:%d\n", ret);
		return ret;
	}

	for (i = optind; i < argc; i++) {
		pid = (uid_t) atoi(argv[i]);

		if (cg_specified)
			ret = change_group_path(pid, cgroup_list);
		else
			ret = change_group_uid_gid(pid);

		/* if any group change fails */
		if (ret)
			exit_code = 1;
	}
	return exit_code;

}
