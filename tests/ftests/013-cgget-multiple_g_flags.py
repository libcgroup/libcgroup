#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-only
#
# Advanced cgget functionality test - multiple '-g' flags
#
# Copyright (c) 2021 Oracle and/or its affiliates.
# Author: Tom Hromatka <tom.hromatka@oracle.com>
#

from cgroup import Cgroup, CgroupVersion
import consts
import ftests
import sys
import os

CONTROLLER1 = 'pids'
CONTROLLER2 = 'cpu'
CGNAME = '013cgget'
OUT_PREFIX = '013cgget:\n'


def prereqs(config):
    pass


def setup(config):
    Cgroup.create(config, CONTROLLER1, CGNAME)
    Cgroup.create(config, CONTROLLER2, CGNAME)


def test(config):
    result = consts.TEST_PASSED
    cause = None

    # Append pid controller [0] and cpu controller [N - 1]
    EXPECTED_OUT_V1 = [OUT_PREFIX + consts.EXPECTED_PIDS_OUT[0] + expected_out
                       for expected_out in consts.EXPECTED_CPU_OUT_V1[:-1]]
    # Append pid controller [1] and cpu controller [N]
    EXPECTED_OUT_V1.append(OUT_PREFIX + consts.EXPECTED_PIDS_OUT[1] +
                           consts.EXPECTED_CPU_OUT_V1[-1])

    # Append pid controller [0] and cpu controller [N - 1]
    EXPECTED_OUT_V2 = [OUT_PREFIX + consts.EXPECTED_PIDS_OUT[0] + expected_out
                       for expected_out in consts.EXPECTED_CPU_OUT_V2[:-1]]
    # Append pid controller [1] and cpu controller [N]
    EXPECTED_OUT_V2.append(OUT_PREFIX + consts.EXPECTED_PIDS_OUT[1] +
                           consts.EXPECTED_CPU_OUT_V2[-1])

    out = Cgroup.get(config, controller=[CONTROLLER1, CONTROLLER2],
                     cgname=CGNAME)
    version = CgroupVersion.get_version(CONTROLLER1)

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
    ver1 = CgroupVersion.get_version(CONTROLLER1)
    ver2 = CgroupVersion.get_version(CONTROLLER2)

    if ver1 == CgroupVersion.CGROUP_V2 and \
       ver2 == CgroupVersion.CGROUP_V2:
        # If both controllers are cgroup v2, then we only need to delete
        # one cgroup.  The path will be the same for both
        Cgroup.delete(config, [CONTROLLER1, CONTROLLER2], CGNAME)
    else:
        Cgroup.delete(config, CONTROLLER1, CGNAME)
        Cgroup.delete(config, CONTROLLER2, CGNAME)


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
