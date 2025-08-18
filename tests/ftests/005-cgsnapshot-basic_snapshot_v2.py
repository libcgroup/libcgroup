#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-only
#
# Basic cgsnapshot functionality test
#
# Copyright (c) 2020 Oracle and/or its affiliates.
# Author: Tom Hromatka <tom.hromatka@oracle.com>
#

from cgroup import Cgroup, Mode
import consts
import ftests
import sys
import os

CONTROLLER = 'cpuset'
CGNAME = '005cgsnapshot'
CGSNAPSHOT = [
    """group 005cgsnapshot {
            cpuset {
                    cpuset.cpus.partition="member";
                    cpuset.mems="";
                    cpuset.cpus="";
            }
    }""",
    """group 005cgsnapshot {
            cpuset {
                    cpuset.cpus.exclusive="";
                    cpuset.cpus.partition="member";
                    cpuset.mems="";
                    cpuset.cpus="";
            }
    }""",
    """group 005cgsnapshot {
            cpuset {
                    cpuset.cpus.exclusive.effective="";
                    cpuset.cpus.exclusive="";
                    cpuset.cpus.partition="member";
                    cpuset.mems="";
                    cpuset.cpus="";
            }
    }"""
]


def prereqs(config):
    result = consts.TEST_PASSED
    cause = None

    if Cgroup.get_cgroup_mode(config) != Mode.CGROUP_MODE_UNIFIED:
        result = consts.TEST_SKIPPED
        cause = 'This test requires the unified cgroup hierarchy'

    return result, cause


def setup(config):
    Cgroup.create(config, CONTROLLER, CGNAME)


def test(config):
    result = consts.TEST_PASSED
    cause = None

    expected_1 = Cgroup.snapshot_to_dict(CGSNAPSHOT[0])
    expected_2 = Cgroup.snapshot_to_dict(CGSNAPSHOT[1])
    expected_sudo = Cgroup.snapshot_to_dict(CGSNAPSHOT[2])
    actual = Cgroup.snapshot(config, controller=CONTROLLER)

    if (
            expected_1[CGNAME].controllers[CONTROLLER] !=
            actual[CGNAME].controllers[CONTROLLER] and
            expected_2[CGNAME].controllers[CONTROLLER] !=
            actual[CGNAME].controllers[CONTROLLER] and
            expected_sudo[CGNAME].controllers[CONTROLLER] !=
            actual[CGNAME].controllers[CONTROLLER]
       ):
        result = consts.TEST_FAILED
        cause = 'Expected cgsnapshot result did not equal actual cgsnapshot'

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
