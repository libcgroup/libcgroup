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

/* The messages that may be useful to the user */
char info[NUM_MSGS][SIZE] = {
	" Parameter nullcgroup\n", 			/* NULLGRP */
	" Parameter commoncgroup\n",			/* COMMONGRP */
	" Parameter not created group\n",		/* NOTCRTDGRP */
	" Parameter same cgroup\n",			/* SAMEGRP */
	" Task found in group/s\n",			/* TASKINGRP */
	" Task not found in group/s\n",			/* TASKNOTINGRP */
	" Task not found in all groups\n",		/* TASKNOTINANYGRP */
	" group found in filesystem\n",			/* GRPINFS */
	" group not found in filesystem\n",		/* GRPNOTINFS */
	" group found under both controllers\n",	/* GRPINBOTHCTLS */
	" group not found under second controller\n",	/* GRPNOTIN2NDCTL */
	" group not found under first controller\n",	/* GRPNOTIN1STCTL */
	" group modified under both controllers\n",	/* GRPMODINBOTHCTLS */
	" group not modified under second controller\n",/* GRPNOTMODIN2NDCTL */
	" group not modified under any controller\n",	/* GRPNOTMODINANYCTL */
	" Group deleted from filesystem\n",		/* GRPDELETEDINFS */
	" Group not deleted from filesystem\n",		/* GRPNOTDELETEDINFS */
	" Group not deleted globally\n",		/* GRPNOTDELETEDGLOBALY */
	/* In case there is no extra info messages to be printed */
	"\n",						/* NOMESSAGE */
};

int main(int argc, char *argv[])
{
	int retval;
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
	dbg("C:DBG: fs_mounted as recieved from script=%d\n", fs_mounted);
	/* All possible controller will be element of an enum */
	if (fs_mounted) {
		ctl1 = atoi(argv[2]);
		ctl2 = atoi(argv[3]);
		strncpy(mountpoint, argv[4], sizeof(mountpoint));
		dbg("C:DBG: mountpoint1 as recieved from script=%s\n",
								 mountpoint);
		if (fs_mounted == FS_MULTI_MOUNTED) {
			strncpy(mountpoint2, argv[5], sizeof(mountpoint2));
			dbg("C:DBG: mountpoint2 as recieved from script=%s\n",
								 mountpoint2);
		}

	}

	/*
	 * check if one of the supported controllers is cpu or memory
	 */
	get_controllers("cpu", &cpu);
	get_controllers("memory", &memory);
	if (cpu == 0 && memory == 0) {
		fprintf(stderr, "none of cpu and memory controllers"
						" is enabled in kernel\n");
		fprintf(stderr, "Exiting the libcgroup testcases......\n");
		exit(1);
	}

	/* Set default control permissions */
	control_uid = 0;
	control_gid = 0;
	tasks_uid = 0;
	tasks_gid = 0;

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
					 NULL, NULL, NULLGRP, 2);

		/*
		 * Test03: Create a valid cgroup ds and check all return values
		 * Exp outcome: no error
		 */

		cgroup1 = create_new_cgroup_ds(0, "group1", STRING, 3);

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
						 NULL, NULL, NULLGRP, 1);

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

		test_cgroup_attach_task(0, nullcgroup,
						 NULL, NULL, NULLGRP, 3);
		/*
		 * Test04: Call cgroup_attach_task_pid() with null group
		 * and invalid pid
		 * Exp outcome: error
		 */
		retval = cgroup_attach_task_pid(nullcgroup, -1);
		if (retval != 0)
			message(4, PASS, "attach_task_pid()", retval, extra);
		else
			message(4, FAIL, "attach_task_pid()", retval, extra);

		/*
		 * Test05: Create a valid cgroup structure
		 * Exp outcome: no error. 0 return value
		 */
		cgroup1 = create_new_cgroup_ds(ctl1, "group1", STRING, 5);
		if (!cgroup1) {
			fprintf(stderr, "Failed to create new cgroup ds. "
					"Trying with second controller\n");
			cgroup1 = create_new_cgroup_ds(ctl2, "group1", STRING,
									5);
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
								 NOMESSAGE, 7);

		/*
		 * Test08: modify cgroup with the same cgroup
		 * Exp outcome: zero return value. No change.
		 */
		build_path(path_control_file, mountpoint,
						 "group1", control_file);
		retval = cgroup_modify_cgroup(cgroup1);
		/* Check if the values are changed */
		if (!retval && !group_modified(path_control_file, STRING))
			message(8, PASS, "modify_cgroup()", retval,
							 info[SAMEGRP]);
		else
			message(8, FAIL, "modify_cgroup()", retval,
							 info[SAMEGRP]);

		/*
		 * Create another valid cgroup structure with same group
		 * to modify the existing group
		 */
		cgroup2 = create_new_cgroup_ds(ctl1, "group1", STRING, 9);
		if (!cgroup2) {
			fprintf(stderr, "Failed to create new cgroup ds. "
					"Trying with second controller\n");
			cgroup2 = create_new_cgroup_ds(ctl2, "group1", STRING,
									9);
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
		val_int64 = 262144;
		cgroup3 = create_new_cgroup_ds(ctl2, "group1", INT64, 12);
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
		test_cgroup_get_cgroup(ctl1, ctl2, 14);

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

		test_cgroup_attach_task(0, nullcgroup, NULL, NULL, NULLGRP, 2);

		/*
		 * Test03: Create a valid cgroup structure
		 * Exp outcome: no error. 0 return value
		 */
		ctl1_cgroup1 = create_new_cgroup_ds(ctl1, "ctl1_group1",
								 STRING, 3);
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
								 STRING, 5);
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
							 NULL, NOMESSAGE, 8);

		/*
		 * Test07: Call cgroup_attach_task() with a group with ctl2
		 * controller and check if return values are correct. If yes
		 * check if task exists in the groups under both controller's
		 * hierarchy.
		 */

		test_cgroup_attach_task(0, ctl2_cgroup1, "ctl1_group1",
						 "ctl2_group1", NOMESSAGE, 9);

		/*
		 * Test: Create a valid cgroup structure
		 * Exp outcome: no error. 0 return value
		 */
		ctl2_cgroup2 = create_new_cgroup_ds(ctl2, "ctl2_group2",
								 STRING, 10);
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
						 NULL, NULL, NOTCRTDGRP, 11);

		/*
		 * Create another valid cgroup structure with same group name
		 * to modify the existing group ctl1_group1
		 * Exp outcome: no error. 0 return value
		 */
		mod_ctl1_cgroup1 = create_new_cgroup_ds(ctl1, "ctl1_group1",
								 STRING, 12);
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
								 STRING, 14);
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
								 STRING, 18);
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
			message(15, FAIL, "add_controller()", retval, extra);
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
						 "commongroup", COMMONGRP, 20);

		/*
		 * Test18: Create a valid cgroup structure to modify the
		 * commongroup which is under multiple controllers
		 * Exp outcome: no error. 0 return value
		 */
		mod_common_cgroup = create_new_cgroup_ds(ctl1, "commongroup",
								 STRING, 21);
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
			message(18, FAIL, "add_controller()", retval, extra);
			fprintf(stderr, "Adding second controller failled "
				" Exiting without running further testcases\n");
			exit(1);
		}

		strncpy(val_string, "7000064", sizeof(val_string));
		retval = cgroup_add_value_string(sec_controller,
						 control_file, val_string);
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
		test_cgroup_get_cgroup(ctl1, ctl2, 24);

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


void test_cgroup_init(int retcode, int i)
{
	int retval;
	char extra[SIZE] = "\n";

	retval = cgroup_init();
	if (retval == retcode)
		message(i, PASS, "init()\t", retval, extra);
	else
		message(i, FAIL, "init()",  retval, extra);
}

void test_cgroup_attach_task(int retcode, struct cgroup *cgrp,
				 const char *group1, const char *group2,
								int k, int i)
{
	int retval;
	char tasksfile[FILENAME_MAX], tasksfile2[FILENAME_MAX];
	/* Check, In case some error is expected due to a negative scenario */
	if (retcode) {
		retval = cgroup_attach_task(cgrp);
		if (retval == retcode)
			message(i, PASS, "attach_task()", retval, info[k]);
		else
			message(i, FAIL, "attach_task()", retval, info[k]);

		return;
	}

	/* Now there is no error and it is a genuine call */
	retval = cgroup_attach_task(cgrp);
	if (retval == 0) { /* API returned success, so perform check */
		build_path(tasksfile, mountpoint,
					 group1, "tasks");

		if (check_task(tasksfile)) {
			if (fs_mounted == 2) { /* multiple mounts */
				build_path(tasksfile2, mountpoint2,
							 group2, "tasks");
				if (check_task(tasksfile2)) {
					message(i, PASS, "attach_task()",
						 retval, info[TASKINGRP]);
				} else {
					message(i, FAIL, "attach_task()",
						 retval, info[TASKNOTINANYGRP]);
				}
			} else { /* single mount */
				message(i, PASS, "attach_task()",
						 retval, info[TASKINGRP]);
			}
		} else {
			message(i, FAIL, "attach_task()", retval,
							 info[TASKNOTINGRP]);
		}
	} else {
		message(i, FAIL, "attach_task()", retval, (char *)"\n");
	}
}


struct cgroup *create_new_cgroup_ds(int ctl, const char *grpname,
						 int value_type, int i)
{
	int retval;
	char group[FILENAME_MAX];
	char controller_name[FILENAME_MAX], control_file[FILENAME_MAX];

	strncpy(group, grpname, sizeof(group));
	retval = set_controller(ctl, controller_name, control_file);
	if (retval) {
		fprintf(stderr, "Setting controller failled\n");
		return NULL;
	}

	/* val_string is still global. Will replace soon with config file */
	switch (ctl) {
		/* control values are controller specific, so will be set
		 * accordingly from the config file */
	case CPU:
		strncpy(val_string, "260000", sizeof(val_string));
		break;

	case MEMORY:
		strncpy(val_string, "7000064", sizeof(val_string));
		break;

	/* To be added for other controllers */
	default:
		printf("Invalid controller name passed. Setting control value"
					" failed. Dependent tests may fail\n");
		return NULL;
		break;
	}

	return new_cgroup(group, controller_name, control_file, value_type, i);
}


void test_cgroup_create_cgroup(int retcode, struct cgroup *cgrp,
			 const char *name, int common, int mpnt, int ign, int i)
{
	int retval;
	char path1_group[FILENAME_MAX], path2_group[FILENAME_MAX];
	/* Check, In case some error is expected due to a negative scenario */
	if (retcode) {
		retval = cgroup_create_cgroup(cgrp, ign);
		if (retval == retcode)
			message(i, PASS, "create_cgroup()", retval,
							 info[NOMESSAGE]);
		else
			message(i, FAIL, "create_cgroup()", retval,
							 info[NOMESSAGE]);

		return;
	}

	/* Now there is no error and it is a genuine call */
	retval = cgroup_create_cgroup(cgrp, ign);
	if (retval) {
		message(i, FAIL, "create_cgroup()", retval,  info[NOMESSAGE]);
		return;
	}

	/* Let us now check if the group exist in file system */
	if (!common) { /* group only under one mountpoint */
		if (mpnt == 1)  /* group under mountpoint */
			build_path(path1_group, mountpoint, name, NULL);
		else		/* group under mountpoint2 */
			build_path(path1_group, mountpoint2, name, NULL);

		if (group_exist(path1_group) == 0)
			message(i, PASS, "create_cgroup()", retval,
							 info[GRPINFS]);
		else
			message(i, FAIL, "create_cgroup()", retval,
							 info[GRPNOTINFS]);

	} else { /* group under both mountpoints */
		/* Check if the group exists under both controllers */
		build_path(path1_group, mountpoint, name, NULL);
		if (group_exist(path1_group) == 0) {
			build_path(path2_group, mountpoint2, name, NULL);

			if (group_exist(path2_group) == 0)
				message(i, PASS, "create_cgroup()",
						 retval, info[GRPINBOTHCTLS]);
			else
				message(i, FAIL, "create_cgroup()",
						 retval, info[GRPNOTIN2NDCTL]);
		} else {
			message(i, FAIL, "create_cgroup()", retval,
							 info[GRPNOTIN1STCTL]);
		}
	}

	return;
}

void test_cgroup_delete_cgroup(int retcode, struct cgroup *cgrp,
			 const char *name, int common, int mpnt, int ign, int i)
{
	int retval;
	char path1_group[FILENAME_MAX], path2_group[FILENAME_MAX];
	/* Check, In case some error is expected due to a negative scenario */
	if (retcode) {
		retval = cgroup_delete_cgroup(cgrp, ign);
		if (retval == retcode)
			message(i, PASS, "delete_cgroup()", retval,
							 info[NOMESSAGE]);
		else
			message(i, FAIL, "delete_cgroup()", retval,
							 info[NOMESSAGE]);

		return;
	}

	/* Now there is no error and it is a genuine call */
	retval = cgroup_delete_cgroup(cgrp, ign);
	if (retval) {
		message(i, FAIL, "delete_cgroup()", retval,  info[NOMESSAGE]);
		return;
	}

	/* Let us now check if the group has been deleted from file system */
	if (!common) { /* check only under one mountpoint */
		if (mpnt == 1)  /* check group under mountpoint */
			build_path(path1_group, mountpoint, name, NULL);
		else		/* check group under mountpoint2 */
			build_path(path1_group, mountpoint2, name, NULL);

		if (group_exist(path1_group) == -1)
			message(i, PASS, "delete_cgroup()", retval,
						 info[GRPDELETEDINFS]);
		else
			message(i, FAIL, "delete_cgroup()", retval,
						 info[GRPNOTDELETEDINFS]);

	} else { /* check group under both mountpoints */
		/* Check if the group deleted under both controllers */
		build_path(path1_group, mountpoint, name, NULL);
		if (group_exist(path1_group) == -1) {
			build_path(path2_group, mountpoint2, name, NULL);

			if (group_exist(path2_group) == -1)
				message(i, PASS, "delete_cgroup()",
						 retval, info[GRPDELETEDINFS]);
			else
				message(i, FAIL, "delete_cgroup()",
					 retval, info[GRPNOTDELETEDGLOBALY]);
		} else {
			message(i, FAIL, "delete_cgroup()", retval,
						 info[GRPNOTDELETEDINFS]);
		}
	}

}

void test_cgroup_modify_cgroup(int retcode, struct cgroup *cgrp,
			 const char *name, int which_ctl, int ctl1,
					 int ctl2, int value_type, int i)
{
	int retval;
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
		strncpy(val_string, "260000", sizeof(val_string));

		if (!group_modified(path1_control_file, value_type))
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
		strncpy(val_string, "7000064", sizeof(val_string));
		if (!group_modified(path2_control_file, value_type))
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
		strncpy(val_string, "260000", sizeof(val_string));
		if (!group_modified(path1_control_file, value_type)) {
			strncpy(val_string, "7000064", sizeof(val_string));
			if (!group_modified(path2_control_file, value_type))
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

void get_controllers(const char *name, int *exist)
{
	int hierarchy, num_cgroups, enabled;
	FILE *fd;
	char subsys_name[FILENAME_MAX];
	fd = fopen("/proc/cgroups", "r");
	if (!fd)
		return;

	while (!feof(fd)) {
		fscanf(fd, "%s, %d, %d, %d", subsys_name,
					 &hierarchy, &num_cgroups, &enabled);
		if (strncmp(name, subsys_name, sizeof(*name)) == 0)
			*exist = 1;
	}
}

static int group_exist(char *path_group)
{
	int ret;
	ret = open(path_group, O_DIRECTORY);
	if (ret == -1)
		return ret;
	return 0;
}

static int set_controller(int controller, char *controller_name,
							 char *control_file)
{
	switch (controller) {
	case MEMORY:
		if (memory == 0)
			return 1;

		strncpy(controller_name, "memory", FILENAME_MAX);
		strncpy(control_file, "memory.limit_in_bytes", FILENAME_MAX);
		return 0;
		break;

	case CPU:
		if (cpu == 0)
			return 1;

		strncpy(controller_name, "cpu", FILENAME_MAX);
		strncpy(control_file, "cpu.shares", FILENAME_MAX);
		return 0;
		break;

	case CPUSET:
		strncpy(controller_name, "cpuset", FILENAME_MAX);
		/* What is the exact control file?? */
		strncpy(control_file, "cpuset.mem_exclusive", FILENAME_MAX);
		return 0;
		break;
		/* Future controllers can be added here */

	default:
		printf("Invalid controller name passed. Setting controller"
					" failed. Dependent tests may fail\n");
		return 1;
		break;
	}
}

static int group_modified(char *path_control_file, int value_type)
{
	bool bool_val;
	int64_t int64_val;
	u_int64_t uint64_val;
	char string_val[FILENAME_MAX]; /* Doubt: what should be the size ? */
	FILE *fd;

	fd = fopen(path_control_file, "r");
	if (!fd) {
		fprintf(stderr, "Error in opening %s\n", path_control_file);
		fprintf(stderr, "Skipping modified values check....\n");
		return 1;
	}

	switch (value_type) {

	case BOOL:
		fscanf(fd, "%d", &bool_val);
		if (bool_val == val_bool)
			return 0;
		break;
	case INT64:
		fscanf(fd, "%lld", &int64_val);
		if (int64_val == val_int64)
			return 0;
		break;
	case UINT64:
		fscanf(fd, "%llu", &uint64_val);
		if (uint64_val == val_uint64)
			return 0;
		break;
	case STRING:
		fscanf(fd, "%s", string_val);
		if (!strncmp(string_val, val_string, strlen(string_val)))
			return 0;
		break;
	default:
		fprintf(stderr, "Wrong value_type passed "
						"in group_modified()\n");
		fprintf(stderr, "Skipping modified values check....\n");
		return 0;	/* Can not report test result as failure */
		break;
	}
	return 1;
}
static int add_control_value(struct cgroup_controller *newcontroller,
				 char * control_file, char *wr, int value_type)
{
	int retval;

	switch (value_type) {

	case BOOL:
		retval = cgroup_add_value_bool(newcontroller,
					 control_file, val_bool);
		snprintf(wr, SIZE, "add_value_bool()");
		break;
	case INT64:
		retval = cgroup_add_value_int64(newcontroller,
					 control_file, val_int64);
		snprintf(wr, SIZE, "add_value_int64()");
		break;
	case UINT64:
		retval = cgroup_add_value_uint64(newcontroller,
					 control_file, val_uint64);
		snprintf(wr, SIZE, "add_value_uint64()");
		break;
	case STRING:
		retval = cgroup_add_value_string(newcontroller,
					 control_file, val_string);
		snprintf(wr, SIZE, "add_value_string()");
		break;
	default:
		printf("ERROR: wrong value in add_control_value()\n");
		return 1;
		break;
	}
	return retval;
}

struct cgroup *new_cgroup(char *group, char *controller_name,
				 char *control_file, int value_type, int i)
{
	int retval;
	char wr[SIZE]; /* Names of wrapper apis */
	struct cgroup *newcgroup;
	struct cgroup_controller *newcontroller;

	newcgroup = cgroup_new_cgroup(group);

	if (newcgroup) {
		retval = cgroup_set_uid_gid(newcgroup, tasks_uid, tasks_gid,
						control_uid, control_gid);

		if (retval) {
			snprintf(wr, SIZE, "set_uid_gid()");
			message(i++, FAIL, wr, retval, extra);
		}

		newcontroller = cgroup_add_controller(newcgroup,
							 controller_name);
		if (newcontroller) {
			retval =  add_control_value(newcontroller,
						 control_file, wr, value_type);

			if (!retval) {
				message(i++, PASS, "new_cgroup()",
								 retval, extra);
			} else {
				message(i++, FAIL, wr, retval, extra);
				return NULL;
			}
		 } else {
			/* Since these wrappers do not return an int so -1 */
			message(i++, FAIL, "add_controller", -1, extra);
			return NULL;
		}
	} else {
		message(i++, FAIL, "new_cgroup", -1, extra);
		return NULL;
	}
	return newcgroup;
}

int check_fsmounted(int multimnt)
{
	int count = 0;
	struct mntent *entry, *tmp_entry;
	/* Need a better mechanism to decide memory allocation size here */
	char entry_buffer[FILENAME_MAX * 4];
	FILE *proc_file;

	tmp_entry = (struct mntent *) malloc(sizeof(struct mntent));
	if (!tmp_entry) {
		perror("Error: failled to mallloc for mntent\n");
		return 1;
	}

	proc_file = fopen("/proc/mounts", "r");
	if (!proc_file) {
		printf("Error in opening /proc/mounts.\n");
		return EIO;
	}
	while ((entry = getmntent_r(proc_file, tmp_entry, entry_buffer,
						 FILENAME_MAX*4)) != NULL) {
		if (!strncmp(entry->mnt_type, "cgroup", strlen("cgroup"))) {
			count++;
			if (multimnt) {
				if (count >= 2) {
					printf("sanity check pass. %s\n",
							 entry->mnt_type);
					return 0;
				}
			} else {
				printf("sanity check pass. %s\n",
							 entry->mnt_type);
				return 0;
			}
		}
	}
	return 1;
}

static int check_task(char *tasksfile)
{
	FILE *file;
	pid_t curr_tid, tid;
	int pass = 0;

	file = fopen(tasksfile, "r");
	if (!file) {
		printf("ERROR: in opening %s\n", tasksfile);
		printf("Exiting without running other testcases in this set\n");
		exit(1);
	}

	curr_tid = cgrouptest_gettid();
	while (!feof(file)) {
		fscanf(file, "%u", &tid);
		if (tid == curr_tid) {
			pass = 1;
			break;
		}
	}

	return pass;
}

static inline void message(int num, int pass, const char *api,
					 int retval, char *extra)
{
	char res[10];
	char buf[2*SIZE];
	if (pass)
		strncpy(res, "PASS :", 10);
	else
		strncpy(res, "FAIL :", 10);

	/* Populate message buffer for the api */
	snprintf(buf, sizeof(buf), "cgroup_%s\t\t Ret Value = ", api);
	fprintf(stdout, "TEST%2d:%s %s%d\t%s", num, res, buf, retval, extra);
}

/* builds the path to target file/group */
static inline void build_path(char *target, char *mountpoint,
				const char *group, const char *file)
{
	strncpy(target, mountpoint, FILENAME_MAX);

	if (group) {
		strncat(target, "/", FILENAME_MAX);
		strncat(target, group, FILENAME_MAX);
	}

	if (file) {
		strncat(target, "/", FILENAME_MAX);
		strncat(target, file, FILENAME_MAX);
	}
}

void test_cgroup_compare_cgroup(int ctl1, int ctl2, int i)
{
	int retval;
	struct cgroup *cgroup1, *cgroup2;
	struct cgroup_controller *controller;
	char controller_name[FILENAME_MAX], control_file[FILENAME_MAX];
	char wr[SIZE], extra[] = "in cgroup_compare_cgroup";

	retval = cgroup_compare_cgroup(NULL, NULL);
	if (retval)
		message(i++, PASS, "compare_cgroup()", retval, info[NULLGRP]);
	else
		message(i++, FAIL, "compare_cgroup()", retval, info[NULLGRP]);

	cgroup1 = cgroup_new_cgroup("testgroup");
	cgroup2 = cgroup_new_cgroup("testgroup");
	cgroup_set_uid_gid(cgroup1, 0, 0, 0, 0);
	cgroup_set_uid_gid(cgroup2, 0, 0, 0, 0);

	retval = set_controller(ctl1, controller_name, control_file);

	controller = cgroup_add_controller(cgroup1, controller_name);
	if (controller) {
		retval =  add_control_value(controller,
						 control_file, wr, STRING);
		if (retval)
			message(i++, FAIL, wr, retval, extra);
	}

	controller = cgroup_add_controller(cgroup2, controller_name);
	if (controller) {
		retval =  add_control_value(controller,
						 control_file, wr, STRING);
		if (retval)
			message(i++, FAIL, wr, retval, extra);
	}

	retval = cgroup_compare_cgroup(cgroup1, cgroup2);
	if (retval)
		message(i++, FAIL, "compare_cgroup()", retval, info[NOMESSAGE]);
	else
		message(i++, PASS, "compare_cgroup()", retval, info[NOMESSAGE]);

	/* Test the api by putting diff number of controllers in cgroups */
	retval = set_controller(ctl2, controller_name, control_file);
	controller = cgroup_add_controller(cgroup2, controller_name);
	if (controller) {
		retval =  add_control_value(controller,
						 control_file, wr, STRING);
		if (retval)
			message(i++, FAIL, wr, retval, extra);
	}

	retval = cgroup_compare_cgroup(cgroup1, cgroup2);
	if (retval == ECGROUPNOTEQUAL)
		message(i++, PASS, "compare_cgroup()", retval, info[NOMESSAGE]);
	else
		message(i++, FAIL, "compare_cgroup()", retval, info[NOMESSAGE]);

	cgroup_free(&cgroup1);
	cgroup_free(&cgroup2);
}

void test_cgroup_get_cgroup(int ctl1, int ctl2, int i)
{
	struct cgroup *cgroup_filled, *cgroup_a, *cgroup_b;
	struct cgroup_controller *controller;
	char controller_name[FILENAME_MAX], control_file[FILENAME_MAX];
	int ret;

	/*
	 * No need to test the next 3 scenarios again for Multimnt
	 * so testing them only under single mount
	 */
	if (fs_mounted == FS_MOUNTED) {
		/* 1. Test with nullcgroup first */
		ret = cgroup_get_cgroup(NULL);
		if (ret == ECGROUPNOTALLOWED)
			message(i++, PASS, "get_cgroup()", ret, info[NULLGRP]);
		else
			message(i++, FAIL, "get_cgroup()", ret, info[NULLGRP]);

		/* 2. Test with invalid name filled cgroup(non existing) */
		cgroup_filled = cgroup_new_cgroup("nogroup");
		if (!cgroup_filled)
			message(i++, FAIL, "new_cgroup()", 0, info[NOMESSAGE]);

		ret = cgroup_get_cgroup(cgroup_filled);
		if (ret)
			message(i++, PASS, "get_cgroup()", ret,
							 info[NOTCRTDGRP]);
		else
			message(i++, FAIL, "get_cgroup()", ret,
							 info[NOTCRTDGRP]);

		/* 3.
		 * Test with name filled cgroup. Ensure the group group1 exists
		 * in the filesystem before calling this test function
		 */
		cgroup_filled = cgroup_new_cgroup("group1");
		if (!cgroup_filled)
			message(i++, FAIL, "new_cgroup()", 0, info[NOMESSAGE]);

		ret = cgroup_get_cgroup(cgroup_filled);
		if (!ret)
			message(i++, PASS, "get_cgroup()", ret,
							 info[NOMESSAGE]);
		else
			message(i++, FAIL, "get_cgroup()", ret,
							 info[NOMESSAGE]);
	}

	/* MULTIMOUNT: Create, get and compare a cgroup under both mounts */

	/* get cgroup_a ds and create group_a in filesystem */
	cgroup_a = create_new_cgroup_ds(ctl1, "group_a", STRING, 00);
	if (fs_mounted == FS_MULTI_MOUNTED) {
		/* Create under another controller also */
		ret = set_controller(ctl2, controller_name, control_file);
		controller = cgroup_add_controller(cgroup_a, controller_name);
	}
	test_cgroup_create_cgroup(0, cgroup_a, "group_a", 0, 1, 1, 00);

	/* create group_b ds to be filled by cgroup_get_cgroup */
	cgroup_b = cgroup_new_cgroup("group_a");
	if (!cgroup_b)
		message(i++, FAIL, "new_cgroup()", 0, info[NOMESSAGE]);
	/* Fill the ds and compare the two */
	ret = cgroup_get_cgroup(cgroup_b);
	if (!ret) {
		ret = cgroup_compare_cgroup(cgroup_a, cgroup_b);
		if (ret == 0)
			message(i++, PASS, "get_cgroup()", ret, info[SAMEGRP]);
		else
			message(i++, FAIL, "get_cgroup()", ret,
							 info[NOMESSAGE]);
	} else {
		message(i++, FAIL, "get_cgroup()", ret, info[NOMESSAGE]);
	}

	cgroup_free(&cgroup_a);
	cgroup_free(&cgroup_b);
	cgroup_free(&cgroup_filled);
}

void test_cgroup_add_free_controller(int i)
{
	struct cgroup *cgroup1 = NULL, *cgroup2 = NULL;
	struct cgroup_controller *cgctl1, *cgctl2;

	/* Test with a Null cgroup */
	cgctl1 = cgroup_add_controller(cgroup1, "cpu");
	if (!cgctl1)
		message(i++, PASS, "add_controller()", 0, info[NOMESSAGE]);
	else
		message(i++, FAIL, "add_controller()", -1, info[NOMESSAGE]);

	cgroup1 = cgroup_new_cgroup("testgroup");
	cgctl1 = cgroup_add_controller(cgroup1, "cpuset");
	if (cgctl1)
		message(i++, PASS, "add_controller()", 0, info[NOMESSAGE]);
	else
		message(i++, FAIL, "add_controller()", -1, info[NOMESSAGE]);

	cgctl2 = cgroup_add_controller(cgroup1, "cpu");
	if (cgctl2)
		message(i++, PASS, "add_controller()", 0, info[NOMESSAGE]);
	else
		message(i++, FAIL, "add_controller()", -1, info[NOMESSAGE]);

	cgroup_free(&cgroup1);
	cgroup_free_controllers(cgroup2);

}
