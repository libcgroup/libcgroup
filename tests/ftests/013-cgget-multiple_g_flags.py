#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-only
#
# Advanced cgget functionality test - multiple '-g' flags
#
# Copyright (c) 2021 Oracle and/or its affiliates.
# Author: Tom Hromatka <tom.hromatka@oracle.com>
#

from distro import ConstsCommon as consts
from cgroup import Cgroup, CgroupVersion
from distro import consts_distro
import ftests
import utils
import sys
import os

CONTROLLER1 = 'pids'
CONTROLLER2 = 'cpu'
CGNAME = '013cgget'
OUT_PREFIX = '013cgget:\n'


def prereqs(config):
    pass


def setup(config):
    Cgroup.create(config, CONTROLLER1, CGNAME)
    Cgroup.create(config, CONTROLLER2, CGNAME)


def test(config):
    result = consts.TEST_FAILED
    cause = None

    out = Cgroup.get(config, controller=[CONTROLLER1, CONTROLLER2],
                     cgname=CGNAME, print_headers=False)

    EXPECTED_OUT = consts_distro.consts.expected_cpu_out_013()

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
    ver1 = CgroupVersion.get_version(CONTROLLER1)
    ver2 = CgroupVersion.get_version(CONTROLLER2)

    if ver1 == CgroupVersion.CGROUP_V2 and \
       ver2 == CgroupVersion.CGROUP_V2:
        # If both controllers are cgroup v2, then we only need to delete
        # one cgroup.  The path will be the same for both
        Cgroup.delete(config, [CONTROLLER1, CONTROLLER2], CGNAME)
    else:
        Cgroup.delete(config, CONTROLLER1, CGNAME)
        Cgroup.delete(config, CONTROLLER2, CGNAME)


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
