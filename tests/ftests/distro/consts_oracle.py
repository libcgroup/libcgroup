# SPDX-License-Identifier: LGPL-2.1-only
#
# Oracle Linux controller settings constants for the
# libcgroup functional tests
#
# Copyright (c) 2025 Oracle and/or its affiliates.
#
# Author: Tom Hromatka <tom.hromatka@oracle.com>
# Author: Kamalesh Babulal <kamalesh.babulal@oracle.com>
#

from distro import ConstsCommon as consts
from distro.consts_abstract import ConstsAbstract
from cgroup import CgroupVersion

EXPECTED_CPU_OUT_V1 = [
    # OL8 4.18.0-553.58.1
    """cpu.cfs_period_us: 100000
    cpu.stat: nr_periods 0
            nr_throttled 0
            throttled_time 0
    cpu.shares: 1024
    cpu.cfs_quota_us: -1
    cpu.rt_runtime_us: 0
    cpu.rt_period_us: 1000000""",
    # OL8 5.15.0-309.180.4
    """cpu.cfs_burst_us: 0
    cpu.cfs_period_us: 100000
    cpu.stat: nr_periods 0
            nr_throttled 0
            throttled_time 0
    cpu.shares: 1024
    cpu.idle: 0
    cpu.cfs_quota_us: -1"""
]

EXPECTED_CPU_OUT_V2 = [
    # OL8 4.18.0-553.58.1
    """cpu.weight: 100
    cpu.stat: usage_usec 0
            user_usec 0
            system_usec 0
            nr_periods 0
            nr_throttled 0
            throttled_usec 0
    cpu.weight.nice: 0
    cpu.max: max 100000""",
    # OL8 5.15.0-309.180.4
    """cpu.weight: 100
    cpu.stat: usage_usec 0
            user_usec 0
            system_usec 0
            nr_periods 0
            nr_throttled 0
            throttled_usec 0
    cpu.weight.nice: 0
    cpu.pressure:
    cpu.idle: 0
    cpu.max.burst: 0
    cpu.max: max 100000"""
]


class ConstsOracle(ConstsAbstract):

    def expected_cpu_out_009(self, controller='cpu'):
        version = CgroupVersion.get_version(controller)
        if version == CgroupVersion.CGROUP_V1:
            return EXPECTED_CPU_OUT_V1

        return EXPECTED_CPU_OUT_V2

    def expected_cpu_out_010(self, controller='cpu'):
        return self.expected_cpu_out_009(controller)

    def expected_cpu_out_013(self, controller='cpu'):
        EXPECTED_CPU_OUT = self.expected_cpu_out_009(controller)
        # Append pid controller [0] and cpu controller [N - 2]
        EXPECTED_OUT = [consts.EXPECTED_PIDS_OUT[0] + expected_out
                        for expected_out in EXPECTED_CPU_OUT[:-2]]
        # Append pid controller [1] and cpu controller [N, N - 1]
        EXPECTED_OUT.extend(consts.EXPECTED_PIDS_OUT[1] + expected_out
                            for expected_out in EXPECTED_CPU_OUT[-2:])
        # Append pid controller [2] and cpu controller [N, N - 1]
        EXPECTED_OUT.extend(consts.EXPECTED_PIDS_OUT[2] + expected_out
                            for expected_out in EXPECTED_CPU_OUT[-2:])

        return EXPECTED_OUT

# vim: set et ts=4 sw=4:
