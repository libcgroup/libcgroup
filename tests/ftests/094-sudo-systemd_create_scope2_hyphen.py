#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-only
#
# Create a systemd scope with an existing PID and slice name
# with hyphen.
#
# Heavily adapted from 058-sudo-systemd_create_scope2.py
#
# Copyright (c) 2026 Oracle and/or its affiliates.
# Author: Kamalesh Babulal <kamalesh.babulal@oracle.com>
#

from cgroup import CgroupVersion as CgroupCliVersion
from cgroup import Cgroup as CgroupCli
from libcgroup import Cgroup, Version
from systemd import Systemd
from process import Process
from run import RunError
import ftests
import consts
import utils
import stat
import sys
import os

pid = None
CGNAME = 'libcg-test-s.slice/094delegated.scope'
EXP_CGNAME = 'libcg.slice/libcg-test.slice/libcg-test-s.slice/094delegated.scope'

# Which controller isn't all that important, but it is important that we
# have a cgroup v2 controller
CONTROLLER = 'cpu'

# 0751
DIR_MODE = stat.S_IRWXU | stat.S_IRGRP | stat.S_IXGRP | stat.S_IXOTH
# 0644
CTRL_MODE = stat.S_IRUSR | stat.S_IWUSR | stat.S_IRGRP | stat.S_IROTH
# 0775
TASK_MODE = stat.S_IRWXU | stat.S_IRWXG | stat.S_IXOTH

TASKS_UID = 2468
TASKS_GID = 3579
CTRL_UID = 4680
CTRL_GID = 5791


def prereqs(config):
    result = consts.TEST_PASSED
    cause = None

    if config.args.container:
        result = consts.TEST_SKIPPED
        cause = 'This test cannot be run within a container'
        return result, cause

    if CgroupCliVersion.get_version(CONTROLLER) != CgroupCliVersion.CGROUP_V2:
        result = consts.TEST_SKIPPED
        cause = 'This test requires cgroup v2'

    if not Systemd.is_systemd_enabled():
        result = consts.TEST_SKIPPED
        cause = 'Systemd support not compiled in'

    return result, cause


def setup(config):
    pass


def test(config):
    global pid

    result = consts.TEST_PASSED
    cause = None

    pid = config.process.create_process(config)

    cg = Cgroup(CGNAME, Version.CGROUP_V2)

    cg.add_controller(CONTROLLER)
    cg.set_permissions(DIR_MODE, CTRL_MODE, TASK_MODE)
    cg.set_uid_gid(TASKS_UID, TASKS_GID, CTRL_UID, CTRL_GID)

    cg.create_scope2(ignore_ownership=False, pid=pid)

    if os.path.exists(os.path.basename(CGNAME)):
        result = consts.TEST_FAILED
        cause = 'Cgroup hierarchy with non-expanded slice name created: {}'.format(CGNAME)

    if not Systemd.is_delegated(config, os.path.basename(EXP_CGNAME)):
        result = consts.TEST_FAILED
        cause = 'Cgroup is not delegated'

    if not CgroupCli.is_controller_enabled(config, EXP_CGNAME, CONTROLLER):
        result = consts.TEST_FAILED
        tmp_cause = 'Controller {} is not enabled in the parent cgroup'.format(CONTROLLER)
        cause = '\n'.join(filter(None, [cause, tmp_cause]))

    dir_path = os.path.join(CgroupCli.get_controller_mount_point(CONTROLLER), EXP_CGNAME)

    dir_mode = utils.get_file_permissions(config, dir_path)
    if int(dir_mode, 8) != DIR_MODE:
        result = consts.TEST_FAILED
        tmp_cause = "Expected directory mode to be {} but it's {}".format(
                    format(DIR_MODE, '03o'), dir_mode)
        cause = '\n'.join(filter(None, [cause, tmp_cause]))

    ctrl_path = os.path.join(CgroupCli.get_controller_mount_point(CONTROLLER), EXP_CGNAME,
                             'cgroup.procs')

    ctrl_mode = utils.get_file_permissions(config, ctrl_path)
    if int(ctrl_mode, 8) != CTRL_MODE:
        result = consts.TEST_FAILED
        tmp_cause = "Expected cgroup.procs mode to be {} but it's {}".format(
                    format(CTRL_MODE, '03o'), ctrl_mode)
        cause = '\n'.join(filter(None, [cause, tmp_cause]))

    uid = utils.get_file_owner_uid(config, ctrl_path)
    if uid != CTRL_UID:
        result = consts.TEST_FAILED
        tmp_cause = "Expected cgroup.procs owner to be {} but it's {}".format(CTRL_UID, uid)
        cause = '\n'.join(filter(None, [cause, tmp_cause]))

    gid = utils.get_file_owner_gid(config, ctrl_path)
    if gid != CTRL_GID:
        result = consts.TEST_FAILED
        tmp_cause = "Expected cgroup.procs group to be {} but it's {}".format(CTRL_GID, gid)
        cause = '\n'.join(filter(None, [cause, tmp_cause]))

    return result, cause


def teardown(config, result):
    result_tmp = consts.TEST_PASSED
    cause_tmp = None

    Process.kill(config, pid)

    if result != consts.TEST_PASSED:
        # Something went wrong.  Let's force the removal of the cgroups just to be safe.
        # Note that this should remove the cgroup, but it won't remove it from systemd's
        # internal caches, so the system may not return to its 'pristine' prior-to-this-test
        # state
        try:
            CgroupCli.delete(config, None, CGNAME)
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
        CgroupCli.delete(config, None, EXP_CGNAME.split('/')[0], recursive=True)
    except RunError as e:
        cause_tmp = "Failed to remove expanded slice {}: {}".format(EXP_CGNAME, e)
        result_tmp = consts.TEST_FAILED

    return result_tmp, cause_tmp


def main(config):
    [result, cause] = prereqs(config)
    if result != consts.TEST_PASSED:
        return [result, cause]

    try:
        result = consts.TEST_FAILED
        setup(config)
        [result, cause] = test(config)
    finally:
        [result, cause] = teardown(config, result)

    return [result, cause]


if __name__ == '__main__':
    config = ftests.parse_args()
    # this test was invoked directly.  run only it
    config.args.num = int(os.path.basename(__file__).split('-')[0])
    sys.exit(ftests.main(config))

# vim: set et ts=4 sw=4:
