/**
 * libcgroup googletest for cgroup_compare_wildcard_procname()
 *
 * Copyright (c) 2019 Oracle and/or its affiliates.  All rights reserved.
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

#include "gtest/gtest.h"

#include "libcgroup-internal.h"

class ProcnameWildcardTest : public ::testing::Test {
};

TEST_F(ProcnameWildcardTest, ProcnameWildcard_NoAsterisk)
{
	char rule_procname[] = "systemd";
	char procname[] = "bash";
	bool ret;

	ret = cgroup_compare_wildcard_procname(rule_procname, procname);
	ASSERT_EQ(ret, false);
}

TEST_F(ProcnameWildcardTest, ProcnameWildcard_AsteriskNoMatch)
{
	char rule_procname[] = "BobIsYour*";
	char procname[] = "Linda";
	bool ret;

	ret = cgroup_compare_wildcard_procname(rule_procname, procname);
	ASSERT_EQ(ret, false);
}

TEST_F(ProcnameWildcardTest, ProcnameWildcard_AsteriskMatch)
{
	char rule_procname[] = "HelloWorl*";
	char procname[] = "HelloWorld";
	bool ret;

	ret = cgroup_compare_wildcard_procname(rule_procname, procname);
	ASSERT_EQ(ret, true);
}

TEST_F(ProcnameWildcardTest, ProcnameWildcard_AsteriskNoMatch2)
{
	char rule_procname[] = "HelloW*";
	char procname[] = "Hello";
	bool ret;

	ret = cgroup_compare_wildcard_procname(rule_procname, procname);
	ASSERT_EQ(ret, false);
}

TEST_F(ProcnameWildcardTest, ProcnameWildcard_AsteriskMatchExactly)
{
	char rule_procname[] = "strace*";
	char procname[] = "strace";
	bool ret;

	ret = cgroup_compare_wildcard_procname(rule_procname, procname);
	ASSERT_EQ(ret, true);
}

TEST_F(ProcnameWildcardTest, ProcnameWildcard_NoAsteriskMatchExactly)
{
	char rule_procname[] = "systemd-cgls";
	char procname[] = "systemd-cgls";
	bool ret;

	ret = cgroup_compare_wildcard_procname(rule_procname, procname);
	ASSERT_EQ(ret, false);
}

TEST_F(ProcnameWildcardTest, ProcnameWildcard_AsteriskFirstChar)
{
	char rule_procname[] = "*";
	char procname[] = "tomcat";
	bool ret;

	ret = cgroup_compare_wildcard_procname(rule_procname, procname);
	ASSERT_EQ(ret, true);
}
