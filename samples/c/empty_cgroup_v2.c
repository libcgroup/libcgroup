// SPDX-License-Identifier: LGPL-2.1-only
/**
 * Simple program to add empty cgroup v2
 *
 * Copyright (c) 2022 Oracle and/or its affiliates.
 * Author: Kamalesh babulal <kamalesh.baulal@oracle.com>
 */

#include <libcgroup.h>

#include <stdlib.h>
#include <stdio.h>

#define CGRP_NAME "empty_cgrp"

int main(int argc, char **argv)
{
	struct cgroup *cgrp = NULL;
	int ret = 0;

	ret = cgroup_init();
	if (ret) {
		fprintf(stderr, "cgroup_init failed\n");
		exit(1);
	}

	cgrp = cgroup_new_cgroup(CGRP_NAME);
	if (!cgrp) {
		fprintf(stderr, "Failed to allocate cgroup %s\n", CGRP_NAME);
		exit(1);
	}

	ret = cgroup_create_cgroup(cgrp, 0);
	if (ret)
		fprintf(stderr, "Failed to create cgroup %s\n", CGRP_NAME);

	cgroup_free(&cgrp);

	return ret;
}
