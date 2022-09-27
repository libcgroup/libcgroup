#!/usr/bin/env python3
#
# cgconfigparser functionality test using a configuration file
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
CGNAME = '017cgconfig'
CFS_PERIOD = '500000'
CFS_QUOTA = '100000'
SHARES = '999'

CONFIG_FILE = '''group
{} {{
    {} {{
        cpu.cfs_period_us = {};
        cpu.cfs_quota_us = {};
        cpu.shares = {};
    }}
}}'''.format(CGNAME, CONTROLLER, CFS_PERIOD, CFS_QUOTA, SHARES)

CONFIG_FILE_NAME = os.path.join(os.getcwd(), '017cgconfig.conf')

def prereqs(config):
    result = consts.TEST_PASSED
    cause = None

    if CgroupVersion.get_version('cpu') != CgroupVersion.CGROUP_V1:
        result = consts.TEST_SKIPPED
        cause = "This test requires the cgroup v1 cpu controller"
        return result, cause

    return result, cause

def setup(config):
    f = open(CONFIG_FILE_NAME, 'w')
    f.write(CONFIG_FILE)
    f.close()

def test(config):
    result = consts.TEST_PASSED
    cause = None

    Cgroup.configparser(config, load_file=CONFIG_FILE_NAME)

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

    return result, cause

def teardown(config):
    Cgroup.delete(config, CONTROLLER, CGNAME)
    os.remove(CONFIG_FILE_NAME)

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
