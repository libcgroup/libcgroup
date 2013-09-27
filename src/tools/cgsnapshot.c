/* " Copyright (C) 2010 Red Hat, Inc. All Rights Reserved.
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
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <libcgroup.h>
#include <libcgroup-internal.h>
#include <pwd.h>
#include <grp.h>

enum flag{
    FL_LIST =		1,
    FL_SILENT =		2,  /* do-not show any warning/error output */
    FL_STRICT =		4,  /* don show the variables which are not on
			    whitelist */
    FL_OUTPUT =		8,  /* output should be redirect to the given file */
    FL_BLACK =		16, /* blacklist set */
    FL_WHITE =		32, /* whitelist set */
};

#define BLACKLIST_CONF		"/etc/cgsnapshot_blacklist.conf"
#define WHITELIST_CONF		"/etc/cgsnapshot_whitelist.conf"

struct black_list_type {
	char *name;			/* variable name */
	struct black_list_type *next;	/* pointer to the next record */
};

struct black_list_type *black_list;
struct black_list_type *white_list;

typedef char cont_name_t[FILENAME_MAX];

int flags;
FILE *of;

/*
 * Display the usage
 */
static void usage(int status, const char *program_name)
{
	if (status != 0) {
		fprintf(stderr, "Wrong input parameters,"
			" try %s -h' for more information.\n",
			program_name);
		return;
	}
	printf("Usage: %s [-h] [-s] [-b FILE] [-w FILE] [-f FILE] "\
		"[controller] [...]\n", program_name);
	printf("Generate the configuration file for given controllers\n");
	printf("  -b, --blacklist=FILE		Set the blacklist"\
		" configuration file (default %s)\n", BLACKLIST_CONF);
	printf("  -f, --file=FILE		Redirect the output"\
		" to output_file\n");
	printf("  -h, --help			Display this help\n");
	printf("  -s, --silent			Ignore all warnings\n");
	printf("  -t, --strict			Don't show variables "\
		"which are not on the whitelist\n");
	printf("  -w, --whitelist=FILE		Set the whitelist"\
		" configuration file (don't used by default)\n");
}

/* cache values from blacklist file to the list structure */

int load_list(char *filename, struct black_list_type **p_list)
{
	FILE *fw;
	int i = 0;
	int ret;
	char buf[FILENAME_MAX];
	char name[FILENAME_MAX];

	struct black_list_type *start = NULL;
	struct black_list_type *end = NULL;
	struct black_list_type *new;

	fw = fopen(filename, "r");
	if (fw == NULL) {
		fprintf(stderr, "ERROR: Failed to open file %s: %s\n",
			filename, strerror(errno));
		*p_list = NULL;
		return 1;
	}

	/* go through the configuration file and search the line */
	while (fgets(buf, FILENAME_MAX, fw) != NULL) {
		buf[FILENAME_MAX-1] = '\0';
		i = 0;

		/* if the record start with # then skip it */
		while ((buf[i] == ' ') || (buf[i] == '\n'))
			i++;

		if ((buf[i] == '#') || (buf[i] == '\0'))
			continue;

		ret = sscanf(buf, "%s", name);
		if (ret == 0)
			continue;

		new = (struct black_list_type *)malloc(sizeof
			(struct black_list_type));
		if (new == NULL) {
			fprintf(stderr, "ERROR: Memory allocation problem "
				"(%s)\n", strerror(errno));
			ret = 1;
			goto err;
		}

		new->name = strdup(name);
		if (new->name == NULL) {
			fprintf(stderr, "ERROR: Memory allocation problem "
				"(%s)\n", strerror(errno));
			ret = 1;
			free(new);
			goto err;
		}
		new->next = NULL;

		/* update the variables list */
		if (start == NULL) {
			start = new;
			end = new;
		} else {
			end->next = new;
			end = new;
		}
	}

	fclose(fw);

	*p_list = start;
	return 0;

err:
	fclose(fw);
	new = start;
	while (new != NULL) {
		end = new->next;
		free(new->name);
		free(new);
		new = end;
	}
	*p_list = NULL;
	return ret;
}

/* free list structure */
void free_list(struct black_list_type *list)
{
	struct black_list_type *now;
	struct black_list_type *next;

	now = list;
	while (now != NULL) {
		next = now->next;
		free(now->name);
		free(now);
		now = next;
	}
	return;
}

/* Test whether the variable is on the list
 * return values are:
 * 1 ... was found
 * 0 ... no record was found
 */
int is_on_list(char *name, struct black_list_type *list)
{
	struct black_list_type *record;

	record = list;
	/* go through the list of all values */
	while (record != NULL) {
		/* if the variable name is found */
		if (strcmp(record->name, name) == 0) {
			/* return its value */
			return 1;
		}
		record = record->next;
	}

	/* the variable was not found */
	return 0;

}

/* Display permissions record for the given group
 * defined by path
 */
static int display_permissions(const char *path,
		const char *program_name)
{
	int ret;
	struct stat sba;
	struct stat sbt;
	struct passwd *pw;
	struct group *gr;
	char tasks_path[FILENAME_MAX];

	/* admin permissions record */
	/* get the directory statistic */
	ret = stat(path, &sba);
	if (ret) {
		fprintf(stderr, "ERROR: can't read statistics about %s\n",
			path);
		return -1;
	}

	/* tasks permissions record */
	/* get tasks file statistic */
	strncpy(tasks_path, path, FILENAME_MAX);
	tasks_path[FILENAME_MAX-1] = '\0';
	strncat(tasks_path, "/tasks", FILENAME_MAX - strlen(tasks_path) - 1);
	tasks_path[FILENAME_MAX-1] = '\0';
	ret = stat(tasks_path, &sbt);
	if (ret) {
		fprintf(stderr, "ERROR: can't read statistics about %s\n",
			tasks_path);
		return -1;
	}

	if ((sba.st_uid) || (sba.st_gid) ||
		(sbt.st_uid) || (sbt.st_gid)) {
		/* some uid or gid is nonroot, admin permission
		   tag is necessery */

		/* print the header */
		fprintf(of, "\tperm {\n");

		/* find out the user and group name */
		pw = getpwuid(sba.st_uid);
		if (pw == NULL) {
			fprintf(stderr, "ERROR: can't get %d user name\n",
				sba.st_uid);
			return -1;
		}

		gr = getgrgid(sba.st_gid);
		if (gr == NULL) {
			fprintf(stderr, "ERROR: can't get %d group name\n",
				sba.st_gid);
			return -1;
		}

		/* print the admin record */
		fprintf(of, "\t\tadmin {\n"\
			"\t\t\tuid = %s;\n"\
			"\t\t\tgid = %s;\n"\
			"\t\t}\n", pw->pw_name, gr->gr_name);

		/* find out the user and group name */
		pw = getpwuid(sbt.st_uid);
		if (pw == NULL) {
			fprintf(stderr, "ERROR: can't get %d user name\n",
				sbt.st_uid);
			return -1;
		}

		gr = getgrgid(sbt.st_gid);
		if (gr == NULL) {
			fprintf(stderr, "ERROR: can't get %d group name\n",
				sbt.st_gid);
			return -1;
		}

		/* print the task record */
		fprintf(of, "\t\ttask {\n"\
			"\t\t\tuid = %s;\n"\
			"\t\t\tgid = %s;\n"\
			"\t\t}\n", pw->pw_name, gr->gr_name);

		fprintf(of, "\t}\n");
	}

	return 0;
}

/* Displey the control group record:
 * header
 *   permission record
 *   controllers records
 * tail
 */

static int display_cgroup_data(struct cgroup *group,
		char controller[CG_CONTROLLER_MAX][FILENAME_MAX],
		const char *group_path, int root_path_len, int first,
		const char *program_name)
{
	int i = 0, j;
	int bl, wl = 0; /* is on the blacklist/whitelist flag */
	int nr_var = 0;
	char *name;
	char *output_name;
	struct cgroup_controller *group_controller = NULL;
	char *value = NULL;
	char var_path[FILENAME_MAX];
	int ret = 0;
	struct stat sb;

	/* print the  group definition header */
	fprintf(of, "group %s {\n", group->name);

	/* display the permission tags */
	ret = display_permissions(group_path, program_name);
	if (ret)
		return ret;

	/* for all wanted controllers display controllers tag */
	while (controller[i][0] != '\0') {

		group_controller = cgroup_get_controller(group, controller[i]);
		if (group_controller == NULL) {
			printf("cannot find controller "\
				"'%s' in group '%s'\n",
				controller[i], group->name);
			i++;
			ret = -1;
			continue;
		}

		/* print the controller header */
		if (strncmp(controller[i], "name=", 5) == 0)
			fprintf(of, "\t\"%s\" {\n", controller[i]);
		else
			fprintf(of, "\t%s {\n", controller[i]);
		i++;
		nr_var = cgroup_get_value_name_count(group_controller);

		for (j = 0; j < nr_var; j++) {
			name = cgroup_get_value_name(group_controller, j);

			/* For the non-root groups cgconfigparser set
			   permissions of variable files to 777. Thus
			   It is necessary to test the permissions of
			   variable files in the root group to find out
			   whether the variable is writable.
			 */
			if (root_path_len >= FILENAME_MAX)
				root_path_len = FILENAME_MAX - 1;
			strncpy(var_path, group_path, root_path_len);
			var_path[root_path_len] = '\0';
			strncat(var_path, "/", FILENAME_MAX -
					strlen(var_path) - 1);
			var_path[FILENAME_MAX-1] = '\0';
			strncat(var_path, name, FILENAME_MAX -
					strlen(var_path) - 1);
			var_path[FILENAME_MAX-1] = '\0';

			/* test whether the  write permissions */
			ret = stat(var_path, &sb);
			/* freezer.state is not in root group so ret != 0,
			 * but it should be listed
			 * device.list should be read to create
			 * device.allow input
			 */
			if ((ret == 0) && ((sb.st_mode & S_IWUSR) == 0) &&
			    (strcmp("devices.list", name) != 0)) {
				/* variable is not writable */
				continue;
			}

			/* find whether the variable is blacklisted or
			   whitelisted */
			bl = is_on_list(name, black_list);
			wl = is_on_list(name, white_list);

			/* if it is blacklisted skip it and continue */
			if (bl)
				continue;

			/* if it is not whitelisted and strict tag is used
			   skip it and continue too */
			if ((!wl) && (flags &  FL_STRICT))
				continue;

			/* if it is not whitelisted and silent tag is not
			   used write an warning */
			if ((!wl) && !(flags &  FL_SILENT) && (first))
				fprintf(stderr, "WARNING: variable %s is "\
					"neither blacklisted nor "\
					"whitelisted\n", name);

			output_name = name;

			/* deal with devices variables:
			 * - omit devices.deny and device.allow,
			 * - generate devices.{deny,allow} from
			 * device.list variable (deny all and then
			 * all device.list devices */
			if ((strcmp("devices.deny", name) == 0) ||
				(strcmp("devices.allow", name) == 0)) {
				continue;
			}
			if (strcmp("devices.list", name) == 0) {
				output_name = "devices.allow";
				fprintf(of,
					"\t\tdevices.deny=\"a *:* rwm\";\n");
			}

			ret = cgroup_get_value_string(group_controller,
				name, &value);

			/* variable can not be read */
			if (ret != 0) {
				ret = 0;
				fprintf(stderr, "ERROR: Value of "\
					"variable %s can be read\n",
					name);
				goto err;
			}
			fprintf(of, "\t\t%s=\"%s\";\n", output_name, value);
		}
		fprintf(of, "\t}\n");
	}

	/* tail of the record */
	fprintf(of, "}\n\n");

err:
	return ret;
}

/*
 * creates the record about the hierarchie which contains
 * "controller" subsystem
 */
static int display_controller_data(
		char controller[CG_CONTROLLER_MAX][FILENAME_MAX],
		const char *program_name)
{
	int ret;
	void *handle;
	int first = 1;

	struct cgroup_file_info info;
	int lvl;

	int prefix_len;
	char cgroup_name[FILENAME_MAX];

	struct cgroup *group = NULL;

	/* start to parse the structure for the first controller -
	   controller[0] attached to hierarchie */
	ret = cgroup_walk_tree_begin(controller[0], "/", 0,
		&handle, &info, &lvl);
	if (ret != 0)
		return ret;

	prefix_len = strlen(info.full_path);

	/* go through all files and directories */
	while ((ret = cgroup_walk_tree_next(0, &handle, &info, lvl)) == 0) {
		/* some group starts here */
		if (info.type == CGROUP_FILE_TYPE_DIR) {
			/* parse the group name from full_path*/
			strncpy(cgroup_name, &info.full_path[prefix_len],
				FILENAME_MAX);
			cgroup_name[FILENAME_MAX-1] = '\0';


			/* start to grab data about the new group */
			group = cgroup_new_cgroup(cgroup_name);
			if (group == NULL) {
				printf("cannot create group '%s'\n",
					cgroup_name);
				ret = ECGFAIL;
				goto err;
			}

			ret = cgroup_get_cgroup(group);
			if (ret != 0) {
				printf("cannot read group '%s': %s\n",
				cgroup_name, cgroup_strerror(ret));
				goto err;
			}

			display_cgroup_data(group, controller, info.full_path,
				prefix_len, first, program_name);
			first = 0;
			cgroup_free(&group);
		}
	}

err:
	cgroup_walk_tree_end(&handle);
	if (ret == ECGEOF)
		ret = 0;

	return ret;

}

static int is_ctlr_on_list(char controllers[CG_CONTROLLER_MAX][FILENAME_MAX],
			cont_name_t wanted_conts[FILENAME_MAX])
{
	int i = 0;
	int j = 0;

	while (controllers[i][0] != '\0') {
		while (wanted_conts[j][0] != '\0') {
			if (strcmp(controllers[i], wanted_conts[j]) == 0)
				return 1;
			j++;
		}
		j = 0;
		i++;
	}

	return 0;
}


/* print data about input cont_name controller */
static int parse_controllers(cont_name_t cont_names[CG_CONTROLLER_MAX],
	const char *program_name)
{
	int ret = 0;
	void *handle;
	char path[FILENAME_MAX];
	struct cgroup_mount_point controller;

	char controllers[CG_CONTROLLER_MAX][FILENAME_MAX];
	int max = 0;

	path[0] = '\0';

	ret = cgroup_get_controller_begin(&handle, &controller);

	/* go through the list of controllers/mount point pairs */
	while (ret == 0) {
		if (strcmp(path, controller.path) == 0) {
			/* if it is still the same mount point */
			if (max < CG_CONTROLLER_MAX) {
				strncpy(controllers[max],
					controller.name, FILENAME_MAX);
				(controllers[max])[FILENAME_MAX-1] = '\0';
				max++;
			}
		} else {

			/* we got new mount point, print it if needed */
			if ((!(flags & FL_LIST) ||
				(is_ctlr_on_list(controllers, cont_names)))
				&& (max != 0)) {
				(controllers[max])[0] = '\0';
				ret = display_controller_data(
					controllers, program_name);
			}

			strncpy(controllers[0], controller.name, FILENAME_MAX);
			(controllers[0])[FILENAME_MAX-1] = '\0';

			strncpy(path, controller.path, FILENAME_MAX);
			path[FILENAME_MAX-1] = '\0';
			max = 1;
		}

		/* the actual controller should not be printed */
		ret = cgroup_get_controller_next(&handle, &controller);
	}

	if ((!(flags & FL_LIST) ||
		(is_ctlr_on_list(controllers, cont_names)))
		&& (max != 0)) {
		(controllers[max])[0] = '\0';
		ret = display_controller_data(
			controllers, program_name);
	}

	cgroup_get_controller_end(&handle);
	if (ret != ECGEOF)
		return ret;
	return 0;
}

static int show_mountpoints(const char *controller)
{
	char path[FILENAME_MAX];
	int ret;
	void *handle;
	int quote = 0;

	if (strncmp(controller, "name=", 5) == 0)
		quote = 1;

	ret = cgroup_get_subsys_mount_point_begin(controller, &handle, path);
	if (ret)
		return ret;

	while (ret == 0) {
		if (quote)
			fprintf(of, "\t\"%s\" = %s;\n", controller, path);
		else
			fprintf(of, "\t%s = %s;\n", controller, path);
		ret = cgroup_get_subsys_mount_point_next(&handle, path);
	}
	cgroup_get_subsys_mount_point_end(&handle);

	if (ret != ECGEOF)
		return ret;
	return 0;
}

/* parse whether data about given controller "name" should be displayed.
 * If yes then the data are printed. "cont_names" is list of controllers
 * which should be shown.
 */
static void parse_mountpoint(cont_name_t cont_names[CG_CONTROLLER_MAX],
	char *name)
{
	int i;

	/* if there is no controller list show all mounted controllers */
	if (!(flags & FL_LIST)) {
		if (show_mountpoints(name)) {
			/* the controller is not mounted */
			if ((flags & FL_SILENT) == 0)
				fprintf(stderr, "ERROR: %s hierarchy "\
					"not mounted\n", name);
		}
		return;
	}

	/* there is controller list - show wanted mounted controllers only */
	for (i = 0; i <= CG_CONTROLLER_MAX-1; i++) {
		if (!strncmp(cont_names[i], name, strlen(name)+1)) {
			/* controller is on the list */
			if (show_mountpoints(name)) {
				/* the controller is not mounted */
				if ((flags & FL_SILENT) == 0) {
					fprintf(stderr, "ERROR: %s hierarchy "\
						"not mounted\n", name);
				}
			break;
			}
		break;
		}
	}

	return;
}

/* print data about input mount points */
static int parse_mountpoints(cont_name_t cont_names[CG_CONTROLLER_MAX],
	const char *program_name)
{
	int ret, final_ret = 0;
	void *handle;
	struct controller_data info;
	struct cgroup_mount_point mount;

	/* start mount section */
	fprintf(of, "mount {\n");

	/* go through the controller list */
	ret = cgroup_get_all_controller_begin(&handle, &info);
	while (ret == 0) {

		/* the controller attached to some hierarchy */
		if  (info.hierarchy != 0)
			parse_mountpoint(cont_names, info.name);

		/* next controller */
		ret = cgroup_get_all_controller_next(&handle, &info);
	}
	if (ret != ECGEOF) {
		if ((flags &  FL_SILENT) != 0) {
			fprintf(stderr,
				"E: in get next controller %s\n",
				cgroup_strerror(ret));
		}
		final_ret = ret;
	}
	cgroup_get_all_controller_end(&handle);

	/* process also named hierarchies */
	ret = cgroup_get_controller_begin(&handle, &mount);
	while (ret == 0) {
		if (strncmp(mount.name, "name=", 5) == 0)
			parse_mountpoint(cont_names, mount.name);
		ret = cgroup_get_controller_next(&handle, &mount);
	}

	if (ret != ECGEOF) {
		if ((flags &  FL_SILENT) != 0) {
			fprintf(stderr,
				"E: in get next controller %s\n",
				cgroup_strerror(ret));
		}
		final_ret = ret;
	}
	cgroup_get_controller_end(&handle);

	/* finish mount section */
	fprintf(of, "}\n\n");
	return final_ret;
}

int main(int argc, char *argv[])
{
	int ret = 0, err;
	int c;

	int i;
	int c_number = 0;
	cont_name_t wanted_cont[CG_CONTROLLER_MAX];

	char bl_file[FILENAME_MAX];  /* blacklist file name */
	char wl_file[FILENAME_MAX];  /* whitelist file name */

	static struct option long_opts[] = {
		{"help", no_argument, NULL, 'h'},
		{"silent" , no_argument, NULL, 's'},
		{"blacklist", required_argument, NULL, 'b'},
		{"whitelist", required_argument, NULL, 'w'},
		{"strict", no_argument, NULL, 't'},
		{"file", required_argument, NULL, 'f'},
		{0, 0, 0, 0}
	};

	for (i = 0; i < CG_CONTROLLER_MAX; i++)
		wanted_cont[i][0] = '\0';
	flags = 0;

	/* parse arguments */
	while ((c = getopt_long(argc, argv, "hsb:w:tf:", long_opts, NULL))
		> 0) {
		switch (c) {
		case 'h':
			usage(0, argv[0]);
			return 0;
		case 's':
			flags |= FL_SILENT;
			break;
		case 'b':
			flags |= FL_BLACK;
			strncpy(bl_file, optarg, FILENAME_MAX);
			bl_file[FILENAME_MAX-1] = '\0';
			break;
		case 'w':
			flags |= FL_WHITE;
			strncpy(wl_file, optarg, FILENAME_MAX);
			wl_file[FILENAME_MAX-1] = '\0';
			break;
		case 't':
			flags |= FL_STRICT;
			break;
		case 'f':
			flags |= FL_OUTPUT;
			of = fopen(optarg, "w");
			if (of == NULL) {
				fprintf(stderr, "%s: Failed to open file %s\n",
					argv[0], optarg);
				return ECGOTHER;
			}
			break;
		default:
			usage(1, argv[0]);
			return -1;
		}
	}

	/* read the list of controllers */
	while (optind < argc) {
		flags |= FL_LIST;
		strncpy(wanted_cont[c_number], argv[optind], FILENAME_MAX);
		(wanted_cont[c_number])[FILENAME_MAX-1] = '\0';
		c_number++;
		optind++;
		if (optind == CG_CONTROLLER_MAX-1) {
			fprintf(stderr, "too many parameters\n");
			break;
		}
	}

	if ((flags & FL_OUTPUT) == 0)
		of = stdout;

	/* blacklkist */
	if (flags & FL_BLACK) {
		ret  = load_list(bl_file, &black_list);
	} else {
		/* load the blacklist from the default location */
		ret  = load_list(BLACKLIST_CONF, &black_list);
	}
	if (ret != 0)
		goto finish;

	/* whitelist */
	if (flags & FL_WHITE)
		ret = load_list(wl_file, &white_list);
	if (ret != 0)
		goto finish;

	/* print the header */
	fprintf(of, "# Configuration file generated by cgsnapshot\n");

	/* initialize libcgroup */
	ret = cgroup_init();
	if (ret)
		/* empty configuration file */
		goto finish;

	/* print mount points section */
	ret = parse_mountpoints(wanted_cont, argv[0]);
	/* continue with processing on error*/

	/* print hierarchies section */
	/*replace error from parse_mountpoints() only with another error*/
	err = parse_controllers(wanted_cont, argv[0]);
	if (err)
		ret = err;

finish:
	free_list(black_list);
	free_list(white_list);

	if (of != stdout)
		fclose(of);

	return ret;
}
