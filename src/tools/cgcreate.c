// SPDX-License-Identifier: LGPL-2.1-only
/**
 * Copyright Red Hat, Inc. 2009
 *
 * Authors:	Ivana Hutarova Varekova <varekova@redhat.com>
 */

#include "tools-common.h"

#include <libcgroup.h>
#include <libcgroup-internal.h>

#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>

#include <sys/types.h>

/*
 * Display the usage
 */
static void usage(int status, const char *program_name)
{
	if (status != 0) {
		err("Wrong input parameters, try %s -h for more information.\n", program_name);
		return;
	}

	info("Usage: %s [-h] [-f mode] [-d mode] [-s mode] ", program_name);
	info("[-t <tuid>:<tgid>] [-a <agid>:<auid>] -g <controllers>:<path> [-g ...]\n");
	info("Create control group(s)\n");
	info("  -a <tuid>:<tgid>		Owner of the group and all its files\n");
	info("  -d, --dperm=mode		Group directory permissions\n");
	info("  -f, --fperm=mode		Group file permissions\n");
	info("  -g <controllers>:<path>	Control group which should be added\n");
	info("  -h, --help			Display this help\n");
	info("  -s, --tperm=mode		Tasks file permissions\n");
	info("  -t <tuid>:<tgid>		Owner of the tasks file\n");
}

int main(int argc, char *argv[])
{
	static struct option long_opts[] = {
		{"help",	      no_argument, NULL, 'h'},
		{"task",	required_argument, NULL, 't'},
		{"admin",	required_argument, NULL, 'a'},
		{"",		required_argument, NULL, 'g'},
		{"dperm",	required_argument, NULL, 'd'},
		{"fperm",	required_argument, NULL, 'f'},
		{"tperm",	required_argument, NULL, 's'},
		{0, 0, 0, 0},
	};

	uid_t tuid = CGRULE_INVALID, auid = CGRULE_INVALID;
	gid_t tgid = CGRULE_INVALID, agid = CGRULE_INVALID;

	struct cgroup_group_spec **cgroup_list;
	struct cgroup_controller *cgc;
	struct cgroup *cgroup;

	/* approximation of max. numbers of groups that will be created */
	int capacity = argc;

	/* permission variables */
	mode_t tasks_mode = NO_PERMS;
	mode_t file_mode = NO_PERMS;
	mode_t dir_mode = NO_PERMS;
	int filem_change = 0;
	int dirm_change = 0;

	int ret = 0;
	int i, j;
	int c;

	/* no parametr on input */
	if (argc < 2) {
		usage(1, argv[0]);
		return -1;
	}

	cgroup_list = calloc(capacity, sizeof(struct cgroup_group_spec *));
	if (cgroup_list == NULL) {
		err("%s: out of memory\n", argv[0]);
		ret = -1;
		goto err;
	}

	/* parse arguments */
	while ((c = getopt_long(argc, argv, "a:t:g:hd:f:s:", long_opts, NULL)) > 0) {
		switch (c) {
		case 'h':
			usage(0, argv[0]);
			ret = 0;
			goto err;
		case 'a':
			/* set admin uid/gid */
			if (parse_uid_gid(optarg, &auid, &agid, argv[0]))
				goto err;
			break;
		case 't':
			/* set task uid/gid */
			if (parse_uid_gid(optarg, &tuid, &tgid, argv[0]))
				goto err;
			break;
		case 'g':
			ret = parse_cgroup_spec(cgroup_list, optarg, capacity);
			if (ret) {
				err("%s: cgroup controller and path parsing failed (%s)\n",
				    argv[0], argv[optind]);
				ret = -1;
				goto err;
			}
			break;
		case 'd':
			dirm_change = 1;
			ret = parse_mode(optarg, &dir_mode, argv[0]);
			if (ret)
				goto err;
			break;
		case 'f':
			filem_change = 1;
			ret = parse_mode(optarg, &file_mode, argv[0]);
			if (ret)
				goto err;
			break;
		case 's':
			filem_change = 1;
			ret = parse_mode(optarg, &tasks_mode, argv[0]);
			if (ret)
				goto err;
			break;
		default:
			usage(1, argv[0]);
			ret = -1;
			goto err;
		}
	}

	/* no cgroup name */
	if (argv[optind]) {
		err("%s: wrong arguments (%s)\n", argv[0], argv[optind]);
		ret = -1;
		goto err;
	}

	/* initialize libcg */
	ret = cgroup_init();
	if (ret) {
		err("%s: libcgroup initialization failed: %s\n", argv[0], cgroup_strerror(ret));
		goto err;
	}

	/* for each new cgroup */
	for (i = 0; i < capacity; i++) {
		if (!cgroup_list[i])
			break;

		/* create the new cgroup structure */
		cgroup = cgroup_new_cgroup(cgroup_list[i]->path);
		if (!cgroup) {
			ret = ECGFAIL;
			err("%s: can't add new cgroup: %s\n", argv[0], cgroup_strerror(ret));
			goto err;
		}

		/* set uid and gid for the new cgroup based on input options */
		ret = cgroup_set_uid_gid(cgroup, tuid, tgid, auid, agid);
		if (ret)
			goto err;

		/* add controllers to the new cgroup */
		j = 0;
		while (cgroup_list[i]->controllers[j]) {
			if (strcmp(cgroup_list[i]->controllers[j], "*") == 0) {
				/* it is meta character, add all controllers */
				ret = cgroup_add_all_controllers(cgroup);
				if (ret != 0) {
					ret = ECGINVAL;
					err("%s: can't add all controllers\n", argv[0]);
					cgroup_free(&cgroup);
					goto err;
				}
			} else {
				cgc = cgroup_add_controller(cgroup,
					cgroup_list[i]->controllers[j]);
				if (!cgc) {
					ret = ECGINVAL;
					err("%s: controller %s can't be add\n", argv[0],
					    cgroup_list[i]->controllers[j]);
					cgroup_free(&cgroup);
					goto err;
				}
			}
			j++;
		}

		/* all variables set so create cgroup */
		if (dirm_change | filem_change)
			cgroup_set_permissions(cgroup, dir_mode, file_mode, tasks_mode);

		ret = cgroup_create_cgroup(cgroup, 0);
		if (ret) {
			err("%s: can't create cgroup %s: %s\n", argv[0], cgroup->name,
			    cgroup_strerror(ret));
			cgroup_free(&cgroup);
			goto err;
		}
		cgroup_free(&cgroup);
	}

err:
	if (cgroup_list) {
		for (i = 0; i < capacity; i++) {
			if (cgroup_list[i])
				cgroup_free_group_spec(cgroup_list[i]);
		}
		free(cgroup_list);
	}

	return ret;
}
