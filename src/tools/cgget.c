#include <libcgroup.h>
#include <libcgroup-internal.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

#include "tools-common.h"

#define MODE_SHOW_HEADERS		1
#define MODE_SHOW_NAMES			2
#define MODE_SHOW_ALL_CONTROLLERS	4

#define LL_MAX				100

static struct option const long_options[] =
{
	{"variable", required_argument, NULL, 'r'},
	{"help", no_argument, NULL, 'h'},
	{"all",  no_argument, NULL, 'a'},
	{"values-only", no_argument, NULL, 'v'},
	{NULL, 0, NULL, 0}
};

enum group{
    GR_NOTSET = 0,	/* 0 .. path not specified -g<g>  or -a not used */
    GR_GROUP,		/* 1 .. path not specified -g<g>  or -a used */
    GR_LIST		/* 2 .. path specified using -g<g>:<p> */
};


static void usage(int status, const char *program_name)
{
	if (status != 0)
		fprintf(stderr, "Wrong input parameters,"
			" try %s -h' for more information.\n",
			program_name);
	else {
		fprintf(stdout, "Usage: %s [-nv] [-r <name>] "\
			"[-g <controller>] [-a] <path> ...\n", program_name);
		fprintf(stdout, "   or: %s [-nv] [-r <name>] "\
			"-g <controller>:<path> ...\n", program_name);
		fprintf(stdout, "Print parameter(s) of given group(s).\n");
		fprintf(stdout, "  -a, --all			"\
			"Print info about all relevant controllers\n");
		fprintf(stdout, "  -g <controller>		"\
			"Controller which info should be displaied\n");
		fprintf(stdout, "  -g <controller>:<path>	"\
			"Control group whih info should be displaied\n");
		fprintf(stdout, "  -h, --help			"\
			"Display this help\n");
		fprintf(stdout, "  -n				"\
			"Do not print headers\n");
		fprintf(stdout, "  -r, --variable <name>	"\
			"Define parameter to display\n");
		fprintf(stdout, "  -v, --values-only		"\
			"Print only values, not parameter names\n");
	}
}

static int display_record(char *name,
	struct cgroup_controller *group_controller,
	const char *group_name, const char *program_name, int mode)
{
	int ret = 0;
	void *handle;
	char line[LL_MAX];
	int ind = 0;

	if (mode & MODE_SHOW_NAMES)
		printf("%s: ", name);

	/* start the reading of the variable value */
	ret = cgroup_read_value_begin(group_controller->name,
		group_name, name, &handle, line, LL_MAX);

	if (ret == ECGEOF) {
		printf("\n");
		goto read_end;
	}

	if (ret != 0)
		goto end;

	printf("%s", line);
	if (line[strlen(line)-1] == '\n')
		/* if value continue on the next row. indent it */
		ind = 1;


	/* read iteratively the whole value  */
	while ((ret = cgroup_read_value_next(&handle, line, LL_MAX)) == 0) {
		if (ind == 1)
			printf("\t");
		printf("%s", line);
		ind = 0;

		/* if value continue on the next row. indent it */
		if (line[strlen(line)-1] == '\n')
			ind = 1;
	}

read_end:
	cgroup_read_value_end(&handle);
	if (ret == ECGEOF)
		ret = 0;

end:
	if (ret != 0)
		fprintf(stderr, "variable file read failed %s\n",
			cgroup_strerror(ret));
	return ret;
}



static int display_name_values(char **names, const char* group_name,
		const char *program_name, int mode)
{
	int i;
	struct cgroup_controller *group_controller = NULL;
	struct cgroup *group = NULL;
	int ret = 0;
	char *controller = NULL, *dot;

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

	i = 0;
	while (names[i] != NULL) {
		/* Parse the controller out of 'controller.parameter'. */
		free(controller);
		controller = strdup(names[i]);
		if (controller == NULL) {
			fprintf(stderr, "%s: out of memory\n", program_name);
			ret = -1;
			goto err;
		}
		dot = strchr(controller, '.');
		if (dot == NULL) {
			fprintf(stderr, "%s: error parsing parameter name\n" \
					" '%s'", program_name, names[i]);
			ret = -1;
			goto err;
		}
		*dot = '\0';

		/* Find appropriate controller. */
		group_controller = cgroup_get_controller(group, controller);
		if (group_controller == NULL) {
			fprintf(stderr, "%s: cannot find controller " \
					"'%s' in group '%s'\n", program_name,
					controller, group_name);
			ret = -1;
			goto err;
		}

		/* Finally read the parameter value.*/
		ret = display_record(names[i], group_controller,
			group_name, program_name, mode);
		if (ret != 0)
			goto err;
		i++;
	}
err:
	if (controller)
		free(controller);
	if (group)
		cgroup_free(&group);
	return ret;
}

static int display_controller_values(char **controllers, const char *group_name,
	const char *program_name, int mode)
{
	struct cgroup *group = NULL;
	struct cgroup_controller *group_controller = NULL;
	char *name;
	int i, j;
	int name_count;
	int ret = 0;
	int result = 0;

	/* initialize group_name variable */
	group = cgroup_new_cgroup(group_name);
	if (group == NULL) {
		fprintf(stderr, "%s:cannot create group '%s'\n",
			program_name, group_name);
		return -1;
	}

	ret = cgroup_get_cgroup(group);
	if (ret != 0) {
		if (!(mode & MODE_SHOW_ALL_CONTROLLERS))
			fprintf(stderr, "%s: cannot read group '%s': %s\n",
				program_name, group_name, cgroup_strerror(ret));
	}

	/* for all wanted controllers */
	j = 0;
	while (controllers[j] != NULL) {
		/* read the controller group data */
		group_controller = cgroup_get_controller(group, controllers[j]);
		if (group_controller == NULL) {
			if (!(mode & MODE_SHOW_ALL_CONTROLLERS)) {
				fprintf(stderr, "%s: cannot find controller "\
					"'%s' in group '%s'\n", program_name,
					controllers[j], group_name);
				result = -1;
			}
		}

		/* for each variable of given group print the statistic */
		name_count = cgroup_get_value_name_count(group_controller);
		for (i = 0; i < name_count; i++) {
			name = cgroup_get_value_name(group_controller, i);
			if (name != NULL) {
				ret = display_record(name, group_controller,
					group_name, program_name, mode);
				if (ret) {
					result = ret;
					goto err;
				}
			}
		}
		j = j+1;
	}

err:
	cgroup_free(&group);
	return result;

}

static int display_values(char **controllers, int max, const char *group_name,
	char **names, int mode, const char *program_name)
{
	int ret, result = 0;

	/* display the directory if needed */
	if (mode & MODE_SHOW_HEADERS)
		printf("%s:\n", group_name);

	/* display all wanted variables */
	if (names[0] != NULL) {
		ret = display_name_values(names, group_name, program_name,
			mode);
		if (ret)
			result = ret;
	}

	/* display all wanted controllers */
	if (controllers[0] != NULL) {
		ret = display_controller_values(controllers, group_name,
			program_name, mode);
		if (ret)
			result = ret;
	}

	/* separate each group with empty line. */
	if (mode & MODE_SHOW_HEADERS)
		printf("\n");

	return result;
}

int add_record_to_buffer(char **buffer, char *record, int capacity)
{
	int i;

	/* find first free entry inside the cgroup data array */
	for (i = 0; i < capacity; i++) {
		if (!buffer[i])
			break;
	}

	if (i < capacity) {
		buffer[i] = strdup(record);
		if (buffer[i] == NULL)
			return 1;
		return 0;
	}
	return 1;
}


int main(int argc, char *argv[])
{
	int ret = 0;
	int result = 0;
	int c, i;
	int group_needed = GR_NOTSET;

	int capacity = argc + CG_CONTROLLER_MAX; /* maximal number of records */
	struct cgroup_group_spec **cgroup_list; /* list of all groups */
	char **names;			/* list of wanted variable names */
	char **controllers;		/* list of wanted controllers*/

	void *handle;
	struct cgroup_mount_point controller;

	int mode = MODE_SHOW_NAMES | MODE_SHOW_HEADERS;

	/* No parameter on input? */
	if (argc < 2) {
		usage(1, argv[0]);
		return 1;
	}

	names = (char **)calloc(capacity+1, sizeof(char *));
	controllers = (char **)calloc(capacity+1, sizeof(char *));
	cgroup_list = (struct cgroup_group_spec **)calloc(capacity,
		sizeof(struct cgroup_group_spec *));
	if ((names == NULL) || (controllers == NULL) ||
		(cgroup_list == NULL)) {
		fprintf(stderr, "%s: out of memory\n", argv[0]);
		ret = -1;
		goto err_free;
	}

	/* Parse arguments. */
	while ((c = getopt_long(argc, argv, "r:hnvg:a", long_options, NULL))
		> 0) {
		switch (c) {
		case 'h':
			usage(0, argv[0]);
			result = 0;
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
			ret = add_record_to_buffer(names, optarg, capacity);
			if (ret) {
				result = ret;
				goto err;
			}
			break;
		case 'g':
			if (strchr(optarg, ':') == NULL) {
				/* -g <group> */
				if (group_needed == 2) {
					usage(1, argv[0]);
					result = -1;
					goto err;
				}
				group_needed = 1;
				add_record_to_buffer(controllers, optarg,
					capacity);
			} else {
				/* -g <group>:<path> */
				if (group_needed == 1) {
					usage(1, argv[0]);
					result = -1;
					goto err;
				}
				group_needed = 2;
				ret = parse_cgroup_spec(cgroup_list, optarg,
					capacity);
				if (ret) {
					fprintf(stderr, "%s: cgroup controller/"
						"path parsing failed (%s)\n",
						argv[0], argv[optind]);
					ret = -1;
					goto err;
				}
			}
			break;
		case 'a':
			if (group_needed == 2) {
				usage(1, argv[0]);
				result = -1;
				goto err;
			}
			group_needed = 1;
			/* go through cgroups for all possible controllers */
			mode |=  MODE_SHOW_ALL_CONTROLLERS;
			break;
		default:
			usage(1, argv[0]);
			result = -1;
			goto err;
		}
	}

	if (((group_needed == 2) && (argv[optind])) ||
	    ((group_needed != 2) && (!argv[optind]))) {
		/* mixed -g <controller>:<path> and <path> or path not set */
		usage(1, argv[0]);
		result = -1;
		goto err;
	}

	/* Initialize libcgroup. */
	ret = cgroup_init();
	if (ret) {
		fprintf(stderr, "%s: libcgroup initialization failed: %s\n",
			argv[0], cgroup_strerror(ret));
		result = ret;
		goto err;
	}

	if ((mode & MODE_SHOW_ALL_CONTROLLERS) ||
		((controllers[0] == NULL) && (names[0] == NULL)
		&& (cgroup_list[0] == NULL))) {
		/* show info about all controllers */
		ret = cgroup_get_controller_begin(&handle, &controller);
		/* go through list of controllers, add them to the list */
		while (ret == 0) {
			add_record_to_buffer(controllers, controller.name,
				capacity);
			ret = cgroup_get_controller_next(&handle, &controller);
		}
		cgroup_get_controller_end(&handle);
	}

	/* Parse control groups set by -g<c>:<p> pairs */
	for (i = 0; i < capacity; i++) {
		if (!cgroup_list[i])
			break;
		ret |= display_values(cgroup_list[i]->controllers, capacity,
			cgroup_list[i]->path, names, mode, argv[0]);
	}

	/* Parse control groups and print them .*/
	for (i = optind; i < argc; i++) {
		ret |= display_values(controllers, capacity,
			argv[i], names, mode, argv[0]);
	}

err:
	for (i = 0; i < capacity; i++) {
		if (cgroup_list[i])
			cgroup_free_group_spec(cgroup_list[i]);
		if (controllers[i])
			free(controllers[i]);
		if (names[i])
			free(names[i]);
	}

err_free:
	free(cgroup_list);
	free(controllers);
	free(names);

	return result;
}
