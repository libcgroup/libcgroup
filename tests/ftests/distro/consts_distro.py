# SPDX-License-Identifier: LGPL-2.1-only
#
# Constants for the libcgroup functional tests
#
# Copyright (c) 2025 Oracle and/or its affiliates.
#
# Author: Tom Hromatka <tom.hromatka@oracle.com>
# Author: Kamalesh Babulal <kamalesh.babulal@oracle.com>
#

from distro import consts_ubuntu, consts_oracle


class ConstsDistro:

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

    @staticmethod
    def get_consts(config):
        distro = ConstsDistro.get_distro(config)

        if distro == "ubuntu":
            return consts_ubuntu.ConstsUbuntu()
        elif distro == "oracle":
            return consts_oracle.ConstsOracle()

# vim: set et ts=4 sw=4:
