#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-only
#
# cgxget functionality test - various cpuset settings
#
# Copyright (c) 2021-2025 Oracle and/or its affiliates.
# Author: Tom Hromatka <tom.hromatka@oracle.com>
#

from cgroup import Cgroup, CgroupVersion
from run import RunError
import consts
import ftests
import sys
import os

CONTROLLER = 'memory'
CGNAME = '093cgxget'

CGRP_VER_V1 = CgroupVersion.CGROUP_V1
CGRP_VER_V2 = CgroupVersion.CGROUP_V2

TABLE = [
    # writesetting, writeval, writever, readsetting, readval, readver

    # memory.limit_in_bytes -> memory.max
    ['memory.limit_in_bytes', '0', CGRP_VER_V1,
     'memory.limit_in_bytes', '0', CGRP_VER_V1],
    ['memory.limit_in_bytes', '0', CGRP_VER_V1,
     'memory.max', '0', CGRP_VER_V2],

    ['memory.limit_in_bytes', '4096000', CGRP_VER_V1,
     'memory.limit_in_bytes', '4096000', CGRP_VER_V1],
    ['memory.limit_in_bytes', '8192000', CGRP_VER_V1,
     'memory.max', '8192000', CGRP_VER_V2],

    ['memory.limit_in_bytes', '9223372036854771712', CGRP_VER_V1,
     'memory.limit_in_bytes', '9223372036854771712', CGRP_VER_V1],
    ['memory.limit_in_bytes', '9223372036854771712', CGRP_VER_V1,
     'memory.max', 'max', CGRP_VER_V2],

    ['memory.limit_in_bytes', '9223372036860000000', CGRP_VER_V1,
     'memory.limit_in_bytes', '9223372036854771712', CGRP_VER_V1],
    ['memory.limit_in_bytes', '9223372036860000000', CGRP_VER_V1,
     'memory.max', 'max', CGRP_VER_V2],

    ['memory.limit_in_bytes', '-1', CGRP_VER_V1,
     'memory.limit_in_bytes', '9223372036854771712', CGRP_VER_V1],
    ['memory.limit_in_bytes', '-1', CGRP_VER_V1,
     'memory.max', 'max', CGRP_VER_V2],

    # memory.soft_limit_in_bytes -> memory.high
    ['memory.soft_limit_in_bytes', '0', CGRP_VER_V1,
     'memory.soft_limit_in_bytes', '0', CGRP_VER_V1],
    ['memory.soft_limit_in_bytes', '0', CGRP_VER_V1,
     'memory.high', '0', CGRP_VER_V2],

    ['memory.soft_limit_in_bytes', '409600', CGRP_VER_V1,
     'memory.soft_limit_in_bytes', '409600', CGRP_VER_V1],
    ['memory.soft_limit_in_bytes', '819200', CGRP_VER_V1,
     'memory.high', '819200', CGRP_VER_V2],

    ['memory.soft_limit_in_bytes', '9223372036854771712', CGRP_VER_V1,
     'memory.soft_limit_in_bytes', '9223372036854771712', CGRP_VER_V1],
    ['memory.soft_limit_in_bytes', '9223372036854771712', CGRP_VER_V1,
     'memory.high', 'max', CGRP_VER_V2],

    ['memory.soft_limit_in_bytes', '9223372036860000000', CGRP_VER_V1,
     'memory.soft_limit_in_bytes', '9223372036854771712', CGRP_VER_V1],
    ['memory.soft_limit_in_bytes', '9223372036860000000', CGRP_VER_V1,
     'memory.high', 'max', CGRP_VER_V2],

    ['memory.soft_limit_in_bytes', '-1', CGRP_VER_V1,
     'memory.soft_limit_in_bytes', '9223372036854771712', CGRP_VER_V1],
    ['memory.soft_limit_in_bytes', '-1', CGRP_VER_V1,
     'memory.high', 'max', CGRP_VER_V2],

    # memory.max -> memory.limit_in_bytes
    ['memory.max', '0', CGRP_VER_V2,
     'memory.max', '0', CGRP_VER_V2],
    ['memory.max', '0', CGRP_VER_V2,
     'memory.limit_in_bytes', '0', CGRP_VER_V1],

    ['memory.max', '40960', CGRP_VER_V2,
     'memory.max', '40960', CGRP_VER_V2],
    ['memory.max', '81920', CGRP_VER_V2,
     'memory.limit_in_bytes', '81920', CGRP_VER_V1],

    ['memory.max', 'max', CGRP_VER_V2,
     'memory.max', 'max', CGRP_VER_V2],
    ['memory.max', 'max', CGRP_VER_V2,
     'memory.limit_in_bytes', '9223372036854771712', CGRP_VER_V1],

    # memory.high -> memory.soft_limit_in_bytes
    ['memory.high', '0', CGRP_VER_V2,
     'memory.high', '0', CGRP_VER_V2],
    ['memory.high', '0', CGRP_VER_V2,
     'memory.soft_limit_in_bytes', '0', CGRP_VER_V1],

    ['memory.high', '4096', CGRP_VER_V2,
     'memory.high', '4096', CGRP_VER_V2],
    ['memory.high', '8192', CGRP_VER_V2,
     'memory.soft_limit_in_bytes', '8192', CGRP_VER_V1],

    ['memory.high', 'max', CGRP_VER_V2,
     'memory.high', 'max', CGRP_VER_V2],
    ['memory.high', 'max', CGRP_VER_V2,
     'memory.soft_limit_in_bytes', '9223372036854771712', CGRP_VER_V1],
]


def prereqs(config):
    result = consts.TEST_PASSED
    cause = None

    return result, cause


def setup(config):
    Cgroup.create(config, CONTROLLER, CGNAME)


def invalid_values_test(config):
    result = consts.TEST_PASSED
    cause = None

    try:
        Cgroup.xset(config, cgname=CGNAME, setting='memory.limit_in_bytes',
                    value='-10', version=CGRP_VER_V1)
    except RunError:
        # we passed in an invalid value.  this should fail
        pass
    else:
        result = consts.TEST_FAILED
        cause = 'cgxset unexpectedly succeeded'

    try:
        Cgroup.xset(config, cgname=CGNAME, setting='memory.max',
                    value='-10', version=CGRP_VER_V2)
    except RunError:
        # we passed in an invalid value.  this should fail
        pass
    else:
        result = consts.TEST_FAILED
        cause = 'cgxset unexpectedly succeeded'

    return result, cause


def test(config):
    result = consts.TEST_PASSED
    cause = None

    result, cause = invalid_values_test(config)
    if result != consts.TEST_PASSED:
        return result, cause

    for entry in TABLE:
        Cgroup.xset(config, cgname=CGNAME, setting=entry[0],
                    value=entry[1], version=entry[2])

        out = Cgroup.xget(config, cgname=CGNAME, setting=entry[3],
                          version=entry[5], values_only=True,
                          print_headers=False)

        if out != entry[4]:
            result = consts.TEST_FAILED
            cause = (
                        'After setting {}={}, expected {}={}, but received '
                        '{}={}'
                        ''.format(entry[0], entry[1], entry[3], entry[4],
                                  entry[3], out)
                    )
            return result, cause

    return result, cause


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
