/**
 * libcgroup googletest for cgroup_parse_rules_options()
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

class ParseRulesOptionsTest : public ::testing::Test {
};

TEST_F(ParseRulesOptionsTest, RulesOptions_Ignore)
{
	struct cgroup_rule rule;
	char options[] = "ignore";
	int ret;

	rule.is_ignore = false;

	ret = cgroup_parse_rules_options(options, &rule);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(rule.is_ignore, true);
}

TEST_F(ParseRulesOptionsTest, RulesOptions_IgnoreWithComma)
{
	struct cgroup_rule rule;
	char options[] = "ignore,";
	int ret;

	rule.is_ignore = false;

	ret = cgroup_parse_rules_options(options, &rule);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(rule.is_ignore, true);
}

TEST_F(ParseRulesOptionsTest, RulesOptions_InvalidOption)
{
	struct cgroup_rule rule;
	char options[] = "ignoretypo";
	int ret;

	rule.is_ignore = false;

	ret = cgroup_parse_rules_options(options, &rule);
	ASSERT_EQ(ret, -EINVAL);
	ASSERT_EQ(rule.is_ignore, false);
}

TEST_F(ParseRulesOptionsTest, RulesOptions_InvalidOption2)
{
	struct cgroup_rule rule;
	char options[] = "ignore,foobar";
	int ret;

	rule.is_ignore = false;

	ret = cgroup_parse_rules_options(options, &rule);
	ASSERT_EQ(ret, -EINVAL);
	ASSERT_EQ(rule.is_ignore, true);
}

TEST_F(ParseRulesOptionsTest, RulesOptions_EmptyOptions)
{
	struct cgroup_rule rule;
	char options[] = "";
	int ret;

	rule.is_ignore = false;

	ret = cgroup_parse_rules_options(options, &rule);
	ASSERT_EQ(ret, -EINVAL);
	ASSERT_EQ(rule.is_ignore, false);
}

TEST_F(ParseRulesOptionsTest, RulesOptions_NullOptions)
{
	struct cgroup_rule rule;
	char *options = NULL;
	int ret;

	rule.is_ignore = false;

	ret = cgroup_parse_rules_options(options, &rule);
	ASSERT_EQ(ret, -EINVAL);
	ASSERT_EQ(rule.is_ignore, false);
}
