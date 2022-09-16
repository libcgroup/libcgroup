#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-only
#
# Stressing cgroup_int() code
#
# Copyright (c) 2022 Oracle and/or its affiliates.  All rights reserved.
# Author: Kamalesh Babulal <kamalesh.babulal@oracle.com>
#

from libcgroup import Cgroup
from run import Run
import consts
import ftests
import sys
import os

MNT_COUNT = 101
MNT_POINT = '/tmp/999stress/'
DIR_PREFIX = 'name'


def cgroup_path(count):
    return MNT_POINT + DIR_PREFIX + str(count)


def prereqs(config):
    return consts.TEST_PASSED, None


def setup(config):
    cmd = ['sudo', 'mkdir']

    cmd.append(MNT_POINT)

    for count in range(MNT_COUNT):
        cmd.append(cgroup_path(count))

    # execute mkdir top-level top-level/sub-directory* at once.
    Run.run(cmd)

    for count in range(MNT_COUNT):
        cmd = ['sudo', 'mount', '-t', 'cgroup', '-o']
        cmd.append('none,name=' + DIR_PREFIX + str(count))
        cmd.append('none')
        cmd.append(cgroup_path(count))
        Run.run(cmd)


def test(config):
    result = consts.TEST_PASSED
    cause = None

    try:
        Cgroup.cgroup_init()
    except RuntimeError as re:
        if 'Failed to initialize libcgroup: 50008' not in str(re):
            cause = str(re)
            result = consts.TEST_FAILED

    return result, cause


def teardown(config):
    result = consts.TEST_PASSED
    cause = None

    for count in range(MNT_COUNT):
        cmd = ['sudo', 'umount']
        cmd.append(cgroup_path(count))
        Run.run(cmd)

    cmd = ['sudo', 'rmdir']
    for count in range(MNT_COUNT):
        cmd.append(cgroup_path(count))

    cmd.append(MNT_POINT)

    # execute rmdir top-level top-level/sub-directory* at once.
    Run.run(cmd)

    return result, cause


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
