#
# Cgroup class for the libcgroup functional tests
#
# Copyright (c) 2019-2021 Oracle and/or its affiliates.
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
from controller import Controller
from enum import Enum
import os
from run import Run
import utils

class CgroupVersion(Enum):
    CGROUP_UNK = 0
    CGROUP_V1 = 1
    CGROUP_V2 = 2

    # given a controller name, get the cgroup version of the controller
    @staticmethod
    def get_version(controller):
        with open('/proc/mounts', 'r') as mntf:
            for line in mntf.readlines():
                mnt_path = line.split()[1]

                if line.split()[0] == 'cgroup':
                    for option in line.split()[3].split(','):
                        if option == controller:
                            return CgroupVersion.CGROUP_V1
                elif line.split()[0] == 'cgroup2':
                    with open(os.path.join(mnt_path, 'cgroup.controllers'), 'r') as ctrlf:
                        controllers = ctrlf.readline()
                        for ctrl in controllers.split():
                            if ctrl == controller:
                                return CgroupVersion.CGROUP_V2

        return CgroupVersion.CGROUP_UNK

class Cgroup(object):
    # This class is analogous to libcgroup's struct cgroup
    def __init__(self, name):
        self.name = name
        # self.controllers maps to
        # struct cgroup_controller *controller[CG_CONTROLLER_MAX];
        self.controllers = dict()

    def __str__(self):
        out_str = "Cgroup {}\n".format(self.name)
        for ctrl_key in self.controllers:
            out_str += utils.indent(str(self.controllers[ctrl_key]), 4)

        return out_str

    def __eq__(self, other):
        if not isinstance(other, Cgroup):
            return False

        if not self.name == other.name:
            return False

        if self.controllers != other.controllers:
            return False

        return True

    @staticmethod
    def build_cmd_path(cmd):
        return os.path.join(consts.LIBCG_MOUNT_POINT,
                            'src/tools/{}'.format(cmd))

    # TODO - add support for all of the cgcreate options
    @staticmethod
    def create(config, controller_list, cgname):
        if isinstance(controller_list, str):
            controller_list = [controller_list]

        cmd = list()
        cmd.append(Cgroup.build_cmd_path('cgcreate'))

        controllers_and_path = '{}:{}'.format(
            ','.join(controller_list), cgname)

        cmd.append('-g')
        cmd.append(controllers_and_path)

        if config.args.container:
            config.container.run(cmd)
        else:
            Run.run(cmd)

    @staticmethod
    def delete(config, controller_list, cgname, recursive=False):
        if isinstance(controller_list, str):
            controller_list = [controller_list]

        cmd = list()
        cmd.append(Cgroup.build_cmd_path('cgdelete'))

        if recursive:
            cmd.append('-r')

        controllers_and_path = '{}:{}'.format(
            ''.join(controller_list), cgname)

        cmd.append('-g')
        cmd.append(controllers_and_path)

        if config.args.container:
            config.container.run(cmd)
        else:
            Run.run(cmd)

    @staticmethod
    def set(config, cgname, setting, value):
        cmd = list()
        cmd.append(Cgroup.build_cmd_path('cgset'))

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

        if config.args.container:
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
            print_headers=True, values_only=False,
            all_controllers=False):
        cmd = list()
        cmd.append(Cgroup.build_cmd_path('cgget'))

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

        if config.args.container:
            ret = config.container.run(cmd)
        else:
            ret = Run.run(cmd)

        return ret

    @staticmethod
    def classify(config, controller, cgname, pid_list, sticky=False,
                 cancel_sticky=False):
        cmd = list()
        cmd.append(Cgroup.build_cmd_path('cgclassify'))
        cmd.append('-g')
        cmd.append('{}:{}'.format(controller, cgname))

        if isinstance(pid_list, str):
            cmd.append(pid_list)
        elif isinstance(pid_list, int):
            cmd.append(str(pid_list))
        elif isinstance(pid_list, list):
            for pid in pid_list:
                cmd.append(pid)

        if config.args.container:
            config.container.run(cmd)
        else:
            Run.run(cmd)

    @staticmethod
    # given a stdout of cgsnapshot-like data, create a dictionary of cgroup objects
    def snapshot_to_dict(cgsnapshot_stdout):
        cgdict = dict()

        class parsemode(Enum):
            UNKNOWN = 0
            GROUP = 1
            CONTROLLER = 2
            SETTING = 3

        mode = parsemode.UNKNOWN

        for line in cgsnapshot_stdout.splitlines():
            line = line.strip()

            if mode == parsemode.UNKNOWN:
                if line.startswith("#"):
                    continue

                elif line.startswith("group") and line.endswith("{"):
                    cg_name = line.split()[1]
                    if cg_name in cgdict:
                        # We already have a cgroup with this name.  This block
                        # of text contains the next controller for this cgroup
                        cg = cgdict[cg_name]
                    else:
                        cg = Cgroup(cg_name)

                    mode = parsemode.GROUP

            elif mode == parsemode.GROUP:
                if line.endswith("{"):
                    ctrl_name = line.split()[0]
                    cg.controllers[ctrl_name] = Controller(ctrl_name)

                    mode = parsemode.CONTROLLER
                elif line.endswith("}"):
                    # we've found the end of this group
                    cgdict[cg_name] = cg

                    mode = parsemode.UNKNOWN

            elif mode == parsemode.CONTROLLER:
                if line.endswith("\";"):
                    # this is a setting on a single line
                    setting = line.split("=")[0]
                    value = line.split("=")[1]

                    cg.controllers[ctrl_name].settings[setting] = value

                elif line.endswith("}"):
                    # we've found the end of this controller
                    mode = parsemode.GROUP

                else:
                    # this is a multi-line setting
                    setting = line.split("=")[0]
                    value = "{}\n".format(line.split("=")[1])
                    mode = parsemode.SETTING

            elif mode == parsemode.SETTING:
                if line.endswith("\";"):
                    # this is the last line of the multi-line setting
                    value += line

                    cg.controllers[ctrl_name].settings[setting] = value
                    mode = parsemode.CONTROLLER

                else:
                    value += "{}\n".format(line)

        return cgdict

    @staticmethod
    def snapshot(config, controller=None):
        cmd = list()
        cmd.append(Cgroup.build_cmd_path('cgsnapshot'))
        if controller is not None:
            cmd.append(controller)

        if config.args.container:
            # if we're running in a container, it's unlikely that libcgroup
            # was installed and thus /etc/cgsnapshot_blacklist.conf likely
            # doesn't exist.  Let's make it
            config.container.run(['sudo', 'touch', '/etc/cgsnapshot_blacklist.conf'])

        if config.args.container:
            res = config.container.run(cmd)
        else:
            res = Run.run(cmd)

        # convert the cgsnapshot stdout to a dict of cgroup objects
        return Cgroup.snapshot_to_dict(res)
