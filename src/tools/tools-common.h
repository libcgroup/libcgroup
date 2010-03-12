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

#ifndef __TOOLS_COMMON

#define __TOOLS_COMMON

#include "config.h"
#include <libcgroup.h>
#include <libcgroup-internal.h>

#ifdef CGROUP_DEBUG
#define cgroup_dbg(x...) printf(x)
#else
#define cgroup_dbg(x...) do {} while (0)
#endif

/**
 * Auxiliary specifier of group, used to store parsed command line options.
 */
struct cgroup_group_spec {
	char path[FILENAME_MAX];
	char *controllers[CG_CONTROLLER_MAX];
};


/**
 * Parse command line option with group specifier into provided data structure.
 * The option must have form of 'controller1,controller2,..:group_name'.
 *
 * The parsed list of controllers and group name is added at the end of
 * provided cdptr, i.e. on place of first NULL cgroup_group_spec*.
 *
 * @param cdptr Target data structure to fill. New item is allocated and added
 * 		at the end.
 * @param optarg Argument to parse.
 * @param capacity Capacity of the cdptr array.
 * @return 0 on success, != 0 on error.
 */
int parse_cgroup_spec(struct cgroup_group_spec **cdptr, char *optarg,
		int capacity);

/**
 * Free a single cgroup_group_spec structure.
 * 	@param cl The structure to free from memory
 */
void cgroup_free_group_spec(struct cgroup_group_spec *cl);

#endif /* TOOLS_COMMON */
