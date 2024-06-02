#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-only
#
# Advanced cgget functionality test - '-g' <controller>
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
CGNAME = '009cgget'
OUT_PREFIX = '009cgget:\n'


def prereqs(config):
    pass


def setup(config):
    Cgroup.create(config, CONTROLLER, CGNAME)


def test(config):
    result = consts.TEST_PASSED
    cause = None

    EXPECTED_OUT_V1 = [OUT_PREFIX + expected_out for expected_out in consts.EXPECTED_CPU_OUT_V1]
    EXPECTED_OUT_V2 = [OUT_PREFIX + expected_out for expected_out in consts.EXPECTED_CPU_OUT_V2]

    out = Cgroup.get(config, controller=CONTROLLER, cgname=CGNAME)
    version = CgroupVersion.get_version(CONTROLLER)

    if version == CgroupVersion.CGROUP_V1:
        for expected_out in EXPECTED_OUT_V1:
            if len(out.splitlines()) == len(expected_out.splitlines()):
                break
    elif version == CgroupVersion.CGROUP_V2:
        for expected_out in EXPECTED_OUT_V2:
            if len(out.splitlines()) == len(expected_out.splitlines()):
                break

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
    prereqs(config)
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
