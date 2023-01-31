/* SPDX-License-Identifier: LGPL-2.1-only */
/**
 * libcgroup googletest for fuzz testing APIs with negative values.
 *
 * Copyright (c) 2023 Oracle and/or its affiliates.  All rights reserved.
 * Author: Kamalesh Babulal <kamalesh.babulal@oracle.com>
 */

#include <sys/stat.h>

#include "gtest/gtest.h"

#include "libcgroup-internal.h"

class APIArgsTest: public :: testing:: Test {
	protected:

	void SetUp() override {
		/* Stub */
	}
};

/**
 * Pass NULL cgroup for setting permissions
 * @param APIArgsTest googletest test case name
 * @param API_cgroup_set_permissions test name
 *
 * This test will pass NULL cgroup to the cgroup_set_permissions()
 * and check it handles it gracefully.
 */
TEST_F(APIArgsTest, API_cgroup_set_permissions)
{
	mode_t dir_mode, ctrl_mode, task_mode;
	struct cgroup * cgroup = NULL;

	dir_mode = (S_IRWXU | S_IXGRP | S_IXOTH);
	ctrl_mode = (S_IRUSR | S_IWUSR | S_IRGRP);
	task_mode = (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

	testing::internal::CaptureStdout();

	cgroup_set_permissions(cgroup, dir_mode, ctrl_mode, task_mode);

	std::string result = testing::internal::GetCapturedStdout();
	ASSERT_EQ(result, "Error: Cgroup, operation not allowed\n");
}
