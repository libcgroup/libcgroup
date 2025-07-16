#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-only
#
# 'lssubsys -a' test
#
# Copyright (c) 2021 Oracle and/or its affiliates.
# Author: Tom Hromatka <tom.hromatka@oracle.com>
#

from distro import ConstsCommon as consts
from cgroup import Cgroup
import ftests
import utils
import sys
import os


def prereqs(config):
    pass


def setup(config):
    pass


def test(config):
    result = consts.TEST_PASSED
    cause = None

    mount_list = Cgroup.get_cgroup_mounts(config, expand_v2_mounts=True)

    # cgroup v2 mounts won't show up unless '-a' is specified
    lssubsys_list = Cgroup.lssubsys(config, ls_all=True)

    for mount in mount_list:
        if mount.controller == 'name=systemd' or mount.controller == 'systemd':
            continue

        found = False
        for lsmount in lssubsys_list.splitlines():
            if ',' in lsmount:
                for ctrl in lsmount.split(','):
                    if ctrl == mount.controller:
                        found = True
                        break

            if lsmount == mount.controller:
                found = True
                break

            if lsmount == 'blkio' and mount.controller == 'io':
                found = True
                break

        if not found and (mount.controller == 'cpuset' or
                          mount.controller == 'memory'):
            kernel_ver = utils.get_kernel_version(config)
            if int(kernel_ver[0]) >= 6 and int(kernel_ver[1]) >= 12:
                # Starting 6.12 cpuset and memory split into v1 and v2,
                # where v1 is compiled only when CONFIG_CPUSET_V1 and
                # CONFIG_MEMCG_v1 is enabled respectively.
                found = True

        if not found:
            result = consts.TEST_FAILED
            cause = (
                        'Failed to find {} in lssubsys list'
                        ''.format(mount.controller)
                    )
            return result, cause

    ret = Cgroup.lssubsys(config, cghelp=True)
    if 'Usage:' not in ret:
        result = consts.TEST_FAILED
        cause = 'Failed to print help text'

    return result, cause


def teardown(config):
    pass


def main(config):
    prereqs(config)

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

# vim: set et ts=4 sw=4:
