#!/usr/bin/env python3
#
# Cgroup cgexec test
#
# Copyright (c) 2021 Oracle and/or its affiliates.
# Author: Tom Hromatka <tom.hromatka@oracle.com>
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
import consts
import ftests
import os
from process import Process
from run import Run
import sys

import time

CONTROLLER = 'cpuset'
CGNAME = '034cgexec'

def prereqs(config):
    if not config.args.container:
        result = consts.TEST_SKIPPED
        cause = "This test must be run within a container"
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
        cause = "No processes were found in cgroup {}".format(CGNAME)
        return result, cause

    # run cgexec -h
    ret = Cgroup.cgexec(config, controller=CONTROLLER, cgname=CGNAME,
                        cmdline=None, cghelp=True)
    if not "Run the task in given control group(s)" in ret:
        result = consts.TEST_FAILED
        cause = "Failed to print cgexec help text: {}".format(ret)
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
