/**
 * libcgroup googletest for parse_r_flag() in cgset
 *
 * Copyright (c) 2021 Oracle and/or its affiliates.
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

#include <ftw.h>

#include "gtest/gtest.h"

#include "libcgroup-internal.h"
#include "tools-common.h"

static const char * const PARENT_DIR = "test016cgset/";

static const char * const NAME = "io.max";
static const char * const VALUE = "\"8:16 wbps=1024\"";

class CgsetParseRFlagTest : public ::testing::Test {
};

TEST_F(CgsetParseRFlagTest, EqualCharInValue)
{
	struct control_value name_value;
	char name_value_str[4092];
	int ret;

	ret = snprintf(name_value_str, sizeof(name_value_str) -1,
		       "%s=%s", NAME, VALUE);
	ASSERT_GT(ret, 0);

	ret = parse_r_flag("cgset", name_value_str, &name_value);
	ASSERT_EQ(ret, 0);

	ASSERT_STREQ(name_value.name, NAME);
	ASSERT_STREQ(name_value.value, VALUE);
}
