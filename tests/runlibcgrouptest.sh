#!/bin/bash
# usage ./runlibcgrouptest.sh
# Copyright IBM Corporation. 2008
#
# Author:	Sudhir Kumar <skumar@linux.vnet.ibm.com>
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of version 2.1 of the GNU Lesser General Public License
# as published by the Free Software Foundation.
#
# This program is distributed in the hope that it would be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
# Description: This script runs the the basic tests for testing libcgroup apis.
#

DEBUG=true		# for debug messages
FS_MOUNTED=0;		# 0 for not mounted, 1 for mounted, 2 for multimounted
MOUNTPOINT="/tmp";	# Just to initialize
TARGET=/dev/cgroup_controllers;
CONTROLLERS=cpu,memory;

debug()
{
	# Function parameter is the string to print out
	if [ $DEBUG ]
	then
		echo SH:DBG: $1;
	fi
}

umount_fs ()
{
	PROC_ENTRY=`cat /proc/mounts|grep cgroup|tr -s [:space:]|cut -d" " -f2`;

	# Need to handle multiple mount points ?
	if [ -n "$PROC_ENTRY" ] && [ "$PROC_ENTRY" != "$TARGET" ]
	then
		TARGET=$PROC_ENTRY;
	fi;

	# Need to take care if there are tasks running in any group ??
	# Also need to take care if there are groups in the hierarchy ?
	rmdir $TARGET/* 2> /dev/null ;
	umount $TARGET;
	rmdir  $TARGET;
	FS_MOUNTED=0;
	TARGET=/dev/cgroup_controllers;
	echo "Cleanup done";
}

check_mount_fs()
{
	CGROUP=`cat /proc/mounts|grep -w cgroup|tr -s [:space:]|cut -d" " -f3`;
	if [ "$CGROUP" = "cgroup" ]
	then
		FS_MOUNTED=1;
	else
		FS_MOUNTED=0;
	fi
}

# Check if kernel is not having any of the controllers enabled
no_controllers()
{
	local CPU;
	local MEMORY;
	if [ -e /proc/cgroups ]
	then
		CPU=`cat /proc/cgroups|grep -w cpu|cut -f1`;
		MEMORY=`cat /proc/cgroups|grep -w memory|cut -f1`;
	fi;

	if [ -n $CPU ] && [ -n $MEMORY ]
	then
		CONTROLLERS=$CPU,$MEMORY ;
		return 1;	# false
	elif [ -n $CPU ]
	then
		CONTROLLERS=$CPU ;
		return 1;	# false
	elif [ -n $MEMORY ]
	then
		CONTROLLERS=$MEMORY ;
		return 1;	# false
	fi;
	# Kernel has no controllers enabled
	return 0;	# true
}

mount_fs ()
{
	if no_controllers
	then
		echo "Kernel has no controllers enabled";
		echo "Recompile your kernel with controllers enabled"
		echo "Exiting the tests.....";
		exit 1;
	fi;

	# Proceed further as kernel has controllers support
	if [ -e $TARGET ]
	then
		echo "WARN: $TARGET already exist..overwriting"; # any issue ?
		umount_fs;
	fi;

	mkdir $TARGET;

	mount -t cgroup -o $CONTROLLERS cgroup $TARGET; # 2> /dev/null?
	if [ $? -ne 0 ]
	then
		echo "ERROR: Could not mount cgroup filesystem on $TARGET."
		echo "Exiting test";
		umount_fs;
		exit -1;
	fi

	# Group created earlier may again be visible if not cleaned properly.
	# So clean them all
	if [ -e $TARGET/group1 ] # first group that is created
	then
		rmdir $TARGET/* 2>/dev/null
		echo "WARN: Earlier groups found and removed...";
	fi
	FS_MOUNTED=1;
	debug "INFO: cgroup filesystem mounted on $TARGET  directory"
}

get_mountpoint()
{
	# ??? need to handle multiple mount point
	MOUNTPOINT=`cat /proc/mounts|grep -w cgroup|tr -s [:space:]| \
							cut -d" " -f2`;
	debug "mountpoint is $MOUNTPOINT"
}
runtest()
{
	MOUNT_INFO=$1;
	TEST_EXEC=$2;
	if [ -f $TEST_EXEC ]
	then
		./$TEST_EXEC $MOUNT_INFO $MOUNTPOINT;
		if [ $? -ne 0 ]
		then
			echo Error in running ./$TEST_EXEC
			echo Exiting tests.
		else
			PID=$!;
		fi;
	else
		echo Sources not compiled. please run make;
	fi
}
# TestSet01: Run tests without mounting cgroup filesystem
	echo;
	echo Running first set of testcases;
	echo ==============================
	FS_MOUNTED=0;
	FILE=libcgrouptest01;
	check_mount_fs;
	# unmount fs if already mounted
	if [ $FS_MOUNTED -eq 1 ]
	then
		umount_fs;
	fi;
	debug "FS_MOUNTED = $FS_MOUNTED"
	runtest $FS_MOUNTED $FILE

	wait $PID;
	RC=$?;
	if [ $RC -ne 0 ]
	then
		echo Test binary $FILE exited abnormaly with return value $RC;
		# Do not exit here. Failure in this case does not imply
		# failure in other cases also
	fi;

# TestSet02: Run tests with mounting cgroup filesystem
	echo;
	echo Running second set of testcases;
	echo ==============================
	FILE=libcgrouptest01;
	check_mount_fs;
	# mount fs if not already mounted
	if [ $FS_MOUNTED -eq 0 ]
	then
		mount_fs;
	fi;
	debug "FS_MOUNTED = $FS_MOUNTED"
	get_mountpoint;
	runtest $FS_MOUNTED $FILE

	wait $PID;
	RC=$?;
	if [ $RC -ne 0 ]
	then
		echo Test binary $FILE exited abnormaly with return value $RC;
		# Same commments as above
	fi;


# TestSet03: Run tests with mounting cgroup filesystem at multiple points
	echo;
	echo Running third set of testcases;
	echo ==============================
	# To be done

	umount_fs;
	exit 0;
