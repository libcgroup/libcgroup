/*
 * Copyright IBM Corporation. 2008
 *
 * Author:	Dhaval Giani <dhaval@linux.vnet.ibm.com>
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
#ifndef __LIBCG_INTERNAL

#define __LIBCG_INTERNAL

__BEGIN_DECLS

#include <libcgroup.h>

struct control_value {
	char name[FILENAME_MAX];
	char value[CG_VALUE_MAX];
};

struct cgroup_controller {
	char name[FILENAME_MAX];
	struct control_value *values[CG_NV_MAX];
	int index;
};

struct cgroup {
	char name[FILENAME_MAX];
	struct cgroup_controller *controller[CG_CONTROLLER_MAX];
	int index;
	uid_t tasks_uid;
	gid_t tasks_gid;
	uid_t control_uid;
	gid_t control_gid;
};


struct cg_mount_table_s {
	char name[FILENAME_MAX];
	char path[FILENAME_MAX];
};

__END_DECLS

#endif
