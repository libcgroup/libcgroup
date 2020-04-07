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
 * Description: This file contains the test code for testing libcgroup apis.
 */

#include "libcgrouptest.h"
#include <errno.h>

int cpu, memory;
int fs_mounted;
/* We use mountpoint for single mount.
 * For multimount we use mountpoint and mountpoint2.
 */
char mountpoint[FILENAME_MAX], mountpoint2[FILENAME_MAX];

int main(int argc, char *argv[])
{
	int retval;
	struct uid_gid_t ids = {0}; /* Set default control permissions */

	struct cntl_val_t cval;
	cval.val_int64 = 200000;
	cval.val_uint64 = 200000;
	cval.val_bool = 1;
	strcpy(cval.val_string, "200000");

	struct cgroup *cgroup1, *cgroup2, *cgroup3, *nullcgroup = NULL;
	struct cgroup_controller *sec_controller;
	/* In case of multimount for readability we use the controller name
	 * before the cgroup structure name */
	struct cgroup *ctl1_cgroup1, *ctl2_cgroup1, *ctl2_cgroup2;
	struct cgroup *mod_ctl1_cgroup1, *mod_ctl2_cgroup1, *mod_common_cgroup;
	struct cgroup *common_cgroup;
	char controller_name[FILENAME_MAX], control_file[FILENAME_MAX];
	char path_control_file[FILENAME_MAX];

	/* Get controllers name from script */
	int ctl1 = CPU, ctl2 = MEMORY;

	if ((argc < 2) || (argc > 6) || (atoi(argv[1]) < 0)) {
		printf("ERROR: Wrong no of parameters recieved from script\n");
		printf("Exiting the libcgroup testset\n");
		exit(1);
	}
	fs_mounted = atoi(argv[1]);
	cgroup_dbg("C:DBG: fs_mounted as recieved from script=%d\n",
								fs_mounted);
	/* All possible controller will be element of an enum */
	if (fs_mounted) {
		ctl1 = atoi(argv[2]);
		ctl2 = atoi(argv[3]);
		strncpy(mountpoint, argv[4], sizeof(mountpoint) - 1);
		cgroup_dbg("C:DBG: mountpoint1 as recieved from script=%s\n",
								 mountpoint);
		if (fs_mounted == FS_MULTI_MOUNTED) {
			strncpy(mountpoint2, argv[5], sizeof(mountpoint2) - 1);
			cgroup_dbg("C:DBG: mountpoint2 as recieved from "
					"script=%s\n", mountpoint2);
		}

	}

	/*
	 * check if one of the supported controllers is cpu or memory
	 */
	is_subsystem_enabled("cpu", &cpu);
	is_subsystem_enabled("memory", &memory);
	if (cpu == 0 && memory == 0) {
		fprintf(stderr, "none of cpu and memory controllers"
						" is enabled in kernel\n");
		fprintf(stderr, "Exiting the libcgroup testcases......\n");
		exit(1);
	}

	/*
	 * Testsets: Testcases are broadly devided into 3 categories based on
	 * filesystem(fs) mount scenario. fs not mounted, fs mounted, fs multi
	 * mounted. Call different apis in these different scenarios.
	 */

	switch (fs_mounted) {

	case FS_NOT_MOUNTED:

		/*
		 * Test01: call cgroup_init() and check return values
		 * Exp outcome: error ECGROUPNOTMOUNTED
		 */

		test_cgroup_init(ECGROUPNOTMOUNTED, 1);

		/*
		 * Test02: call cgroup_attach_task() with null group
		 * Exp outcome: error non zero return value
		 */

		test_cgroup_attach_task(ECGROUPNOTINITIALIZED, nullcgroup,
						 NULL, NULL, 0, NULLGRP, 2);

		/*
		 * Test03: Create a valid cgroup ds and check all return values
		 * Exp outcome: no error
		 */

		cgroup1 = create_new_cgroup_ds(0, "group1",
						 STRING, cval, ids, 3);

		/*
		 * Test04: Then Call cgroup_create_cgroup() with this valid grp
		 * Exp outcome: non zero return value
		 */
		test_cgroup_create_cgroup(ECGROUPNOTINITIALIZED, cgroup1,
							 "group1", 0, 1, 1, 4);

		/*
		 * Test05: delete cgroup
		 * Exp outcome: non zero return value but what ?
		 */
		test_cgroup_delete_cgroup(ECGROUPNOTINITIALIZED, cgroup1,
							 "group1", 0, 1, 1, 5);

		/*
		 * Test06: Check if cgroup_create_cgroup() handles a NULL cgroup
		 * Exp outcome: error ECGROUPNOTALLOWED
		 */
		test_cgroup_create_cgroup(ECGROUPNOTINITIALIZED, nullcgroup,
							 "group1", 0, 1, 1, 6);

		/*
		 * Test07: delete nullcgroup
		 */
		test_cgroup_delete_cgroup(ECGROUPNOTINITIALIZED, nullcgroup,
							 "group1", 0, 1, 1, 7);
		/* Test08: test the wrapper */
		test_cgroup_add_free_controller(8);

		cgroup_free(&nullcgroup);
		cgroup_free(&cgroup1);

		break;

	case FS_MOUNTED:

		/* Do a sanity check if cgroup fs is mounted */
		if (check_fsmounted(0)) {
			printf("Sanity check fails. cgroup fs not mounted\n");
			printf("Exiting without running this set of tests\n");
			exit(1);
		}

		/*
		 * Test01: call cgroup_attach_task() with null group
		 * without calling cgroup_init(). We can check other apis too.
		 * Exp outcome: error ECGROUPNOTINITIALIZED
		 */

		test_cgroup_attach_task(ECGROUPNOTINITIALIZED, nullcgroup,
						 NULL, NULL, 0, NULLGRP, 1);

		/*
		 * Test02: call cgroup_init() and check return values
		 * Exp outcome:  no error. return value 0
		 */

		test_cgroup_init(0, 2);

		/*
		 * Test03: Call cgroup_attach_task() with null group and check
		 * if return values are correct. If yes check if task exists in
		 * root group tasks file
		 * TODO: This test needs some modification in script
		 * Exp outcome: current task should be attached to root group
		 */

		test_cgroup_attach_task(0, nullcgroup, NULL, NULL, 0,
							 NULLGRP, 3);
		/*
		 * Test04: Call cgroup_attach_task_pid() with null group
		 * and invalid pid
		 * Exp outcome: error
		 */
		retval = cgroup_attach_task_pid(nullcgroup, -1);
		if (retval != 0)
			message(4, PASS, "attach_task_pid()", retval,
							 info[NOMESSAGE]);
		else
			message(4, FAIL, "attach_task_pid()", retval,
							 info[NOMESSAGE]);

		/*
		 * Test05: Create a valid cgroup structure
		 * Exp outcome: no error. 0 return value
		 */
		cgroup1 = create_new_cgroup_ds(ctl1, "group1",
							 STRING, cval, ids, 5);
		if (!cgroup1) {
			fprintf(stderr, "Failed to create new cgroup ds. "
					"Trying with second controller\n");
			cgroup1 = create_new_cgroup_ds(ctl2, "group1", STRING,
								cval, ids, 5);
			if (!cgroup1) {
				fprintf(stderr, "Failed to create cgroup ds. "
					"Tests dependent on this structure "
					"will fail. So exiting...\n");
				exit(1);
			}
		}

		/*
		 * Test06: Then Call cgroup_create_cgroup() with this group
		 * Exp outcome: zero return value
		 */
		test_cgroup_create_cgroup(0, cgroup1, "group1", 0, 1, 1, 6);

		/*
		 * Test07: Call cgroup_attach_task() with valid cgroup and check
		 * if return values are correct. If yes check if task exists in
		 * that group's tasks file
		 * Exp outcome: current task should be attached to that group
		 */

		test_cgroup_attach_task(0, cgroup1, "group1", NULL,
							 0, NOMESSAGE, 7);

		/*
		 * Test08: modify cgroup with the same cgroup
		 * Exp outcome: zero return value. No change.
		 */
		set_controller(ctl1, controller_name, control_file);
		build_path(path_control_file, mountpoint,
						 "group1", control_file);
		strncpy(cval.val_string, "260000", sizeof(cval.val_string));
		retval = cgroup_modify_cgroup(cgroup1);
		/* Check if the values are changed. cval contains orig values */
		if (!retval && !group_modified(path_control_file, STRING, cval))
			message(8, PASS, "modify_cgroup()", retval,
							 info[SAMEGRP]);
		else
			message(8, FAIL, "modify_cgroup()", retval,
							 info[SAMEGRP]);

		/*
		 * Create another valid cgroup structure with same group
		 * to modify the existing group
		 */
		cgroup2 = create_new_cgroup_ds(ctl1, "group1",
						 STRING, cval, ids, 9);
		if (!cgroup2) {
			fprintf(stderr, "Failed to create new cgroup ds. "
					"Trying with second controller\n");
			cgroup2 = create_new_cgroup_ds(ctl2, "group1",
							 STRING, cval, ids, 9);
			if (!cgroup2) {
				fprintf(stderr, "Failed to create cgroup ds. "
					"Tests dependent on this structure "
					"will fail. So exiting...\n");
				exit(1);
			}
		}

		/*
		 * Test10: modify cgroup with this new cgroup
		 * Exp outcome: zero return value
		 * Drawback: In case of first attempt failure above for
		 * create_new_cgroup_ds(), this test will fail
		 */
		test_cgroup_modify_cgroup(0, cgroup2, "group1",
					 1, ctl1, ctl2, STRING, 10);

		/*
		 * Test11: modify cgroup with the null cgroup
		 * Exp outcome: zero return value.
		 */

		test_cgroup_modify_cgroup(ECGROUPNOTALLOWED, nullcgroup,
					 "group1", 1, ctl1, ctl2, STRING, 11);

		/*
		 * Create another valid cgroup structure with diff controller
		 * to modify the existing group
		 */
		cval.val_int64 = 262144;
		cgroup3 = create_new_cgroup_ds(ctl2, "group1",
						 INT64, cval, ids, 12);
		if (!cgroup3) {
			fprintf(stderr, "Failed to create new cgroup ds. "
					"Tests dependent on this structure "
					"will fail. So exiting...\n");
			exit(1);
		}

		/*
		 * Test13: modify existing group with this cgroup
		 * Exp outcome: zero return value
		 */
		test_cgroup_modify_cgroup(0, cgroup3, "group1",
						 2, ctl1, ctl2, INT64, 13);

		/* Test14: Test cgroup_get_cgroup() api
		 * The group group1 has been created and modified in the
		 * filesystem. Read it using the api and check if the values
		 * are correct as we know all the control values now.
		 * WARN: If any of the previous api fails and control reaches
		 * here, this api also will fail. Also the test function assumes
		 * that "group1" exists in fs. So call cgroup_create_cgroup()
		 * with "group1" named group before calling this test function.
		 */
		test_cgroup_get_cgroup(ctl1, ctl2, ids, 14);

		/*
		 * Test16: delete cgroup
		 * Exp outcome: zero return value
		 */
		test_cgroup_delete_cgroup(0, cgroup1, "group1", 0, 1, 1, 16);

		/*
		 * Test16: Check if cgroup_create_cgroup() handles a NULL cgroup
		 * Exp outcome: error ECGROUPNOTALLOWED
		 */
		test_cgroup_create_cgroup(ECGROUPNOTALLOWED, nullcgroup,
							 "group1", 0, 1, 1, 17);

		/*
		 * Test16: delete nullcgroup
		 */
		test_cgroup_delete_cgroup(ECGROUPNOTALLOWED, NULL,
							 "group1", 0, 1, 1, 18);

		/* Test17: Test the wrapper to compare cgroup
		 * Create 2 cgroups and test it
		 */
		test_cgroup_compare_cgroup(ctl1, ctl2, 19);

		cgroup_free(&nullcgroup);
		cgroup_free(&cgroup1);
		cgroup_free(&cgroup2);
		cgroup_free(&cgroup3);

		break;

	case FS_MULTI_MOUNTED:

		/* Do a sanity check if cgroup fs is multi mounted */
		if (check_fsmounted(1)) {
			printf("Sanity check fails. cgroup fs is not multi "
				"mounted. Exiting without running this set "
					"of testcases\n");
			exit(1);
		}

		/*
		 * Test01: call apis and check return values
		 * Exp outcome:
		 */

		/*
		 * Scenario 1: cgroup fs is multi mounted
		 * Exp outcome: no error. 0 return value
		 */

		test_cgroup_init(0, 1);

		/*
		 * Test02: Call cgroup_attach_task() with null group and check
		 * if return values are correct. If yes check if task exists in
		 * root group tasks file for each controller
		 * TODO: This test needs some modification in script
		 * Exp outcome: current task should be attached to root groups
		 */

		test_cgroup_attach_task(0, nullcgroup, NULL, NULL,
							 0, NULLGRP, 2);

		/*
		 * Test03: Create a valid cgroup structure
		 * Exp outcome: no error. 0 return value
		 */
		ctl1_cgroup1 = create_new_cgroup_ds(ctl1, "ctl1_group1",
							 STRING, cval, ids, 3);
		if (!ctl1_cgroup1) {
			fprintf(stderr, "Failed to create new cgroup ds. "
					"Tests dependent on this structure "
					"will fail. So exiting...\n");
			exit(1);
		}

		/*
		 * Test04: Then Call cgroup_create_cgroup() with this valid grp
		 * Exp outcome: zero return value
		 */
		test_cgroup_create_cgroup(0, ctl1_cgroup1,
						 "ctl1_group1", 0, 1, 1, 4);

		/*
		 * Test05: Create a valid cgroup structure
		 * Exp outcome: no error. 0 return value
		 */
		ctl2_cgroup1 = create_new_cgroup_ds(ctl2, "ctl2_group1",
							 STRING, cval, ids, 5);
		if (!ctl2_cgroup1) {
			fprintf(stderr, "Failed to create new cgroup ds. "
					"Tests dependent on this structure "
					"will fail. So exiting...\n");
			exit(1);
		}

		/*
		 * Test06: Then Call cgroup_create_cgroup() with this valid grp
		 * Exp outcome: zero return value
		 */
		test_cgroup_create_cgroup(0, ctl2_cgroup1,
						 "ctl2_group1", 0, 2, 1, 6);

		/*
		 * Test07: Call cgroup_create_cgroup() with the same group
		 * Exp outcome: zero return value as the latest changes in api
		 */
		test_cgroup_create_cgroup(0, ctl2_cgroup1,
						 "ctl2_group1", 0, 2, 1, 7);

		/*
		 * Test06: Call cgroup_attach_task() with a group with ctl1
		 * controller and check if return values are correct. If yes
		 * check if task exists in that group under only ctl1 controller
		 * hierarchy and in the root group under other controllers
		 * hierarchy.
		 */

		test_cgroup_attach_task(0, ctl1_cgroup1, "ctl1_group1",
						 NULL, 0, NOMESSAGE, 8);

		/*
		 * Test07: Call cgroup_attach_task() with a group with ctl2
		 * controller and check if return values are correct. If yes
		 * check if task exists in the groups under both controller's
		 * hierarchy.
		 */

		test_cgroup_attach_task(0, ctl2_cgroup1, "ctl1_group1",
					 "ctl2_group1", 0, NOMESSAGE, 9);

		/*
		 * Test: Create a valid cgroup structure
		 * Exp outcome: no error. 0 return value
		 */
		ctl2_cgroup2 = create_new_cgroup_ds(ctl2, "ctl2_group2",
							 STRING, cval, ids, 10);
		if (!ctl2_cgroup2) {
			fprintf(stderr, "Failed to create new cgroup ds. "
					"Tests dependent on this structure "
					"will fail. So exiting...\n");
			exit(1);
		}

		/*
		 * Test08: Try to attach a task to this non existing group.
		 * Group does not exist in fs so should return ECGROUPNOTEXIST
		 */

		test_cgroup_attach_task(ECGROUPNOTEXIST, ctl2_cgroup2,
					 NULL, NULL, 0, NOTCRTDGRP, 11);

		/*
		 * Create another valid cgroup structure with same group name
		 * to modify the existing group ctl1_group1
		 * Exp outcome: no error. 0 return value
		 */
		mod_ctl1_cgroup1 = create_new_cgroup_ds(ctl1, "ctl1_group1",
							 STRING, cval, ids, 12);
		if (!mod_ctl1_cgroup1) {
			fprintf(stderr, "Failed to create new cgroup ds. "
					"Tests dependent on this structure "
					"will fail. So exiting...\n");
			exit(1);
		}

		/*
		 * Test09: modify existing cgroup with this new cgroup
		 * Exp outcome: zero return value and control value modified
		 */
		test_cgroup_modify_cgroup(0, mod_ctl1_cgroup1, "ctl1_group1",
						 1, ctl1, ctl2, STRING, 13);

		/*
		 * Create another valid cgroup structure with same group name
		 * to modify the existing group ctl2_group1
		 * Exp outcome: no error. 0 return value
		 */
		mod_ctl2_cgroup1 = create_new_cgroup_ds(ctl2, "ctl2_group1",
							 STRING, cval, ids, 14);
		if (!mod_ctl2_cgroup1) {
			fprintf(stderr, "Failed to create new cgroup ds. "
					"Tests dependent on this structure "
					"will fail. So exiting...\n");
			exit(1);
		}

		/*
		 * Test10: modify existing cgroup with this new cgroup
		 * Exp outcome: zero return value and control value modified
		 */
		test_cgroup_modify_cgroup(0, mod_ctl2_cgroup1, "ctl2_group1",
						 2, ctl1, ctl2, STRING, 15);

		/*
		 * Test11: delete cgroups
		 * Exp outcome: zero return value
		 */
		test_cgroup_delete_cgroup(0, ctl1_cgroup1,
						 "ctl1_group1", 0, 1, 1, 16);

		/*
		 * Test09: delete other cgroups too
		 * Exp outcome: zero return value
		 */
		test_cgroup_delete_cgroup(0, ctl2_cgroup1,
						 "ctl2_group1", 0, 1, 1, 17);

		/*
		 * Test15: Create a valid cgroup structure
		 * which has multiple controllers
		 * Exp outcome: no error. 0 return value
		 */
		common_cgroup = create_new_cgroup_ds(ctl1, "commongroup",
							 STRING, cval, ids, 18);
		if (!common_cgroup) {
			fprintf(stderr, "Failed to create new cgroup ds. "
					"Tests dependent on this structure "
					"will fail. So exiting...\n");
			exit(1);
		}

		/* Add one more controller to the cgroup */
		/* This also needs to be a function.. will do?? */
		retval = set_controller(ctl2, controller_name, control_file);
		if (retval) {
			fprintf(stderr, "Setting controller failled "
				" Exiting without running further testcases\n");
			exit(1);
		}
		if (!cgroup_add_controller(common_cgroup, controller_name)) {
			message(15, FAIL, "add_controller()", retval,
							 info[NOMESSAGE]);
			fprintf(stderr, "Adding second controller failled "
				" Exiting without running further testcases\n");
			exit(1);
		}

		/*
		 * Test11: Then Call cgroup_create_cgroup() with this valid grp
		 * Exp outcome: zero return value
		 */
		test_cgroup_create_cgroup(0, common_cgroup,
						 "commongroup", 1, 2, 1, 19);

		/*
		 * Test12: Call cgroup_attach_task() with this common group
		 * and check if return values are correct. If yes check if
		 * task exists in the group under both controller's hierarchy
		 */

		test_cgroup_attach_task(0, common_cgroup, "commongroup",
					 "commongroup", 0, COMMONGRP, 20);

		/*
		 * Test18: Create a valid cgroup structure to modify the
		 * commongroup which is under multiple controllers
		 * Exp outcome: no error. 0 return value
		 */
		mod_common_cgroup = create_new_cgroup_ds(ctl1, "commongroup",
							 STRING, cval, ids, 21);
		if (!common_cgroup) {
			fprintf(stderr, "Failed to create new cgroup ds. "
					"Tests dependent on this structure "
					"will fail. So exiting...\n");
			exit(1);
		}

		/* Add one more controller to the cgroup */
		/* This also needs to be a function.. will do?? */
		retval = set_controller(ctl2, controller_name, control_file);
		if (retval) {
			fprintf(stderr, "Setting controller failled "
				" Exiting without running further testcases\n");
			exit(1);
		}
		sec_controller = cgroup_add_controller(mod_common_cgroup,
							 controller_name);
		if (!sec_controller) {
			message(18, FAIL, "add_controller()", retval,
							 info[NOMESSAGE]);
			fprintf(stderr, "Adding second controller failled "
				" Exiting without running further testcases\n");
			exit(1);
		}

		strncpy(cval.val_string, "7000064", sizeof(cval.val_string));
		retval = cgroup_add_value_string(sec_controller,
						 control_file, cval.val_string);
		if (retval)
			printf("The cgroup_modify_cgroup() test will fail\n");

		/*
		 * Test14: modify existing cgroup with this new cgroup
		 * Exp outcome: zero return value and control value modified
		 */
		test_cgroup_modify_cgroup(0, mod_common_cgroup, "commongroup",
						 0, ctl1, ctl2, STRING, 22);

		/*
		 * Test15: delete this common cgroup
		 * Exp outcome: zero return value
		 */
		test_cgroup_delete_cgroup(0, common_cgroup,
						 "commongroup", 1, 2, 1, 23);
		test_cgroup_get_cgroup(ctl1, ctl2, ids, 24);

		/* Free the cgroup structures */
		cgroup_free(&nullcgroup);
		cgroup_free(&ctl1_cgroup1);
		cgroup_free(&ctl2_cgroup1);
		cgroup_free(&ctl2_cgroup2);

		break;

	default:
		fprintf(stderr, "ERROR: Wrong parameters recieved from script. \
						Exiting tests\n");
		exit(1);
		break;
	}
	return 0;
}

void test_cgroup_modify_cgroup(int retcode, struct cgroup *cgrp,
			 const char *name, int which_ctl, int ctl1,
					 int ctl2, int value_type, int i)
{
	int retval;
	struct cntl_val_t cval = {0, 0, 0, "1000"};
	char path1_control_file[FILENAME_MAX], path2_control_file[FILENAME_MAX];
	char controller_name[FILENAME_MAX], control_file[FILENAME_MAX];

	/* Check, In case some error is expected due to a negative scenario */
	if (retcode) {
		retval = cgroup_modify_cgroup(cgrp);
		if (retval == retcode)
			message(i, PASS, "modify_cgroup()", retval,
							 info[NOMESSAGE]);
		else
			message(i, FAIL, "modify_cgroup()", retval,
							 info[NOMESSAGE]);

		return;
	}

	/* Now there is no error and it is a genuine call */
	retval = cgroup_modify_cgroup(cgrp);
	if (retval) {
		message(i, FAIL, "modify_cgroup()", retval,  info[NOMESSAGE]);
		return;
	}

	/* Let us now check if the group modified in file system */
	switch (which_ctl) { /* group modified under which controllers */

	case 1: /* group is modified under ctl1 which is always
		 * mounted at mountpoint in both cases */
		set_controller(ctl1, controller_name, control_file);
		build_path(path1_control_file, mountpoint, name, control_file);
		/* this approach will be changed in coming patches */
		strncpy(cval.val_string, "260000", sizeof(cval.val_string));

		if (!group_modified(path1_control_file, value_type, cval))
			message(i, PASS, "modify_cgroup()", retval,
							 info[NOMESSAGE]);
		else
			message(i, FAIL, "modify_cgroup()", retval,
							 info[NOMESSAGE]);

		break;
	case 2: /* group is modified under ctl2 which may be
		 * mounted at mountpoint or mountpoint2 */
		set_controller(ctl2, controller_name, control_file);

		if (fs_mounted == FS_MOUNTED)	/* group under mountpoint */
			build_path(path2_control_file, mountpoint,
						 name, control_file);
		else	/* group under mountpoint2 */
			build_path(path2_control_file, mountpoint2,
						 name, control_file);

		/* this approach will be changed in coming patches */
		strncpy(cval.val_string, "7000064", sizeof(cval.val_string));
		cval.val_int64 = 262144;
		if (!group_modified(path2_control_file, value_type, cval))
			message(i, PASS, "modify_cgroup()", retval,
							 info[NOMESSAGE]);
		else
			message(i, FAIL, "modify_cgroup()", retval,
							 info[NOMESSAGE]);

		break;
	case 0:
		/* ctl1 is always mounted at mountpoint */
		set_controller(ctl1, controller_name, control_file);
		build_path(path1_control_file, mountpoint,
						 name, control_file);
		/* ctl2 may be mounted at mountpoint or mountpoint2 depending
		 * on single or multiple mount case */
		if (fs_mounted == FS_MOUNTED) {	/* group under mountpoint */
			set_controller(ctl2, controller_name, control_file);
			build_path(path2_control_file, mountpoint,
						 name, control_file);
		} else {	/* group under mountpoint2 */
			set_controller(ctl2, controller_name, control_file);
			build_path(path2_control_file, mountpoint2,
						 name, control_file);
		}
		/* this approach will be changed in coming patches */
		strncpy(cval.val_string, "260000", sizeof(cval.val_string));
		if (!group_modified(path1_control_file, value_type, cval)) {
			strncpy(cval.val_string, "7000064",
						 sizeof(cval.val_string));
			if (!group_modified(path2_control_file,
							 value_type, cval))
				message(i, PASS, "modify_cgroup()",
					 retval, info[GRPMODINBOTHCTLS]);
			else
				message(i, FAIL, "modify_cgroup()",
					 retval, info[GRPNOTMODIN2NDCTL]);
		} else {
			message(i, FAIL, "modify_cgroup()", retval,
						 info[GRPNOTMODINANYCTL]);
		}

		break;
	default:
		printf("Wrong controller parameter received....\n");
		message(i, FAIL, "modify_cgroup()", retval, info[NOMESSAGE]);
		break;
	}

	return;
}
