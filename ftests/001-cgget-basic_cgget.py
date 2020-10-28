#!/usr/bin/env python3
#
# Basic cgget functionality test
#
# Copyright (c) 2019 Oracle and/or its affiliates.  All rights reserved.
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

from cgroup import Cgroup
import consts
import ftests
import os
import sys

CONTROLLER='cpu'
CGNAME="001cgget"

SETTING='cpu.shares'
VALUE='512'

def setup(config):
    Cgroup.create(config, CONTROLLER, CGNAME)
    Cgroup.set(config, CGNAME, SETTING, VALUE)

def test(config):
    result = consts.TEST_PASSED
    cause = None

    value = Cgroup.get(config, controller=None, cgname=CGNAME,
                       setting=SETTING, print_headers=False,
                       values_only=True)

    if value != VALUE:
        result = consts.TEST_FAILED
        cause = "cgget expected {} but received {}".format(VALUE, value)

    return result, cause

def teardown(config):
    Cgroup.delete(config, CONTROLLER, CGNAME)

def main(config):
    setup(config)
    [result, cause] = test(config)
    teardown(config)

    return [result, cause]

if __name__ == '__main__':
    config = ftests.parse_args()
    # this test was invoked directly.  run only it
    config.args.num = int(os.path.basename(__file__).split('-')[0])
    sys.exit(ftests.main(config))
