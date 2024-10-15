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
#ifdef WITH_SYSTEMD
	info("  -b				Ignore default systemd");
	info("delegate hierarchy\n");
	info("  -c, --scope			Create a delegated systemd scope\n");
	info("  -p, --pid=pid			Task pid to use to create systemd ");
	info("scope\n");
	info("  -S, --setdefault		Set this scope as the default scope ");
	info("delegate hierarchy\n");
#endif
}

#ifdef WITH_SYSTEMD
static int create_systemd_scope(struct cgroup * const cgrp, const char * const prog_name,
				int set_default, pid_t pid)
{
	struct cgroup_systemd_scope_opts opts;
	char slice[FILENAME_MAX];
	char *scope;
	int ret;
	int len;

	ret = cgroup_set_default_scope_opts(&opts);
	if (ret)
		return ret;

	opts.pid = pid;

	ret = cgroup_create_scope2(cgrp, 0, &opts);
	if (!ret && set_default) {
		scope = strstr(cgrp->name, "/");
		if (!scope) {
			err("%s: Invalid scope name %s, expected <slice>/<scope>\n",
			    prog_name, cgrp->name);
			ret = ECGINVAL;
			goto err;
		}
		len = strlen(cgrp->name) - strlen(scope);
		strncpy(slice, cgrp->name, FILENAME_MAX - 1);
		slice[len] = '\0';
		scope++;

		ret = cgroup_write_systemd_default_cgroup(slice, scope);
		/*
		 * cgroup_write_systemd_default_cgroup() returns 0 on failure
		 */
		if (ret == 0) {
			err("%s: failed to write default %s/%s to /var/run/libcgroup/systemd\n",
			    prog_name, slice, scope);
			ret = ECGINVAL;
			goto err;
		}

		/*
		 * the default was successfully set.  Override the return of "1" back to
		 * the usual "0" on success.
		 */
		ret = 0;
	}

err:
	return ret;
}
#else
static int create_systemd_scope(struct cgroup * const cgrp, const char * const prog_name,
				int set_default, pid_t pid)
{
	return ECGINVAL;
}
#endif /* WITH_SYSTEMD */

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
#ifdef WITH_SYSTEMD
		{"scope",	      no_argument, NULL, 'c'},
		{"setdefault",	      no_argument, NULL, 'S'},
		{"pid",		required_argument, NULL, 'p'},
#endif /* WITH_SYSTEMD */
		{0, 0, 0, 0},
	};

	uid_t tuid = CGRULE_INVALID, auid = CGRULE_INVALID;
	gid_t tgid = CGRULE_INVALID, agid = CGRULE_INVALID;

#ifdef WITH_SYSTEMD
	int ignore_default_systemd_delegate_slice = 0;
#endif
	int set_default_scope = 0;
	int create_scope = 0;
	pid_t scope_pid = -1;

	struct cgroup_group_spec **cgrp_list;
	struct cgroup_controller *cgc;
	struct cgroup *cgrp;

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

	/* no parameter on input */
	if (argc < 2) {
		usage(1, argv[0]);
		exit(EXIT_BADARGS);
	}

	cgrp_list = calloc(capacity, sizeof(struct cgroup_group_spec *));
	if (cgrp_list == NULL) {
		err("%s: out of memory\n", argv[0]);
		ret = -1;
		goto err;
	}

#ifdef WITH_SYSTEMD
	/* parse arguments */
	while ((c = getopt_long(argc, argv, "a:t:g:hd:f:s:bcp:S", long_opts, NULL)) > 0) {
		switch (c) {
		case 'b':
			ignore_default_systemd_delegate_slice = 1;
			break;
		case 'c':
			create_scope = 1;
			break;
		case 'p':
			scope_pid = atoi(optarg);
			if (scope_pid <= 1) {
				err("%s: Invalid pid %s\n", argv[0], optarg);
				ret = EXIT_BADARGS;
				goto err;
			}
			break;
		case 'S':
			set_default_scope = 1;
			break;
#else
	while ((c = getopt_long(argc, argv, "a:t:g:hd:f:s:", long_opts, NULL)) > 0) {
		switch (c) {
#endif
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
			ret = parse_cgroup_spec(cgrp_list, optarg, capacity);
			if (ret) {
				err("%s: cgroup controller and path parsing failed (%s)\n",
				    argv[0], argv[optind]);
				ret = EXIT_BADARGS;
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
			ret = EXIT_BADARGS;
			goto err;
		}
	}

#ifdef WITH_SYSTEMD
	if (ignore_default_systemd_delegate_slice && create_scope) {
		err("%s: \"-b\" and \"-c\" are mutually exclusive\n", argv[0]);
		ret = EXIT_BADARGS;
		goto err;
	}

	if (set_default_scope && !create_scope) {
		err("%s: \"-S\" requires \"-c\" to be provided\n", argv[0]);
		ret = EXIT_BADARGS;
		goto err;
	}

	if (!create_scope && scope_pid != -1) {
		err("%s: \"-p\" requires \"-c\" to be provided\n", argv[0]);
		ret = EXIT_BADARGS;
		goto err;
	}
#endif

	/* no cgroup name */
	if (argv[optind]) {
		err("%s: wrong arguments (%s)\n", argv[0], argv[optind]);
		ret = EXIT_BADARGS;
		goto err;
	}

	/* initialize libcgroup */
	ret = cgroup_init();
	if (ret) {
		err("%s: libcgroup initialization failed: %s\n", argv[0], cgroup_strerror(ret));
		goto err;
	}

#ifdef WITH_SYSTEMD
	if (!create_scope && !ignore_default_systemd_delegate_slice)
		cgroup_set_default_systemd_cgroup();
#endif

	/* for each new cgroup */
	for (i = 0; i < capacity; i++) {
		if (!cgrp_list[i])
			break;

		/* create the new cgroup structure */
		cgrp = cgroup_new_cgroup(cgrp_list[i]->path);
		if (!cgrp) {
			ret = ECGFAIL;
			err("%s: can't add new cgroup: %s\n", argv[0], cgroup_strerror(ret));
			goto err;
		}

		/* set uid and gid for the new cgroup based on input options */
		ret = cgroup_set_uid_gid(cgrp, tuid, tgid, auid, agid);
		if (ret)
			goto err;

		/* add controllers to the new cgroup */
		j = 0;
		while (cgrp_list[i]->controllers[j]) {
			if (strcmp(cgrp_list[i]->controllers[j], "*") == 0) {
				/* it is meta character, add all controllers */
				ret = cgroup_add_all_controllers(cgrp);
				if (ret != 0) {
					ret = ECGINVAL;
					err("%s: can't add all controllers\n", argv[0]);
					cgroup_free(&cgrp);
					goto err;
				}
			} else {
				cgc = cgroup_add_controller(cgrp,
					cgrp_list[i]->controllers[j]);
				if (!cgc) {
					ret = ECGINVAL;
					err("%s: controller %s can't be add\n", argv[0],
					    cgrp_list[i]->controllers[j]);
					cgroup_free(&cgrp);
					goto err;
				}
			}
			j++;
		}

		/* all variables set so create cgroup */
		if (dirm_change | filem_change)
			cgroup_set_permissions(cgrp, dir_mode, file_mode, tasks_mode);

		if (create_scope)
			ret = create_systemd_scope(cgrp, argv[0], set_default_scope, scope_pid);
		else
			ret = cgroup_create_cgroup(cgrp, 0);
		if (ret) {
			err("%s: can't create cgroup %s: %s\n", argv[0], cgrp->name,
			    cgroup_strerror(ret));
			cgroup_free(&cgrp);
			goto err;
		}
		cgroup_free(&cgrp);
	}

err:
	if (cgrp_list) {
		for (i = 0; i < capacity; i++) {
			if (cgrp_list[i])
				cgroup_free_group_spec(cgrp_list[i]);
		}
		free(cgrp_list);
	}

	return ret;
}
