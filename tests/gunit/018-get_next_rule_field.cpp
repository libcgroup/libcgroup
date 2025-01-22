/* SPDX-License-Identifier: LGPL-2.1-only */
/**
 * libcgroup googletest for get_next_rule_field()
 *
 * Copyright (c) 2025 Oracle and/or its affiliates.
 * Author: Tom Hromatka <tom.hromatka@oracle.com>
 */

#include <ftw.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "gtest/gtest.h"

#include "libcgroup-internal.h"

class GetNextRuleField : public ::testing::Test {
	protected:
};

TEST_F(GetNextRuleField, InvalidParameters)
{
	char str[] = "abc123";
	int ret;

	ret = get_next_rule_field(NULL, str, ARRAY_SIZE(str), false);
	ASSERT_EQ(ret, ECGINVAL);

	ret = get_next_rule_field(str, NULL, 0, false);
	ASSERT_EQ(ret, ECGINVAL);
}

TEST_F(GetNextRuleField, FieldLenTooSmall)
{
	char *rule = "123456789 abc 123";
	char field[9] = { '\0' };
	int ret;

	ret = get_next_rule_field(rule, field, ARRAY_SIZE(field), false);
	ASSERT_STREQ(field, "");
	ASSERT_EQ(ret, ECGINVAL);
}

TEST_F(GetNextRuleField, FieldLenTooSmallWithQuotes)
{
	char *rule = "\"12345678\" abc 123";
	char field[] = "7654321";
	int ret;

	ret = get_next_rule_field(rule, field, ARRAY_SIZE(field), true);
	ASSERT_STREQ(field, "7654321");
	ASSERT_EQ(ret, ECGINVAL);
}

TEST_F(GetNextRuleField, FieldLenTooSmallWithQuotes2)
{
	char *rule = "\"1234 5678\" abc 123";
	char field[] = "7654321";
	int ret;

	ret = get_next_rule_field(rule, field, ARRAY_SIZE(field), true);
	ASSERT_STREQ(field, "7654321");
	ASSERT_EQ(ret, ECGINVAL);
}

TEST_F(GetNextRuleField, NoTrailingQuotationMark)
{
	char *rule = "\"This is an invalid rule";
	char field[] = "hello world";
	int ret;

	ret = get_next_rule_field(rule, field, ARRAY_SIZE(field), true);
	ASSERT_STREQ(field, "hello world");
	ASSERT_EQ(ret, ECGINVAL);
}

TEST_F(GetNextRuleField, UnexpectedQuotationMark)
{
	char *rule = "\"valid but unexpected\"";
	char field[] = "goodbye world";
	int ret;

	ret = get_next_rule_field(rule, field, ARRAY_SIZE(field), false);
	ASSERT_STREQ(field, "goodbye world");
	ASSERT_EQ(ret, ECGINVAL);
}

TEST_F(GetNextRuleField, UserAndProcess)
{
	char key[CGRP_RULE_MAXKEY] = { '\0' };
	char controllers[CG_CONTROLLER_MAX] = { '\0' };
	char destination[FILENAME_MAX] = { '\0' };

	char *rule = "WillieMays:/usr/bin/homerun cpu SanFrancisco/Giants/";
	char *iter;
	int ret;

	ret = get_next_rule_field(rule, key, ARRAY_SIZE(key), true);
	ASSERT_STREQ(key, "WillieMays:/usr/bin/homerun");
	ASSERT_EQ(ret, strlen("WillieMays:/usr/bin/homerun"));

	iter = &rule[ret];

	ret = get_next_rule_field(iter, controllers, ARRAY_SIZE(controllers), false);
	ASSERT_STREQ(controllers, "cpu");
	ASSERT_EQ(ret, strlen(" cpu"));

	iter += ret;

	ret = get_next_rule_field(iter, destination, ARRAY_SIZE(destination), true);
	ASSERT_STREQ(destination, "SanFrancisco/Giants/");
	ASSERT_EQ(ret, strlen(" SanFrancisco/Giants/"));
}

TEST_F(GetNextRuleField, UserOnly)
{
	char key[CGRP_RULE_MAXKEY] = { '\0' };
	char controllers[CG_CONTROLLER_MAX] = { '\0' };
	char destination[FILENAME_MAX] = { '\0' };

	char *rule = "LuisAparicio freezer Chicago/WhiteSox";
	char *iter;
	int ret;

	ret = get_next_rule_field(rule, key, ARRAY_SIZE(key), true);
	ASSERT_STREQ(key, "LuisAparicio");
	ASSERT_EQ(ret, strlen("LuisAparicio"));

	iter = &rule[ret];

	ret = get_next_rule_field(iter, controllers, ARRAY_SIZE(controllers), false);
	ASSERT_STREQ(controllers, "freezer");
	ASSERT_EQ(ret, strlen(" freezer"));

	iter += ret;

	ret = get_next_rule_field(iter, destination, ARRAY_SIZE(destination), true);
	ASSERT_STREQ(destination, "Chicago/WhiteSox");
	ASSERT_EQ(ret, strlen(" Chicago/WhiteSox"));
}

TEST_F(GetNextRuleField, KeyWithQuotesAndSpaces)
{
	char key[CGRP_RULE_MAXKEY] = { '\0' };
	char controllers[CG_CONTROLLER_MAX] = { '\0' };
	char destination[FILENAME_MAX] = { '\0' };

	char *rule = "\"Sandy Koufax:/bin/fast ball\" memory Brooklyn/Dodgers";
	char *iter;
	int ret;

	ret = get_next_rule_field(rule, key, ARRAY_SIZE(key), true);
	ASSERT_STREQ(key, "Sandy Koufax:/bin/fast ball");
	ASSERT_EQ(ret, strlen("\"Sandy Koufax:/bin/fast ball\""));

	iter = &rule[ret];

	ret = get_next_rule_field(iter, controllers, ARRAY_SIZE(controllers), false);
	ASSERT_STREQ(controllers, "memory");
	ASSERT_EQ(ret, strlen(" memory"));

	iter += ret;

	ret = get_next_rule_field(iter, destination, ARRAY_SIZE(destination), true);
	ASSERT_STREQ(destination, "Brooklyn/Dodgers");
	ASSERT_EQ(ret, strlen(" Brooklyn/Dodgers"));
}

TEST_F(GetNextRuleField, DestinationWithQuotes)
{
	char key[CGRP_RULE_MAXKEY] = { '\0' };
	char controllers[CG_CONTROLLER_MAX] = { '\0' };
	char destination[FILENAME_MAX] = { '\0' };

	char *rule = "JoshGibson:/homerun/king io,memory,cpu \"Homestead Grays\"";
	char *iter;
	int ret;

	ret = get_next_rule_field(rule, key, ARRAY_SIZE(key), true);
	ASSERT_STREQ(key, "JoshGibson:/homerun/king");
	ASSERT_EQ(ret, strlen("JoshGibson:/homerun/king"));

	iter = &rule[ret];

	ret = get_next_rule_field(iter, controllers, ARRAY_SIZE(controllers), false);
	ASSERT_STREQ(controllers, "io,memory,cpu");
	ASSERT_EQ(ret, strlen(" io,memory,cpu"));

	iter += ret;

	ret = get_next_rule_field(iter, destination, ARRAY_SIZE(destination), true);
	ASSERT_STREQ(destination, "Homestead Grays");
	ASSERT_EQ(ret, strlen(" \"Homestead Grays\""));
}

TEST_F(GetNextRuleField, TabsAsDelimiters)
{
	char key[CGRP_RULE_MAXKEY] = { '\0' };
	char controllers[CG_CONTROLLER_MAX] = { '\0' };
	char destination[FILENAME_MAX] = { '\0' };

	char *rule = "\t\tSatchelPaige\tcpuset\t\"Kansas City/Monarchs\"";
	char *iter;
	int ret;

	ret = get_next_rule_field(rule, key, ARRAY_SIZE(key), true);
	ASSERT_STREQ(key, "SatchelPaige");
	ASSERT_EQ(ret, strlen("\t\tSatchelPaige"));

	iter = &rule[ret];

	ret = get_next_rule_field(iter, controllers, ARRAY_SIZE(controllers), false);
	ASSERT_STREQ(controllers, "cpuset");
	ASSERT_EQ(ret, strlen("\tcpuset"));

	iter += ret;

	ret = get_next_rule_field(iter, destination, ARRAY_SIZE(destination), true);
	ASSERT_STREQ(destination, "Kansas City/Monarchs");
	ASSERT_EQ(ret, strlen(" \"Kansas City/Monarchs\""));
}

TEST_F(GetNextRuleField, RuleWithOptions)
{
	char key[CGRP_RULE_MAXKEY] = { '\0' };
	char controllers[CG_CONTROLLER_MAX] = { '\0' };
	char destination[FILENAME_MAX] = { '\0' };
	char options[CG_OPTIONS_MAX] = { '\0' };

	char *rule = "\t\t\"Roberto Clemente\"\t\tpids\t\"Pittsburgh/Pirates\"\tignore";
	char *iter;
	int ret;

	ret = get_next_rule_field(rule, key, ARRAY_SIZE(key), true);
	ASSERT_STREQ(key, "Roberto Clemente");
	ASSERT_EQ(ret, strlen("\t\t\"Roberto Clemente\""));

	iter = &rule[ret];

	ret = get_next_rule_field(iter, controllers, ARRAY_SIZE(controllers), false);
	ASSERT_STREQ(controllers, "pids");
	ASSERT_EQ(ret, strlen("\t\tpids"));

	iter += ret;

	ret = get_next_rule_field(iter, destination, ARRAY_SIZE(destination), true);
	ASSERT_STREQ(destination, "Pittsburgh/Pirates");
	ASSERT_EQ(ret, strlen(" \"Pittsburgh/Pirates\""));

	iter += ret;

	ret = get_next_rule_field(iter, options, ARRAY_SIZE(options), false);
	ASSERT_STREQ(options, "ignore");
	ASSERT_EQ(ret, strlen("\tignore"));
}
