// SPDX-License-Identifier: LGPL-2.1-only
/**
 * Libcgroup abstraction layer for the memory controller
 *
 * Copyright (c) 2021-2025 Oracle and/or its affiliates.
 * Author: Tom Hromatka <tom.hromatka@oracle.com>
 */

#include "abstraction-common.h"
#include "abstraction-map.h"

#include <libcgroup.h>
#include <libcgroup-internal.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

#define V1_NEG1_STR "-1"
#define V1_MAX_STR "9223372036854771712"
#define V2_MAX_STR "max"

#define LL_MAX 21

static int convert_limit_to_max(struct cgroup_controller * const dst_cgc,
		const char * const in_setting, const char * const in_value,
		const char * const out_setting)
{
	char max_line[LL_MAX] = {0};
	int ret;

	if (strlen(in_value) == 0) {
		ret = cgroup_add_value_string(dst_cgc, out_setting, NULL);
	} else {
		if (strcmp(in_value, V1_NEG1_STR) == 0)
			snprintf(max_line, LL_MAX, "%s", V2_MAX_STR);
		else if (strcmp(in_value, V1_MAX_STR) == 0)
			snprintf(max_line, LL_MAX, "%s", V2_MAX_STR);
		else
			snprintf(max_line, LL_MAX, "%s", in_value);

		ret = cgroup_add_value_string(dst_cgc, out_setting, max_line);
	}

	return ret;
}

int cgroup_convert_memory_limit_to_max(struct cgroup_controller * const dst_cgc,
				const char * const in_value, const char * const out_setting,
				void *in_dflt, void *out_dflt)
{
	return convert_limit_to_max(dst_cgc, "memory.limit_in_bytes", in_value, out_setting);
}

int cgroup_convert_memory_soft_limit_to_max(struct cgroup_controller * const dst_cgc,
				const char * const in_value, const char * const out_setting,
				void *in_dflt, void *out_dflt)
{
	return convert_limit_to_max(dst_cgc, "memory.soft_limit_in_bytes", in_value, out_setting);
}

static int convert_max_to_limit(struct cgroup_controller * const dst_cgc,
		const char * const in_setting,  const char * const in_value,
		const char * const out_setting)
{
	char limit_line[LL_MAX] = {0};
	int ret;

	if (strlen(in_value) == 0) {
		ret = cgroup_add_value_string(dst_cgc, out_setting, NULL);
	} else {
		if (strcmp(in_value, V2_MAX_STR) == 0)
			snprintf(limit_line, LL_MAX, "%s", V1_MAX_STR);
		else
			snprintf(limit_line, LL_MAX, "%s", in_value);

		ret = cgroup_add_value_string(dst_cgc, out_setting, limit_line);
	}

	return ret;
}

int cgroup_convert_memory_max_to_limit(struct cgroup_controller * const dst_cgc,
				const char * const in_value, const char * const out_setting,
				void *in_dflt, void *out_dflt)
{
	return convert_max_to_limit(dst_cgc, "memory.max", in_value, out_setting);
}

int cgroup_convert_memory_high_to_soft_limit(struct cgroup_controller * const dst_cgc,
				const char * const in_value, const char * const out_setting,
				void *in_dflt, void *out_dflt)
{
	return convert_max_to_limit(dst_cgc, "memory.high", in_value, out_setting);
}
