/*
 * Copyright RedHat Inc. 2009
 *
 * Authors:	Jan Safranek <jsafrane@redhat.com>
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

#include <libcgroup.h>
#include <libcgroup-internal.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>

#include "tools-common.h"

static struct option const long_options[] =
{
	{"recursive", no_argument, NULL, 'r'},
	{"help", no_argument, NULL, 'h'},
	{"group", required_argument, NULL, 'g'},
	{NULL, 0, NULL, 0}
};

static void usage(int status, const char *program_name)
{
	if (status != 0)
		fprintf(stderr, "Wrong input parameters,"
			" try %s --help' for more information.\n",
			program_name);
	else {
		fprintf(stdout, "Usage: %s [-h] [-r] "\
			"[[-g] <controllers>:<path>] ...\n", program_name);
		fprintf(stdout, "Remove control group(s)\n");
		fprintf(stdout, "  -g <controllers>:<path>   "\
			"Control group to be removed (-g is optional)\n");
		fprintf(stdout, "  -h, --help                "\
			"Display this help\n");
		fprintf(stdout, "  -r, --recursive           "\
			"Recursively remove all subgroups\n");
	}
}


int main(int argc, char *argv[])
{
	int ret = 0;
	int i, j;
	int c;
	int flags = 0;
	int final_ret = 0;

	struct cgroup_group_spec **cgroup_list = NULL;
	struct cgroup *cgroup;
	struct cgroup_controller *cgc;

	/* initialize libcg */
	ret = cgroup_init();
	if (ret) {
		fprintf(stderr, "%s: "
			"libcgroup initialization failed: %s\n",
			argv[0], cgroup_strerror(ret));
		goto err;
	}

	cgroup_list = calloc(argc, sizeof(struct cgroup_group_spec *));
	if (cgroup_list == NULL) {
		fprintf(stderr, "%s: out of memory\n", argv[0]);
		ret = -1;
		goto err;
	}

	/*
	 * Parse arguments
	 */
	while ((c = getopt_long(argc, argv, "rhg:",
		long_options, NULL)) > 0) {
		switch (c) {
		case 'r':
			flags |= CGFLAG_DELETE_RECURSIVE;
			break;
		case 'g':
			ret = parse_cgroup_spec(cgroup_list, optarg, argc);
			if (ret != 0) {
				fprintf(stderr,
					"%s: error parsing cgroup '%s'\n",
					argv[0], optarg);
				ret = -1;
				goto err;
			}
			break;
		case 'h':
			usage(0, argv[0]);
			ret = 0;
			goto err;
		default:
			usage(1, argv[0]);
			ret = -1;
			goto err;
		}
	}

	/* parse groups on command line */
	for (i = optind; i < argc; i++) {
		ret = parse_cgroup_spec(cgroup_list, argv[i], argc);
		if (ret != 0) {
			fprintf(stderr, "%s: error parsing cgroup '%s'\n",
					argv[0], argv[i]);
			ret = -1;
			goto err;
		}
	}

	/* for each cgroup to be deleted */
	for (i = 0; i < argc; i++) {
		if (!cgroup_list[i])
			break;

		/* create the new cgroup structure */
		cgroup = cgroup_new_cgroup(cgroup_list[i]->path);
		if (!cgroup) {
			ret = ECGFAIL;
			fprintf(stderr, "%s: can't create new cgroup: %s\n",
				argv[0], cgroup_strerror(ret));
			goto err;
		}

		/* add controllers to the cgroup */
		j = 0;
		while (cgroup_list[i]->controllers[j]) {
			cgc = cgroup_add_controller(cgroup,
				cgroup_list[i]->controllers[j]);
			if (!cgc) {
				ret = ECGFAIL;
				fprintf(stderr, "%s: "
					"controller %s can't be added\n",
					argv[0],
					cgroup_list[i]->controllers[j]);
				cgroup_free(&cgroup);
				goto err;
			}
			j++;
		}

		ret = cgroup_delete_cgroup_ext(cgroup, flags);
		/*
		 * Remember the errors and continue, try to remove all groups.
		 */
		if (ret != 0) {
			fprintf(stderr, "%s: cannot remove group '%s': %s\n",
					argv[0], cgroup->name,
					cgroup_strerror(ret));
			final_ret = ret;
		}
		cgroup_free(&cgroup);
	}

	ret = final_ret;
err:
	if (cgroup_list) {
		for (i = 0; i < argc; i++) {
			if (cgroup_list[i])
				cgroup_free_group_spec(cgroup_list[i]);
		}
		free(cgroup_list);
	}
	return ret;
}
