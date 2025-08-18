#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-only
#
# Basic cgget functionality test
#
# Copyright (c) 2019 Oracle and/or its affiliates.  All rights reserved.
# Author: Tom Hromatka <tom.hromatka@oracle.com>
#

from cgroup import Cgroup, Mode
import consts
import ftests
import sys
import os

CONTROLLER = 'cpu'
CGNAME = '001cgget'
SETTING = 'cpu.shares'
VALUE = '512'


def prereqs(config):
    result = consts.TEST_PASSED
    cause = None

    if Cgroup.get_cgroup_mode(config) == Mode.CGROUP_MODE_UNIFIED:
        result = consts.TEST_SKIPPED
        cause = 'This test requires the legacy/hybrid cgroup hierarchy'

    return result, cause


def setup(config):
    Cgroup.create(config, CONTROLLER, CGNAME)
    Cgroup.set(config, CGNAME, SETTING, VALUE)


def test(config):
    Cgroup.get_and_validate(config, CGNAME, SETTING, VALUE)

    return consts.TEST_PASSED, None


def teardown(config):
    Cgroup.delete(config, CONTROLLER, CGNAME)


def main(config):
    [result, cause] = prereqs(config)
    if result != consts.TEST_PASSED:
        return [result, cause]

    setup(config)
    [result, cause] = test(config)
    teardown(config)

    return [result, cause]


if __name__ == '__main__':
    config = ftests.parse_args()
    # this test was invoked directly.  run only it
    config.args.num = int(os.path.basename(__file__).split('-')[0])
    sys.exit(ftests.main(config))

# vim: set et ts=4 sw=4:
