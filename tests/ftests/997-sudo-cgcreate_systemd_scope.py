#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-only
#
# Stress test to create a systemd scope using cgcreate by
# passing invalid systemd slice and scope names
#
# Copyright (c) 2023 Oracle and/or its affiliates.
# Author: Kamalesh Babulal <kamalesh.babulal@oracle.com>
#

from cgroup import Cgroup
from process import Process
from libcgroup import Mode
from run import RunError
import consts
import ftests
import sys
import os

CONTROLLERS = ['cpu', 'pids']
SLICE = 'libcgroup.slice'
SCOPE = '997cgstress.scope'
INVAL_SLICE_NAMES = ['997', '997.slic1', '997.slice1', '.slice']
INVAL_SCOPE_NAMES = ['997', '997.scop1', '997.scope1', '.scope', 'cpu.scope']


def prereqs(config):
    result = consts.TEST_PASSED
    cause = None

    if config.args.container:
        result = consts.TEST_SKIPPED
        cause = 'This test cannot be run within a container'
        return result, cause

    if Cgroup.get_cgroup_mode(config) != Mode.CGROUP_MODE_UNIFIED:
        result = consts.TEST_SKIPPED
        cause = 'This test requires the unified cgroup hierarchy'

    return result, cause


def setup(config):
    pass


def test(config):
    result = consts.TEST_PASSED
    cause = None

    for slice_name in INVAL_SLICE_NAMES:
        CGNAME = os.path.join(slice_name, SCOPE)

        try:
            Cgroup.create_and_validate(config, CONTROLLERS, CGNAME, create_scope=True)
        except RunError as re:
            if 'Invalid unit name' not in str(re.stdout):
                raise re
        else:
            result = consts.TEST_FAILED
            tmp_cause = 'Erroneously succeeded in creating slice {}', format(CGNAME)
            cause = '\n'.join(filter(None, [cause, tmp_cause]))

    for scope_name in INVAL_SCOPE_NAMES:
        CGNAME = os.path.join(SLICE, scope_name)

        try:
            Cgroup.create_and_validate(config, CONTROLLERS, CGNAME, create_scope=True)
        except RunError as re:
            if (
                    'Invalid unit name' not in str(re.stdout) and
                    'Invalid scope name, using controller name' not in str(re.stdout)
               ):
                raise re
        else:
            result = consts.TEST_FAILED
            tmp_cause = 'Erroneously succeeded in creating scope {}', format(CGNAME)
            cause = '\n'.join(filter(None, [cause, tmp_cause]))

    return result, cause


def teardown(config):
    # try deleting INVAL_[SLICE, SCOPE] CGNAME for the cases, where it was erroneously created.
    for slice_name in INVAL_SLICE_NAMES:
        CGNAME = os.path.join(slice_name, SCOPE)

        try:
            pid = int(Cgroup.get(config, None, CGNAME, setting='cgroup.procs',
                      print_headers=False, values_only=True, ignore_systemd=True))
            print(pid)
            Process.kill(config, pid)
        except RunError:
            pass

    for scope_name in INVAL_SCOPE_NAMES:
        CGNAME = os.path.join(SLICE, scope_name)

        try:
            pid = int(Cgroup.get(config, None, CGNAME, setting='cgroup.procs',
                      print_headers=False, values_only=True, ignore_systemd=True))
            Process.kill(config, pid)
        except RunError:
            pass


def main(config):
    [result, cause] = prereqs(config)
    if result != consts.TEST_PASSED:
        return [result, cause]

    setup(config)

    [result, cause] = test(config)
    if result != consts.TEST_PASSED:
        teardown(config)

    return [result, cause]


if __name__ == '__main__':
    config = ftests.parse_args()
    # this test was invoked directly.  run only it
    config.args.num = int(os.path.basename(__file__).split('-')[0])
    sys.exit(ftests.main(config))

# vim: set et ts=4 sw=4:
