/**
 * libcgroup googletest for cgroup_process_v1_mnt()
 *
 * Copyright (c) 2020 Oracle and/or its affiliates.
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

#include <mntent.h>

#include "gtest/gtest.h"
#include "libcgroup-internal.h"

static int mnt_tbl_idx = 0;

class CgroupProcessV1MntTest : public ::testing::Test {
};

TEST_F(CgroupProcessV1MntTest, AddV1Mount)
{
	char *controllers[] = {"cpu", "memory", NULL};
	struct mntent ent = (struct mntent) {
		.mnt_fsname = "cgroup",
		.mnt_dir = "/sys/fs/cgroup/memory",
		.mnt_type = "cgroup",
		.mnt_opts = "rw,nosuid,nodev,noexec,relatime,seclabel,memory",
	};
	int ret;

	ret = cgroup_process_v1_mnt(controllers, &ent, &mnt_tbl_idx);

	ASSERT_EQ(ret, 0);
	ASSERT_EQ(mnt_tbl_idx, 1);
	ASSERT_STREQ(cg_mount_table[0].name, "memory");
	ASSERT_STREQ(cg_mount_table[0].mount.path, ent.mnt_dir);
}

/* The AddV1Mount() test above added the memory controller to the
 * cg_mount_table[].  Now let's add another mount point of the
 * memory controller to test the duplicate mount handling
 */
TEST_F(CgroupProcessV1MntTest, AddV1Mount_Duplicate)
{
	char *controllers[] = {"cpu", "cpuset", "memory", NULL};
	struct mntent ent = (struct mntent) {
		.mnt_fsname = "cgroup",
		.mnt_dir = "/cgroup/memory",
		.mnt_type = "cgroup",
		.mnt_opts = "rw,nosuid,nodev,noexec,relatime,seclabel,memory",
	};
	int ret;

	ASSERT_EQ(NULL, cg_mount_table[0].mount.next);

	ret = cgroup_process_v1_mnt(controllers, &ent, &mnt_tbl_idx);

	ASSERT_EQ(ret, 0);
	ASSERT_EQ(mnt_tbl_idx, 1);
	ASSERT_STREQ(cg_mount_table[0].mount.next->path, ent.mnt_dir);
}

TEST_F(CgroupProcessV1MntTest, AddV1NamedMount)
{
	char *controllers[] = {"cpu", "memory", "systemd", NULL};
	struct mntent ent = (struct mntent) {
		.mnt_fsname = "cgroup",
		.mnt_dir = "/sys/fs/cgroup/systemd",
		.mnt_type = "cgroup",
		.mnt_opts = "rw,nosuid,nodev,noexec,relatime,seclabel,name=systemd",
	};
	int ret;

	ret = cgroup_process_v1_mnt(controllers, &ent, &mnt_tbl_idx);

	ASSERT_EQ(ret, 0);
	ASSERT_EQ(mnt_tbl_idx, 2);
	ASSERT_STREQ(cg_mount_table[1].name, "name=systemd");
	ASSERT_STREQ(cg_mount_table[1].mount.path, ent.mnt_dir);
}

TEST_F(CgroupProcessV1MntTest, AddV1NamedMount_Duplicate)
{
	char *controllers[] = {"cpu", "memory", "systemd", NULL};
	struct mntent ent = (struct mntent) {
		.mnt_fsname = "cgroup",
		.mnt_dir = "/cgroup2/systemd",
		.mnt_type = "cgroup",
		.mnt_opts = "rw,nosuid,nodev,noexec,relatime,seclabel,name=systemd",
	};
	int ret;

	ret = cgroup_process_v1_mnt(controllers, &ent, &mnt_tbl_idx);

	ASSERT_EQ(ret, 0);
	ASSERT_EQ(mnt_tbl_idx, 2);
	ASSERT_STREQ(cg_mount_table[1].name, "name=systemd");
	ASSERT_STREQ(cg_mount_table[1].mount.next->path, ent.mnt_dir);
}
