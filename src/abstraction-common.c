/**
 * Libcgroup abstraction layer
 *
 * Copyright (c) 2021-2022 Oracle and/or its affiliates.
 * Author: Tom Hromatka <tom.hromatka@oracle.com>
 */

/*
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of version 2.1 of the GNU Lesser General Public License as
 * published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses>.
 */

#include <libcgroup.h>
#include <libcgroup-internal.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "abstraction-common.h"

int cgroup_strtol(const char * const in_str, int base,
		  long int * const out_value)
{
	char *endptr = NULL;
	int ret = 0;

	if (out_value == NULL) {
		cgroup_err("Error: Invalid parameter to %s\n", __func__);
		ret = ECGINVAL;
		goto out;
	}

	errno = 0;
	*out_value = strtol(in_str, &endptr, base);

	/* taken directly from strtol's man page */
	if ((errno == ERANGE &&
	     (*out_value == LONG_MAX || *out_value == LONG_MIN))
	    || (errno != 0 && *out_value == 0)) {
		cgroup_err("Error: Failed to convert %s from strtol: %s\n",
			   in_str);
		ret = ECGFAIL;
		goto out;
	}

	if (endptr == in_str) {
		cgroup_err("Error: No long value found in %s\n",
			   in_str);
		ret = ECGFAIL;
		goto out;
	}

out:
	return ret;
}
