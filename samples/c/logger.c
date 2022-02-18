/*
 * Copyright Red Hat Inc., 2012
 *
 * Author:	Jan Safranek <jsafrane@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2.1 of the GNU Lesser General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Description: This file contains the test code for libcgroup logging.
 */

#include "config.h"
#include "libcgroup.h"
#include "../src/libcgroup-internal.h"
#include <string.h>
#include <stdlib.h>

static void mylogger(void *userdata, int loglevel, const char *fmt, va_list ap)
{
	printf("custom: ");
	vprintf(fmt, ap);
}

int main(int argc, char **argv)
{
	int custom = 0;
	int loglevel = -1;
	int i;

	for (i = 1; i < argc; i++) {
		if (strcmp("custom", argv[i]) == 0)
			custom = 1;
		else
			loglevel = atoi(argv[i]);
	}

	if (custom)
		cgroup_set_logger(mylogger, loglevel, NULL);
	else
		cgroup_set_default_logger(loglevel);

	cgroup_dbg("DEBUG message\n");
	cgroup_info("INFO message\n");
	cgroup_warn("WARNING message\n");
	cgroup_err("ERROR message\n");

	return 0;
}
