#!/usr/bin/env python3
#
# cgget/cgset cgroup.type functionality test
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
import sys

# Which controller isn't all that important, but it is important that we
# have a cgroup v2 controller
CONTROLLER = 'cpu'
CGNAME = "035cgset"

SETTING = 'cgroup.type'
BEFORE = 'domain'
AFTER = 'threaded'

def prereqs(config):
    result = consts.TEST_PASSED
    cause = None

    if config.args.container:
        result = consts.TEST_SKIPPED
        cause = "This test cannot be run within a container"
        return result, cause

    if CgroupVersion.get_version(CONTROLLER) != CgroupVersion.CGROUP_V2:
        result = consts.TEST_SKIPPED
        cause = "This test requires cgroup v2"

    return result, cause

def setup(config):
    result = consts.TEST_PASSED
    cause = None

    Cgroup.create(config, CONTROLLER, CGNAME)


    before = Cgroup.get(config, controller=None, cgname=CGNAME,
                        setting=SETTING, print_headers=False,
                        values_only=True)
    if before != BEFORE:
        result = consts.TEST_SKIPPED
        cause = "Skipping test.  Unexpected value in {}: {}".format(SETTING,
                    before)

    return result, cause

def test(config):
    result = consts.TEST_PASSED
    cause = None

    Cgroup.set(config, CGNAME, SETTING, AFTER)

    after = Cgroup.get(config, controller=None, cgname=CGNAME,
                       setting=SETTING, print_headers=False,
                       values_only=True)

    if after != AFTER:
        result = consts.TEST_FAILED
        cause = "cgget expected {} but received {}".format(AFTER, after)

    return result, cause

def teardown(config):
    Cgroup.delete(config, CONTROLLER, CGNAME)

def main(config):
    [result, cause] = prereqs(config)
    if result != consts.TEST_PASSED:
        return [result, cause]

    setup(config)
    if result != consts.TEST_PASSED:
        return [result, cause]

    [result, cause] = test(config)
    teardown(config)

    return [result, cause]

if __name__ == '__main__':
    config = ftests.parse_args()
    # this test was invoked directly.  run only it
    config.args.num = int(os.path.basename(__file__).split('-')[0])
    sys.exit(ftests.main(config))
