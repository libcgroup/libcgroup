#!/usr/bin/env python3
#
# Advanced cgget functionality test - '-g' <controller>:<path>
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

CONTROLLER = 'cpu'
CGNAME = '010cgget'

EXPECTED_OUT = '''cpu.cfs_period_us: 100000
cpu.stat: nr_periods 0
        nr_throttled 0
        throttled_time 0
cpu.shares: 1024
cpu.cfs_quota_us: -1
cpu.uclamp.min: 0.00
cpu.uclamp.max: max
'''

def prereqs(config):
    result = consts.TEST_PASSED
    cause = None

    return result, cause

def setup(config):
    Cgroup.create(config, CONTROLLER, CGNAME)

def test(config):
    result = consts.TEST_PASSED
    cause = None

    out = Cgroup.get(config, controller="{}:{}".format(CONTROLLER, CGNAME),
                     print_headers=False)

    for line_num, line in enumerate(out.splitlines()):
        if line.strip() != EXPECTED_OUT.splitlines()[line_num].strip():
            result = consts.TEST_FAILED
            cause = "Expected line:\n\t{}\nbut received line:\n\t{}".format(
                    EXPECTED_OUT.splitlines()[line_num].strip(), line.strip())
            return result, cause

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
