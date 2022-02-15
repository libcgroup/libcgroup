#!/usr/bin/env python3
#
# get the library version using the python bindings
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

from libcgroup import Cgroup
import consts
import ftests
import sys
import os


def prereqs(config):
    return consts.TEST_PASSED, None


def setup(config):
    return consts.TEST_PASSED, None


def test(config):
    result = consts.TEST_PASSED
    cause = None

    [major, minor, release] = Cgroup.library_version()

    if not isinstance(major, int):
        result = consts.TEST_FAILED
        cause = 'Major version failed. Received {}'.format(major)
        return result, cause

    if not isinstance(minor, int):
        result = consts.TEST_FAILED
        cause = 'Minor version failed. Received {}'.format(minor)
        return result, cause

    if not isinstance(release, int):
        result = consts.TEST_FAILED
        cause = 'Release version failed. Received {}'.format(release)
        return result, cause

    return result, cause


def teardown(config):
    pass


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
