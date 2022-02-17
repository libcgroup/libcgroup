#
# Run class for the libcgroup functional tests
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

from log import Log
import subprocess


class Run(object):
    @staticmethod
    def run(command, shell_bool=False):
        if shell_bool:
            if isinstance(command, str):
                # nothing to do.  command is already formatted as a string
                pass
            elif isinstance(command, list):
                command = ' '.join(command)
            else:
                raise ValueError('Unsupported command type')

        subproc = subprocess.Popen(command, shell=shell_bool,
                                   stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE)
        out, err = subproc.communicate()
        ret = subproc.returncode

        out = out.strip().decode('ascii')
        err = err.strip().decode('ascii')

        if shell_bool:
            Log.log_debug(
                            'run:\n\tcommand = {}\n\tret = {}\n\tstdout = {}'
                            '\n\tstderr = {}'
                            ''.format(command, ret, out, err)
                         )
        else:
            Log.log_debug(
                            'run:\n\tcommand = {}\n\tret = {}\n\tstdout = {}'
                            '\n\tstderr = {}'
                            ''.format(' '.join(command), ret, out, err)
                         )

        if ret != 0:
            raise RunError("Command '{}' failed".format(''.join(command)),
                           command, ret, out, err)
        if ret != 0 or len(err) > 0:
            if err.find('WARNING: cgroup v2 is not fully supported yet') == -1:
                # LXD throws the above warning on systems that are fully
                # running cgroup v2.  Ignore this warning, but fail if any
                # other warnings/errors are raised
                raise RunError("Command '{}' failed".format(''.join(command)),
                               command, ret, out, err)

        return out


class RunError(Exception):
    def __init__(self, message, command, ret, stdout, stderr):
        super(RunError, self).__init__(message)

        self.command = command
        self.ret = ret
        self.stdout = stdout
        self.stderr = stderr

    def __str__(self):
        out_str = 'RunError:\n\tcommand = {}\n\tret = {}'.format(
                  self.command, self.ret)
        out_str += '\n\tstdout = {}\n\tstderr = {}'.format(self.stdout,
                                                           self.stderr)
        return out_str

# vim: set et ts=4 sw=4:
