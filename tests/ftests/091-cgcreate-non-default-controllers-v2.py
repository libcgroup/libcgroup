#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-only
#
# cgcreate non default controller functionality test
#
# Copyright (c) 2023 Oracle and/or its affiliates.
# Author: Kamalesh Babulal <kamalesh.babulal@oracle.com>
#

from distro import ConstsCommon as consts
from cgroup import Cgroup, Mode
import ftests
import sys
import os

CONTROLLERS = ['hugetlb', 'misc']

CGNAME = '090cgcreate'
CGNAME1 = os.path.join(CGNAME, 'childcg')
CGNAME2 = os.path.join(CGNAME1, 'grandchildcg')


def prereqs(config):
    result = consts.TEST_PASSED
    cause = None

    if Cgroup.get_cgroup_mode(config) != Mode.CGROUP_MODE_UNIFIED:
        result = consts.TEST_SKIPPED
        cause = 'This test requires the unified cgroup v2 hierarchy'

    return result, cause


def setup(config):
    Cgroup.create(config, CONTROLLERS, CGNAME2)


def test(config):
    result = consts.TEST_PASSED
    cause = None

    # checking if the controller is enabled in granchildcg, in turn will
    # check its parent cgroup childcg's subtree_control file, if it's enabled
    # in parent cgroup, it's also enabled in the grandparent too.
    if not Cgroup.is_controller_enabled(config, CGNAME2, CONTROLLERS[0]):
        result = consts.TEST_FAILED
        cause = 'Controller {} is not enabled in the child cgroup'.format(CONTROLLERS[0])

    if not Cgroup.is_controller_enabled(config, CGNAME2, CONTROLLERS[1]):
        result = consts.TEST_FAILED
        tmp_cause = 'Controller {} is not enabled in the child cgroup'.format(CONTROLLERS[1])
        cause = '\n'.join(filter(None, [cause, tmp_cause]))

    # for the grandchildcg read it cgroup.subtree_control, is_controller_enabled
    # will check its parent cgroup childcg's subtree_control
    if Cgroup.get(config, None, CGNAME2, setting='cgroup.subtree_control',
                  print_headers=False, values_only=True):
        result = consts.TEST_FAILED
        tmp_cause = 'Controller {} enabled in grandchild cgroup'.format(CONTROLLERS[0])
        cause = '\n'.join(filter(None, [cause, tmp_cause]))

    return result, cause


def teardown(config):
    Cgroup.delete(config, CONTROLLERS, CGNAME, recursive=True)


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
