#!/usr/bin/env python3
#
# empty cgcreate functionality test using the python bindings
#
# Copyright (c) 2021-2022 Oracle and/or its affiliates.
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

from cgroup import Cgroup as CgroupCli
from libcgroup import Cgroup, Version
from cgroup import CgroupVersion
from run import Run
import consts
import ftests
import sys
import os

CONTROLLER = 'cpu'
PARENT_NAME = '044cgcreate'
CGNAME = os.path.join(PARENT_NAME, 'madeviapython')


def prereqs(config):
    result = consts.TEST_PASSED
    cause = None

    if config.args.container:
        result = consts.TEST_SKIPPED
        cause = 'This test cannot be run within a container'
        return result, cause

    if CgroupVersion.get_version(CONTROLLER) != CgroupVersion.CGROUP_V2:
        result = consts.TEST_SKIPPED
        cause = 'This test requires cgroup v2'

    return result, cause


def setup(config):
    user_name = Run.run('whoami', shell_bool=True)
    group_name = Run.run('groups', shell_bool=True).split(' ')[0]

    CgroupCli.create(config, controller_list=CONTROLLER, cgname=PARENT_NAME,
                     user_name=user_name, group_name=group_name)


def test(config):
    result = consts.TEST_PASSED
    cause = None

    cg = Cgroup(CGNAME, Version.CGROUP_V2)
    cg.create()

    # Read a valid file within the newly created cgroup.
    # This should fail if the cgroup was not created successfully
    cg.add_setting(setting_name='cgroup.procs')
    cg.cgxget()

    return result, cause


def teardown(config):
    CgroupCli.delete(config, None, CGNAME)


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
