#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-only
#
# Advanced cgget functionality test - '-g' <controller>:<path>
#
# Copyright (c) 2021 Oracle and/or its affiliates.
# Author: Tom Hromatka <tom.hromatka@oracle.com>
#

from cgroup import Cgroup, CgroupVersion
import consts
import ftests
import sys
import os

CONTROLLER = 'cpu'
CGNAME = '010cgget'

EXPECTED_OUT_V1 = '''cpu.cfs_period_us: 100000
cpu.stat: nr_periods 0
        nr_throttled 0
        throttled_time 0
cpu.shares: 1024
cpu.cfs_quota_us: -1
cpu.uclamp.min: 0.00
cpu.uclamp.max: max
'''

EXPECTED_OUT_V2 = '''cpu.weight: 100
cpu.stat: usage_usec 0
        user_usec 0
        system_usec 0
        nr_periods 0
        nr_throttled 0
        throttled_usec 0
cpu.weight.nice: 0
cpu.pressure: some avg10=0.00 avg60=0.00 avg300=0.00 total=0
cpu.max: max 100000
cpu.uclamp.min: 0.00
cpu.uclamp.max: max
'''

EXPECTED_OUT_V2_PSI = '''cpu.weight: 100
cpu.stat: usage_usec 0
        user_usec 0
        system_usec 0
        nr_periods 0
        nr_throttled 0
        throttled_usec 0
cpu.weight.nice: 0
cpu.pressure: some avg10=0.00 avg60=0.00 avg300=0.00 total=0
        full avg10=0.00 avg60=0.00 avg300=0.00 total=0
cpu.max: max 100000
cpu.uclamp.min: 0.00
cpu.uclamp.max: max
'''


def prereqs(config):
    result = consts.TEST_PASSED
    cause = None

    return result, cause


def setup(config):
    Cgroup.create(config, CONTROLLER, CGNAME)


def test(config):
    result = consts.TEST_PASSED
    cause = None

    out = Cgroup.get(config, controller='{}:{}'.format(CONTROLLER, CGNAME),
                     print_headers=False)

    version = CgroupVersion.get_version(CONTROLLER)

    if version == CgroupVersion.CGROUP_V1:
        expected_out = EXPECTED_OUT_V1
    elif version == CgroupVersion.CGROUP_V2:
        expected_out = EXPECTED_OUT_V2
        if len(out.splitlines()) != len(expected_out.splitlines()):
            expected_out = EXPECTED_OUT_V2_PSI

    if len(out.splitlines()) != len(expected_out.splitlines()):
        result = consts.TEST_FAILED
        cause = (
                    'Expected line count: {}, but received line count: {}'
                    ''.format(len(expected_out.splitlines()),
                              len(out.splitlines()))
                )
        return result, cause

    if len(out.splitlines()) != len(expected_out.splitlines()):
        result = consts.TEST_FAILED
        cause = (
                    'Expected {} lines but received {} lines'
                    ''.format(len(expected_out.splitlines()),
                              len(out.splitlines()))
                )
        return result, cause

    for line_num, line in enumerate(out.splitlines()):
        if line.strip() != expected_out.splitlines()[line_num].strip():
            result = consts.TEST_FAILED
            cause = (
                        'Expected line:\n\t{}\nbut received line:\n\t{}'
                        ''.format(expected_out.splitlines()[line_num].strip(),
                                  line.strip())
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
