#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-only
#
# Attach a task to a cgroup via cgroup_attach_thread_tid()
#
# Copyright (c) 2024 Oracle and/or its affiliates.
# Author: Kamalesh Babulal <kamalesh.babulal@oracle.com>
#

from cgroup import Cgroup as CgroupCli
from libcgroup import Cgroup, Version
from cgroup import CgroupVersion
from process import Process
import ftests
import consts
import sys
import os

CGNAME1 = '092cgattachcg1'
CGNAME2 = '092cgattachcg2'
CONTROLLER = 'cpu'
THREADS = 3


def prereqs(config):
    result = consts.TEST_PASSED
    cause = None

    if config.args.container:
        result = consts.TEST_SKIPPED
        cause = 'This test cannot be run within a container'
        return result, cause

    if CgroupVersion.get_version(CONTROLLER) != CgroupVersion.CGROUP_V1:
        result = consts.TEST_SKIPPED
        cause = 'This test requires cgroup v1'

    return result, cause


def setup(config):
    cg = Cgroup(CGNAME1, Version.CGROUP_V1)
    cg.add_controller(CONTROLLER)
    cg.create()

    cg = Cgroup(CGNAME2, Version.CGROUP_V1)
    cg.add_controller(CONTROLLER)
    cg.create()


def read_cgroup_tasks_file(config, CONTROLLER, CGNAME):
    mnt_path = CgroupCli.get_controller_mount_point(CONTROLLER)
    path = os.path.join(mnt_path, CGNAME, 'tasks')

    cgrp_pids = [line.strip() for line in open(path, 'r')]
    return cgrp_pids


def test(config):
    result = consts.TEST_PASSED
    cause = None

    config.process.create_threaded_process_in_cgroup(config, CONTROLLER, CGNAME1, THREADS)

    threads_tid_cg1 = read_cgroup_tasks_file(config, CONTROLLER, CGNAME1)
    if len(threads_tid_cg1) == 0:
        result = consts.TEST_FAILED
        cause = 'No threads found in cgroup {}'.format(CGNAME1)
        return result, cause

    cg = Cgroup(CGNAME2, Version.CGROUP_V1)
    cg.add_controller(CONTROLLER)
    cg.attach(int(threads_tid_cg1[0]), attach_threads=True)

    threads_tid_cg2 = read_cgroup_tasks_file(config, CONTROLLER, CGNAME2)
    if len(threads_tid_cg2) == 0:
        result = consts.TEST_FAILED
        cause = 'Thread attach from cgroup {} to {}, failed'.format(CGNAME1, CGNAME2)
        return result, cause

    threads_tid_cg1.sort()
    threads_tid_cg2.sort()

    if threads_tid_cg1 != threads_tid_cg2:
        result = consts.TEST_FAILED
        cause = 'Not all threads have moved from cgroup {} to {}'. format(CGNAME1, CGNAME2)
        return result, cause

    threads_tid_cg1 = read_cgroup_tasks_file(config, CONTROLLER, CGNAME1)
    if len(threads_tid_cg1) != 0:
        result = consts.TEST_FAILED
        cause = 'Threads found in cgroup {} expected none'.format(CGNAME1)

    return result, cause


def teardown(config, result):
    # Incase the migration fails, kill the process in CGNAME1
    pids = CgroupCli.get_pids_in_cgroup(config, CGNAME1, CONTROLLER)
    Process.kill(config, pids)

    pids = CgroupCli.get_pids_in_cgroup(config, CGNAME2, CONTROLLER)
    Process.kill(config, pids)

    CgroupCli.delete(config, CONTROLLER, CGNAME1)
    CgroupCli.delete(config, CONTROLLER, CGNAME2)


def main(config):
    [result, cause] = prereqs(config)
    if result != consts.TEST_PASSED:
        return [result, cause]

    try:
        result = consts.TEST_FAILED
        setup(config)
        [result, cause] = test(config)
    finally:
        teardown(config, result)

    return [result, cause]


if __name__ == '__main__':
    config = ftests.parse_args()
    # this test was invoked directly.  run only it
    config.args.num = int(os.path.basename(__file__).split('-')[0])
    sys.exit(ftests.main(config))

# vim: set et ts=4 sw=4:
