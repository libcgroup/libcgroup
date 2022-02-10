#!/usr/bin/env python3
#
# Cgxget test with no mappable settings
#
# Copyright (c) 2022 Oracle and/or its affiliates.
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
CGNAME = '042cgxget'
SETTING = 'cpu.stat'

def prereqs(config):
    result = consts.TEST_PASSED
    cause = None

    return result, cause

def setup(config):
    Cgroup.create(config, CONTROLLER, CGNAME)

def test(config):
    result = consts.TEST_PASSED
    cause = None

    if CgroupVersion.get_version(CONTROLLER) == CgroupVersion.CGROUP_V1:
        # request the opposite version of what this system is running
        requested_ver = CgroupVersion.CGROUP_V2
    else:
        requested_ver = CgroupVersion.CGROUP_V1

    out = Cgroup.xget(config, cgname=CGNAME, setting=SETTING,
                      version=requested_ver, print_headers=False,
                      ignore_unmappable=True)
    if len(out):
        result = consts.TEST_FAILED
        cause = "Expected cgxget to return nothing.  Received {}".format(out)

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
