# SPDX-License-Identifier: LGPL-2.1-only
#
# Utility functions for the libcgroup functional tests
#
# Copyright (c) 2020 Oracle and/or its affiliates.
# Author: Tom Hromatka <tom.hromatka@gmail.com>
#

from run import Run
import platform


# function to indent a block of text by cnt number of spaces
def indent(in_str, cnt):
    leading_indent = cnt * ' '
    return ''.join(leading_indent + line for line in in_str.splitlines(True))


def get_file_owner_uid(config, filename):
    cmd = list()
    cmd.append('stat')
    cmd.append('-c')
    cmd.append("'%u'")
    cmd.append(filename)

    if config.args.container:
        return int(config.container.run(cmd, shell_bool=True))
    else:
        return int(Run.run(cmd, shell_bool=True))


def get_file_owner_username(config, filename):
    cmd = list()
    cmd.append('stat')
    cmd.append('-c')
    cmd.append("'%U'")
    cmd.append(filename)

    if config.args.container:
        return config.container.run(cmd, shell_bool=True)
    else:
        return Run.run(cmd, shell_bool=True)


def get_file_owner_gid(config, filename):
    cmd = list()
    cmd.append('stat')
    cmd.append('-c')
    cmd.append("'%g'")
    cmd.append(filename)

    if config.args.container:
        return int(config.container.run(cmd, shell_bool=True))
    else:
        return int(Run.run(cmd, shell_bool=True))


def get_file_owner_group_name(config, filename):
    cmd = list()
    cmd.append('stat')
    cmd.append('-c')
    cmd.append("'%G'")
    cmd.append(filename)

    if config.args.container:
        return config.container.run(cmd, shell_bool=True)
    else:
        return Run.run(cmd, shell_bool=True)


def get_file_permissions(config, filename):
    cmd = list()
    cmd.append('stat')
    cmd.append('-c')
    cmd.append("'%a'")
    cmd.append(filename)

    if config.args.container:
        return config.container.run(cmd, shell_bool=True)
    else:
        return Run.run(cmd, shell_bool=True)


# get the current kernel version
def get_kernel_version(config):
    kernel_version_str = str(platform.release())
    kernel_version = kernel_version_str.split('.')[0:3]
    return kernel_version


# match if both outputs are same
def is_output_same(config, out, expected_out):
    result = True
    cause = None

    out_lines = out.splitlines()
    exp_lines = expected_out.splitlines()

    for line_num, (line, exp_line) in enumerate(zip(out_lines, exp_lines)):
        if line.strip() != exp_line.strip():
            cause = (
                    'Expected line:\n\t{}\nbut received line:\n\t{}'
                    ''.format(exp_line.strip(), line.strip())
                    )
            result = False
            break

    return result, cause


# get the current linux flavour
def get_distro(config):
    with open('/etc/os-release', 'r') as relfile:
        buf = relfile.read()
        if "Oracle Linux" in buf:
            return "oracle"
        elif "Ubuntu" in buf:
            return "ubuntu"

# vim: set et ts=4 sw=4:
