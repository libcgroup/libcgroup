#!/usr/bin/env python3
#
# cgconfigparser functionality test - invalid and help options
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
from run import RunError
import sys

CONTROLLER = 'cpuset'
CGNAME = '021cgconfig'

CONFIG_FILE = '''group
{} {{
    {} {{
	cpuset.cpus = abc123;
    }}
}}'''.format(CGNAME, CONTROLLER)

USER = 'cguser021'

CONFIG_FILE_NAME = os.path.join(os.getcwd(), '021cgconfig.conf')

def prereqs(config):
    result = consts.TEST_PASSED
    cause = None

    return result, cause

def setup(config):
    f = open(CONFIG_FILE_NAME, 'w')
    f.write(CONFIG_FILE)
    f.close()

def test(config):
    result = consts.TEST_PASSED
    cause = None

    ret = Cgroup.configparser(config, cghelp=True)
    if not "Parse and load the specified cgroups" in ret:
        result = consts.TEST_FAILED
        cause = "Failed to print cgconfigparser help text"
        return result, cause

    try:
        Cgroup.configparser(config, load_file=CONFIG_FILE_NAME)
    except RunError as re:
        if not "Invalid argument" in re.stderr:
            result = consts.TEST_FAILED
            cause = "Expected 'Invalid argument' to be in stderr"
            return result, cause

        if re.ret != 96:
            result = consts.TEST_FAILED
            cause = "Expected return code of 96 but received {}".format(re.ret)
            return result, cause
    else:
        result = consts.TEST_FAILED
        cause = "Test case erroneously passed"
        return result, cause

    return result, cause

def teardown(config):
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

# vim: set et ts=4 sw=4:
