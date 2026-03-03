#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-only
#
# Test to create a default systemd scope with hyphen using cgcreate
#
# Heavily adapted from 079-sudo-cgcreate_default_systemd_scope.py
#
# Copyright (c) 2026 Oracle and/or its affiliates.
# Author: Kamalesh Babulal<kamalesh.babulal@oracle.com>
#

from process import Process
from systemd import Systemd
from libcgroup import Mode
from cgroup import Cgroup
from run import RunError
from log import Log
import consts
import ftests
import sys
import os

CONTROLLERS = ['cpu', 'pids']
SLICE = 'libcg-test-s.slice'
SCOPE_CGNAME = os.path.join(SLICE, '095cgcreate.scope')
CHILD_CGNAME = 'childcg'

EXP_SLICE = 'libcg.slice/libcg-test.slice/libcg-test-s.slice'
EXP_SCOPE_CGNAME = os.path.join(EXP_SLICE, '095cgcreate.scope')


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

    if not Systemd.is_systemd_enabled():
        result = consts.TEST_SKIPPED
        cause = 'Systemd support not compiled in'

    return result, cause


def setup(config):
    pass


def test(config):
    result = consts.TEST_PASSED
    cause = None

    Cgroup.create_and_validate(config, CONTROLLERS, SCOPE_CGNAME, create_scope=True,
                               set_default_scope=True)

    # get the placeholder PID that libcgroup placed in the scope
    try:
        pid = int(Cgroup.get(config, None, EXP_SCOPE_CGNAME, setting='cgroup.procs',
                             print_headers=False, values_only=True, ignore_systemd=True))
        # use the pid variable so that lint is happy
        Log.log_debug('Cgroup {} has pid {}'.format(EXP_SCOPE_CGNAME, pid))
    except RunError:
        result = consts.TEST_FAILED
        cause = "Failed to read pid in {}'s cgroup.procs".format(EXP_SCOPE_CGNAME)
        return result, cause

    Cgroup.create_and_validate(config, None, CHILD_CGNAME)

    return result, cause


def teardown(config, result, cause):
    Cgroup.delete(config, None, CHILD_CGNAME)

    pid = int(Cgroup.get(config, None, EXP_SCOPE_CGNAME, setting='cgroup.procs',
                         print_headers=False, values_only=True, ignore_systemd=True))
    Process.kill(config, pid)

    if result != consts.TEST_PASSED:
        # Something went wrong.  Let's force the removal of the cgroups just to be safe.
        # Note that this should remove the cgroup, but it won't remove it from systemd's
        # internal caches, so the system may not return to its 'pristine' prior-to-this-test
        # state
        pid = int(Cgroup.get(config, None, SCOPE_CGNAME, setting='cgroup.procs',
                             print_headers=False, values_only=True, ignore_systemd=True))
        Process.kill(config, pid)

        try:
            Cgroup.delete(config, None, SLICE)
        except RunError:
            pass

    # Regardless of whether the test passes or fails, the expanded slice name must be
    # removed, since this hierarchy will not be used by other test cases.
    # Removal of the slice hierarchy may fail for two reasons:
    # 1. The attempt to kill the process (libcgroup_systemd_idle_thread) was unsuccessful.
    # 2. The expanded slice cgroup hierarchy was never created.
    # In either case, further investigation is required and the test case should be
    # marked as failed.
    # The previous warning about system state applies here as well.
    try:
        Cgroup.delete(config, None, EXP_SLICE.partition('/')[0], recursive=True)
    except RunError as e:
        result = consts.TEST_FAILED
        tmp_cause = "Failed to remove expanded slice {}: {}".format(EXP_SLICE, e)
        cause = '\n'.join(filter(None, [cause, tmp_cause]))

    return result, cause


def main(config):
    [result, cause] = prereqs(config)
    if result != consts.TEST_PASSED:
        return [result, cause]

    setup(config)

    [result, cause] = test(config)
    [result, cause] = teardown(config, result, cause)

    return [result, cause]


if __name__ == '__main__':
    config = ftests.parse_args()
    # this test was invoked directly.  run only it
    config.args.num = int(os.path.basename(__file__).split('-')[0])
    sys.exit(ftests.main(config))

# vim: set et ts=4 sw=4:
