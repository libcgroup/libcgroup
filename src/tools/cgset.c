#include <libcgroup.h>
#include <libcgroup-internal.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tools-common.h"

int main(int argc, char *argv[])
{
	int ret = 0;
	int i;
	char c;

	char *buf;
	char con[FILENAME_MAX];
	struct control_value *name_value = NULL;
	int nv_number = 0;
	int nv_max = 0;

	struct cgroup *cgroup;
	struct cgroup_controller *cgc;

	/* no parametr on input */
	if (argc < 2) {
		fprintf(stderr, "Usage is %s -r <name=value> "
			"<relative path to cgroup>\n", argv[0]);
		return -1;
	}

	/* parse arguments */
	while ((c = getopt(argc, argv, "r:")) > 0) {
		switch (c) {
		case 'r':
			/* add name-value pair to buffer
				(= name_value variable) */
			if (nv_number >= nv_max) {
				nv_max += CG_NV_MAX;
				name_value = (struct control_value *)
					realloc(name_value,
					nv_max * sizeof(struct control_value));
				if (!name_value) {
					fprintf(stderr, "cgset: "
						"not enough memory\n");
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
		default:
			fprintf(stderr, "%s: invalid command line option\n",
				argv[0]);
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

	if (nv_number == 0) {
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

	while (optind < argc) {

		/* create new cgroup */
		cgroup = cgroup_new_cgroup(argv[optind]);
		if (!cgroup) {
			ret = ECGFAIL;
			fprintf(stderr, "%s: can't add new cgroup: %s\n",
				argv[0], cgroup_strerror(ret));
			ret = -1;
			goto err;
		}

		/* add pairs name-value to
		   relevant controllers of this cgroup */
		for (i = 0; i < nv_number; i++) {

			if ((strchr(name_value[i].name, '.')) == NULL) {
				fprintf(stderr, "%s: "
					"wrong -r  parameter (%s=%s)\n",
					argv[0], name_value[i].name,
					name_value[i].value);
				ret = -1;
				goto cgroup_free_err;
			}

			strncpy(con, name_value[i].name, FILENAME_MAX);
			strtok(con, ".");

			/* add relevant controller */
			cgc = cgroup_add_controller(cgroup, con);
			if (!cgc) {
				fprintf(stderr, "%s: "
					"controller %s can't be add\n",
					argv[0], con);
				goto cgroup_free_err;
			}

			/* add name-value pair to this controller */
			ret = cgroup_add_value_string(cgc,
				name_value[i].name, name_value[i].value);
			if (ret) {
				fprintf(stderr, "%s: "
					"name-value pair %s=%s "
					"can't be set\n",
					argv[0], name_value[i].name,
					name_value[i].value);
				goto cgroup_free_err;
			}
		}

		/* modify cgroup */
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
	if (ret)
		cgroup_free(&cgroup);
err:
	free(name_value);
	return ret;
}
