#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-only
#
# Advanced cgset functionality test - test the '--copy-from' option
#
# Copyright (c) 2021 Oracle and/or its affiliates.
# Author: Tom Hromatka <tom.hromatka@oracle.com>
#

from cgroup import Cgroup, CgroupVersion
import consts
import ftests
import sys
import os

CONTROLLER = 'memory'
SRC_CGNAME = '023cgsetsrc'
DST_CGNAME = '023cgsetdst'

SETTINGS = ['memory.limit_in_bytes',
            'memory.soft_limit_in_bytes',
            'memory.swappiness']
VALUES = ['122880', '40960', '42']


def prereqs(config):
    result = consts.TEST_PASSED
    cause = None

    if CgroupVersion.get_version('memory') != CgroupVersion.CGROUP_V1:
        result = consts.TEST_SKIPPED
        cause = 'This test requires the cgroup v1 memory controller'

    return result, cause


def setup(config):
    Cgroup.create(config, CONTROLLER, SRC_CGNAME)
    Cgroup.create(config, CONTROLLER, DST_CGNAME)
    Cgroup.set(config, cgname=SRC_CGNAME, setting=SETTINGS, value=VALUES)


def test(config):
    Cgroup.set(config, cgname=DST_CGNAME, copy_from=SRC_CGNAME)

    for i, setting in enumerate(SETTINGS):
        Cgroup.get_and_validate(config, DST_CGNAME, setting, VALUES[i])

    return consts.TEST_PASSED, None


def teardown(config):
    Cgroup.delete(config, CONTROLLER, SRC_CGNAME)
    Cgroup.delete(config, CONTROLLER, DST_CGNAME)


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
