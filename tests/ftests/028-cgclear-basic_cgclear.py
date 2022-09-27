#!/usr/bin/env python3
#
# Basic cgclear functionality test
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
import os
from process import Process
from run import Run
import sys

def prereqs(config):
    result = consts.TEST_PASSED
    cause = None

    if config.args.container:
        result = consts.TEST_SKIPPED
        cause = "This test is highly destructive to the cgroup hierarchy and\n" \
                "does not have the necessary permissions within a container."
        return result, cause

    return result, cause

def setup(config):
    pass

def test(config):
    result = consts.TEST_PASSED
    cause = None

    ret = Cgroup.clear(config, cghelp=True)
    if not "Usage:" in ret:
        result = consts.TEST_FAILED
        cause = "Failed to print help text"
        return result, cause

    before = Run.run('mount | wc -l', shell_bool=True)
    Cgroup.clear(config)
    after = Run.run('mount | wc -l', shell_bool=True)

    if after >= before:
        result = consts.TEST_FAILED
        cause = "Cgroups were not unmounted.\n" \
                "Before count {}, after count {}".format(before, after)
        return result, cause

    return result, cause

def teardown(config):
    pass

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
