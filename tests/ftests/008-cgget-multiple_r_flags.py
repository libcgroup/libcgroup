#!/usr/bin/env python3
#
# Advanced cgget functionality test - multiple '-r' flags
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

CONTROLLER = 'memory'
CGNAME = '008cgget'

SETTING1 = 'memory.limit_in_bytes'
VALUE1 = '1048576'

SETTING2='memory.soft_limit_in_bytes'
VALUE2 = '1024000'

def prereqs(config):
    result = consts.TEST_PASSED
    cause = None

    return result, cause

def setup(config):
    Cgroup.create(config, CONTROLLER, CGNAME)
    Cgroup.set(config, CGNAME, SETTING1, VALUE1)
    Cgroup.set(config, CGNAME, SETTING2, VALUE2)

def test(config):
    result = consts.TEST_PASSED
    cause = None

    out = Cgroup.get(config, controller=None, cgname=CGNAME,
                     setting=[SETTING1, SETTING2])

    if out.splitlines()[0] != "{}:".format(CGNAME):
        result = consts.TEST_FAILED
        cause = "cgget expected the cgroup name {} in the first line.\n" \
                "Instead it received {}".format(CGNAME, out.splitlines()[0])

    if out.splitlines()[1] != "{}: {}".format(SETTING1, VALUE1):
        result = consts.TEST_FAILED
        cause = "cgget expected the following:\n\t" \
                "{}: {}\nbut received:\n\t{}".format(
                SETTING1, VALUE1, out.splitlines()[1])

    if out.splitlines()[2] != "{}: {}".format(SETTING2, VALUE2):
        result = consts.TEST_FAILED
        cause = "cgget expected the following:\n\t" \
                "{}: {}\nbut received:\n\t{}".format(
                SETTING2, VALUE2, out.splitlines()[2])

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
