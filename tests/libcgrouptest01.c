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

int main(int argc, char *argv[])
{
	int fs_mounted, retval;
	struct cgroup *cgroup1, *cgroup2, *cgroup3, *nullcgroup = NULL;
	/* In case of multimount for readability we use the controller name
	 * before the cgroup structure name */
	struct cgroup *cpu_cgroup1, *mem_cgroup1, *mem_cgroup2, *common_cgroup;
	struct cgroup_controller *newcontroller;
	char controller_name[FILENAME_MAX], control_file[FILENAME_MAX];
	char path_group[FILENAME_MAX], path_control_file[FILENAME_MAX];

	/* The path to the common group under different controllers */
	char path1_common_group[FILENAME_MAX], path2_common_group[FILENAME_MAX];
	char mountpoint[FILENAME_MAX], tasksfile[FILENAME_MAX];
	char group[FILENAME_MAX];

	/* Hardcode second mountpoint for now. Will update soon */
	char mountpoint2[FILENAME_MAX] = "/dev/cgroup_controllers-2";
	char tasksfile2[FILENAME_MAX];

	if ((argc < 3) || (atoi(argv[1]) < 0)) {
		printf("ERROR: Wrong no of parameters recieved from script\n");
		printf("Exiting the libcgroup testset\n");
		exit(1);
	}
	fs_mounted = atoi(argv[1]);
	strcpy(mountpoint, argv[2]);
	dbg("C:DBG: fs_mounted as recieved from script=%d\n", fs_mounted);
	dbg("C:DBG: mountpoint as recieved from script=%s\n", mountpoint);

	/*
	 * get the list of supported controllers
	 */
	get_controllers("cpu", &cpu);
	get_controllers("memory", &memory);
	if (cpu == 0 && memory == 0) {
		fprintf(stderr, "controllers are not enabled in kernel\n");
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

		retval = cgroup_init();
		if (retval == ECGROUPNOTMOUNTED)
			message(++i, PASS, "init()\t", retval, extra);
		else
			message(++i, FAIL, "init()",  retval, extra);

		/*
		 * Test02: call cgroup_attach_task() with null group
		 * Exp outcome: error non zero return value
		 */
		strncpy(extra, " Called with NULL cgroup argument\n", SIZE);
		retval = cgroup_attach_task(nullcgroup);
		if (retval != 0)
			message(++i, PASS, "attach_task()", retval, extra);
		else
			message(++i, FAIL, "attach_task()", retval, extra);

		strncpy(extra, "\n", SIZE);
		/*
		 * Test03: Create a valid cgroup and check all return values
		 * Exp outcome: no error. 0 return value
		 */

		strncpy(group, "group1", sizeof(group));
		retval = set_controller(MEMORY, controller_name, control_file);
		strncpy(val_string, "40960000", sizeof(val_string));

		if (retval)
			fprintf(stderr, "Setting controller failled\n");

		cgroup1 = new_cgroup(group, controller_name,
						 control_file, STRING);

		/*
		 * Test04: Then Call cgroup_create_cgroup() with this valid grp
		 * Exp outcome: non zero return value
		 */
		retval = cgroup_create_cgroup(cgroup1, 1);
		if (retval)
			message(++i, PASS, "create_cgroup()", retval, extra);
		else
			message(++i, FAIL, "create_cgroup()", retval, extra);

		/*
		 * Test05: delete cgroup
		 * Exp outcome: non zero return value but what ?
		 */
		retval = cgroup_delete_cgroup(cgroup1, 1);
		if (retval)
			message(++i, PASS, "delete_cgroup()", retval, extra);
		else
			message(++i, FAIL, "delete_cgroup()", retval, extra);

		/*
		 * Test06: Check if cgroup_create_cgroup() handles a NULL cgroup
		 * Exp outcome: error ECGINVAL
		 */
		strncpy(extra, " Called with NULL cgroup argument\n", SIZE);
		retval = cgroup_create_cgroup(nullcgroup, 1);
		if (retval)
			message(++i, PASS, "create_cgroup()", retval, extra);
		else
			message(++i, FAIL, "create_cgroup()", retval, extra);

		strncpy(extra, "\n", SIZE);

		/*
		 * Test07: delete nullcgroup
		 */
		strncpy(extra, " Called with NULL cgroup argument\n", SIZE);
		retval = cgroup_delete_cgroup(nullcgroup, 1);
		if (retval)
			message(++i, PASS, "delete_cgroup()", retval, extra);
		else
			message(++i, FAIL, "delete_cgroup()", retval, extra);

		strncpy(extra, "\n", SIZE);

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
		 * without calling cgroup_inti(). We can check other apis too.
		 * Exp outcome: error ECGROUPNOTINITIALIZED
		 */
		strncpy(extra, " Called with NULL cgroup argument\n", SIZE);
		retval = cgroup_attach_task(nullcgroup);
		if (retval == ECGROUPNOTINITIALIZED)
			message(++i, PASS, "attach_task()", retval, extra);
		else
			message(++i, FAIL, "attach_task()", retval, extra);

		strncpy(extra, "\n", SIZE);

		/*
		 * Test02: call cgroup_init() and check return values
		 * Exp outcome:  no error. return value 0
		 */

		retval = cgroup_init();
		if (retval == 0)
			message(++i, PASS, "init()\t", retval, extra);
		else
			message(++i, FAIL, "init()\t",  retval, extra);

		/*
		 * Test03: Call cgroup_attach_task() with null group and check
		 * if return values are correct. If yes check if task exists in
		 * root group tasks file
		 * TODO: This test needs some modification in script
		 * Exp outcome: current task should be attached to root group
		 */
		strncpy(extra, " Called with NULL cgroup argument\n", SIZE);
		retval = cgroup_attach_task(nullcgroup);
		if (retval == 0) {
			build_path(tasksfile, mountpoint, NULL, "tasks");
			if (check_task(tasksfile)) {
				strncpy(extra, " Task found in grp\n", SIZE);
				message(++i, PASS, "attach_task()", retval,
									 extra);
			} else {
				strncpy(extra, " Task not found in grp\n",
									 SIZE);
				message(++i, FAIL, "attach_task()", retval,
									 extra);
			}
		} else {
			message(++i, FAIL, "attach_task()", retval, extra);
		}

		strncpy(extra, "\n", SIZE);

		/*
		 * Test04: Call cgroup_attach_task_pid() with null group
		 * and invalid pid
		 * Exp outcome: error
		 */
		retval = cgroup_attach_task_pid(nullcgroup, -1);
		if (retval != 0)
			message(++i, PASS, "attach_task_pid()", retval, extra);
		else
			message(++i, FAIL, "attach_task_pid()", retval, extra);

		/*
		 * Test05: Create a valid cgroup structure
		 * Exp outcome: no error. 0 return value
		 */
		strncpy(group, "group1", sizeof(group));
		retval = set_controller(MEMORY, controller_name, control_file);
		strncpy(val_string, "40960000", sizeof(val_string));

		if (retval) {
			fprintf(stderr, "Failed to set memory controller. "
						"Trying with cpu controller\n");
			retval = set_controller(CPU, controller_name,
								control_file);
			strncpy(val_string, "2048", sizeof(val_string));
			if (retval)
				fprintf(stderr, "Failed to set any controllers "
					"Tests dependent on this structure will"
					" fail\n");
		}

		cgroup1 = new_cgroup(group, controller_name,
						 control_file, STRING);

		/*
		 * Test06: Then Call cgroup_create_cgroup() with this group
		 * Exp outcome: zero return value
		 */
		retval = cgroup_create_cgroup(cgroup1, 1);
		if (!retval) {
			/* Check if the group exists in the dir tree */
			build_path(path_group, mountpoint, "group1", NULL);
			if (group_exist(path_group) == 0) {
				strncpy(extra, " grp found in fs\n", SIZE);
				message(++i, PASS, "create_cgroup()",
								 retval, extra);
			} else {
				strncpy(extra, " grp not found in fs. "
						"tests dependent on this"
						" grp will fail\n", SIZE);
				message(++i, FAIL, "create_cgroup()",
								 retval, extra);
			}

		} else {
			strncpy(extra, " Tests dependent on this grp "
							"will fail\n", SIZE);
			message(++i, FAIL, "create_cgroup()", retval, extra);
		}
		strncpy(extra, "\n", SIZE);

		/*
		 * Test07: Call cgroup_attach_task() with valid cgroup and check
		 * if return values are correct. If yes check if task exists in
		 * that group's tasks file
		 * Exp outcome: current task should be attached to that group
		 */
		retval = cgroup_attach_task(cgroup1);
		if (retval == 0) {
			build_path(tasksfile, mountpoint, "group1", "tasks");
			if (check_task(tasksfile)) {
				strncpy(extra, " Task found in grp\n", SIZE);
				message(++i, PASS, "attach_task()", retval,
									 extra);
			} else {
				strncpy(extra, " Task not found in grp\n",
									 SIZE);
				message(++i, FAIL, "attach_task()", retval,
									 extra);
			}
		} else {
			message(++i, FAIL, "attach_task()", retval, extra);
		}

		strncpy(extra, "\n", SIZE);

		/*
		 * Test08: modify cgroup with the same cgroup
		 * Exp outcome: zero return value. No change.
		 */
		strncpy(extra, " Called with same cgroup argument\n", SIZE);

		build_path(path_control_file, mountpoint,
						 "group1", control_file);

		retval = cgroup_modify_cgroup(cgroup1);
		/* Check if the values are changed */
		if (!retval && !group_modified(path_control_file, STRING))
			message(++i, PASS, "modify_cgroup()", retval, extra);
		else
			message(++i, FAIL, "modify_cgroup()", retval, extra);

		strncpy(extra, "\n", SIZE);

		/*
		 * Create another valid cgroup structure with same group
		 * Exp outcome: no error. 0 return value
		 */
		strncpy(group, "group1", sizeof(group));
		retval = set_controller(MEMORY, controller_name, control_file);
		strncpy(val_string, "81920000", sizeof(val_string));

		if (retval) {
			fprintf(stderr, "Failed to set first controller. "
					"Trying with second controller\n");
			retval = set_controller(CPU, controller_name,
								control_file);
			strncpy(val_string, "4096", sizeof(val_string));
			if (retval)
				fprintf(stderr, "Failed to set any controllers "
					"Tests dependent on this structure will"
					" fail\n");
		}

		cgroup2 = new_cgroup(group, controller_name,
						 control_file, STRING);

		/*
		 * Test10: modify cgroup with this new cgroup
		 * Exp outcome: zero return value
		 */
		build_path(path_control_file, mountpoint,
						 "group1", control_file);

		retval = cgroup_modify_cgroup(cgroup2);
		/* Check if the values are changed */
		if (!retval && !group_modified(path_control_file, STRING))
			message(++i, PASS, "modify_cgroup()", retval, extra);
		else
			message(++i, FAIL, "modify_cgroup()", retval, extra);

		/*
		 * Test11: modify cgroup with the null cgroup
		 * Exp outcome: zero return value.
		 */

		strncpy(extra, " Called with NULL cgroup argument\n", SIZE);

		retval = cgroup_modify_cgroup(nullcgroup);
		/* No need to check if the values are changed */
		if (retval == ECGINVAL)
			message(++i, PASS, "modify_cgroup()", retval, extra);
		else
			message(++i, FAIL, "modify_cgroup()", retval, extra);

		strncpy(extra, "\n", SIZE);

		/*
		 * Create another valid cgroup structure with diff controller
		 * to modify the existing group
		 * Exp outcome: no error. 0 return value
		 */
		val_int64 = 2048;
		strncpy(group, "group1", sizeof(group));
		retval = set_controller(CPU, controller_name, control_file);
		if (retval)
			fprintf(stderr, "Setting controller failled. "
				"Tests dependent on this struct may fail\n");

		cgroup3 = new_cgroup(group, controller_name,
						 control_file, INT64);

		/*
		 * Test13: modify existing group with this cgroup
		 * Exp outcome: zero return value
		 */
		strncpy(extra, " Called with a cgroup argument with "
						"different controller\n", SIZE);
		build_path(path_control_file, mountpoint,
						 "group1", control_file);

		retval = cgroup_modify_cgroup(cgroup3);
		/* Check if the values are changed */
		if (!retval && !group_modified(path_control_file, STRING))
			message(++i, PASS, "modify_cgroup()", retval, extra);
		else
			message(++i, FAIL, "modify_cgroup()", retval, extra);

		strncpy(extra, "\n", SIZE);

		/*
		 * Test14: delete cgroup
		 * Exp outcome: zero return value
		 */
		retval = cgroup_delete_cgroup(cgroup1, 1);
		if (!retval) {
			/* Check if the group is deleted from the dir tree */
			build_path(path_group, mountpoint, "group1", NULL);
			if (group_exist(path_group) == -1) {
				strncpy(extra, " group deleted from fs\n",
									 SIZE);
				message(++i, PASS, "delete_cgroup()",
								 retval, extra);
			} else {
				strncpy(extra, " group still found in fs\n",
									 SIZE);
				message(++i, FAIL, "delete_cgroup()",
								 retval, extra);
			}

		} else {
			message(++i, FAIL, "delete_cgroup()", retval, extra);
		}
		strncpy(extra, "\n", SIZE);

		/*
		 * Test15: Check if cgroup_create_cgroup() handles a NULL cgroup
		 * Exp outcome: error ECGINVAL
		 */
		strncpy(extra, " Called with NULL cgroup argument\n", SIZE);
		retval = cgroup_create_cgroup(nullcgroup, 1);
		if (retval)
			message(++i, PASS, "create_cgroup()", retval, extra);
		else
			message(++i, FAIL, "create_cgroup()", retval, extra);

		strncpy(extra, "\n", SIZE);

		/*
		 * Test16: delete nullcgroup
		 */
		strncpy(extra, " Called with NULL cgroup argument\n", SIZE);
		retval = cgroup_delete_cgroup(nullcgroup, 1);
		if (retval)
			message(++i, PASS, "delete_cgroup()", retval, extra);
		else
			message(++i, FAIL, "delete_cgroup()", retval, extra);

		strncpy(extra, "\n", SIZE);

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

		retval = cgroup_init();
		if (retval == 0)
			message(++i, PASS, "init()\t", retval, extra);
		else
			message(++i, FAIL, "init()\t",  retval, extra);

		/*
		 * Test02: Call cgroup_attach_task() with null group and check
		 * if return values are correct. If yes check if task exists in
		 * root group tasks file for each controller
		 * TODO: This test needs some modification in script
		 * Exp outcome: current task should be attached to root groups
		 */
		strncpy(extra, " Called with NULL cgroup argument\n", SIZE);
		retval = cgroup_attach_task(nullcgroup);
		if (retval == 0) {
			build_path(tasksfile, mountpoint, NULL, "tasks");
			build_path(tasksfile2, mountpoint2, NULL, "tasks");

			if (check_task(tasksfile) && check_task(tasksfile2)) {
				strncpy(extra, " Task found in grps\n", SIZE);
				message(++i, PASS, "attach_task()",
								 retval, extra);
			} else {
				strncpy(extra, " Task not found in grps\n",
									 SIZE);
				message(++i, FAIL, "attach_task()", retval,
									 extra);
			}
		} else {
			message(++i, FAIL, "attach_task()", retval, extra);
		}

		strncpy(extra, "\n", SIZE);

		/*
		 * Test03: Create a valid cgroup structure
		 * Exp outcome: no error. 0 return value
		 */
		strncpy(group, "cpugroup1", sizeof(group));
		strncpy(val_string, "4096", sizeof(val_string));
		retval = set_controller(CPU, controller_name, control_file);

		if (retval)
			fprintf(stderr, "Setting controller failled\n");

		cpu_cgroup1 = new_cgroup(group, controller_name,
						 control_file, STRING);

		/*
		 * Test04: Then Call cgroup_create_cgroup() with this valid grp
		 * Exp outcome: zero return value
		 */
		retval = cgroup_create_cgroup(cpu_cgroup1, 1);
		if (!retval) {
			/* Check if the group exists in the dir tree */
			build_path(path_group, mountpoint, "cpugroup1", NULL);
			if (group_exist(path_group) == 0) {
				strncpy(extra, " grp found in fs\n", SIZE);
				message(++i, PASS, "create_cgroup()",
								 retval, extra);
			} else {
				strncpy(extra, " grp not found in fs\n", SIZE);
				message(++i, FAIL, "create_cgroup()",
								 retval, extra);
			}

		} else {
			message(++i, FAIL, "create_cgroup()", retval, extra);
		}

		strncpy(extra, "\n", SIZE);

		/*
		 * Test03: Create a valid cgroup structure
		 * Exp outcome: no error. 0 return value
		 */
		strncpy(group, "memgroup1", sizeof(group));
		retval = set_controller(MEMORY, controller_name, control_file);
		strncpy(val_string, "52428800", sizeof(val_string));

		if (retval)
			fprintf(stderr, "Setting controller failled\n");

		mem_cgroup1 = new_cgroup(group, controller_name,
						 control_file, STRING);

		/*
		 * Test04: Then Call cgroup_create_cgroup() with this valid grp
		 * Exp outcome: zero return value
		 */
		retval = cgroup_create_cgroup(mem_cgroup1, 1);
		if (!retval) {
			/* Check if the group exists in the dir tree */
			build_path(path_group, mountpoint2, "memgroup1", NULL);
			if (group_exist(path_group) == 0) {
				strncpy(extra, " grp found in fs\n", SIZE);
				message(++i, PASS, "create_cgroup()",
								 retval, extra);
			} else {
				strncpy(extra, " grp not found in fs\n", SIZE);
				message(++i, FAIL, "create_cgroup()",
								 retval, extra);
			}
		} else {
			message(++i, FAIL, "create_cgroup()", retval, extra);
		}

		strncpy(extra, "\n", SIZE);

		/*
		 * Test05: Call cgroup_create_cgroup() with the same group
		 * Exp outcome: non zero return value
		 */
		strncpy(extra, " Second call with same arg\n", SIZE);
		retval = cgroup_create_cgroup(cpu_cgroup1, 1);
		/* BUG: The error should be ECGROUPALREADYEXISTS */
		if (retval == ECGROUPNOTALLOWED)
			message(++i, PASS, "create_cgroup()", retval, extra);
		else
			message(++i, FAIL, "create_cgroup()", retval, extra);

		/*
		 * Test06: Call cgroup_attach_task() with a group with cpu
		 * controller and check if return values are correct. If yes
		 * check if task exists in that group under only cpu controller
		 * hierarchy and in the root group under other controllers
		 * hierarchy.
		 * TODO: This test needs some modification in script
		 * Shall we hardcode mountpoints for each controller ?
		 */
		retval = cgroup_attach_task(cpu_cgroup1);
		if (retval == 0) {
			build_path(tasksfile, mountpoint, "cpugroup1", "tasks");
			build_path(tasksfile2, mountpoint2, NULL, "tasks");

			if (check_task(tasksfile) && check_task(tasksfile2)) {
				strncpy(extra, " Task found in grps\n", SIZE);
				message(++i, PASS, "attach_task()",
								 retval, extra);
			} else {
				strncpy(extra, " Task not found in grps\n",
									 SIZE);
				message(++i, FAIL, "attach_task()", retval,
									 extra);
			}
		} else {
			message(++i, FAIL, "attach_task()", retval, extra);
		}

		strncpy(extra, "\n", SIZE);

		/*
		 * Test07: Call cgroup_attach_task() with a group with memory
		 * controller and check if return values are correct. If yes
		 * check if task exists in the groups under both cpu controller
		 * hierarchy and other controllers hierarchy.
		 * TODO: This test needs some modification in script
		 * Shall we hardcode mountpoints for each controller ?
		 */
		retval = cgroup_attach_task(mem_cgroup1);
		if (retval == 0) {
			/*Task already attached to cpugroup1 in previous call*/
			build_path(tasksfile, mountpoint, "cpugroup1", "tasks");
			build_path(tasksfile2, mountpoint2,
							 "memgroup1", "tasks");

			if (check_task(tasksfile) && check_task(tasksfile2)) {
				strncpy(extra, " Task found in grps\n", SIZE);
				message(++i, PASS, "attach_task()",
								 retval, extra);
			} else {
				strncpy(extra, " Task not found in grps\n",
									 SIZE);
				message(++i, FAIL, "attach_task()", retval,
									 extra);
			}
		} else {
			message(++i, FAIL, "attach_task()", retval, extra);
		}

		strncpy(extra, "\n", SIZE);

		/*
		 * Test: Create a valid cgroup structure
		 * Exp outcome: no error. 0 return value
		 */
		strncpy(group, "memgroup2", sizeof(group));
		mem_cgroup2 = new_cgroup(group, controller_name,
						 control_file, STRING);

		/*
		 * Test08: Try to attach a task to this non existing group.
		 * Group does not exist in fs so should return ECGROUPNOTEXIST
		 */
		strncpy(extra, " Try attach to non existing group\n", SIZE);
		retval = cgroup_attach_task(mem_cgroup2);
		if (retval == ECGROUPNOTEXIST)
			message(++i, PASS, "attach_task()", retval, extra);
		else
			message(++i, FAIL, "attach_task()", retval, extra);

		strncpy(extra, "\n", SIZE);

		/*
		 * Test09: delete cgroups
		 * Exp outcome: zero return value
		 */
		retval = cgroup_delete_cgroup(cpu_cgroup1, 1);
		if (!retval) {
			/* Check if the group is deleted from the dir tree */
			build_path(path_group, mountpoint, "cpugroup1", NULL);

			if (group_exist(path_group) == -1) {
				strncpy(extra, " group deleted from fs\n",
									 SIZE);
				message(++i, PASS, "delete_cgroup()",
								 retval, extra);
			} else {
				strncpy(extra, " group still found in fs\n",
									 SIZE);
				message(++i, FAIL, "delete_cgroup()",
								 retval, extra);
			}

		} else {
			message(++i, FAIL, "delete_cgroup()", retval, extra);
		}
		strncpy(extra, "\n", SIZE);

		/*
		 * Test09: delete other cgroups too
		 * Exp outcome: zero return value
		 */
		retval = cgroup_delete_cgroup(mem_cgroup1, 1);
		if (!retval) {
			/* Check if the group is deleted from the dir tree */
			build_path(path_group, mountpoint2, "memgroup1", NULL);

			if (group_exist(path_group) == -1) {
				strncpy(extra, " group deleted from fs\n",
									 SIZE);
				message(++i, PASS, "delete_cgroup()",
								 retval, extra);
			} else {
				strncpy(extra, " group still found in fs\n",
									 SIZE);
				message(++i, FAIL, "delete_cgroup()",
								 retval, extra);
			}

		} else {
			message(++i, FAIL, "delete_cgroup()", retval, extra);
		}
		strncpy(extra, "\n", SIZE);


		/*
		 * Test10: Create a valid cgroup structure
		 * which has multiple controllers
		 * Exp outcome: no error. 0 return value
		 */
		strncpy(group, "commongroup", sizeof(group));
		strncpy(val_string, "4096", sizeof(val_string));
		retval = set_controller(CPU, controller_name, control_file);

		if (retval)
			fprintf(stderr, "Setting controller failled\n");

		common_cgroup = new_cgroup(group, controller_name,
						 control_file, STRING);

		/* Add one more controller to the cgroup */
		strncpy(controller_name, "memory", sizeof(controller_name));
		if (!cgroup_add_controller(common_cgroup, controller_name))
			message(++i, FAIL, "add_controller()", retval, extra);

		/*
		 * Test11: Then Call cgroup_create_cgroup() with this valid grp
		 * Exp outcome: zero return value
		 */
		retval = cgroup_create_cgroup(common_cgroup, 1);
		if (!retval) {
			/* Check if the group exists under both controllers */
			build_path(path1_common_group, mountpoint,
							 "commongroup", NULL);
			if (group_exist(path1_common_group) == 0) {
				build_path(path2_common_group, mountpoint2,
							 "commongroup", NULL);

				if (group_exist(path2_common_group) == 0) {
					strncpy(extra, " group found under"
						" both controllers\n", SIZE);
					message(++i, PASS, "create_cgroup()",
								 retval, extra);
				} else {
					strncpy(extra, " group not found "
						"under 2nd controller\n", SIZE);
					message(++i, FAIL, "create_cgroup()",
								 retval, extra);
				}
			} else {
				strncpy(extra, " group not found under any "
							"controller\n", SIZE);
				message(++i, FAIL, "create_cgroup()",
								 retval, extra);
			}
		} else {
			message(++i, FAIL, "create_cgroup()", retval, extra);
		}

		strncpy(extra, "\n", SIZE);

		/*
		 * Test12: delete this common cgroup
		 * Exp outcome: zero return value
		 */
		retval = cgroup_delete_cgroup(common_cgroup, 1);
		if (!retval) {
			/* Check if the group is deleted from both dir tree */
			build_path(path1_common_group, mountpoint,
							 "commongroup", NULL);
			if (group_exist(path1_common_group) == -1) {
				build_path(path2_common_group, mountpoint2,
							 "commongroup", NULL);
				if (group_exist(path2_common_group) == -1) {
					strncpy(extra, " group "
						"deleted globally\n", SIZE);
					message(++i, PASS, "create_cgroup()",
								 retval, extra);
				} else {
					strncpy(extra, " group not "
						"deleted globally\n", SIZE);
					message(++i, FAIL, "create_cgroup()",
								 retval, extra);
				}
			} else {
				strncpy(extra, " group still found in fs\n",
									 SIZE);
				message(++i, FAIL, "create_cgroup()", retval,
									 extra);
			}
		} else {
			message(++i, FAIL, "create_cgroup()", retval, extra);
		}

		strncpy(extra, "\n", SIZE);

		/* Free the cgroup structures */
		cgroup_free(&nullcgroup);
		cgroup_free(&cpu_cgroup1);
		cgroup_free(&mem_cgroup1);
		cgroup_free(&mem_cgroup2);

		break;

	default:
		fprintf(stderr, "ERROR: Wrong parameters recieved from script. \
						Exiting tests\n");
		exit(1);
		break;
	}
	return 0;
}

void get_controllers(char *name, int *exist)
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
		/* Future controllers can be added here */

	default:
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

struct cgroup *new_cgroup(char *group, char *controller_name,
				 char *control_file, int value_type)
{
	int retval;
	char wr[SIZE]; /* Na,es of wrapper apis */
	struct cgroup *newcgroup;
	struct cgroup_controller *newcontroller;
	newcgroup = cgroup_new_cgroup(group);

	if (newcgroup) {
		retval = cgroup_set_uid_gid(newcgroup, tasks_uid, tasks_gid,
						control_uid, control_gid);

		if (retval) {
			printf("Test[1:%2d]\tFAIL: cgroup_set_uid_gid()\n",
								++i);
		}

		newcontroller = cgroup_add_controller(newcgroup, controller_name);
		if (newcontroller) {
			switch (value_type) {

			case BOOL:
				retval = cgroup_add_value_bool(newcontroller,
						 control_file, val_bool);
				snprintf(wr, sizeof(wr), "add_value_bool()");
				break;
			case INT64:
				retval = cgroup_add_value_int64(newcontroller,
						 control_file, val_int64);
				snprintf(wr, sizeof(wr), "add_value_int64()");
				break;
			case UINT64:
				retval = cgroup_add_value_uint64(newcontroller,
						 control_file, val_uint64);
				snprintf(wr, sizeof(wr), "add_value_uint64()");
				break;
			case STRING:
				retval = cgroup_add_value_string(newcontroller,
						 control_file, val_string);
				snprintf(wr, sizeof(wr), "add_value_string()");
				break;
			default:
				printf("ERROR: wrong value in new_cgroup()\n");
				return NULL;
				break;
			}

			if (!retval) {
				message(++i, PASS, "new_cgroup()",
								 retval, extra);
			} else {
				message(++i, FAIL, wr, retval, extra);
				return NULL;
			}
		 } else {
			/* Since these wrappers do not return an int so -1 */
			message(++i, FAIL, "add_controller", -1, extra);
			return NULL;
		}
	} else {
		message(++i, FAIL, "new_cgroup", -1, extra);
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

static inline void message(int num, int pass, char *api,
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
						 char *group, char *file)
{
	strncpy(target, mountpoint, FILENAME_MAX);

	if (group) {
		strncat(target, "/", sizeof("/"));
		strncat(target, group, FILENAME_MAX);
	}

	if (file) {
		strncat(target, "/", sizeof("/"));
		strncat(target, file, FILENAME_MAX);
	}
}
