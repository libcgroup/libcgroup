#!/usr/bin/env python3
#
# Basic lssubsys functionality test
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

from cgroup import Cgroup, CgroupVersion
import consts
import ftests
import os
import sys
import utils

def prereqs(config):
    result = consts.TEST_PASSED
    cause = None

    return result, cause

def setup(config):
    pass

def test(config):
    result = consts.TEST_PASSED
    cause = None

    mount_list = Cgroup.get_cgroup_mounts(config, expand_v2_mounts=False)

    # cgroup v2 mounts won't show up unless '-a' is specified
    lssubsys_list = Cgroup.lssubsys(config, ls_all=False)

    for mount in mount_list:
        if mount.version == CgroupVersion.CGROUP_V2:
            continue

        if mount.controller == "name=systemd" or mount.controller == "systemd":
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

        if not found:
            result = consts.TEST_FAILED
            cause = "Failed to find {} in lssubsys list".format(
                      mount.controller)
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

# vim: set et ts=4 sw=4:
