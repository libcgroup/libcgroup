#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-only
#
# get the library version using the python bindings
#
# Copyright (c) 2021-2022 Oracle and/or its affiliates.
# Author: Tom Hromatka <tom.hromatka@oracle.com>
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
