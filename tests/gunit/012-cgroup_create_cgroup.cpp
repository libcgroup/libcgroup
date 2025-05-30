/* SPDX-License-Identifier: LGPL-2.1-only */
/**
 * libcgroup googletest for cgroup_create_cgroup()
 *
 * Copyright (c) 2020 Oracle and/or its affiliates.
 * Author: Tom Hromatka <tom.hromatka@oracle.com>
 */

#include <dirent.h>
#include <ftw.h>

#include "gtest/gtest.h"
#include "libcgroup-internal.h"

static const char * const PARENT_DIR = "test012cgroup";
static const mode_t MODE = S_IRWXU | S_IRWXG | S_IRWXO;

static const char * const CONTROLLERS[] = {
	"cpu",
	"freezer",
	"memory",
	"cpuset",
	"namespaces",
	"netns",
};
static const int CONTROLLERS_CNT = ARRAY_SIZE(CONTROLLERS);

static cg_version_t VERSIONS[] = {
	CGROUP_V1,
	CGROUP_V2,
	CGROUP_V2,
	CGROUP_V1,
	CGROUP_V1,
	CGROUP_V2,
};
static const int VERSIONS_CNT = ARRAY_SIZE(VERSIONS);

class CgroupCreateCgroupTest : public ::testing::Test {
	protected:

	void SetUp() override
	{
		char tmp_path[FILENAME_MAX];
		int ret, i;
		FILE *f;

		ASSERT_EQ(VERSIONS_CNT, CONTROLLERS_CNT);

		ret = mkdir(PARENT_DIR, MODE);
		ASSERT_EQ(ret, 0);

		/*
		 * Artificially populate the mount table with local
		 * directories
		 */
		memset(&cg_mount_table, 0, sizeof(cg_mount_table));
		memset(&cg_namespace_table, 0, sizeof(cg_namespace_table));

		for (i = 0; i < CONTROLLERS_CNT; i++) {
			snprintf(cg_mount_table[i].name, CONTROL_NAMELEN_MAX, "%s", CONTROLLERS[i]);
			cg_mount_table[i].version = VERSIONS[i];

			switch (VERSIONS[i]) {
			case CGROUP_V1:
				snprintf(cg_mount_table[i].mount.path,
					 FILENAME_MAX, "%s/%s", PARENT_DIR, CONTROLLERS[i]);

				ret = mkdir(cg_mount_table[i].mount.path, MODE);
				ASSERT_EQ(ret, 0);
				break;
			case CGROUP_V2:
				snprintf(cg_mount_table[i].mount.path,
					 FILENAME_MAX, "%s", PARENT_DIR);

				memset(tmp_path, 0, sizeof(tmp_path));
				snprintf(tmp_path, FILENAME_MAX - 1, "%s/cgroup.subtree_control",
					 PARENT_DIR);

				f = fopen(tmp_path, "w");
				ASSERT_NE(f, nullptr);
				fclose(f);

				memset(tmp_path, 0, sizeof(tmp_path));
				snprintf(tmp_path, FILENAME_MAX - 1,
					 "%s/cgroup.controllers",
					 PARENT_DIR);

				f = fopen(tmp_path, "a");
				ASSERT_NE(f, nullptr);
				fprintf(f, "%s ",  CONTROLLERS[i]);
				fclose(f);
				break;
			default:
				/* we shouldn't get here.  fail the test */
				ASSERT_TRUE(false);
			}
		}
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
	}
};

static void verify_cgroup_created(const char * const cgrp_name, const char * const ctrl)
{
	char tmp_path[FILENAME_MAX];
	DIR *dir;

	memset(tmp_path, 0, sizeof(tmp_path));

	if (ctrl)
		snprintf(tmp_path, FILENAME_MAX - 1, "%s/%s/%s", PARENT_DIR, ctrl, cgrp_name);
	else
		snprintf(tmp_path, FILENAME_MAX - 1, "%s/%s", PARENT_DIR, cgrp_name);

	dir = opendir(tmp_path);
	ASSERT_NE(dir, nullptr);
	closedir(dir);
}

static void verify_subtree_contents(const char * const expected)
{
	char tmp_path[FILENAME_MAX], buf[4092];
	FILE *f;

	memset(tmp_path, 0, sizeof(tmp_path));
	snprintf(tmp_path, FILENAME_MAX - 1, "%s/cgroup.subtree_control", PARENT_DIR);
	f = fopen(tmp_path, "r");
	ASSERT_NE(f, nullptr);

	while (fgets(buf, sizeof(buf), f))
		ASSERT_STREQ(buf, expected);
	fclose(f);
}

TEST_F(CgroupCreateCgroupTest, CgroupCreateCgroupV1)
{
	const char * const cgrp_name = "MyV1Cgroup";
	const char * const ctrl_name = "cpu";
	struct cgroup_controller *ctrl;
	struct cgroup *cgrp = NULL;
	int ret;

	cgrp = cgroup_new_cgroup(cgrp_name);
	ASSERT_NE(cgrp, nullptr);

	ctrl = cgroup_add_controller(cgrp, ctrl_name);
	ASSERT_NE(ctrl, nullptr);

	ret = cgroup_create_cgroup(cgrp, 1);
	ASSERT_EQ(ret, 0);

	verify_cgroup_created(cgrp_name, ctrl_name);
}

TEST_F(CgroupCreateCgroupTest, CgroupCreateCgroupV2)
{
	const char * const cgrp_name = "MyV2Cgroup";
	const char * const ctrl_name = "freezer";
	struct cgroup_controller *ctrl;
	struct cgroup *cgrp = NULL;
	int ret;

	cgrp = cgroup_new_cgroup(cgrp_name);
	ASSERT_NE(cgrp, nullptr);

	ctrl = cgroup_add_controller(cgrp, ctrl_name);
	ASSERT_NE(ctrl, nullptr);

	ret = cgroup_create_cgroup(cgrp, 0);
	ASSERT_EQ(ret, 0);

	verify_cgroup_created(cgrp_name, NULL);
	verify_subtree_contents("+freezer");
}

TEST_F(CgroupCreateCgroupTest, CgroupCreateCgroupV1AndV2)
{
	const char * const cgrp_name = "MyV1AndV2Cgroup";
	const char * const ctrl1_name = "memory";
	const char * const ctrl2_name = "cpuset";
	struct cgroup_controller *ctrl;
	struct cgroup *cgrp = NULL;
	int ret;

	cgrp = cgroup_new_cgroup(cgrp_name);
	ASSERT_NE(cgrp, nullptr);

	ctrl = cgroup_add_controller(cgrp, ctrl1_name);
	ASSERT_NE(ctrl, nullptr);
	ctrl = NULL;
	ctrl = cgroup_add_controller(cgrp, ctrl2_name);
	ASSERT_NE(ctrl, nullptr);

	ret = cgroup_create_cgroup(cgrp, 1);
	ASSERT_EQ(ret, 0);

	verify_cgroup_created(cgrp_name, NULL);
	verify_cgroup_created(cgrp_name, ctrl2_name);
	verify_subtree_contents("+memory");
}
