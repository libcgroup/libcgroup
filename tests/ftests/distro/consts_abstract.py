# SPDX-License-Identifier: LGPL-2.1-only
#
# Abstract distro constants definitions for the libcgroup functional tests
#
# Copyright (c) 2026 Oracle and/or its affiliates.
# Author: Kamalesh Babulal <kamalesh.babulal@oracle.com>
#

from abc import ABC, abstractmethod
from typing import Sequence


# Keep distro-specific expectations behind one contract so test code can
# consume a uniform API while each distro supplies its own output variants.
# This enforces that every profile explicitly provides all required expected
# output sets (cpu v1, cpu v2, pids) before it can be instantiated.
class DistroConstsBase(ABC):
    """Abstract interface for distro specific expected outputs."""

    name: str = ''

    @property
    @abstractmethod
    def expected_cpu_out_v1(self) -> Sequence[str]:
        """Expected cpu controller output variants for cgroup v1."""

    @property
    @abstractmethod
    def expected_cpu_out_v2(self) -> Sequence[str]:
        """Expected cpu controller output variants for cgroup v2."""

    @property
    @abstractmethod
    def expected_pids_out(self) -> Sequence[str]:
        """Expected pids controller output variants."""

# vim: set et ts=4 sw=4:
