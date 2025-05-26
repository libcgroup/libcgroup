/* SPDX-License-Identifier: LGPL-2.1-only */
/**
 * libcgroup googletest main entry point
 *
 * Copyright (c) 2019 Oracle and/or its affiliates.  All rights reserved.
 * Author: Tom Hromatka <tom.hromatka@oracle.com>
 */

#include "gtest/gtest.h"

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    if (testing::GTEST_FLAG(shuffle)) {
	    std::cout << "--gtest_shuffle option is not supported!" <<std::endl;
	    return 0;
    }
    return RUN_ALL_TESTS();
}
