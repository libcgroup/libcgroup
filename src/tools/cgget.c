#include <libcgroup.h>
#include <libcgroup-internal.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

#include "tools-common.h"

#define MODE_SHOW_HEADERS	1
#define MODE_SHOW_NAMES		2

void usage(int status, char *program_name)
{
	if (status != 0)
		fprintf(stderr, "Wrong input parameters,"
			" try %s -h' for more information.\n",
			program_name);
	else {
		printf("Usage: %s [-nv] -r <name> [-r <name>] ... <path> ...\n",
			program_name);
	}
}

int display_values(char **names, int count, const char* group_name,
		const char *program_name, int mode)
{
	int i;
	struct cgroup_controller *group_controller = NULL;
	struct cgroup *group = NULL;
	int ret = 0;
	char *controller = NULL, *parameter = NULL;
	char *value = NULL;

	if (mode & MODE_SHOW_HEADERS)
		printf("%s:\n", group_name);

	group = cgroup_new_cgroup(group_name);
	if (group == NULL) {
		fprintf(stderr, "%s: cannot create group '%s'\n", program_name,
				group_name);
		return -1;
	}
	ret = cgroup_get_cgroup(group);
	if (ret != 0) {
		fprintf(stderr, "%s: cannot read group '%s': %s\n",
				program_name, group_name, cgroup_strerror(ret));
		goto err;
	}

	for (i = 0; i < count; i++) {
		/* Parse the controller out of 'controller.parameter'. */
		free(controller);
		controller = strdup(names[i]);
		if (controller == NULL) {
			fprintf(stderr, "%s: out of memory\n", program_name);
			ret = -1;
			goto err;
		}
		parameter = strchr(controller, '.');
		if (parameter == NULL) {
			fprintf(stderr, "%s: error parsing parameter name\n" \
					" '%s'", program_name, names[i]);
			ret = -1;
			goto err;
		}
		*parameter = '\0';
		parameter++;

		/* Find appropriate controller. */
		group_controller = cgroup_get_controller(group, controller);
		if (group_controller == NULL) {
			fprintf(stderr, "%s: cannot find controller " \
					"'%s' in group '%s'\n", program_name,
					controller, group_name);
			ret = -1;
			goto err;
		}

		/*
		 * Finally read the parameter value.
		 */
		ret = cgroup_get_value_string(group_controller, names[i],
				&value);
		if (ret != 0) {
			fprintf(stderr, "%s: cannot read parameter '%s.%s' "\
					"from group '%s': %s\n", program_name,
					controller, parameter, group_name,
					cgroup_strerror(ret));
			goto err;
		}
		if (mode & MODE_SHOW_NAMES)
			printf("%s.%s=", controller, parameter);
		printf("%s\n", value);
		free(value);
	}
err:
	if (controller)
		free(controller);
	if (group)
		cgroup_free(&group);
	return ret;
}

int main(int argc, char *argv[])
{
	int ret = 0;
	int c, i;

	char **names = NULL;
	int n_number = 0;
	int n_max = 0;
	int mode = MODE_SHOW_NAMES | MODE_SHOW_HEADERS;

	/* No parameter on input? */
	if (argc < 2) {
		usage(1, argv[0]);
		return 1;
	}

	/* Parse arguments. */
	while ((c = getopt(argc, argv, "r:hnv")) != -1) {
		switch (c) {
		case 'h':
			usage(0, argv[0]);
			ret = 0;
			goto err;
			break;

		case 'n':
			/* Do not show headers. */
			mode = mode & (INT_MAX ^ MODE_SHOW_HEADERS);
			break;

		case 'v':
			/* Do not show parameter names. */
			mode = mode & (INT_MAX ^ MODE_SHOW_NAMES);
			break;

		case 'r':
			/* Add name to buffer. */
			if (n_number >= n_max) {
				n_max += CG_NV_MAX;
				names = (char **) realloc(names,
					n_max * sizeof(char *));
				if (!names) {
					fprintf(stderr, "%s: "
						"not enough memory\n", argv[0]);
					ret = -1;
					goto err;
				}
			}

			names[n_number] = optarg;
			n_number++;
			break;
		default:
			usage(1, argv[0]);
			ret = 1;
			goto err;
			break;
		}
	}

	if (!argv[optind]) {
		fprintf(stderr, "%s: no cgroup specified\n", argv[0]);
		ret = -1;
		goto err;
	}

	/* Initialize libcgroup. */
	ret = cgroup_init();
	if (ret) {
		fprintf(stderr, "%s: libcgroup initialization failed: %s\n",
			argv[0], cgroup_strerror(ret));
		goto err;
	}

	/* Parse control groups and print them .*/
	for (i = optind; i < argc; i++) {
		ret = display_values(names, n_number, argv[i], argv[0], mode);
		if (ret)
			goto err;
		/* Separate each group with empty line. */
		if (mode & MODE_SHOW_HEADERS && i != argc-1)
			printf("\n");
	}

err:
	free(names);
	return ret;
}
