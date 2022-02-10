#!/usr/bin/env python3
#
# Multithreaded cgroup v2 test
#
# Copyright (c) 2022 Oracle and/or its affiliates.
# Author: Kamalesh Babulal <kamalesh.babulal@oracle.com>
#

#
# This library is free software; you can redistribute it and/or modify it
# under the terms of version 2.1 of the GNU Lesser General Public License as
# published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
# for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this library; if not, see <http://www.gnu.org/licenses>.
#

from cgroup import Cgroup, CgroupVersion
from run import Run
import consts
import ftests
import sys
import os

CONTROLLER = 'cpu'
PARENT_CGNAME = '036threaded'
CHILD_CGPATH = os.path.join(PARENT_CGNAME, 'childcg')

SETTING = 'cgroup.type'
AFTER = 'threaded'
THREADS = 3

def prereqs(config):
    result = consts.TEST_PASSED
    cause = None

    if config.args.container:
        result = consts.TEST_SKIPPED
        cause = "This test cannot be run within a container"
        return result, cause

    if CgroupVersion.get_version(CONTROLLER) != CgroupVersion.CGROUP_V2:
        result = consts.TEST_SKIPPED
        cause = "This test requires the cgroup v2"
        return result, cause

    return result, cause

def setup(config):
    Cgroup.create(config, CONTROLLER, PARENT_CGNAME)
    Cgroup.create(config, CONTROLLER, CHILD_CGPATH)

    Cgroup.set_and_validate(config, CHILD_CGPATH, SETTING, AFTER)

    return consts.TEST_PASSED, None

def test(config):
    config.process.create_threaded_process_in_cgroup(
                                config, CONTROLLER, PARENT_CGNAME, THREADS)

    threads = Cgroup.get(config, controller=None, cgname=PARENT_CGNAME,
                         setting="cgroup.threads", print_headers=False,
                         values_only=True)
    threads = threads.replace('\n', '').split('\t')

    #pick the first thread
    thread_tid = threads[1]

    Cgroup.set_and_validate(config, CHILD_CGPATH, "cgroup.threads", thread_tid)

    return consts.TEST_PASSED, None

def teardown(config):
    # destroy the child processes
    pids = Cgroup.get_pids_in_cgroup(config, PARENT_CGNAME, CONTROLLER)
    if pids:
        for p in pids.splitlines():
            Run.run(['sudo', 'kill', '-9', p])

    Cgroup.delete(config, CONTROLLER, PARENT_CGNAME, recursive=True)

def main(config):
    [result, cause] = prereqs(config)
    if result != consts.TEST_PASSED:
        return [result, cause]

    try:
        setup(config)
        [result, cause] = test(config)
    finally:
        teardown(config)

    return [result, cause]

if __name__ == '__main__':
    config = ftests.parse_args()
    # this test was invoked directly.  run only it
    config.args.num = int(os.path.basename(__file__).split('-')[0])
    sys.exit(ftests.main(config))

# vim: set et ts=4 sw=4:
