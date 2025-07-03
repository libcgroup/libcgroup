#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-only
#
# Advanced cgget functionality test - '-g' <controller>
#
# Copyright (c) 2021 Oracle and/or its affiliates.
# Author: Tom Hromatka <tom.hromatka@oracle.com>
#

from cgroup import Cgroup, CgroupVersion
import consts
import ftests
import utils
import sys
import os

CONTROLLER = 'cpu'
CGNAME = '009cgget'
OUT_PREFIX = '009cgget:\n'


def prereqs(config):
    pass


def setup(config):
    Cgroup.create(config, CONTROLLER, CGNAME)


def test(config):
    result = consts.TEST_FAILED
    cause = None

    out = Cgroup.get(config, controller=CONTROLLER, cgname=CGNAME)
    version = CgroupVersion.get_version(CONTROLLER)

    if version == CgroupVersion.CGROUP_V1:
        EXPECTED_OUT = [OUT_PREFIX + expected_out for expected_out in consts.EXPECTED_CPU_OUT_V1]
    else:
        EXPECTED_OUT = [OUT_PREFIX + expected_out for expected_out in consts.EXPECTED_CPU_OUT_V2]

    for expected_out in EXPECTED_OUT:
        if len(out.splitlines()) == len(expected_out.splitlines()):
            result_, tmp_cause = utils.is_output_same(config, out, expected_out)
            if result_ is True:
                result = consts.TEST_PASSED
                cause = None
                break
            else:
                if cause is None:
                    cause = 'Tried Matching:\n==============='

                cause = '\n'.join(filter(None, [cause, expected_out]))

    return result, cause


def teardown(config):
    Cgroup.delete(config, CONTROLLER, CGNAME)


def main(config):
    prereqs(config)
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
