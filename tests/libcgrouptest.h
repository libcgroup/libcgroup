
/*
 * Copyright IBM Corporation. 2008
 *
 * Author:	Sudhir Kumar <skumar@linux.vnet.ibm.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2.1 of the GNU Lesser General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Description: This file is the header file for libcgroup test programs.
 */

#ifndef __LIBCGROUPTEST_H
#define __LIBCGROUPTEST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>

#include <libcgroup.h>

enum cgroup_mount_t {
	FS_NOT_MOUNTED,
	FS_MOUNTED,
	FS_MULTI_MOUNTED,
};

static inline pid_t cgrouptest_gettid()
{
	return syscall(__NR_gettid);
}
#endif
