#!/usr/bin/env python3
#
# Advanced cgget functionality test - get a multiline value via the '-r' flag
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

from cgroup import Cgroup
import consts
import ftests
import sys
import os

CONTROLLER = 'memory'
CGNAME = '015cgget'

SETTING = 'memory.stat'
VALUE = '512'


def prereqs(config):
    result = consts.TEST_PASSED
    cause = None

    return result, cause


def setup(config):
    Cgroup.create(config, CONTROLLER, CGNAME)


def test(config):
    result = consts.TEST_PASSED
    cause = None

    out = Cgroup.get(config, controller=None, cgname=CGNAME,
                     setting=SETTING, print_headers=True,
                     values_only=False)

    # arbitrary check to ensure we read several lines
    if len(out.splitlines()) < 10:
        result = consts.TEST_FAILED
        cause = (
                    'Expected multiple lines, but only received {}'
                    ''.format(len(out.splitlines()))
                )
        return result, cause

    # arbitrary check for a setting that's in both cgroup v1 and cgroup v2
    # memory.stat
    if '\tunevictable' not in out:
        result = consts.TEST_FAILED
        cause = 'Unexpected output\n{}'.format(out)
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
