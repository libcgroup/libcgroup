# SPDX-License-Identifier: LGPL-2.1-only
#
# Utilities for understanding and evaluating cgroup usage on a machine
#
# Copyright (c) 2021-2024 Oracle and/or its affiliates.
# Author: Tom Hromatka <tom.hromatka@oracle.com>
#

# cython: language_level = 3str


import subprocess

class LibcgroupPid(object):
    def __init__(self, pid, command=None):
        self.pid = pid
        self.command = command
        self.pidstats = dict()

    def __str__(self):
        out_str = 'LibcgroupPid: {}'.format(self.pid)
        out_str += '\n\tcommand = {}'.format(self.command)
        for key, value in self.pidstats.items():
            out_str += '\n\tpidstats[{}] = {}'.format(key, value)

        return out_str

    @staticmethod
    def create_from_pidstat(pid):
        cmd = list()
        cmd.append('pidstat')
        cmd.append('-H')
        cmd.append('-h')
        cmd.append('-r')
        cmd.append('-u')
        cmd.append('-v')
        cmd.append('-p')
        cmd.append('{}'.format(pid))

        out = run(cmd)

        for line in out.splitlines():
            if not len(line.strip()):
                continue
            if line.startswith('Linux'):
                # ignore the kernel info
                continue
            if line.startswith('#'):
                line = line.lstrip('#')
                keys = line.split()
                continue

            # the last line of pidstat is information regarding the pid
            values = line.split()

            cgpid = LibcgroupPid(pid)
            for i, key in enumerate(keys):
                cgpid.pidstats[key] = values[i]

            return cgpid

def run(command, run_in_shell=False):
    if run_in_shell:
        if isinstance(command, str):
            # nothing to do.  command is already formatted as a string
            pass
        elif isinstance(command, list):
            command = ' '.join(command)
        else:
            raise ValueError('Unsupported command type')

    subproc = subprocess.Popen(command, shell=run_in_shell,
                               stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE)
    out, err = subproc.communicate()
    ret = subproc.returncode

    out = out.strip().decode('UTF-8')
    err = err.strip().decode('UTF-8')

    if ret != 0 or len(err) > 0:
        raise RunError("Command '{}' failed".format(''.join(command)),
                       command, ret, out, err)

    return out

def humanize(value):
    if type(value) is not int:
        raise TypeError('Unsupported type {}'.format(type(value)))

    if value < 1024:
        return value
    elif value < 1024 ** 2:
        value = value / 1024
        return '{}K'.format(int(value))
    elif value < 1024 ** 3:
        value = value / (1024 ** 2)
        return '{}M'.format(int(value))
    elif value < 1024 ** 4:
        value = value / (1024 ** 3)
        return '{}G'.format(int(value))
    elif value < 1024 ** 5:
        value = value / (1024 ** 4)
        return '{}T'.format(int(value))
    else:
        return value

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
