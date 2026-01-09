# SPDX-License-Identifier: LGPL-2.1-only
#
# Abstract class for distro consts
#
# Copyright (c) 2025 Oracle and/or its affiliates.
#
# Author: Tom Hromatka <tom.hromatka@oracle.com>
# Author: Kamalesh Babulal <kamalesh.babulal@oracle.com>

from abc import ABC, abstractmethod


class ConstsAbstract(ABC):
    @abstractmethod
    def expected_cpu_out_009(self, controller='cpu'):
        raise NotImplementedError("Must be implemented in the distro class")

    @abstractmethod
    def expected_cpu_out_010(self, controller='cpu'):
        raise NotImplementedError("Must be implemented in the distro class")

    @abstractmethod
    def expected_cpu_out_013(self, controller='cpu'):
        raise NotImplementedError("Must be implemented in the distro class")

# vim: set et ts=4 sw=4:
