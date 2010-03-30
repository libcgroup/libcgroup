#include <libcgroup.h>
#include <libcgroup-internal.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

#include "tools-common.h"

#define FL_RULES	1
#define FL_COPY		2

enum {
	COPY_FROM_OPTION = CHAR_MAX + 1
};

static struct option const long_options[] =
{
	{"rule", required_argument, NULL, 'r'},
	{"help", no_argument, NULL, 'h'},
	{"copy-from", required_argument, NULL, COPY_FROM_OPTION},
	{NULL, 0, NULL, 0}
};

int flags; /* used input method */

static struct cgroup *copy_name_value_from_cgroup(char src_cg_path[FILENAME_MAX])
{
	int ret = 0;
	struct cgroup *src_cgroup;

	/* create source cgroup */
	src_cgroup = cgroup_new_cgroup(src_cg_path);
	if (!src_cgroup) {
		fprintf(stderr, "can't create cgroup: %s\n",
			cgroup_strerror(ECGFAIL));
		goto scgroup_err;
	}

	/* copy the name-version values to the cgroup structure */
	ret = cgroup_get_cgroup(src_cgroup);
	if (ret != 0) {
		fprintf(stderr, "cgroup %s error: %s \n",
			src_cg_path, cgroup_strerror(ret));
		goto scgroup_err;
	}

	return src_cgroup;

scgroup_err:
	cgroup_free(&src_cgroup);
	return NULL;
}


static void usage(int status, const char *program_name)
{
	if (status != 0)
		fprintf(stderr, "Wrong input parameters,"
			" try %s --help' for more information.\n",
			program_name);
	else {
		printf("Usage: %s [-r <name=value>]  <cgroup_path> ...\n"
			"   or: %s --copy-from <source_cgrup_path> "
			    "<cgroup_path> ...\n",
			program_name, program_name);
	}
}

int main(int argc, char *argv[])
{
	int ret = 0;
	int c;

	char *buf;
	struct control_value *name_value = NULL;
	int nv_number = 0;
	int nv_max = 0;

	char src_cg_path[FILENAME_MAX];
	struct cgroup *src_cgroup;
	struct cgroup *cgroup;

	/* no parametr on input */
	if (argc < 2) {
		fprintf(stderr, "Usage is %s -r <name=value> "
			"<relative path to cgroup>\n", argv[0]);
		return -1;
	}

	/* parse arguments */
	while ((c = getopt_long (argc, argv,
		"r:h", long_options, NULL)) != -1) {
		switch (c) {
		case 'h':
			usage(0, argv[0]);
			ret = 0;
			goto err;
			break;

		case 'r':
			if ((flags &  FL_COPY) != 0) {
				usage(1, argv[0]);
				ret = -1;
				goto err;
			}
			flags |= FL_RULES;

			/* add name-value pair to buffer
				(= name_value variable) */
			if (nv_number >= nv_max) {
				nv_max += CG_NV_MAX;
				name_value = (struct control_value *)
					realloc(name_value,
					nv_max * sizeof(struct control_value));
				if (!name_value) {
					fprintf(stderr, "%s: "
						"not enough memory\n",
						argv[0]);
					ret = -1;
					goto err;
				}
			}

			/* parse optarg value */
			/* there is necessary to input the tuple n=v */
			buf = strtok(optarg, "=");
			if (buf == NULL) {
				fprintf(stderr, "%s: "
					"wrong parameter of option -r: %s\n",
					argv[0], optarg);
				ret = -1;
				goto err;
			}

			strncpy(name_value[nv_number].name, buf, FILENAME_MAX);
			name_value[nv_number].name[FILENAME_MAX-1] = '\0';

			buf = strtok(NULL, "=");
			if (buf == NULL) {
				fprintf(stderr, "%s: "
					"wrong parameter of option -r: %s\n",
					argv[0], optarg);
				ret = -1;
				goto err;
			}

			strncpy(name_value[nv_number].value, buf, CG_VALUE_MAX);
			name_value[nv_number].value[CG_VALUE_MAX-1] = '\0';

			nv_number++;
			break;
		case COPY_FROM_OPTION:
			if (flags != 0) {
				usage(1, argv[0]);
				ret = -1;
				goto err;
			}
			flags |= FL_COPY;
			strncpy(src_cg_path, optarg, FILENAME_MAX);
			src_cg_path[FILENAME_MAX-1] = '\0';
			break;
		default:
			usage(1, argv[0]);
			ret = -1;
			goto err;
			break;
		}
	}

	/* no cgroup name */
	if (!argv[optind]) {
		fprintf(stderr, "%s: no cgroup specified\n", argv[0]);
		ret = -1;
		goto err;
	}

	if (flags == 0) {
		fprintf(stderr, "%s: no name-value pair was set\n", argv[0]);
		ret = -1;
		goto err;
	}

	/* initialize libcgroup */
	ret = cgroup_init();
	if (ret) {
		fprintf(stderr, "%s: libcgroup initialization failed: %s\n",
			argv[0], cgroup_strerror(ret));
		goto err;
	}

	/* copy the name-value pairs from -r options */
	if ((flags & FL_RULES) != 0) {
		src_cgroup = create_cgroup_from_name_value_pairs(
			"tmp", name_value, nv_number);
		if (src_cgroup == NULL)
			goto err;
	}

	/* copy the name-value from the given group */
	if ((flags & FL_COPY) != 0) {
		src_cgroup = copy_name_value_from_cgroup(src_cg_path);
		if (src_cgroup == NULL)
			goto err;
	}

	while (optind < argc) {
		/* create new cgroup */
		cgroup = cgroup_new_cgroup(argv[optind]);
		if (!cgroup) {
			ret = ECGFAIL;
			fprintf(stderr, "%s: can't add new cgroup: %s\n",
				argv[0], cgroup_strerror(ret));
			goto cgroup_free_err;
		}

		/* copy the values from the source cgroup to new one */
		ret = cgroup_copy_cgroup(cgroup, src_cgroup);
		if (ret != 0) {
			fprintf(stderr, "%s: cgroup %s error: %s \n",
				argv[0], src_cg_path, cgroup_strerror(ret));
			goto cgroup_free_err;
		}

		/* modify cgroup based on values of the new one */
		ret = cgroup_modify_cgroup(cgroup);
		if (ret) {
			fprintf(stderr, "%s: "
				"the group can't be modified\n",
				argv[0]);
			goto cgroup_free_err;
		}

		optind++;
		cgroup_free(&cgroup);
	}

cgroup_free_err:
	if (cgroup)
		cgroup_free(&cgroup);
	cgroup_free(&src_cgroup);

err:
	free(name_value);
	return ret;
}
