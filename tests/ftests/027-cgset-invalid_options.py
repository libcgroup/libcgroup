#!/usr/bin/env python3
#
# Advanced cgset functionality test - invalid options
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

CONTROLLER = 'cpu'
CGNAME1 = '027cgset1'
CGNAME2 = '027cgset2'

def prereqs(config):
    result = consts.TEST_PASSED
    cause = None

    return result, cause

def setup(config):
    Cgroup.create(config, CONTROLLER, CGNAME1)
    Cgroup.create(config, CONTROLLER, CGNAME2)

def test(config):
    result = consts.TEST_PASSED
    cause = None

    try:
        # cgset -r cpu.shares=100 --copy-from 027cgset2 027cgset1
        Cgroup.set(config, cgname=CGNAME1, setting="cpu.shares", value="100",
                   copy_from=CGNAME2)
    except RunError as re:
        if not "Wrong input parameters," in re.stderr:
            result = consts.TEST_FAILED
            cause = "#1 Expected 'Wrong input parameters' to be in stderr"
            return result, cause

        if re.ret != 255:
            result = consts.TEST_FAILED
            cause = "#1 Expected return code of 255 but received {}".format(re.ret)
            return result, cause
    else:
        result = consts.TEST_FAILED
        cause = "Test case #1 erroneously passed"
        return result, cause

    try:
        # cgset -r cpu.shares=100
        Cgroup.set(config, cgname=None, setting="cpu.shares", value="100")
    except RunError as re:
        if not "cgset: no cgroup specified" in re.stderr:
            result = consts.TEST_FAILED
            cause = "#2 Expected 'no cgroup specified' to be in stderr"
            return result, cause

        if re.ret != 255:
            result = consts.TEST_FAILED
            cause = "#2 Expected return code of 255 but received {}".format(re.ret)
            return result, cause
    else:
        result = consts.TEST_FAILED
        cause = "Test case #2 erroneously passed"
        return result, cause

    try:
        # cgset 027cgset1
        Cgroup.set(config, cgname=CGNAME1)
    except RunError as re:
        if not "cgset: no name-value pair was set" in re.stderr:
            result = consts.TEST_FAILED
            cause = "#3 Expected 'no name-value pair' to be in stderr"
            return result, cause

        if re.ret != 255:
            result = consts.TEST_FAILED
            cause = "#3 Expected return code of 255 but received {}".format(re.ret)
            return result, cause
    else:
        result = consts.TEST_FAILED
        cause = "Test case #3 erroneously passed"
        return result, cause

    try:
        # cgset - no flags provided
        Cgroup.set(config)
    except RunError as re:
        if not "Usage is" in re.stderr:
            result = consts.TEST_FAILED
            cause = "#4 Expected 'Usage is' to be in stderr"
            return result, cause

        if re.ret != 255:
            result = consts.TEST_FAILED
            cause = "#4 Expected return code of 255 but received {}".format(re.ret)
            return result, cause
    else:
        result = consts.TEST_FAILED
        cause = "Test case #4 erroneously passed"
        return result, cause

    try:
        # cgset -r cpu.shares= 027cgset1
        Cgroup.set(config, cgname=CGNAME1, setting="cpu.shares", value="")
    except RunError as re:
        if not "wrong parameter of option -r" in re.stderr:
            result = consts.TEST_FAILED
            cause = "#5 Expected 'Wrong parameter of option' to be in stderr"
            return result, cause

        if re.ret != 255:
            result = consts.TEST_FAILED
            cause = "#5 Expected return code of 255 but received {}".format(re.ret)
            return result, cause
    else:
        result = consts.TEST_FAILED
        cause = "Test case #5 erroneously passed"
        return result, cause

    # cgset -h
    ret = Cgroup.set(config, cghelp=True)
    if not "Usage:" in ret:
        result = consts.TEST_FAILED
        cause = "#6 Failed to print help text"
        return result, cause

    return result, cause

def teardown(config):
    Cgroup.delete(config, CONTROLLER, CGNAME1)
    Cgroup.delete(config, CONTROLLER, CGNAME2)

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
