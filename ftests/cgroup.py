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
import multiprocessing as mp
import os
from run import Run, RunError
import time
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

        raise IndexError("Unknown version for controller {}".format(controller))

class Cgroup(object):
    # This class is analogous to libcgroup's struct cgroup
    def __init__(self, name):
        self.name = name
        # self.controllers maps to
        # struct cgroup_controller *controller[CG_CONTROLLER_MAX];
        self.controllers = dict()

        self.children = list()

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

    @staticmethod
    def build_daemon_path(cmd):
        return os.path.join(consts.LIBCG_MOUNT_POINT,
                            'src/daemon/{}'.format(cmd))

    # TODO - add support for all of the cgcreate options
    @staticmethod
    def create(config, controller_list, cgname):
        if isinstance(controller_list, str):
            controller_list = [controller_list]

        cmd = list()

        if not config.args.container:
            cmd.append('sudo')
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

        if not config.args.container:
            cmd.append('sudo')
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

        if not config.args.container:
            cmd.append('sudo')
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
    def get(config, controller=None, cgname=None, setting=None,
            print_headers=True, values_only=False,
            all_controllers=False, cghelp=False):
        """cgget equivalent method

        Returns:
        str: The stdout result of cgget

        The following variants of cgget() are being tested by the
        automated functional tests:

        Command                                          Test Number
        cgget -r cpuset.cpus mycg                                001
        cgget -r cpuset.cpus -r cpuset.mems mycg                 008
        cgget -g cpu mycg                                        009
        cgget -g cpu:mycg                                        010
        cgget -r cpuset.cpus mycg1 mycg2                         011
        cgget -r cpuset.cpus -r cpuset.mems mycg1 mycg2          012
        cgget -g cpu -g freezer mycg                             013
        cgget -a mycg                                            014
        cgget -r memory.stat mycg (multiline value read)         015
        various invalid flag combinations                        016
        """
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

        if cghelp:
            cmd.append('-h')

        if config.args.container:
            ret = config.container.run(cmd)
        else:
            try:
                ret = Run.run(cmd)
            except RunError as re:
                if "profiling" in re.stderr:
                    ret = re.stdout
                else:
                    raise re

        return ret

    @staticmethod
    def classify(config, controller, cgname, pid_list, sticky=False,
                 cancel_sticky=False):
        cmd = list()

        if not config.args.container:
            cmd.append('sudo')
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
            PERM = 4
            ADMIN = 5
            TASK = 6

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
                if line.startswith("perm {"):
                    mode = parsemode.PERM
                elif line.endswith("{"):
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

            elif mode == parsemode.PERM:
                if line.startswith("admin {"):
                    mode = parsemode.ADMIN
                elif line.startswith("task {"):
                    mode = parsemode.TASK
                elif line.endswith("}"):
                    mode = parsemode.GROUP

            elif mode == parsemode.ADMIN or mode == parsemode.TASK:
                # todo - handle these modes
                if line.endswith("}"):
                    mode = parsemode.PERM

        return cgdict

    @staticmethod
    def snapshot(config, controller=None):
        cmd = list()
        cmd.append(Cgroup.build_cmd_path('cgsnapshot'))
        if controller is not None:
            cmd.append(controller)

        # ensure the deny list file exists
        if config.args.container:
            try:
                config.container.run(['sudo', 'touch', '/etc/cgsnapshot_blacklist.conf'])
            except RunError as re:
                if re.ret == 0 and "unable to resolve host" in re.stderr:
                    pass
        else:
            Run.run(['sudo', 'touch', '/etc/cgsnapshot_blacklist.conf'])

        try:
            if config.args.container:
                res = config.container.run(cmd)
            else:
                res = Run.run(cmd)
        except RunError as re:
            if re.ret == 0 and \
               "neither blacklisted nor whitelisted" in re.stderr:
                res = re.stdout

        # convert the cgsnapshot stdout to a dict of cgroup objects
        return Cgroup.snapshot_to_dict(res)

    @staticmethod
    def set_cgrules_conf(config, line, append=True):
        cmd = list()

        cmd.append('sudo')
        cmd.append('su')
        cmd.append('-c')

        if append:
            redirect_str = '>>'
        else:
            redirect_str = '>'

        subcmd = '"echo {} {} {}"'.format(line, redirect_str,
                                          consts.CGRULES_FILE)
        cmd.append(subcmd)

        if config.args.container:
            config.container.run(cmd, shell_bool=True)
        else:
            Run.run(cmd, shell_bool=True)

    @staticmethod
    def init_cgrules(config):
        cmd = list()

        cmd.append('sudo')
        cmd.append('mkdir')
        cmd.append('/etc/cgconfig.d')

        try:
            if config.args.container:
                config.container.run(cmd, shell_bool=True)
            else:
                Run.run(cmd, shell_bool=True)
        except:
            # todo - check the errno to ensure the directory exists rather
            # than receiving a different error
            pass

        cmd2 = list()

        cmd2.append('sudo')
        cmd2.append('touch')
        cmd2.append('/etc/cgconfig.conf')

        if config.args.container:
            config.container.run(cmd2, shell_bool=True)
        else:
            Run.run(cmd2, shell_bool=True)

    # note that this runs cgrulesengd in this process and does not fork
    # the daemon
    @staticmethod
    def __run_cgrules(config):
        cmd = list()

        cmd.append('sudo')
        cmd.append(Cgroup.build_daemon_path('cgrulesengd'))
        cmd.append('-d')
        cmd.append('-n')

        if config.args.container:
            raise ValueError("Running cgrules within a container is not supported")
        else:
            Run.run(cmd, shell_bool=True)

    def start_cgrules(self, config):
        Cgroup.init_cgrules(config)

        p = mp.Process(target=Cgroup.__run_cgrules,
                       args=(config, ))
        p.start()
        time.sleep(2)

        self.children.append(p)

    def join_children(self, config):
        # todo - make this smarter.  this is ugly, but it works for now
        cmd = ['sudo', 'killall', 'cgrulesengd']
        try:
            if config.args.container:
                config.container.run(cmd, shell_bool=True)
            else:
                Run.run(cmd, shell_bool=True)
        except:
            # ignore any errors during the kill command.  this is belt
            # and suspenders code
            pass

        for child in self.children:
            child.join(1)

    @staticmethod
    def configparser(config, load_file=None, load_dir=None, dflt_usr=None,
                     dflt_grp=None, dperm=None, fperm=None, cghelp=False,
                     tperm=None, tasks_usr=None, tasks_grp=None):
        cmd = list()

        if not config.args.container:
            cmd.append('sudo')
        cmd.append(Cgroup.build_cmd_path('cgconfigparser'))

        if load_file is not None:
            cmd.append('-l')
            cmd.append(load_file)

        if load_dir is not None:
            cmd.append('-L')
            cmd.append(load_dir)

        if dflt_usr is not None and dflt_grp is not None:
            cmd.append('-a')
            cmd.append('{}:{}'.format(dflt_usr, dflt_grp))

        if dperm is not None:
            cmd.append('-d')
            cmd.append(dperm)

        if fperm is not None:
            cmd.append('-f')
            cmd.append(fperm)

        if cghelp:
            cmd.append('-h')

        if tperm is not None:
            cmd.append('-s')
            cmd.append(tperm)

        if tasks_usr is not None and tasks_grp is not None:
            cmd.append('-t')
            cmd.append('{}:{}'.format(tasks_usr, tasks_grp))

        if config.args.container:
            return config.container.run(cmd)
        else:
            return Run.run(cmd)
