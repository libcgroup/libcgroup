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

from container import ContainerError
from controller import Controller
from run import Run, RunError
import multiprocessing as mp
from enum import Enum
import consts
import utils
import time
import copy
import os


class CgroupMount(object):
    def __init__(self, mount_line):
        entries = mount_line.split()

        if entries[2] == 'cgroup':
            self.version = CgroupVersion.CGROUP_V1
        elif entries[2] == 'cgroup2':
            self.version = CgroupVersion.CGROUP_V2
        else:
            raise ValueError('Unknown cgroup version')

        self.mount_point = entries[1]

        self.controller = None
        if self.version == CgroupVersion.CGROUP_V1:
            self.controller = entries[3].split(',')[-1]

            if self.controller == 'clone_children':
                # the cpuset controller may append this option to the end
                # rather than the controller name like all other controllers
                self.controller = 'cpuset'

    def __str__(self):
        out_str = 'CgroupMount'
        out_str += '\n\tMount Point = {}'.format(self.mount_point)
        out_str += '\n\tCgroup Version = {}'.format(self.version)
        if self.controller is not None:
            out_str += '\n\tController = {}'.format(self.controller)

        return out_str


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
                    ctrlf_path = os.path.join(mnt_path, 'cgroup.controllers')
                    with open(ctrlf_path, 'r') as ctrlf:
                        controllers = ctrlf.readline()
                        for ctrl in controllers.split():
                            if ctrl == controller:
                                return CgroupVersion.CGROUP_V2

        raise IndexError(
                            'Unknown version for controller {}'
                            ''.format(controller)
                        )


class Cgroup(object):
    # This class is analogous to libcgroup's struct cgroup
    def __init__(self, name):
        self.name = name
        # self.controllers maps to
        # struct cgroup_controller *controller[CG_CONTROLLER_MAX];
        self.controllers = dict()

        self.children = list()

    def __str__(self):
        out_str = 'Cgroup {}\n'.format(self.name)
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

    @staticmethod
    def create(config, controller_list, cgname, user_name=None,
               group_name=None, dperm=None, fperm=None, tperm=None,
               tasks_user_name=None, tasks_group_name=None, cghelp=False):
        if isinstance(controller_list, str):
            controller_list = [controller_list]

        cmd = list()

        if not config.args.container:
            cmd.append('sudo')
        cmd.append(Cgroup.build_cmd_path('cgcreate'))

        if user_name is not None and group_name is not None:
            cmd.append('-a')
            cmd.append('{}:{}'.format(user_name, group_name))

        if dperm is not None:
            cmd.append('-d')
            cmd.append(dperm)

        if fperm is not None:
            cmd.append('-f')
            cmd.append(fperm)

        if tperm is not None:
            cmd.append('-s')
            cmd.append(tperm)

        if tasks_user_name is not None and tasks_group_name is not None:
            cmd.append('-t')
            cmd.append('{}:{}'.format(tasks_user_name, tasks_group_name))

        if cghelp:
            cmd.append('-h')

        if controller_list:
            controllers_and_path = '{}:{}'.format(
                ','.join(controller_list), cgname)
        else:
            controllers_and_path = ':{}'.format(cgname)

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

        if controller_list:
            controllers_and_path = '{}:{}'.format(
                ','.join(controller_list), cgname)
        else:
            controllers_and_path = ':{}'.format(cgname)

        cmd.append('-g')
        cmd.append(controllers_and_path)

        if config.args.container:
            config.container.run(cmd)
        else:
            Run.run(cmd)

    @staticmethod
    def __set(config, cmd, cgname=None, setting=None, value=None,
              copy_from=None, cghelp=False):
        if setting is not None or value is not None:
            if isinstance(setting, str) and isinstance(value, str):
                cmd.append('-r')
                cmd.append('{}={}'.format(setting, value))
            elif isinstance(setting, list) and isinstance(value, list):
                if len(setting) != len(value):
                    raise ValueError(
                                        'Settings list length must equal '
                                        'values list length'
                                    )

                for idx, stg in enumerate(setting):
                    cmd.append('-r')
                    cmd.append('{}={}'.format(stg, value[idx]))
            else:
                raise ValueError(
                                    'Invalid inputs to cgget:\nsetting: {}\n'
                                    'value{}'
                                    ''.format(setting, value)
                                )

        if copy_from is not None:
            cmd.append('--copy-from')
            cmd.append(copy_from)

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
            return config.container.run(cmd)
        else:
            return Run.run(cmd)

    @staticmethod
    def set(config, cgname=None, setting=None, value=None, copy_from=None,
            cghelp=False):
        """cgset equivalent method

        The following variants of cgset are being tested by the
        automated functional tests:

        Command                                          Test Number
        cgset -r setting=value cgname                        various
        cgset -r setting1=val1 -r setting2=val2
              -r setting3=val2 cgname                            022
        cgset --copy_from foo bar                                023
        cgset --copy_from foo bar1 bar2                          024
        cgset -r setting=value foo bar                           025
        cgset -r setting1=value1 setting2=value2 foo bar         026
        various invalid flag combinations                        027
        """
        cmd = list()
        if not config.args.container:
            cmd.append('sudo')
        cmd.append(Cgroup.build_cmd_path('cgset'))

        return Cgroup.__set(config, cmd, cgname, setting, value, copy_from,
                            cghelp)

    @staticmethod
    def xset(config, cgname=None, setting=None, value=None, copy_from=None,
             version=CgroupVersion.CGROUP_UNK, cghelp=False,
             ignore_unmappable=False):
        """cgxset equivalent method
        """
        cmd = list()
        if not config.args.container:
            cmd.append('sudo')
        cmd.append(Cgroup.build_cmd_path('cgxset'))

        if version == CgroupVersion.CGROUP_V1:
            cmd.append('-1')
        elif version == CgroupVersion.CGROUP_V2:
            cmd.append('-2')

        if ignore_unmappable:
            cmd.append('-i')

        return Cgroup.__set(config, cmd, cgname, setting, value, copy_from,
                            cghelp)

    @staticmethod
    def __get(config, cmd, controller=None, cgname=None, setting=None,
              print_headers=True, values_only=False,
              all_controllers=False, cghelp=False):
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
                if 'profiling' in re.stderr:
                    ret = re.stdout
                else:
                    raise re

        return ret

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

        return Cgroup.__get(config, cmd, controller, cgname, setting,
                            print_headers, values_only, all_controllers,
                            cghelp)

    @staticmethod
    def xget(config, controller=None, cgname=None, setting=None,
             print_headers=True, values_only=False,
             all_controllers=False, version=CgroupVersion.CGROUP_UNK,
             cghelp=False, ignore_unmappable=False):
        """cgxget equivalent method

        Returns:
        str: The stdout result of cgxget
        """
        cmd = list()
        cmd.append(Cgroup.build_cmd_path('cgxget'))

        if version == CgroupVersion.CGROUP_V1:
            cmd.append('-1')
        elif version == CgroupVersion.CGROUP_V2:
            cmd.append('-2')

        if ignore_unmappable:
            cmd.append('-i')

        return Cgroup.__get(config, cmd, controller, cgname, setting,
                            print_headers, values_only, all_controllers,
                            cghelp)

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
    # given a stdout of cgsnapshot-like data, create a dictionary of cgroup
    # objects
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
                if line.startswith('#'):
                    continue

                elif line.startswith('group') and line.endswith('{'):
                    cg_name = line.split()[1]
                    if cg_name in cgdict:
                        # We already have a cgroup with this name.  This block
                        # of text contains the next controller for this cgroup
                        cg = cgdict[cg_name]
                    else:
                        cg = Cgroup(cg_name)

                    mode = parsemode.GROUP

            elif mode == parsemode.GROUP:
                if line.startswith('perm {'):
                    mode = parsemode.PERM
                elif line.endswith('{'):
                    ctrl_name = line.split()[0]
                    cg.controllers[ctrl_name] = Controller(ctrl_name)

                    mode = parsemode.CONTROLLER
                elif line.endswith('}'):
                    # we've found the end of this group
                    cgdict[cg_name] = cg

                    mode = parsemode.UNKNOWN

            elif mode == parsemode.CONTROLLER:
                if line.endswith('";'):
                    # this is a setting on a single line
                    setting = line.split('=')[0]
                    value = line.split('=')[1]

                    cg.controllers[ctrl_name].settings[setting] = value

                elif line.endswith('}'):
                    # we've found the end of this controller
                    mode = parsemode.GROUP

                else:
                    # this is a multi-line setting
                    setting = line.split('=')[0]
                    value = '{}\n'.format(line.split('=')[1])
                    mode = parsemode.SETTING

            elif mode == parsemode.SETTING:
                if line.endswith('";'):
                    # this is the last line of the multi-line setting
                    value += line

                    cg.controllers[ctrl_name].settings[setting] = value
                    mode = parsemode.CONTROLLER

                else:
                    value += '{}\n'.format(line)

            elif mode == parsemode.PERM:
                if line.startswith('admin {'):
                    mode = parsemode.ADMIN
                elif line.startswith('task {'):
                    mode = parsemode.TASK
                elif line.endswith('}'):
                    mode = parsemode.GROUP

            elif mode == parsemode.ADMIN or mode == parsemode.TASK:
                # todo - handle these modes
                if line.endswith('}'):
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
                config.container.run(
                                        ['sudo',
                                         'touch',
                                         '/etc/cgsnapshot_blacklist.conf']
                                    )
            except RunError as re:
                if re.ret == 0 and 'unable to resolve host' in re.stderr:
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
               'neither blacklisted nor whitelisted' in re.stderr:
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
        except RunError as re:
            # check the errno to ensure the directory exists rather
            # than receiving a different error
            if re.ret == 1 and \
               'File exists' in re.stderr:
                pass
            else:
                raise re

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
            raise ValueError(
                                'Running cgrules within a container is not '
                                'supported'
                            )
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
        except (RunError, ContainerError):
            # ignore any errors during the kill command.  this is belt
            # and suspenders code
            pass

        for child in self.children:
            child.join(1)

    @staticmethod
    def configparser(config, load_file=None, load_dir=None, dflt_usr=None,
                     dflt_grp=None, dperm=None, fperm=None, cghelp=False,
                     tperm=None, tasks_usr=None, tasks_grp=None):
        """cgconfigparser equivalent method

        Returns:
        str: The stdout result of cgconfigparser

        The following variants of cgconfigparser are being tested by the
        automated functional tests:

        Command                                          Test Number
        cgconfigparser -l conf_file                              017
        cgconfigparser -L conf_dir                               018
        cgconfigparser -l conf_file -a usr:grp -d mode -f mode   019
        cgconfigparser -l conf_file -s mode -t usr:grp           020
        cgconfigparser -h                                        021
        cgconfigparser -l improper_conf_file                     021
        """
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

    @staticmethod
    def __get_controller_mount_point_v1(ctrl_name):
        with open('/proc/mounts', 'r') as mntf:
            for line in mntf.readlines():
                mnt_path = line.split()[1]

                if line.split()[0] == 'cgroup':
                    for option in line.split()[3].split(','):
                        if option == ctrl_name:
                            return mnt_path

        raise IndexError(
                            'Unknown mount point for controller {}'
                            ''.format(ctrl_name)
                        )

    @staticmethod
    def __get_controller_mount_point_v2(ctrl_name):
        with open('/proc/mounts', 'r') as mntf:
            for line in mntf.readlines():
                mnt_path = line.split()[1]

                if line.split()[0] == 'cgroup2':
                    ctrl_file = os.path.join(line.split()[1],
                                             'cgroup.controllers')

                    with open(ctrl_file, 'r') as ctrlf:
                        controllers = ctrlf.readline()
                        for controller in controllers.split():
                            if controller == ctrl_name:
                                return mnt_path

        raise IndexError(
                            'Unknown mount point for controller {}'
                            ''.format(ctrl_name)
                        )

    @staticmethod
    def get_controller_mount_point(ctrl_name):
        vers = CgroupVersion.get_version(ctrl_name)

        if vers == CgroupVersion.CGROUP_V1:
            return Cgroup.__get_controller_mount_point_v1(ctrl_name)
        elif vers == CgroupVersion.CGROUP_V2:
            return Cgroup.__get_controller_mount_point_v2(ctrl_name)
        else:
            raise ValueError('Unsupported cgroup version')

    @staticmethod
    def clear(config, empty=False, cghelp=False, load_file=None,
              load_dir=None):
        cmd = list()

        if not config.args.container:
            cmd.append('sudo')
        cmd.append(Cgroup.build_cmd_path('cgclear'))

        if empty:
            cmd.append('-e')

        if cghelp:
            cmd.append('-h')

        if load_file is not None:
            cmd.append('-l')
            cmd.append(load_file)

        if load_dir is not None:
            cmd.append('-L')
            cmd.append(load_dir)

        if config.args.container:
            return config.container.run(cmd)
        else:
            return Run.run(cmd)

    @staticmethod
    def lssubsys(config, ls_all=False, cghelp=False, hierarchies=False,
                 mount_points=False, all_mount_points=False):
        cmd = list()

        cmd.append(Cgroup.build_cmd_path('lssubsys'))

        if ls_all:
            cmd.append('-a')

        if cghelp:
            cmd.append('-h')

        if hierarchies:
            cmd.append('-i')

        if mount_points:
            cmd.append('-m')

        if all_mount_points:
            cmd.append('-M')

        if config.args.container:
            ret = config.container.run(cmd)
        else:
            try:
                ret = Run.run(cmd)
            except RunError as re:
                if 'profiling' in re.stderr:
                    ret = re.stdout
                else:
                    raise re

        return ret

    @staticmethod
    def get_cgroup_mounts(config, expand_v2_mounts=True):
        mount_list = list()

        with open('/proc/mounts') as mntf:
            for line in mntf.readlines():
                entry = line.split()

                if entry[0] != 'cgroup' and entry[0] != 'cgroup2':
                    continue

                mount = CgroupMount(line)

                if mount.version == CgroupVersion.CGROUP_V1 or \
                   expand_v2_mounts is False:
                    mount_list.append(mount)
                    continue

                with open(os.path.join(mount.mount_point,
                                       'cgroup.controllers')) as ctrlf:
                    for line in ctrlf.readlines():
                        for ctrl in line.split():
                            mount_copy = copy.deepcopy(mount)
                            mount_copy.controller = ctrl
                            mount_list.append(mount_copy)

        return mount_list

    @staticmethod
    def lscgroup(config, cghelp=False, controller=None, path=None):
        cmd = list()

        cmd.append(Cgroup.build_cmd_path('lscgroup'))

        if cghelp:
            cmd.append('-h')

        if controller is not None and path is not None:
            if isinstance(controller, list):
                for idx, ctrl in enumerate(controller):
                    cmd.append('-g')
                    cmd.append('{}:{}'.format(ctrl, path[idx]))
            elif isinstance(controller, str):
                cmd.append('-g')
                cmd.append('{}:{}'.format(controller, path))
            else:
                raise ValueError('Unsupported controller value')

        if config.args.container:
            ret = config.container.run(cmd)
        else:
            try:
                ret = Run.run(cmd)
            except RunError as re:
                if 'profiling' in re.stderr:
                    ret = re.stdout
                else:
                    raise re

        return ret

    # exec is a keyword in python, so let's name this function cgexec
    @staticmethod
    def cgexec(config, controller, cgname, cmdline, sticky=False,
               cghelp=False):
        """cgexec equivalent method
        """
        cmd = list()

        if not config.args.container:
            cmd.append('sudo')
        cmd.append(Cgroup.build_cmd_path('cgexec'))
        cmd.append('-g')
        cmd.append('{}:{}'.format(controller, cgname))

        if sticky:
            cmd.append('--sticky')

        if isinstance(cmdline, str):
            cmd.append(cmdline)
        elif isinstance(cmdline, list):
            for entry in cmdline:
                cmd.append(entry)

        if cghelp:
            cmd.append('-h')

        if config.args.container:
            return config.container.run(cmd, shell_bool=True)
        else:
            return Run.run(cmd, shell_bool=True)

    @staticmethod
    def get_pids_in_cgroup(config, cgroup, controller):
        mounts = Cgroup.get_cgroup_mounts(config)

        for mount in mounts:
            if mount.controller == controller:
                proc_file = os.path.join(
                                            mount.mount_point,
                                            cgroup,
                                            'cgroup.procs'
                                        )
                cmd = ['cat', proc_file]

                if config.args.container:
                    return config.container.run(cmd, shell_bool=True)
                else:
                    return Run.run(cmd, shell_bool=True)

        return None

    @staticmethod
    def get_and_validate(config, cgname, setting, expected_value):
        """get the requested setting and validate the value received

        This is a helper method for the functional tests and there is no
        equivalent libcgroup command line interface.  This method will
        raise a CgroupError if the comparison fails
        """
        value = Cgroup.get(config, controller=None, cgname=cgname,
                           setting=setting, print_headers=False,
                           values_only=True)

        if value != expected_value:
            raise CgroupError('cgget expected {} but received {}'.format(
                              expected_value, value))

    @staticmethod
    def set_and_validate(config, cgname, setting, value):
        """set the requested setting and validate the write

        This is a helper method for the functional tests and there is no
        equivalent libcgroup command line interface.  This method will
        raise a CgroupError if the comparison fails
        """
        Cgroup.set(config, cgname, setting, value)
        Cgroup.get_and_validate(config, cgname, setting, value)


class CgroupError(Exception):
    def __init__(self, message):
        super(CgroupError, self).__init__(message)

# vim: set et ts=4 sw=4:
