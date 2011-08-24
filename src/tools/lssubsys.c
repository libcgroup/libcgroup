/* " Copyright (C) 2009 Red Hat, Inc. All Rights Reserved.
 * " Written by Ivana Hutarova Varekova <varekova@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2.1 of the GNU Lesser General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include <libcgroup.h>
#include <libcgroup-internal.h>

enum flag{
    FL_MOUNT = 1,	/* show the mount points */
    FL_LIST = 2,
    FL_ALL = 4,		/* show all subsystems - not mounted too */
    FL_HIERARCHY = 8,	/* show info about hierarchies */
    FL_MOUNT_ALL = 16	/* show all mount points of hierarchies */
};

typedef char cont_name_t[FILENAME_MAX];

static void usage(int status, const char *program_name)
{
	if (status != 0) {
		fprintf(stderr, "Wrong input parameters,"
			" try %s -h' for more information.\n",
			program_name);
	} else {
		fprintf(stdout, "Usage: %s [-m] [controller] [...]\n",
			program_name);
		fprintf(stdout, "Usage: %s -a [-m] \n",
			program_name);
		fprintf(stdout, "List information about given controller(s). "\
			"If no controller is set list information about "\
			"all mounted controllers.\n");
		fprintf(stdout, "  -h, --help			"\
			"Display this help\n");
		fprintf(stdout, "  -m, --mount-points		"\
			"Display mount points\n");
		fprintf(stdout, "  -M, --all-mount-points	"\
			"Display all mount points\n");
		fprintf(stdout, "  -a, --all			"\
			"Display information about all controllers "\
			"(including not mounted ones) \n");
		fprintf(stdout, "  -i, --hierarchies		"\
			"Display information about hierarchies\n");
	}
}

static int print_controller_mount(const char *controller,
		int flags,  cont_name_t cont_names, int hierarchy)
{
	int ret = 0;
	void *handle;
	char path[FILENAME_MAX];

	if (!(flags & FL_MOUNT) && !(flags & FL_HIERARCHY)) {
		/* print only hierarchy name */
		printf("%s\n", cont_names);
	}
	if (!(flags & FL_MOUNT) && (flags & FL_HIERARCHY)) {
		/* print only hierarchy name and number*/
		printf("%s %d\n", cont_names, hierarchy);
	}
	if (flags & FL_MOUNT) {
		/* print hierarchy name and mount point(s) */
		ret = cgroup_get_subsys_mount_point_begin(controller, &handle,
				path);
		/* intentionally ignore error from above call */
		while (ret == 0) {
			printf("%s %s\n", cont_names, path);
			if (!(flags & FL_MOUNT_ALL))
				/* first mount record is enough */
				goto stop;
			ret = cgroup_get_subsys_mount_point_next(&handle, path);
		}
		if (ret == ECGEOF)
			ret = 0;
stop:
		cgroup_get_subsys_mount_point_end(&handle);
	}
	return ret;
}

/* display all controllers attached to the given hierarchy */
static int print_all_controllers_in_hierarchy(const char *tname,
	int hierarchy, int flags)
{
	int ret = 0;
	void *handle;
	struct controller_data info;
	int first = 1;
	cont_name_t cont_names;
	cont_name_t cont_name;

	/*
	 * Initialize libcgroup and intentionally ignore its result,
	 * no mounted controller is valid use case.
	 */
	(void) cgroup_init();

	ret = cgroup_get_all_controller_begin(&handle, &info);
	if ((ret != 0) && (ret != ECGEOF)) {
		fprintf(stderr, "cannot read controller data: %s\n",
			cgroup_strerror(ret));
		return ret;
	}

	while (ret != ECGEOF) {
		/* controller is in the hierrachy */
		if (info.hierarchy != hierarchy)
			goto next;

		if (first) {
			/* the first controller in the hierarchy */
			memset(cont_name, 0, FILENAME_MAX);
			strncpy(cont_name, info.name, FILENAME_MAX-1);
			memset(cont_names, 0, FILENAME_MAX);
			strncpy(cont_names, info.name, FILENAME_MAX-1);
			first = 0;
		} else {
			/* the next controller in the hierarchy */
			strncat(cont_names, ",", FILENAME_MAX-1);
			strncat(cont_names, info.name, FILENAME_MAX-1);
		}
next:
		ret = cgroup_get_all_controller_next(&handle, &info);
		if (ret && ret != ECGEOF)
			goto end;
	}

	ret = print_controller_mount(cont_name,
		flags, cont_names, hierarchy);

end:
	cgroup_get_all_controller_end(&handle);

	if (ret == ECGEOF)
		ret = 0;

	return ret;
}


/* go through the list of all controllers gather them based on hierarchy number
 and print them */
static int cgroup_list_all_controllers(const char *tname,
	cont_name_t cont_name[CG_CONTROLLER_MAX], int c_number, int flags)
{
	int ret = 0;
	void *handle;
	struct controller_data info;

	int h_list[CG_CONTROLLER_MAX];	/* list of hierarchies */
	int counter = 0;
	int j;
	int is_on_list = 0;

	ret = cgroup_get_all_controller_begin(&handle, &info);
	while (ret == 0) {
		if (info.hierarchy == 0) {
			/* the controller is not attached to any hierrachy */
			if (flags & FL_ALL)
				/* display only if -a flag is set */
				printf("%s\n", info.name);
		}
		is_on_list = 0;
		j = 0;
		while ((is_on_list == 0) && (j < c_number)) {
			if (strcmp(info.name, cont_name[j]) == 0) {
				is_on_list = 1;
				break;
			}
			j++;
		}

		if ((info.hierarchy != 0) &&
			((flags & FL_ALL) ||
			(!(flags & FL_LIST) || (is_on_list == 1)))) {
			/* the controller is attached to some hierarchy
			   and either should be output all controllers,
			   or the controller is on the output list */

			h_list[counter] = info.hierarchy;
			counter++;
			for (j = 0; j < counter-1; j++) {
				/*
				 * the hierarchy already was on the list
				 * so remove the new record
				 */
				if (h_list[j] == info.hierarchy) {
					counter--;
					break;
				}
			}
		}

		ret = cgroup_get_all_controller_next(&handle, &info);
	}
	cgroup_get_all_controller_end(&handle);
	if (ret == ECGEOF)
		ret = 0;
	if (ret) {
		fprintf(stderr,
			"cgroup_get_controller_begin/next failed (%s)\n",
			cgroup_strerror(ret));
		return ret;
	}


	for (j = 0; j < counter; j++)
		ret = print_all_controllers_in_hierarchy(tname,
			h_list[j], flags);

	return ret;

}

int main(int argc, char *argv[])
{

	int ret = 0;
	int c;

	int flags = 0;

	int i;
	int c_number = 0;
	cont_name_t cont_name[CG_CONTROLLER_MAX];

	static struct option options[] = {
		{"help", 0, 0, 'h'},
		{"mount-points", 0, 0, 'm'},
		{"all-mount-points", 0, 0, 'M'},
		{"all", 0, 0, 'a'},
		{"hierarchies", 0, 0, 'i'},
		{0, 0, 0, 0}
	};

	for (i = 0; i < CG_CONTROLLER_MAX; i++)
		cont_name[i][0] = '\0';

	/* parse arguments */
	while ((c = getopt_long(argc, argv, "mMhai", options, NULL)) > 0) {
		switch (c) {
		case 'h':
			usage(0, argv[0]);
			return 0;
		case 'm':
			flags |= FL_MOUNT;
			break;
		case 'M':
			flags |= FL_MOUNT | FL_MOUNT_ALL;
			break;
		case 'a':
			flags |= FL_ALL;
			break;
		case 'i':
			flags |= FL_HIERARCHY;
			break;
		default:
			usage(1, argv[0]);
			return -1;
		}
	}

	/* read the list of controllers */
	while (optind < argc) {
		flags |= FL_LIST;
		strncpy(cont_name[c_number], argv[optind], FILENAME_MAX);
		cont_name[c_number][FILENAME_MAX-1] = '\0';
		c_number++;
		optind++;
		if (optind == CG_CONTROLLER_MAX) {
			fprintf(stderr, "Warning: too many parameters\n");
			break;
		}
	}

	ret = cgroup_list_all_controllers(argv[0], cont_name, c_number, flags);

	return ret;
}
