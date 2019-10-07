#
# Container class for the libcgroup functional tests
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
import getpass
from log import Log
import os
from run import Run

class Container(object):
    def __init__(self, name, stop_timeout=None, arch=None, cfg_path=None,
                 distro=None, release=None):
        self.name = name
        self.privileged = True

        if stop_timeout:
            self.stop_timeout = stop_timeout
        else:
            self.stop_timeout = consts.DEFAULT_CONTAINER_STOP_TIMEOUT

        if arch:
            self.arch = arch
        else:
            self.arch = consts.DEFAULT_CONTAINER_ARCH

        if cfg_path:
            self.cfg_path = cfg_path
        else:
            self.cfg_path = consts.DEFAULT_CONTAINER_CFG_PATH

        if distro:
            self.distro = distro
        else:
            self.distro = consts.DEFAULT_CONTAINER_DISTRO

        if release:
            self.release = release
        else:
            self.release = consts.DEFAULT_CONTAINER_RELEASE

        ftest_dir = os.path.dirname(os.path.abspath(__file__))
        tests_dir = os.path.dirname(ftest_dir)
        libcg_dir = os.path.dirname(tests_dir)

        self.tmp_cfg_path = os.path.join(ftest_dir, consts.TEMP_CONTAINER_CFG_FILE)
        try:
            Run.run(['rm', '-f', self.tmp_cfg_path])
        except:
            pass

        Run.run(['cp', self.cfg_path, self.tmp_cfg_path])

        conf_line = 'lxc.arch = {}'.format(self.arch)
        Run.run(['echo', conf_line, '>>', self.tmp_cfg_path], shell_bool=True)

        conf_line = 'lxc.mount.entry = {} {} none bind,ro 0 0'.format(
                    libcg_dir, consts.LIBCG_MOUNT_POINT)
        Run.run(['echo', conf_line, '>>', self.tmp_cfg_path], shell_bool=True)

        if not self.privileged:
            conf_line = 'lxc.idmap = u 0 100000 65536'
            Run.run(['echo', conf_line, '>>', self.tmp_cfg_path], shell_bool=True)
            conf_line = 'lxc.idmap = g 0 100000 65536'
            Run.run(['echo', conf_line, '>>', self.tmp_cfg_path], shell_bool=True)

    def __str__(self):
        out_str = "{}".format(self.name)
        out_str += "\n\tdistro = {}".format(self.distro)
        out_str += "\n\trelease = {}".format(self.release)
        out_str += "\n\tarch = {}".format(self.arch)
        out_str += "\n\tcfg_path = {}".format(self.cfg_path)
        out_str += "\n\tstop_timeout = {}".format(self.stop_timeout)

        return out_str

    def create(self):
        cmd = list()

        if self.privileged:
            cmd.append('sudo')

        cmd.append('lxc-create')
        cmd.append('-t')
        cmd.append( 'download')

        cmd.append('-n')
        cmd.append(self.name)

        if self.privileged:
            cmd.append('sudo')
        cmd.append('-f')
        cmd.append(self.tmp_cfg_path)

        cmd.append('--')

        cmd.append('-d')
        cmd.append(self.distro)

        cmd.append('-r')
        cmd.append(self.release)

        cmd.append('-a')
        cmd.append(self.arch)

        return Run.run(cmd)

    def destroy(self):
        cmd = list()

        if self.privileged:
            cmd.append('sudo')

        cmd.append('lxc-destroy')

        cmd.append('-n')
        cmd.append(self.name)

        return Run.run(cmd)

    def info(self, cfgname):
        cmd = list()

        if self.privileged:
            cmd.append('sudo')

        cmd.append('lxc-info')

        cmd.append('--config={}'.format(cfgname))

        cmd.append('-n')
        cmd.append(self.name)

        return Run.run(cmd)

    def rootfs(self):
        # try to read lxc.rootfs.path first
        ret = self.info('lxc.rootfs.path')
        if len(ret.strip()) > 0:
            return ret.decode()

        # older versions of lxc used lxc.rootfs.  Try that.
        ret = self.info('lxc.rootfs')
        if len(ret.strip()) == 0:
            # we failed to get the rootfs
            raise ContainerError('Failed to get the rootfs')
        return ret.decode()

    def run(self, cntnr_cmd):
        cmd = list()

        if self.privileged:
            cmd.append('sudo')

        cmd.append('lxc-attach')

        cmd.append('-n')
        cmd.append(self.name)

        cmd.append('--')

        # concatenate the lxc-attach command with the command to be run
        # inside the container
        if isinstance(cntnr_cmd, str):
            cmd.append(cntnr_cmd)
        elif isinstance(cntnr_cmd, list):
            cmd = cmd + cntnr_cmd
        else:
            raise ContainerError('Unsupported command type')

        return Run.run(cmd).decode('ascii')

    def start(self):
        cmd = list()

        if self.privileged:
            cmd.append('sudo')

        cmd.append('lxc-start')
        cmd.append('-d')

        cmd.append('-n')
        cmd.append(self.name)

        return Run.run(cmd)

    def stop(self, kill=True):
        cmd = list()

        if self.privileged:
            cmd.append('sudo')

        cmd.append('lxc-stop')

        cmd.append('-n')
        cmd.append(self.name)

        if kill:
            cmd.append('-k')
        else:
            cmd.append('-t')
            cmd.append(str(self.stop_timeout))

        return Run.run(cmd)

    def version(self):
        cmd = ['lxc-create', '--version']
        return Run.run(cmd)

class ContainerError(Exception):
    def __init__(self, message, ret):
        super(RunError, self).__init__(message)

    def __str__(self):
        out_str = "ContainerError:\n\tmessage = {}".format(self.message)
        return out_str
