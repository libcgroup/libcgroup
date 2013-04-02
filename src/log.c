/*
 * Copyright Red Hat, Inc. 2012
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
 */

#include <libcgroup.h>
#include <libcgroup-internal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

static cgroup_logger_callback cgroup_logger;
static void *cgroup_logger_userdata;
static int cgroup_loglevel;

static void cgroup_default_logger(void *userdata, int level, const char *fmt,
				  va_list ap)
{
	vfprintf(stdout, fmt, ap);
}

void cgroup_log(int level, const char *fmt, ...)
{
	va_list ap;

	if (!cgroup_logger)
		return;

	if (level > cgroup_loglevel)
		return;

	va_start(ap, fmt);
	cgroup_logger(cgroup_logger_userdata, level, fmt, ap);
	va_end(ap);
}

void cgroup_set_logger(cgroup_logger_callback logger, int loglevel,
		void *userdata)
{
	cgroup_logger = logger;
	cgroup_set_loglevel(loglevel);
	cgroup_logger_userdata = userdata;
}

void cgroup_set_default_logger(int level)
{
	if (!cgroup_logger)
		cgroup_set_logger(cgroup_default_logger, level, NULL);
}

void cgroup_set_loglevel(int loglevel)
{
	if (loglevel != -1)
		cgroup_loglevel = loglevel;
	else {
		char *level_str = getenv("CGROUP_LOGLEVEL");
		if (level_str != NULL)
			/*
			 * TODO: add better loglevel detection, e.g. strings
			 * instead of plain numbers.
			 */
			cgroup_loglevel = atoi(level_str);
		else
			cgroup_loglevel = CGROUP_DEFAULT_LOGLEVEL;
	}
}
