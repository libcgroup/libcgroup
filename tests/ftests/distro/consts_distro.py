# SPDX-License-Identifier: LGPL-2.1-only
#
# Constants for the libcgroup functional tests
#
# Copyright (c) 2025 Oracle and/or its affiliates.
#
# Author: Tom Hromatka <tom.hromatka@oracle.com>
# Author: Kamalesh Babulal <kamalesh.babulal@oracle.com>
#

from distro import consts_ubuntu


# get the current linux flavour
def get_distro(config):
    with open('/etc/os-release', 'r') as relfile:
        buf = relfile.read()
        if "Oracle Linux" in buf:
            return "oracle"
        elif "Ubuntu" in buf:
            return "ubuntu"
        else:
            raise NotImplementedError("Unsupported Distro")


consts = consts_ubuntu.ConstsUbuntu()

# vim: set et ts=4 sw=4:
