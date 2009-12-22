/*
 * Copyright IBM Corporation. 2009
 *
 * Authors:	Dhaval Giani <dhaval@linux.vnet.ibm.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2.1 of the GNU Lesser General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Code initiated and designed by Dhaval Giani. All faults are most likely
 * his mistake.
 */

#include <libcgroup.h>
#include <libcgroup-internal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int main(int argc, char *argv[])
{
	int error;

	error = cgroup_unload_cgroups();
	/* Don't spit an error when there is nothing to clear. */
	if (error == ECGROUPNOTMOUNTED)
		error = 0;

	if (error) {
		printf("%s failed with %s\n", argv[0], cgroup_strerror(error));
		exit(3);
	}

	return 0;
}
