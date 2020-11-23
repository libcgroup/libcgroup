#
# Cgroup class for the libcgroup functional tests
#
# Copyright (c) 2019 Oracle and/or its affiliates.  All rights reserved.
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

import consts
from enum import Enum
import os
from run import Run

class Cgroup(Enum):
    CGROUP_UNK = 0
    CGROUP_V1 = 1
    CGROUP_V2 = 2

    @staticmethod
    def build_cmd_path(in_container, cmd):
        if in_container:
            return os.path.join(consts.LIBCG_MOUNT_POINT,
                                'src/tools/{}'.format(cmd))
        else:
            return cmd

    # TODO - add support for all of the cgcreate options
    @staticmethod
    def create(config, controller_list, cgname, in_container=True):
        if isinstance(controller_list, str):
            controller_list = [controller_list]

        cmd = list()
        cmd.append(Cgroup.build_cmd_path(in_container, 'cgcreate'))

        controllers_and_path = '{}:{}'.format(
            ','.join(controller_list), cgname)

        cmd.append('-g')
        cmd.append(controllers_and_path)

        if in_container:
            config.container.run(cmd)
        else:
            Run.run(cmd)

    @staticmethod
    def delete(config, controller_list, cgname, in_container=True, recursive=False):
        if isinstance(controller_list, str):
            controller_list = [controller_list]

        cmd = list()
        cmd.append(Cgroup.build_cmd_path(in_container, 'cgdelete'))

        if recursive:
            cmd.append('-r')

        controllers_and_path = '{}:{}'.format(
            ''.join(controller_list), cgname)

        cmd.append('-g')
        cmd.append(controllers_and_path)

        if in_container:
            config.container.run(cmd)
        else:
            Run.run(cmd)

    @staticmethod
    def set(config, cgname, setting, value, in_container=True):
        cmd = list()
        cmd.append(Cgroup.build_cmd_path(in_container, 'cgset'))

        if isinstance(setting, str) and isinstance(value, str):
            cmd.append('-r')
            cmd.append('{}={}'.format(setting, value))
        elif isinstance(setting, list) and isinstance(value, list):
            if len(setting) != len(value):
                raise ValueError('Settings list length must equal values list length')

            for idx, stg in enumerate(setting):
                cmd.append('-r')
                cmd.append('{}={}'.format(stg, value[idx]))

        cmd.append(cgname)

        if in_container:
            config.container.run(cmd)
        else:
            Run.run(cmd)

    @staticmethod
    # valid cpuset commands:
    #     Read one setting:
    #         cgget -r cpuset.cpus tomcpuset
    #     Read two settings:
    #         cgget -r cpuset.cpus -r cpuset.cpu_exclusive tomcpuset
    #     Read one setting from two cgroups:
    #         cgget -r cpuset.cpu_exclusive tomcgroup1 tomcgroup2
    #     Read two settings from two cgroups:
    #         cgget -r cpuset.cpu_exclusive -r cpuset.cpu_exclusive tomcgroup1 tomcgroup2
    #
    #     Read all of the settings in a cgroup
    #         cgget -g cpuset tomcpuset
    #     Read all of the settings in multiple controllers
    #         cgget -g cpuset -g cpu -g memory tomcgroup
    #     Read all of the settings from a cgroup at a specific path
    #         cgget -g memory:tomcgroup/tomcgroup
    def get(config, controller=None, cgname=None, setting=None,
            in_container=True, print_headers=True, values_only=False,
            all_controllers=False):
        cmd = list()
        cmd.append(Cgroup.build_cmd_path(in_container, 'cgget'))

        if not print_headers:
            cmd.append('-n')
        if values_only:
            cmd.append('-v')

        if setting is not None:
            if isinstance(setting, str):
                # the user provided a simple string.  use it as is
                cmd.append('-r')
                cmd.append(setting)
            elif isinstance(setting, list):
                for sttng in setting:
                    cmd.append('-r')
                    cmd.append(sttng)
            else:
                raise ValueError('Unsupported setting value')

        if controller is not None:
            if isinstance(controller, str) and ':' in controller:
                # the user provided a controller:cgroup.  use it as is
                cmd.append('-g')
                cmd.append(controller)
            elif isinstance(controller, str):
                # the user provided a controller only.  use it as is
                cmd.append('-g')
                cmd.append(controller)
            elif isinstance(controller, list):
                for ctrl in controller:
                    cmd.append('-g')
                    cmd.append(ctrl)
            else:
                raise ValueError('Unsupported controller value')

        if all_controllers:
            cmd.append('-a')

        if cgname is not None:
            if isinstance(cgname, str):
                # use the string as is
                cmd.append(cgname)
            elif isinstance(cgname, list):
                for cg in cgname:
                    cmd.append(cg)

        if in_container:
            ret = config.container.run(cmd)
        else:
            ret = Run.run(cmd)

        return ret

    @staticmethod
    def version(controller):
        with open('/proc/mounts', 'r') as mntf:
            for line in mntf.readlines():
                mnt_path = line.split()[1]

                if line.split()[0] == 'cgroup':
                    for option in line.split()[3].split(','):
                        if option == controller:
                            return Cgroup.CGROUP_V1
                elif line.split()[0] == 'cgroup2':
                    with open(os.path.join(mnt_path, 'cgroup.controllers'), 'r') as ctrlf:
                        controllers = ctrlf.readline()
                        for ctrl in controllers.split():
                            if ctrl == controller:
                                return Cgroup.CGROUP_V2

        return Cgroup.CGROUP_UNK

    @staticmethod
    def classify(config, controller, cgname, pid_list, sticky=False,
                 cancel_sticky=False, in_container=True):
        cmd = list()
        cmd.append(Cgroup.build_cmd_path(in_container, 'cgclassify'))
        cmd.append('-g')
        cmd.append('{}:{}'.format(controller, cgname))

        if isinstance(pid_list, str):
            cmd.append(pid_list)
        elif isinstance(pid_list, int):
            cmd.append(str(pid_list))
        elif isinstance(pid_list, list):
            for pid in pid_list:
                cmd.append(pid)

        if in_container:
            config.container.run(cmd)
        else:
            Run.run(cmd)
