#!/usr/bin/env python3
#
# cgxget functionality test - various cpu settings
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

from cgroup import Cgroup, CgroupVersion
import consts
import ftests
import os
import sys

CONTROLLER = 'cpu'
CGNAME = '037cgxget'

TABLE = [
    # writesetting, writeval, writever, readsetting, readval, readver
    ['cpu.shares', '512', CgroupVersion.CGROUP_V1, 'cpu.shares', '512', CgroupVersion.CGROUP_V1],
    ['cpu.shares', '512', CgroupVersion.CGROUP_V1, 'cpu.weight', '50', CgroupVersion.CGROUP_V2],

    ['cpu.weight', '200', CgroupVersion.CGROUP_V2, 'cpu.shares', '2048', CgroupVersion.CGROUP_V1],
    ['cpu.weight', '200', CgroupVersion.CGROUP_V2, 'cpu.weight', '200', CgroupVersion.CGROUP_V2],

    ['cpu.cfs_quota_us', '10000', CgroupVersion.CGROUP_V1, 'cpu.cfs_quota_us', '10000', CgroupVersion.CGROUP_V1],
    ['cpu.cfs_period_us', '100000', CgroupVersion.CGROUP_V1, 'cpu.cfs_period_us', '100000', CgroupVersion.CGROUP_V1],
    ['cpu.cfs_period_us', '50000', CgroupVersion.CGROUP_V1, 'cpu.max', '10000 50000', CgroupVersion.CGROUP_V2],

    ['cpu.cfs_quota_us', '-1', CgroupVersion.CGROUP_V1, 'cpu.cfs_quota_us', '-1', CgroupVersion.CGROUP_V1],
    ['cpu.cfs_period_us', '100000', CgroupVersion.CGROUP_V1, 'cpu.max', 'max 100000', CgroupVersion.CGROUP_V2],

    ['cpu.max', '5000 25000', CgroupVersion.CGROUP_V2, 'cpu.max', '5000 25000', CgroupVersion.CGROUP_V2],
    ['cpu.max', '6000 26000', CgroupVersion.CGROUP_V2, 'cpu.cfs_quota_us', '6000', CgroupVersion.CGROUP_V1],
    ['cpu.max', '7000 27000', CgroupVersion.CGROUP_V2, 'cpu.cfs_period_us', '27000', CgroupVersion.CGROUP_V1],

    ['cpu.max', 'max 40000', CgroupVersion.CGROUP_V2, 'cpu.max', 'max 40000', CgroupVersion.CGROUP_V2],
    ['cpu.max', 'max 41000', CgroupVersion.CGROUP_V2, 'cpu.cfs_quota_us', '-1', CgroupVersion.CGROUP_V1],
]

def prereqs(config):
    result = consts.TEST_PASSED
    cause = None

    return result, cause

def setup(config):
    Cgroup.create(config, CONTROLLER, CGNAME)

def test(config):
    result = consts.TEST_PASSED
    cause = None

    for entry in TABLE:
        Cgroup.xset(config, cgname=CGNAME, setting=entry[0], value=entry[1],
                    version=entry[2])

        out = Cgroup.xget(config, cgname=CGNAME, setting=entry[3],
                          version=entry[5], values_only=True,
                          print_headers=False)
        if out != entry[4]:
            result = consts.TEST_FAILED
            cause = "After setting {}={}, expected {}={}, but received {}={}".format(
                    entry[0], entry[1], entry[3], entry[4], entry[3], out)
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

# vim: set et ts=4 sw=4:
