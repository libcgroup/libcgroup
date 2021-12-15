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
#include "abstraction-map.h"

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

int cgroup_convert_int(struct cgroup_controller * const dst_cgc,
		       const char * const in_value,
		       const char * const out_setting,
		       void *in_dflt, void *out_dflt)
{
#define OUT_VALUE_STR_LEN 20

	long int in_dflt_int = (long int)in_dflt;
	long int out_dflt_int = (long int)out_dflt;
	char *out_value_str = NULL;
	long int out_value;
	int ret;

	if (!in_value)
		return ECGINVAL;

	if (strlen(in_value) > 0) {
		ret = cgroup_strtol(in_value, 10, &out_value);
		if (ret)
			goto out;

		/* now scale from the input range to the output range */
		out_value = out_value * out_dflt_int / in_dflt_int;

		out_value_str = calloc(sizeof(char), OUT_VALUE_STR_LEN);
		if (!out_value_str) {
			ret = ECGOTHER;
			goto out;
		}

		ret = snprintf(out_value_str, OUT_VALUE_STR_LEN, "%ld", out_value);
		if (ret == OUT_VALUE_STR_LEN) {
			/* we ran out of room in the string. throw an error */
			cgroup_err("Error: output value too large for string: %d\n",
				   out_value);
			ret = ECGFAIL;
			goto out;
		}
	}

	ret = cgroup_add_value_string(dst_cgc, out_setting, out_value_str);

out:
	if (out_value_str)
		free(out_value_str);

	return ret;
}

int cgroup_convert_name_only(struct cgroup_controller * const dst_cgc,
			     const char * const in_value,
			     const char * const out_setting,
			     void *in_dflt, void *out_dflt)
{
	return cgroup_add_value_string(dst_cgc, out_setting, in_value);
}

static int convert_setting(struct cgroup_controller * const out_cgc,
			   const struct control_value * const in_ctrl_val)
{
	const struct cgroup_abstraction_map *convert_tbl;
	int tbl_sz = 0;
	int ret = ECGINVAL;
	int i;

	switch (out_cgc->version) {
	case CGROUP_V1:
		convert_tbl = cgroup_v2_to_v1_map;
		tbl_sz = cgroup_v2_to_v1_map_sz;
		break;
	case CGROUP_V2:
		convert_tbl = cgroup_v1_to_v2_map;
		tbl_sz = cgroup_v1_to_v2_map_sz;
		break;
	default:
		ret = ECGFAIL;
		goto out;
	}

	for (i = 0; i < tbl_sz; i++) {
		if (strcmp(convert_tbl[i].in_setting, in_ctrl_val->name) == 0) {
			ret = convert_tbl[i].cgroup_convert(out_cgc,
					in_ctrl_val->value,
					convert_tbl[i].out_setting,
					convert_tbl[i].in_dflt,
					convert_tbl[i].out_dflt);
			if (ret)
				goto out;
		}
	}

out:
	return ret;
}

static int convert_controller(struct cgroup_controller * const out_cgc,
			      const struct cgroup_controller * const in_cgc)
{
	int ret;
	int i;


	if (in_cgc->version == out_cgc->version) {
		ret = cgroup_copy_controller_values(out_cgc, in_cgc);
		/* regardless of success/failure, there's nothing more to do */
		goto out;
	}

	for (i = 0; i < in_cgc->index; i++) {
		ret = convert_setting(out_cgc, in_cgc->values[i]);
		if (ret)
			goto out;
	}

out:
	return ret;
}

int cgroup_convert_cgroup(struct cgroup * const out_cgroup,
			  enum cg_version_t out_version,
			  const struct cgroup * const in_cgroup,
			  enum cg_version_t in_version)
{
	struct cgroup_controller *cgc;
	int ret = 0;
	int i;

	for (i = 0; i < in_cgroup->index; i++) {
		cgc = cgroup_add_controller(out_cgroup,
					    in_cgroup->controller[i]->name);
		if (cgc == NULL) {
			ret = ECGFAIL;
			goto out;
		}

		/* the user has overridden the version */
		if (in_version == CGROUP_V1 || in_version == CGROUP_V2) {
			in_cgroup->controller[i]->version = in_version;
		}

		cgc->version = out_version;

		if (cgc->version == CGROUP_UNK ||
		    cgc->version == CGROUP_DISK) {
			ret = cgroup_get_controller_version(cgc->name,
				&cgc->version);
			if (ret)
				goto out;
		}

		ret = convert_controller(cgc, in_cgroup->controller[i]);
		if (ret)
			goto out;
	}

out:
	return ret;
}
