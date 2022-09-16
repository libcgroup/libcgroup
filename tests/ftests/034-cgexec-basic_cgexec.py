#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-only
#
# Cgroup cgexec test
#
# Copyright (c) 2021 Oracle and/or its affiliates.
# Author: Tom Hromatka <tom.hromatka@oracle.com>
#

from cgroup import Cgroup
from run import Run
import consts
import ftests
import sys
import os


CONTROLLER = 'cpuset'
CGNAME = '034cgexec'


def prereqs(config):
    if not config.args.container:
        result = consts.TEST_SKIPPED
        cause = 'This test must be run within a container'
        return result, cause

    return consts.TEST_PASSED, None


def setup(config):
    Cgroup.create(config, CONTROLLER, CGNAME)


def test(config):
    config.process.create_process_in_cgroup(config, CONTROLLER, CGNAME,
                                            cgclassify=False)

    pids = Cgroup.get_pids_in_cgroup(config, CGNAME, CONTROLLER)
    if pids is None:
        result = consts.TEST_FAILED
        cause = 'No processes were found in cgroup {}'.format(CGNAME)
        return result, cause

    # run cgexec -h
    ret = Cgroup.cgexec(config, controller=CONTROLLER, cgname=CGNAME,
                        cmdline=None, cghelp=True)
    if 'Run the task in given control group(s)' not in ret:
        result = consts.TEST_FAILED
        cause = 'Failed to print cgexec help text: {}'.format(ret)
        return result, cause

    return consts.TEST_PASSED, None


def teardown(config):
    pids = Cgroup.get_pids_in_cgroup(config, CGNAME, CONTROLLER)
    if pids:
        for p in pids.splitlines():
            if config.args.container:
                config.container.run(['kill', '-9', p])
            else:
                Run.run(['sudo', 'kill', '-9', p])

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
