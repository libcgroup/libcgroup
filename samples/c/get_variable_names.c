// SPDX-License-Identifier: LGPL-2.1-only
#include "../src/libcgroup-internal.h"
#include <libcgroup.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
	struct cgroup_controller *cgrp_controller = NULL;
	struct cgroup *cgrp = NULL;
	char cgrp_name[] = "/";
	char *name;
	int count;
	int ret;
	int i, j;

	if (argc < 2) {
		printf("no list of groups provided\n");
		return -1;
	}

	ret = cgroup_init();
	if (ret) {
		printf("cgroup_init failed with %s\n", cgroup_strerror(ret));
		exit(1);
	}

	cgrp = cgroup_new_cgroup(cgrp_name);
	if (cgrp == NULL) {
		printf("cannot create cgrp '%s'\n", cgrp_name);
		return -1;
	}

	ret = cgroup_get_cgroup(cgrp);
	if (ret != 0) {
		printf("cannot read group '%s': %s\n",
			cgrp_name, cgroup_strerror(ret));
	}

	for (i = 1; i < argc; i++) {

		cgrp_controller = cgroup_get_controller(cgrp, argv[i]);
		if (cgrp_controller == NULL) {
			printf("cannot find controller '%s' in group '%s'\n",
			       argv[i], cgrp_name);
			ret = -1;
			continue;
		}

		count = cgroup_get_value_name_count(cgrp_controller);
		for (j = 0; j < count; j++) {
			name = cgroup_get_value_name(cgrp_controller, j);
			if (name != NULL)
				printf("%s\n", name);
		}
	}

	return ret;
}
