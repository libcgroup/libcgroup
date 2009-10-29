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

int parse_cgroup_spec(struct cgroup_group_spec **cdptr, char *optarg,
		int capacity)
{
	struct cgroup_group_spec *ptr;
	int i, j;
	char *cptr, *pathptr, *temp;

	ptr = *cdptr;

	/* Find first free entry inside the cgroup data array */
	for (i = 0; i < capacity; i++, ptr++) {
		if (!cdptr[i])
			break;
	}

	if (i == capacity) {
		/* No free slot found */
		fprintf(stderr, "Max allowed hierarchies %d reached\n",
				capacity);
		return -1;
	}

	/* Extract list of controllers */
	cptr = strtok(optarg, ":");
	cgroup_dbg("list of controllers is %s\n", cptr);
	if (!cptr)
		return -1;

	/* Extract cgroup path */
	pathptr = strtok(NULL, ":");
	cgroup_dbg("cgroup path is %s\n", pathptr);
	if (!pathptr)
		return -1;

	/* instanciate cgroup_data. */
	cdptr[i] = calloc(1, sizeof(struct cgroup_group_spec));
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


/**
 * Free a single cgroup_group_spec structure
 * <--->@param cl The structure to free from memory.
 */
void cgroup_free_group_spec(struct cgroup_group_spec *cl)
{
	/* Loop variable */
	int i = 0;

	/* Make sure our structure is not NULL, first. */
	if (!cl) {
		cgroup_dbg("Warning: Attempted to free NULL rule.\n");
		return;
	}

	/* We must free any used controller strings, too. */
	for (i = 0; i < CG_CONTROLLER_MAX; i++) {
		if (cl->controllers[i])
			free(cl->controllers[i]);
	}

	free(cl);
}


