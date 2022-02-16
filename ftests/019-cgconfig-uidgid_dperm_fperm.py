#!/usr/bin/env python3
#
# cgconfigparser functionality test - '-a', '-d', '-f' flags
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

from container import ContainerError
from run import Run, RunError
from cgroup import Cgroup
import consts
import ftests
import utils
import sys
import os

CONTROLLER = 'cpuset'
CGNAME = '019cgconfig'

CONFIG_FILE = '''group
{} {{
    {} {{
    }}
}}'''.format(CGNAME, CONTROLLER)

USER = 'cguser019'
GROUP = 'cggroup019'
DPERM = '515'
FPERM = '246'

CONFIG_FILE_NAME = os.path.join(os.getcwd(), '019cgconfig.conf')


def prereqs(config):
    result = consts.TEST_PASSED
    cause = None

    return result, cause


def setup(config):
    f = open(CONFIG_FILE_NAME, 'w')
    f.write(CONFIG_FILE)
    f.close()

    if config.args.container:
        config.container.run(['useradd', '-p', 'Test019#1', USER])
        config.container.run(['groupadd', GROUP])
    else:
        Run.run(['sudo', 'useradd', '-p', 'Test019#1', USER])
        Run.run(['sudo', 'groupadd', GROUP])


def test(config):
    result = consts.TEST_PASSED
    cause = None

    Cgroup.configparser(config, load_file=CONFIG_FILE_NAME, dflt_usr=USER,
                        dflt_grp=GROUP, dperm=DPERM, fperm=FPERM)

    mnt_path = Cgroup.get_controller_mount_point(CONTROLLER)
    cpus_path = os.path.join(mnt_path, CGNAME, 'cpuset.cpus')

    user = utils.get_file_owner_username(config, cpus_path)
    group = utils.get_file_owner_group_name(config, cpus_path)

    if user != USER:
        result = consts.TEST_FAILED
        cause = (
                    'Owner name failed.  Expected {}, received {}\n'
                    ''.format(USER, user)
                )
        return result, cause

    if group != GROUP:
        result = consts.TEST_FAILED
        cause = (
                    'Owner group failed.  Expected {}, received {}\n'
                    ''.format(GROUP, group)
                )
        return result, cause

    fperm = utils.get_file_permissions(config, cpus_path)
    if fperm != FPERM:
        result = consts.TEST_FAILED
        cause = (
                    'File permissions failed.  Expected {}, received {}\n'
                    ''.format(FPERM, fperm)
                )
        return result, cause

    dperm = utils.get_file_permissions(config, os.path.join(mnt_path, CGNAME))
    if dperm != DPERM:
        result = consts.TEST_FAILED
        cause = (
                    'Directory permissions failed.  Expected {}, received {}\n'
                    ''.format(DPERM, dperm)
                )
        return result, cause

    return result, cause


def teardown(config):
    os.remove(CONFIG_FILE_NAME)

    try:
        if config.args.container:
            config.container.run(['userdel', USER])
            config.container.run(['groupdel', GROUP])
        else:
            Run.run(['sudo', 'userdel', USER])
            Run.run(['sudo', 'groupdel', GROUP])
    except (ContainerError, RunError, ValueError):
        pass

    Cgroup.delete(config, CONTROLLER, CGNAME)


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
