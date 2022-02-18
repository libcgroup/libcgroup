
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
#include <sys/stat.h>
#include <libcgroup.h>
#include "../config.h"
#include <unistd.h>
#include <inttypes.h>

#define SIZE 100	/* Max size of a message to be printed */
#define NUM_MSGS 20	/* Number of such messsages */
#define PASS 1		/* test passed */
#define FAIL 0		/* test failed */

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

enum info_message_t {
	NULLGRP,
	COMMONGRP,
	NOTCRTDGRP,
	SAMEGRP,
	TASKINGRP,
	TASKNOTINGRP,
	TASKNOTINANYGRP,
	GRPINFS,
	GRPNOTINFS,
	GRPINBOTHCTLS,
	GRPNOTIN2NDCTL,
	GRPNOTIN1STCTL,
	GRPMODINBOTHCTLS,
	GRPNOTMODIN2NDCTL,
	GRPNOTMODINANYCTL,
	GRPDELETEDINFS,
	GRPNOTDELETEDINFS,
	GRPNOTDELETEDGLOBALY,
	NOMESSAGE,
};

/* Keep a single struct of all ids */
struct uid_gid_t {
	uid_t control_uid;
	gid_t control_gid;
	uid_t tasks_uid;
	gid_t tasks_gid;
};

/* Keep a single struct of all control values */
struct cntl_val_t {
	int64_t val_int64;
	u_int64_t val_uint64;
	bool val_bool;
	/* size worth of 100 digit num is fair enough */
	char val_string[100];	/* string value of control parameter */
};

extern int cpu, memory;

/* The set of verbose messages useful to the user */
extern char info[NUM_MSGS][SIZE];

/* this variable is never modified */
extern int fs_mounted;

/* The mountpoints as received from script
 * We use mountpoint for single mount.
 * For multimount we use mountpoint and mountpoint2.
 */
extern char mountpoint[], mountpoint2[];

/* Functions to test each API */
void test_cgroup_init(int retcode, int i);
void test_cgroup_attach_task(int retcode, struct cgroup *cgroup1,
		const char *group1, const char *group2, pid_t pid,
		int k, int i);
struct cgroup *create_new_cgroup_ds(int ctl, const char *grpname,
	 int value_type, struct cntl_val_t cval, struct uid_gid_t ids, int i);
void test_cgroup_create_cgroup(int retcode, struct cgroup *cgrp,
		 const char *name, int common, int mpnt, int ign, int i);
void test_cgroup_delete_cgroup(int retcode, struct cgroup *cgrp,
		 const char *name, int common, int mpnt, int ign, int i);
void test_cgroup_modify_cgroup(int retcode, struct cgroup *cgrp,
		 const char *name, int which_ctl, int ctl1, int ctl2,
						 int value_type, int i);
void test_cgroup_get_cgroup(int ctl1, int ctl2, struct uid_gid_t ids, int i);
/* API test functions end here */

void test_cgroup_compare_cgroup(int ctl1, int ctl2, int i);
void test_cgroup_add_free_controller(int i);
void is_subsystem_enabled(const char *name, int *exist);
int group_exist(char *path_group);
int set_controller(int controller, char *controller_name,
						 char *control_file);
int group_modified(char *path_control_file, int value_type,
						 struct cntl_val_t cval);
int add_control_value(struct cgroup_controller *newcontroller,
	 char *control_file, char *wr, int value_type, struct cntl_val_t cval);
struct cgroup *new_cgroup(char *group, char *controller_name,
	 char *control_file, int value_type, struct cntl_val_t cval,
					 struct uid_gid_t ids, int i);
int check_fsmounted(int multimnt);
int check_task(char *tasksfile, pid_t pid);
/* function to print messages in better format */
void message(int num, int pass, const char *api,
						 int ret, char *extra);
void build_path(char *target, char *mountpoint,
				 const char *group, const char *file);
pid_t cgrouptest_gettid();

#ifdef CGROUP_DEBUG
#define cgroup_dbg(p...)	printf(p)
#else
#define cgroup_dbg(p...)	do {} while (0);
#endif

#endif
