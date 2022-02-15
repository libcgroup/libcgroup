#!/usr/bin/env python3
#
# lscgroup functionality test
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
import utils
import sys
import os

CONTROLLER = 'cpuset'
PARENT_CGNAME = '031lscgroup'
CHILD_CGNAME = 'childlscgroup'
GRANDCHILD_CGNAME = 'grandchildlscgroup'

# lscgroup is inconsistent in its handling of trailing slashes
#
# When invoking lscgroup with no flags, no trailing slashes are present in
# any of the cgroups.
#
# When invoking lscgroup with the -g flag, a trailing slash is present on
# the first cgroup returned (i.e. the cgroup specified in the -g flag)
#
EXPECTED_OUT1 = '''{}:/{}/
{}:/{}/{}
{}:/{}/{}/{}'''.format(CONTROLLER, PARENT_CGNAME,
                       CONTROLLER, PARENT_CGNAME, CHILD_CGNAME,
                       CONTROLLER, PARENT_CGNAME, CHILD_CGNAME,
                       GRANDCHILD_CGNAME)


def prereqs(config):
    result = consts.TEST_PASSED
    cause = None

    v2_cnt = 0
    mount_list = Cgroup.get_cgroup_mounts(config)

    for mount in mount_list:
        if mount.version == CgroupVersion.CGROUP_V2:
            v2_cnt += 1

    if v2_cnt > 1:
        # There is a bug in lscgroup - see issue #50 - where it doesn't
        # properly list the enabled controllers for a cgroup v2 cgroup.
        # Skip this test because of this
        result = consts.TEST_SKIPPED
        cause = 'See Github Issue #50 - lscgroup lists controllers...'
        return result, cause

    return result, cause


def setup(config):
    Cgroup.create(config, CONTROLLER, PARENT_CGNAME)
    Cgroup.create(
                    config, CONTROLLER, os.path.join(
                        PARENT_CGNAME, CHILD_CGNAME)
                 )
    Cgroup.create(config, CONTROLLER,
                  os.path.join(PARENT_CGNAME, CHILD_CGNAME, GRANDCHILD_CGNAME))


def test(config):
    result = consts.TEST_PASSED
    cause = None

    out = Cgroup.lscgroup(config, controller=CONTROLLER, path=PARENT_CGNAME)
    if out != EXPECTED_OUT1:
        result = consts.TEST_FAILED
        cause = (
                    "Expected lscgroup output doesn't match received output\n'"
                    "Expected:\n{}\n"
                    "Received:\n{}\n"
                    "".format(utils.indent(EXPECTED_OUT1, 4),
                              utils.indent(out, 4))
                )
        return result, cause

    return result, cause


def teardown(config):
    Cgroup.delete(config, CONTROLLER, PARENT_CGNAME, recursive=True)


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
