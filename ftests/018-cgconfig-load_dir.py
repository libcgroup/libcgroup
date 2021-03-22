#!/usr/bin/env python3
#
# cgconfigparser functionality test using a configuration directory
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

CPU_CTRL = 'cpu'
MEMORY_CTRL = 'memory'
CGNAME = '018cgconfig'
CFS_PERIOD = '400000'
CFS_QUOTA = '50000'
SHARES = '123'

LIMIT_IN_BYTES = '409600'
SOFT_LIMIT_IN_BYTES = '376832'

CONFIG_FILE = '''group
{} {{
    {} {{
        cpu.cfs_period_us = {};
        cpu.cfs_quota_us = {};
        cpu.shares = {};
    }}
    {} {{
        memory.limit_in_bytes = {};
        memory.soft_limit_in_bytes = {};
    }}
}}'''.format(CGNAME, CPU_CTRL, CFS_PERIOD, CFS_QUOTA, SHARES,
             MEMORY_CTRL, LIMIT_IN_BYTES, SOFT_LIMIT_IN_BYTES)

CONFIG_FILE_DIR = os.path.join(os.getcwd(), '018cgconfig')
CONFIG_FILE_NAME = os.path.join(CONFIG_FILE_DIR, 'cgconfig.conf')

def prereqs(config):
    result = consts.TEST_PASSED
    cause = None

    if CgroupVersion.get_version('cpu') != CgroupVersion.CGROUP_V1:
        result = consts.TEST_SKIPPED
        cause = "This test requires the cgroup v1 cpu controller"
        return result, cause

    if CgroupVersion.get_version('memory') != CgroupVersion.CGROUP_V1:
        result = consts.TEST_SKIPPED
        cause = "This test requires the cgroup v1 memory controller"
        return result, cause

    return result, cause

def setup(config):
    os.mkdir(CONFIG_FILE_DIR)

    f = open(CONFIG_FILE_NAME, 'w')
    f.write(CONFIG_FILE)
    f.close()

def test(config):
    result = consts.TEST_PASSED
    cause = None

    Cgroup.configparser(config, load_dir=CONFIG_FILE_DIR)

    period = Cgroup.get(config, cgname=CGNAME, setting='cpu.cfs_period_us',
                        print_headers=False, values_only=True)
    if period != CFS_PERIOD:
            result = consts.TEST_FAILED
            cause = "cfs_period_us failed.  Expected {}, Received {}".format(
                    CFS_PERIOD, period)
            return result, cause

    quota = Cgroup.get(config, cgname=CGNAME, setting='cpu.cfs_quota_us',
                       print_headers=False, values_only=True)
    if quota != CFS_QUOTA:
            result = consts.TEST_FAILED
            cause = "cfs_quota_us failed.  Expected {}, Received {}".format(
                    CFS_QUOTA, quota)
            return result, cause

    shares = Cgroup.get(config, cgname=CGNAME, setting='cpu.shares',
                        print_headers=False, values_only=True)
    if shares != SHARES:
            result = consts.TEST_FAILED
            cause = "shares failed.  Expected {}, Received {}".format(
                    SHARES, shares)
            return result, cause

    limit = Cgroup.get(config, cgname=CGNAME, setting='memory.limit_in_bytes',
                       print_headers=False, values_only=True)
    if limit != LIMIT_IN_BYTES:
            result = consts.TEST_FAILED
            cause = "limit_in_bytes failed.  Expected {}, Received {}".format(
                    LIMIT_IN_BYTES, limit)
            return result, cause

    soft_limit = Cgroup.get(config, cgname=CGNAME,
                            setting='memory.soft_limit_in_bytes',
                            print_headers=False, values_only=True)
    if soft_limit != SOFT_LIMIT_IN_BYTES:
            result = consts.TEST_FAILED
            cause = "soft_limit_in_bytes failed.  Expected {}, Received {}".format(
                    SOFT_LIMIT_IN_BYTES, soft_limit)
            return result, cause

    return result, cause

def teardown(config):
    Cgroup.delete(config, CPU_CTRL, CGNAME)
    Cgroup.delete(config, MEMORY_CTRL, CGNAME)
    os.remove(CONFIG_FILE_NAME)
    os.rmdir(CONFIG_FILE_DIR)

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
