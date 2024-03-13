# SPDX-License-Identifier: LGPL-2.1-only
#
# Libcgroup list class
#
# Copyright (c) 2021-2024 Oracle and/or its affiliates.
# Author: Tom Hromatka <tom.hromatka@oracle.com>
#

#
#!/usr/bin/env python3
#
# Libcgroup list class
#
# Copyright (c) 2021-2024 Oracle and/or its affiliates.
# Author: Tom Hromatka <tom.hromatka@oracle.com>
#

# pip install treelib
# https://treelib.readthedocs.io/en/latest/
from treelib import Node, Tree
import os

from libcgroup import Version

from libcgrouptree import LibcgroupTree
from libcgrouputils import LibcgroupPid

float_metrics = ['%usr', '%system', '%guest', '%wait', '%CPU', '%MEM', 'minflt/s', 'majflt/s']
int_metrics = ['Time', 'UID', 'PID', 'CPU', 'RSS', 'threads', 'fd-nr']
str_metrics = ['Command']

class LibcgroupList(LibcgroupTree):
    def __init__(self, name, version=Version.CGROUP_V2, controller='cpu', depth=None,
                 metric='%CPU', threshold=1.0, limit=None):
        super().__init__(name, version, controller, depth=depth, files=False)

        self.metric = metric
        self.threshold = threshold
        self.cgpid_list = list()
        self.limit = limit

    def walk_action(self, cg):
        cg.get_pids()

        for pid in cg.pids:
            cgpid = LibcgroupPid.create_from_pidstat(pid)

            try:
                cgpid.cgroup = cg.path[len(self.start_path):]

                if self.metric in float_metrics:
                   if float(cgpid.pidstats[self.metric]) >= self.threshold:
                        self.cgpid_list.append(cgpid)

                elif self.metric in int_metrics:
                    if int(cgpid.pidstats[self.metric]) >= self.threshold:
                        self.cgpid_list.append(cgpid)

                else:
                    self.cgpid_list.append(cgpid)

            except AttributeError:
                # The pid could have been deleted between when we read cgroup.procs
                # and when we ran pidstat.  Ignore it and move on
                pass

    def sort(self):
        if self.metric in float_metrics:
            self.cgpid_list = sorted(self.cgpid_list, reverse=True,
                                     key=lambda cgpid: float(cgpid.pidstats[self.metric]))

        elif self.metric in int_metrics:
            self.cgpid_list = sorted(self.cgpid_list, reverse=True,
                                     key=lambda cgpid: int(cgpid.pidstats[self.metric]))

        else:
            self.cgpid_list = sorted(self.cgpid_list, reverse=True,
                                     key=lambda cgpid: cgpid.pidstats[self.metric])

    def show(self, sort=True):
        if sort:
            self.sort()

        print('{0: >10} {1: >16}  {2: >8} {3: <50}'.format(
            'PID', 'COMMAND', self.metric, 'CGROUP'))

        for i, cgpid in enumerate(self.cgpid_list):
            if self.limit and i >= self.limit:
                break

            if self.metric in float_metrics:
                print('{0: >10} {1: >16} {2: 9.2f} {3: <50}'.format(cgpid.pid,
                      cgpid.pidstats['Command'], float(cgpid.pidstats[self.metric]),
                      cgpid.cgroup))
            elif self.metric in int_metrics:
                print('{0: >10} {1: >16}    {2: 7d} {3: <50}'.format(cgpid.pid,
                      cgpid.pidstats['Command'], int(cgpid.pidstats[self.metric]),
                      cgpid.cgroup))
            else:
                print('{0: >10} {1: >16}    {2: >6} {3: <50}'.format(cgpid.pid,
                      cgpid.pidstats['Command'], cgpid.pidstats[self.metric],
                      cgpid.cgroup))

class LibcgroupPsiList(LibcgroupTree):
    def __init__(self, name, controller='cpu', depth=None, psi_field='some-avg10',
                 threshold=None, limit=None):
        super().__init__(name, version=Version.CGROUP_V2, controller=controller,
                         depth=depth)

        self.controller = controller
        self.psi_field = psi_field
        self.cglist = list()
        self.threshold = threshold
        self.limit = limit

    def walk_action(self, cg):
        cg.get_psi(self.controller)

        if not self.threshold:
            self.cglist.append(cg)
        elif cg.psi[self.psi_field] >= self.threshold:
            self.cglist.append(cg)

    def sort(self):
        self.cglist = sorted(self.cglist, reverse=True,
                             key=lambda cg: cg.psi[self.psi_field])

    def _show_float(self):
        print('{0: >10} {1: >3} {2: <16}'.format(self.psi_field, 'PSI', 'CGROUP'))

        for i, cg in enumerate(self.cglist):
            if self.limit and i >= self.limit:
                break

            print('        {0: 6.2f} {1: <16}'.format(cg.psi[self.psi_field],
                                                      cg.path[len(self.start_path):]))

    def _show_int(self):
        print('{0: >10} {1: >3} {2: <16}'.format(self.psi_field, 'PSI', 'CGROUP'))

        for i, cg in enumerate(self.cglist):
            if self.limit and i >= self.limit:
                break

            print('   {0: 11d} {1: <16}'.format(cg.psi[self.psi_field],
                                                cg.path[len(self.start_path):]))

    def show(self, sort=True):
        if sort:
            self.sort()

        if 'total' in self.psi_field:
            self._show_int()
        else:
            self._show_float()
