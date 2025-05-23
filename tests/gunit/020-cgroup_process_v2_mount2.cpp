/* SPDX-License-Identifier: LGPL-2.1-only */
/**
 * libcgroup googletest for cgroup_process_v2_mnt()
 *
 * Copyright (c) 2020 Oracle and/or its affiliates.
 * Author: Tom Hromatka <tom.hromatka@oracle.com>
 */

#include <ftw.h>
#include <mntent.h>

#include "gtest/gtest.h"
#include "libcgroup-internal.h"

static const char * const PARENT_DIR = "test008cgroup";
static const char * const PARENT2_DIR = "test008cgroup2";
static const mode_t MODE = S_IRWXU | S_IRWXG | S_IRWXO;

static const char * const CONTROLLERS[] = {
	"cpuset",
	"cpu",
	"io",
	"memory",
	"pids",
	"rdma",
};
static const int CONTROLLERS_CNT = ARRAY_SIZE(CONTROLLERS);

static int mnt_tbl_idx = 0;

class CgroupProcessV2MntTest2 : public ::testing::Test {
	protected:

	void CreateHierarchy(const char * const dir)
	{
		char tmp_path[FILENAME_MAX];
		int i, ret;
		FILE *f;

		ret = mkdir(dir, MODE);
		ASSERT_EQ(ret, 0);

		memset(tmp_path, 0, sizeof(tmp_path));
		snprintf(tmp_path, FILENAME_MAX - 1, "%s/cgroup.controllers",
			 dir);

		f = fopen(tmp_path, "w");
		ASSERT_NE(f, nullptr);

		for (i = 0; i < CONTROLLERS_CNT; i++)
			fprintf(f, "%s ", CONTROLLERS[i]);

		fclose(f);
	}

	void SetUp() override
	{
		CreateHierarchy(PARENT_DIR);

		/* make another directory to test the duplicate logic */
		CreateHierarchy(PARENT2_DIR);
	}

	/*
	 * https://stackoverflow.com/questions/5467725/how-to-delete-a-directory-and-its-contents-in-posix-c
	 */
	static int unlink_cb(const char *fpath, const struct stat *sb, int typeflag,
		      struct FTW *ftwbuf)
	{
		return remove(fpath);
	}

	int rmrf(const char * const path)
	{
		return nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
	}

	void TearDown() override
	{
		int ret = 0;

		ret = rmrf(PARENT_DIR);
		ASSERT_EQ(ret, 0);

		ret = rmrf(PARENT2_DIR);
		ASSERT_EQ(ret, 0);
	}
};

TEST_F(CgroupProcessV2MntTest2, EmptyControllersFile)
{
	char tmp_path[FILENAME_MAX];
	char *mnt_dir = strdup(PARENT_DIR);
	struct mntent ent = (struct mntent) {
		.mnt_fsname = "cgroup2",
		.mnt_dir = mnt_dir,
		.mnt_type = "cgroup2",
		.mnt_opts = "rw,relatime,seclabel",
	};
	FILE *f;
	int ret;

	memset(tmp_path, 0, sizeof(tmp_path));
	snprintf(tmp_path, FILENAME_MAX - 1, "%s/cgroup.controllers",
		 PARENT_DIR);

	/* clear the cgroup.controllers file */
	f = fopen(tmp_path, "w");
	ASSERT_NE(f, nullptr);
	fclose(f);

	/* reset the mount table count */
	mnt_tbl_idx = 0;

	ret = cgroup_process_v2_mnt(&ent, &mnt_tbl_idx);

	ASSERT_EQ(ret, ECGEOF);
	ASSERT_EQ(mnt_tbl_idx, 0);
}
