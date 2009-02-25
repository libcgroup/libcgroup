/*
 * Copyright Red Hat, Inc. 2009
 *
 * Author:	Vivek Goyal <vgoyal@redhat.com>
 *		Jan Safranek <jsafrane@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2.1 of the GNU Lesser General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <libcgroup.h>
#include "tools-common.h"

int parse_cgroup_spec(struct cgroup_group_spec *cdptr[], char *optarg)
{
	struct cgroup_group_spec *ptr;
	int i, j;
	char *cptr, *pathptr, *temp;

	ptr = *cdptr;

	/* Find first free entry inside the cgroup data array */
	for (i = 0; i < CG_HIER_MAX; i++, ptr++) {
		if (!cdptr[i])
			break;
	}

	if (i == CG_HIER_MAX) {
		/* No free slot found */
		fprintf(stderr, "Max allowed hierarchies %d reached\n",
				CG_HIER_MAX);
		return -1;
	}

	/* Extract list of controllers */
	cptr = strtok(optarg, ":");
	dbg("list of controllers is %s\n", cptr);
	if (!cptr)
		return -1;

	/* Extract cgroup path */
	pathptr = strtok(NULL, ":");
	dbg("cgroup path is %s\n", pathptr);
	if (!pathptr)
		return -1;

	/* instanciate cgroup_data. */
	cdptr[i] = malloc(sizeof(struct cgroup_group_spec));
	if (!cdptr[i]) {
		fprintf(stderr, "%s\n", strerror(errno));
		return -1;
	}
	/* Convert list of controllers into an array of strings. */
	j = 0;
	do {
		if (j == 0)
			temp = strtok(cptr, ",");
		else
			temp = strtok(NULL, ",");

		if (temp) {
			cdptr[i]->controllers[j] = strdup(temp);
			if (!cdptr[i]->controllers[j]) {
				free(cdptr[i]);
				fprintf(stderr, "%s\n", strerror(errno));
				return -1;
			}
		}
		j++;
	} while (temp);

	/* Store path to the cgroup */
	strncpy(cdptr[i]->path, pathptr, FILENAME_MAX);
	cdptr[i]->path[FILENAME_MAX-1] = '\0';

	return 0;
}
