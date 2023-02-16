#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-only
#
# set_default_systemd_cgroup functionality test using the python bindings
#
# Copyright (c) 2023 Oracle and/or its affiliates.
# Author: Kamalesh Babulal <kamalesh.babulal@oracle.com>
#

from cgroup import Cgroup as CgroupCli, Mode
from libcgroup import Version, Cgroup
import consts
import ftests
import sys
import os


CONTROLLER = 'cpu'
SYSTEMD_CGNAME = '071_cg_in_scope'
OTHER_CGNAME = '071_cg_not_in_scope'

SLICE = 'libcgtests.slice'
SCOPE = 'test071.scope'

CONFIG_FILE_NAME = os.path.join(os.getcwd(), '071cgconfig.conf')
SYSTEMD_DEFAULT_CGROUP_DIR = '/var/run/libcgroup'
SYSTEMD_DEFAULT_CGROUP_FILE = '/var/run/libcgroup/systemd'

# List of libcgroup.Cgroup objects
CGRPS_LIST = []
MODE = Mode.CGROUP_MODE_UNK


def prereqs(config):
    result = consts.TEST_PASSED
    cause = None

    if config.args.container:
        result = consts.TEST_SKIPPED
        cause = 'This test cannot be run within a container'

    return result, cause


def create_cgrp(config, CGNAME, controller=CONTROLLER, ignore_systemd=False):
    global CGRPS_LIST, MODE

    result = consts.TEST_PASSED
    cause = None

    cgrp = Cgroup(CGNAME, Version.CGROUP_V1)
    if controller is not None:
        cgrp.add_controller(controller)
    cgrp.create()

    if not CgroupCli.exists(config, controller, CGNAME, ignore_systemd=ignore_systemd):
        result = consts.TEST_FAILED
        if MODE == Mode.CGROUP_MODE_UNIFIED:
            cause = 'Failed to create {}'.format(os.path.join('/sys/fs/cgroup', CGNAME))
        else:
            cause = (
                        'Failed to create {}'
                        ''.format(os.path.join('/sys/fs/cgroup', (controller or ''), CGNAME))
                    )
        return result, cause

    CGRPS_LIST.append(cgrp)

    return result, cause


def setup(config):
    global MODE

    result = consts.TEST_PASSED
    cause = None

    # probe the current cgroup set up mode
    MODE = int(Cgroup.cgroup_mode())

    if not os.path.isdir(SYSTEMD_DEFAULT_CGROUP_DIR):
        os.mkdir(SYSTEMD_DEFAULT_CGROUP_DIR)

    # Emulate the systemd slice/scope creation
    f = open(SYSTEMD_DEFAULT_CGROUP_FILE, 'w')
    f.write(os.path.join(SLICE, SCOPE))
    f.close()

    if not os.path.exists(SYSTEMD_DEFAULT_CGROUP_FILE):
        result = consts.TEST_FAILED
        cause = 'Failed to create %s' % SYSTEMD_DEFAULT_CGROUP_FILE
        return result, cause

    # create /sys/fs/cgroup/cpu/libcgtests.slice (v1) or /sys/fs/cgroup/libcgtests.slice (v2)
    result, cause = create_cgrp(config, SLICE, ignore_systemd=True)
    if result == consts.TEST_FAILED:
        return result, cause

    # create /sys/fs/cgroup/cpu/libcgtests.slice/test071.scope (v1) or
    #        /sys/fs/cgroup/libcgtests.slice/tests071.scope (v2)
    result, cause = create_cgrp(config, os.path.join(SLICE, SCOPE), ignore_systemd=True)
    if result == consts.TEST_FAILED:
        return result, cause

    # In hybrid mode /sys/fs/cgroup/unified/libcgtests.slice/test071.scope
    # is created by the systemd and we relay on it for checking if the
    # slice and scope were set as default systemd cgroup (new cgroup root)
    if MODE == Mode.CGROUP_MODE_HYBRID:
        result, cause = create_cgrp(config, SLICE, None, ignore_systemd=True)
        if result == consts.TEST_FAILED:
            return result, cause

        result, cause = create_cgrp(config, os.path.join(SLICE, SCOPE), None, ignore_systemd=True)

    return result, cause


def test(config):
    result = consts.TEST_PASSED
    cause = None

    # Create cgroup before setting the default systemd cgroup
    result, cause = create_cgrp(config, OTHER_CGNAME, ignore_systemd=True)
    if result == consts.TEST_FAILED:
        return result, cause

    Cgroup.cgroup_set_default_systemd_cgroup()

    # Create cgroup after setting the default systemd cgroup
    result, cause = create_cgrp(config, SYSTEMD_CGNAME)
    if result == consts.TEST_FAILED:
        return result, cause

    return result, cause


def teardown(config):
    global CGRPS_LIST

    # the last object in the list is created with the default
    # systemd cgroup set for others we need unset it
    cgroup = CGRPS_LIST.pop()
    cgroup.delete()

    # unset the default systemd cgroup by deleting the
    # /var/run/libcgroup/systemd and calling
    # cgroup_set_default_systemd_cgroup()
    if os.path.exists(SYSTEMD_DEFAULT_CGROUP_FILE):
        os.unlink(SYSTEMD_DEFAULT_CGROUP_FILE)

    Cgroup.cgroup_set_default_systemd_cgroup()

    for cgroup in reversed(CGRPS_LIST):
        cgroup.delete()


def main(config):
    [result, cause] = prereqs(config)
    if result != consts.TEST_PASSED:
        return [result, cause]

    [result, cause] = setup(config)
    if result != consts.TEST_PASSED:
        return [result, cause]

    try:
        [result, cause] = test(config)
    finally:
        teardown(config)

    return [result, cause]


if __name__ == '__main__':
    config = ftests.parse_args()
    # this test was invoked directly.  run only it
    config.args.num = int(os.path.basename(__file__).split('-')[0])
    sys.exit(ftests.main(config))

# vim: set et ts=4 sw=4:
