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


static void usage(int status, const char *program_name)
{
	if (status != 0)
		fprintf(stderr, "Wrong input parameters,"
			" try %s -h' for more information.\n",
			program_name);
	else {
		printf("Usage: %s [-nv] [-r<name>] [-g<controller>] [-a] ..."\
			"<path> ...\n", program_name);
	}
}

static int display_one_record(char *name, struct cgroup_controller *group_controller,
	const char *program_name, int mode)
{
	int ret;
	char *value = NULL;

	ret = cgroup_get_value_string(group_controller, name, &value);
	if (ret != 0) {
		fprintf(stderr, "%s: cannot read parameter '%s' "\
			"from group '%s': %s\n", program_name, name,
			group_controller->name, cgroup_strerror(ret));
		return ret;
	}

	if (mode & MODE_SHOW_NAMES)
		printf("%s=", name);

	if (strcmp(strchr(name, '.')+1, "stat"))
		printf("%s\n", value);

	else {
		void *handle;
		struct cgroup_stat stat;

		cgroup_read_stats_begin(group_controller->name,
			"/", &handle, &stat);
		if (ret != 0) {
			fprintf(stderr, "stats read failed\n");
			return ret;
		}
		printf("%s %s", stat.name, stat.value);

		while ((ret = cgroup_read_stats_next(&handle, &stat)) !=
				ECGEOF) {
			printf("\t%s %s", stat.name, stat.value);
		}

		cgroup_read_stats_end(&handle);
	}

	free(value);
	return ret;
}


static int display_name_values(char **names, int count, const char* group_name,
		const char *program_name, int mode)
{
	int i;
	struct cgroup_controller *group_controller = NULL;
	struct cgroup *group = NULL;
	int ret = 0;
	char *controller = NULL, *parameter = NULL;

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

		/* Finally read the parameter value.*/
		ret = display_one_record(names[i], group_controller,
			program_name, mode);
		if (ret != 0)
			goto err;
	}
err:
	if (controller)
		free(controller);
	if (group)
		cgroup_free(&group);
	return ret;
}

static int display_controller_values(char **controllers, int count,
		const char *group_name, const char *program_name, int mode)
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
	for (j = 0; j < count; j++) {

		/* read the controller group data */
		group_controller = cgroup_get_controller(group, controllers[j]);
		if (group_controller == NULL) {
			if (!(mode & MODE_SHOW_ALL_CONTROLLERS))
				fprintf(stderr, "%s: cannot find controller "\
					"'%s' in group '%s'\n", program_name,
					controllers[j], group_name);
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
				ret = display_one_record(name, group_controller,
					program_name, mode);
				if (ret) {
					result = ret;
					goto err;
				}
			}
		}
	}

err:
	cgroup_free(&group);
	return result;

}

static int display_all_controllers(const char *group_name,
	const char *program_name, int mode)
{
	void *handle;
	int ret;
	struct cgroup_mount_point controller;
	char *name;
	int succ = 0;

	ret = cgroup_get_controller_begin(&handle, &controller);

	/* go through the list of controllers/mount point pairs */
	while (ret == 0) {
		name = controller.name;
		succ |= display_controller_values(&name, 1,
			group_name, program_name, mode);
		ret = cgroup_get_controller_next(&handle, &controller);
	}

	cgroup_get_controller_end(&handle);
	return succ;
}

static int add_record_to_buffer(int *p_number,
	int *p_max, char ***p_records, char *new_rec)
{

	if (*p_number >= *p_max) {
		*p_max += CG_NV_MAX;
		*p_records = (char **) realloc(*p_records,
			*p_max * sizeof(char *));
		if (!(*p_records)) {
			fprintf(stderr, "not enough memory\n");
			return -1;
		}
	}

	(*p_records)[*p_number] = new_rec;
	(*p_number)++;

	return 0;
}

int main(int argc, char *argv[])
{
	int ret = 0;
	int result = 0;
	int c, i;

	char **names = NULL;
	int n_number = 0;
	int n_max = 0;

	char **controllers = NULL;
	int c_number = 0;
	int c_max = 0;

	int mode = MODE_SHOW_NAMES | MODE_SHOW_HEADERS;

	/* No parameter on input? */
	if (argc < 2) {
		usage(1, argv[0]);
		return 1;
	}

	/* Parse arguments. */
	while ((c = getopt(argc, argv, "r:hnvg:a")) != -1) {
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
			ret = add_record_to_buffer(
				&n_number, &n_max, &names, optarg);
			if (ret) {
				result = ret;
				goto err;
			}
			break;
		case 'g':
			/* for each controller add all variables to list */
			ret = add_record_to_buffer(&c_number,
				&c_max, &controllers, optarg);
			if (ret) {
				result = ret;
				goto err;
			}
			break;
		case 'a':
			/* go through cgroups for all possible controllers */
			mode |=  MODE_SHOW_ALL_CONTROLLERS;
			break;
		default:
			usage(1, argv[0]);
			result = -1;
			goto err;
			break;
		}
	}

	if (!argv[optind]) {
		fprintf(stderr, "%s: no cgroup specified\n", argv[0]);
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

	if (!argv[optind]) {
		fprintf(stderr, "%s: no cgroup specified\n", argv[0]);
		result = -1;
		goto err;
	}

	/* Parse control groups and print them .*/
	for (i = optind; i < argc; i++) {

		/* display the directory if needed */
		if (mode & MODE_SHOW_HEADERS)
			printf("%s:\n", argv[i]);

		ret = display_name_values(names,
			n_number, argv[i], argv[0], mode);
		if (ret)
			result = ret;

		ret = display_controller_values(controllers, c_number, argv[i],
			argv[0], mode - (mode & MODE_SHOW_ALL_CONTROLLERS));
		if (ret)
			goto err;

		if (mode & MODE_SHOW_ALL_CONTROLLERS)
			display_all_controllers(argv[i], argv[0], mode);
		if (ret)
			result = ret;

		/* Separate each group with empty line. */
		if (mode & MODE_SHOW_HEADERS && i != argc-1)
			printf("\n");
	}

err:
	free(names);
	return result;
}
