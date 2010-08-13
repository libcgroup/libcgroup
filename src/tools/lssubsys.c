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
    FL_HIERARCHY = 8	/* show info about hierarchies */
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
		fprintf(stdout, "  -h, --help		Display this help\n");
		fprintf(stdout, "  -m, --mount-points	Display mount points\n");
		fprintf(stdout, "  -a, --all		"\
			"Display information about all controllers "\
			"(including not mounted ones) \n");
		fprintf(stdout, "  -i, --hierarchies	Display information "\
			"about hierarchies\n");
	}
}

/* print data about input cont_name controller */
static int print_controller(cont_name_t cont_name, int flags)
{
	int ret = 0;
	char name[FILENAME_MAX];
	char path[FILENAME_MAX];
	void *handle;
	struct cgroup_mount_point controller;
	int output = 0;

	ret = cgroup_get_controller_begin(&handle, &controller);
	if (ret != 0) {
		fprintf(stderr, "cannot read controller data: %s\n",
			cgroup_strerror(ret));
		return ret;
	}

	path[0] = '\0';
	name[0] = '\0';

	/* go through the list of controllers */
	while (ret == 0) {
		if (strcmp(path, controller.path) == 0) {
			/* if it is still the same mount point */
			strncat(name, "," , FILENAME_MAX-strlen(name)-1);
			strncat(name, controller.name,
				FILENAME_MAX-strlen(name)-1);
		} else {
			/*
			 * we got new mount point
			 * print the old one if needed
			 */
			if (output) {
				if ((flags &  FL_MOUNT) != 0)
					printf("%s %s\n", name, path);
				else
					printf("%s\n", name);
				if ((flags & FL_LIST) != 0) {
					/* we succesfully finish printing */
					output = 0;
					break;
				}
			}

			output = 0;
			strncpy(name, controller.name, FILENAME_MAX);
			name[FILENAME_MAX-1] = '\0';
			strncpy(path, controller.path, FILENAME_MAX);
			path[FILENAME_MAX-1] = '\0';
		}

		/* set output flag */
		if ((!output) && (!(flags & FL_LIST) ||
			(strcmp(controller.name, cont_name) == 0)))
			output = 1;

		/* the actual controller should not be printed */
		ret = cgroup_get_controller_next(&handle, &controller);
	}

	if (output) {
		if ((flags &  FL_MOUNT) != 0)
			printf("%s %s\n", name, path);
		else
			printf("%s\n", name);
		if ((flags & FL_LIST) != 0)
			ret = 0;
	}

	cgroup_get_controller_end(&handle);
	return ret;

}

/* list the controllers */
static int cgroup_list_controllers(const char *tname,
	cont_name_t cont_name[CG_CONTROLLER_MAX], int flags)
{
	int ret = 0;
	int final_ret = 0;
	int i = 0;

	/* initialize libcgroup */
	ret = cgroup_init();

	if (ret) {
		if (flags & FL_ALL) {
			return 0;
		} else {
			return ret;
		}
	}

	if ((flags & FL_LIST) == 0) {
		/* we have to print all controllers */
		ret = print_controller(NULL, flags);
		if (ret == ECGEOF)
			final_ret = 0;
		else
			fprintf(stderr, "controllers can't be listed: %s\n",
				cgroup_strerror(ret));
	} else
		/* we have he list of controllers which should be print */
		while ((i < CG_CONTROLLER_MAX) && (cont_name[i][0] != '\0')
			&& ((ret == ECGEOF) || (ret == 0))) {
			ret = print_controller(cont_name[i], flags);
			if (ret != 0) {
				if (ret == ECGEOF)
					/* controller was not found */
					final_ret = ECGFAIL;
				else
					/* other problem */
					final_ret = ret;
				fprintf(stderr,
					"%s: cannot find group %s: %s\n",
					tname, cont_name[i],
					cgroup_strerror(final_ret));
			}
			i++;
		}

	return final_ret;
}

/* display all controllers attached to the given hierarchy */
static int print_all_controllers_in_hierarchy(const char *tname,
	int hierarchy, int flags)
{
	int ret = 0;
	void *handle;
	struct controller_data info;
	int first = 1;
	cont_name_t cont_name;
	int init = 1;

	/* initialize libcgroup */
	ret = cgroup_init();
	if (ret) {
		/*
		 * if the group is not initialised we assume
		 * no mount points are available
		 */
		init = 0;
	}

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

		/* the first controller in the hierrachy*/
		if (first) {
			/*
			 * if mounted flag is set then
			 * test whether it is mounted
			 */
			if ((flags & FL_MOUNT) && (init == 1)) {
				memset(cont_name, 0, FILENAME_MAX);
				strncpy(cont_name, info.name,
					FILENAME_MAX-1);

				ret = print_controller(cont_name,
					flags + FL_LIST);
				/*
				 * mount point was found,
				 * output is done
				 */
				if (ret == 0) {
					cgroup_get_all_controller_end(
						&handle);
					return 0;
				}
			}
			printf("%s", info.name);
			first = 0;
		} else
			printf(",%s", info.name);

next:
		ret = cgroup_get_all_controller_next(&handle, &info);
		if (ret && ret != ECGEOF) {
			fprintf(stderr,
				"%s: cgroup_get_controller_next failed (%s)\n",
				tname, cgroup_strerror(ret));
			cgroup_get_all_controller_end(&handle);
			return ret;
		}
	}

	cgroup_get_all_controller_end(&handle);
	if (flags & FL_HIERARCHY)
		printf(" %d\n", hierarchy);
	else
		printf("\n");

	if (ret == ECGEOF)
		ret = 0;

	return ret;
}

/* go through the list of all controllers gather them based on hierarchy number
 and print them */
static int cgroup_list_all_controllers(const char *tname, int flags)
{
	int ret = 0;
	void *handle;
	struct controller_data info;

	int h_list[CG_CONTROLLER_MAX];	/* list of hierarchies */
	int counter = 0;
	int j;

	ret = cgroup_get_all_controller_begin(&handle, &info);

	while (ret != ECGEOF) {
		if (info.hierarchy == 0) {
			/* the controller is not attached to any hierrachy */
			printf("%s\n", info.name);
		} else {
			/* the controller is attached to some hierarchy */
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
		if (ret && ret != ECGEOF) {
			fprintf(stderr,
				"cgroup_get_controller_next failed (%s)\n",
				cgroup_strerror(ret));
			return ret;
		}
	}

	ret = cgroup_get_all_controller_end(&handle);

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
		{"all", 0, 0, 'a'},
		{"hierarchies", 0, 0, 'i'},
		{0, 0, 0, 0}
	};

	for (i = 0; i < CG_CONTROLLER_MAX; i++)
		cont_name[i][0] = '\0';

	/* parse arguments */
	while ((c = getopt_long(argc, argv, "mhai", options, NULL)) > 0) {
		switch (c) {
		case 'h':
			usage(0, argv[0]);
			return 0;
		case 'm':
			flags |= FL_MOUNT;
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
		if (flags & FL_ALL) {
			fprintf(stderr, "Warning: too many parameters\n");
			break;
		}
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

	if (flags & FL_ALL)
		/* print the information about all controllers */
		ret = cgroup_list_all_controllers(argv[0], flags);
	else
		/* print information about mounted controllers */
		ret = cgroup_list_controllers(argv[0], cont_name, flags);

	return ret;
}
