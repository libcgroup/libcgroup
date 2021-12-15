/**
 * Libcgroup abstraction layer mappings
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

const struct cgroup_abstraction_map cgroup_v1_to_v2_map[] = {
	{cgroup_convert_int, "cpu.shares", (void *)1024, "cpu.weight", (void *)100},
	{cgroup_convert_cpu_quota_to_max, "cpu.cfs_quota_us", NULL, "cpu.max", NULL},
	{cgroup_convert_cpu_period_to_max, "cpu.cfs_period_us", NULL, "cpu.max", NULL},

	/* cpuset controller */
	{cgroup_convert_name_only, "cpuset.effective_cpus", NULL,
		"cpuset.cpus.effective", NULL},
	{cgroup_convert_name_only, "cpuset.effective_mems", NULL,
		"cpuset.mems.effective", NULL},
};
const int cgroup_v1_to_v2_map_sz = sizeof(cgroup_v1_to_v2_map) /
				   sizeof(cgroup_v1_to_v2_map[0]);

const struct cgroup_abstraction_map cgroup_v2_to_v1_map[] = {
	{cgroup_convert_int, "cpu.weight", (void *)100, "cpu.shares", (void *)1024},
	{cgroup_convert_cpu_max_to_quota, "cpu.max", NULL, "cpu.cfs_quota_us", NULL},
	{cgroup_convert_cpu_max_to_period, "cpu.max", NULL, "cpu.cfs_period_us", NULL},

	/* cpuset controller */
	{cgroup_convert_name_only, "cpuset.cpus.effective", NULL,
		"cpuset.effective_cpus", NULL},
	{cgroup_convert_name_only, "cpuset.mems.effective", NULL,
		"cpuset.effective_mems", NULL},
};
const int cgroup_v2_to_v1_map_sz = sizeof(cgroup_v2_to_v1_map) /
				   sizeof(cgroup_v2_to_v1_map[0]);
