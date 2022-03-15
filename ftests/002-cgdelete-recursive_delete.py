#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-only
#
# Cgroup recursive cgdelete functionality test
#
# Copyright (c) 2020-2021 Oracle and/or its affiliates.
# Author: Tom Hromatka <tom.hromatka@oracle.com>
#

from cgroup import Cgroup, CgroupVersion
import consts
import ftests
import sys
import os

CONTROLLER = 'cpuset'
PARENT = '002cgdelete'
CHILD = 'childcg'
GRANDCHILD = 'grandchildcg'


def prereqs(config):
    # This test should run on both cgroup v1 and v2
    return consts.TEST_PASSED, None


def setup(config):
    Cgroup.create(config, CONTROLLER, PARENT)
    Cgroup.create(config, CONTROLLER, os.path.join(PARENT, CHILD))
    Cgroup.create(config, CONTROLLER, os.path.join(PARENT, CHILD, GRANDCHILD))

    version = CgroupVersion.get_version(CONTROLLER)
    if version == CgroupVersion.CGROUP_V1:
        # cgdelete in a cgroup v1 controller should be able to move a process
        # from a child cgroup to its parent.
        #
        # Moving a process from a child cgroup to its parent isn't (easily)
        # supported in cgroup v2 because of cgroup v2's restriction that
        # processes only be located in leaf cgroups
        grandchild_cgrp_path = os.path.join(PARENT, CHILD, GRANDCHILD)
        config.process.create_process_in_cgroup(config, CONTROLLER,
                                                grandchild_cgrp_path)


def test(config):
    Cgroup.delete(config, CONTROLLER, PARENT, recursive=True)

    return consts.TEST_PASSED, None


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
