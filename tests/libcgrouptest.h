
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

#include <errno.h>
#include <fcntl.h>
#include <mntent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>

#include <libcgroup.h>

#define SIZE 100	/* Max size of a message to be printed */
#define NUM_MSGS 10	/* Number of such messsages */
#define PASS 1		/* test passed */
#define FAIL 0		/* test failed */

int cpu = 0, memory = 0;

enum cgroup_mount_t {
	FS_NOT_MOUNTED,
	FS_MOUNTED,
	FS_MULTI_MOUNTED,
};

enum controller_t {
	CPU,
	MEMORY,
	CPUSET,
	/* Add new controllers here */
};

enum cgroup_control_val_t {
	BOOL,
	INT64,
	UINT64,
	STRING,
};

/* Create a matrix of possible info messages */
char **info;

int64_t val_int64;
u_int64_t val_uint64;
bool val_bool;
/* Doubt: size of following string. is'nt this wrong ?*/
char val_string[FILENAME_MAX];	/* string value of control parameter */
uid_t control_uid;
gid_t control_gid;
uid_t tasks_uid;
gid_t tasks_gid;
static int i;

/* The mountpoints as received from script */
char mountpoint[FILENAME_MAX], mountpoint2[FILENAME_MAX];

/* No extra message unless specified */
char extra[SIZE] = "\n";

/* Functions to test each API */
void test_cgroup_init(int retcode, int i);
void test_cgroup_attach_task(int retcode, struct cgroup *cgroup1,
				const char *group1, const char *group2,
				int fs_info, int k, int i);
/* API test functions end here */

void get_controllers(const char *name, int *exist);
static int group_exist(char *path_group);
static int set_controller(int controller, char *controller_name,
						 char *control_file);
static int group_modified(char *path_control_file, int value_type);
static int add_control_value(struct cgroup_controller *newcontroller,
				 char * control_file, char *wr, int value_type);
struct cgroup *new_cgroup(char *group, char *controller_name,
				 char *control_file, int value_type);
int check_fsmounted(int multimnt);
static int check_task(char *tasksfile);
/* function to print messages in better format */
static inline void message(int num, int pass, const char *api,
						 int ret, char *extra);
static inline void build_path(char *target, char *mountpoint,
				 const char *group, const char *file);

/* Allocate memory and populate info messages */
void set_info_msgs();
/* Free the allocated memory for info messages */
void free_info_msgs();

static inline pid_t cgrouptest_gettid()
{
	return syscall(__NR_gettid);
}
#endif
