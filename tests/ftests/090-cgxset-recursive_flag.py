#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-only
#
# Advanced cgxset functionality test - set values recursively via the
#                                      '-r' and '-R' flags
#
# Copyright (c) 2023 Oracle and/or its affiliates.
# Author: Kamalesh Babulal <kamalesh.babulal@oracle.com>
#

from cgroup import Cgroup, CgroupVersion as Version
from distro import ConstsCommon as consts
from libcgroup import Mode
import ftests
import sys
import os

CONTROLLERS = ['cpu', 'cpuset', 'pids']

PARENT = '090cgxset'
CHILD = os.path.join(PARENT, 'childcg')
GRANDCHILD = os.path.join(CHILD, 'grandchildcg')

SETTING_V1 = 'cpu.shares'
SETTING_V2 = 'cpu.weight'
SETTING_V1_V2 = 'cpuset.cpus'
SETTING_V2_SUBTREE = 'cgroup.subtree_control'

VALUE_V1 = '512'
VALUE_V2 = '50'
VALUE_V1_V2 = '0'
VALUE_V2_SUBTREE = '+cpuset -pids'
VALUE_V1_V2_SUBTREE = '+cpuset'

DEFAULT_VALUE_V1 = '1024'
DEFAULT_VALUE_V2 = '100'
DEFAULT_VALUE_V1_V2 = '0-1'

CGRP_VER_V1 = Version.CGROUP_V1
CGRP_VER_V2 = Version.CGROUP_V2


def prereqs(config):
    pass


def is_hybrid_with_ctrl(config):
    if Cgroup.get_cgroup_mode(config) == Mode.CGROUP_MODE_HYBRID:
        if (Version.get_version(CONTROLLERS[1]) == CGRP_VER_V2):
            return True

    return False


def setup(config):
    Cgroup.create(config, CONTROLLERS[0], GRANDCHILD)

    if (is_hybrid_with_ctrl(config)):
        Cgroup.create(config, None, GRANDCHILD)
        Cgroup.create(config, CONTROLLERS[1], PARENT)


def cgroup_settings_validate(config, CGRPNAME, SETTING, VALUE, VERSION):
    out = Cgroup.xget(config, cgname=CGRPNAME, setting=SETTING, version=VERSION,
                      values_only=True, print_headers=False)
    if (out != VALUE):
        raise Exception('cgxget expected {} but received {}'.format(VALUE, out))


def cgroup_settings_helper(config, SETTING, VALUE, DEF_VAL, VERSION):

    Cgroup.xset(config, cgname=PARENT, setting=SETTING, value=VALUE,
                version=VERSION, recursive=True)

    cgroup_settings_validate(config, PARENT, SETTING, VALUE, VERSION)

    cgroup_settings_validate(config, CHILD,  SETTING, VALUE, VERSION)
    cgroup_settings_validate(config, GRANDCHILD, SETTING, VALUE, VERSION)

    Cgroup.xset(config, cgname=PARENT, setting=SETTING, value=DEF_VAL,
                version=VERSION, recursive=False)

    cgroup_settings_validate(config, PARENT, SETTING, DEF_VAL, VERSION)
    cgroup_settings_validate(config, CHILD, SETTING, VALUE, VERSION)
    cgroup_settings_validate(config, GRANDCHILD, SETTING, VALUE, VERSION)


def cgroup_subtree_helper(config, SUBTREE, SUBTREE_VAL, VERSION):
    result = consts.TEST_PASSED
    cause = None

    Cgroup.xset(config, cgname=PARENT, setting=SUBTREE, value=SUBTREE_VAL,
                version=VERSION, recursive=True)

    # checking if the controller is enabled in granchildcg, in turn will
    # check its parent cgroup childcg's subtree_control file, if it's enabled
    # in parent cgroup, it's also enabled in the grandparent too.
    # check if 'cpuset' controller is enabled
    if not Cgroup.is_controller_enabled(config, GRANDCHILD, CONTROLLERS[1]):
        result = consts.TEST_FAILED
        cause = 'Controller {} is not enabled in the child cgroup'.format(CONTROLLERS[1])

    if Cgroup.get_cgroup_mode(config) == Mode.CGROUP_MODE_UNIFIED:
        if not Cgroup.is_controller_enabled(config, GRANDCHILD, CONTROLLERS[0]):
            result = consts.TEST_FAILED
            tmp_cause = ('Controller {} is not enabled in the child cgroup'
                         ''.format(CONTROLLERS[0]))
            cause = '\n'.join(filter(None, [cause, tmp_cause]))

        # check if 'pids' controller is enabled
        if Cgroup.is_controller_enabled(config, GRANDCHILD, CONTROLLERS[2]):
            result = consts.TEST_FAILED
            tmp_cause = ('Controller {} is enabled in the child cgroup'
                         ''.format(CONTROLLERS[2]))
            cause = '\n'.join(filter(None, [cause, tmp_cause]))

    # for the grandchildcg read it's cgroup.subtree_control, is_controller_enabled
    # will check its parent cgroup childcg's subtree_control
    if Cgroup.get(config, None, GRANDCHILD, setting='cgroup.subtree_control',
                  print_headers=False, values_only=True):
        result = consts.TEST_FAILED
        tmp_cause = 'Controller {} enabled in grandchild cgroup'.format(CONTROLLERS[1])
        cause = '\n'.join(filter(None, [cause, tmp_cause]))

    return result, cause


def test_cgroup_legacy(config):
    cgroup_settings_helper(config, SETTING_V2, VALUE_V2, DEFAULT_VALUE_V2, CGRP_VER_V2)

    return consts.TEST_PASSED, None


def test_cgroup_hybrid(config):
    result = consts.TEST_PASSED
    cause = None

    cgroup_settings_helper(config, SETTING_V2, VALUE_V2, DEFAULT_VALUE_V2, CGRP_VER_V2)

    if (is_hybrid_with_ctrl(config)):
        result, cause = cgroup_subtree_helper(config, SETTING_V2_SUBTREE,
                                              VALUE_V1_V2_SUBTREE, CGRP_VER_V2)

        cgroup_settings_helper(config, SETTING_V1_V2, VALUE_V1_V2, DEFAULT_VALUE_V1_V2, CGRP_VER_V2)

    return result, cause


def test_cgroup_unified(config):
    Cgroup.xset(config, cgname=PARENT + "/../", setting=SETTING_V2_SUBTREE,
                value=VALUE_V1_V2_SUBTREE, version=CGRP_VER_V2, recursive=False)
    cgroup_settings_helper(config, SETTING_V1, VALUE_V1, DEFAULT_VALUE_V1, CGRP_VER_V1)
    result, cause = cgroup_subtree_helper(config, SETTING_V2_SUBTREE, VALUE_V2_SUBTREE, CGRP_VER_V2)

    return result, cause


def test(config):

    if Cgroup.get_cgroup_mode(config) == Mode.CGROUP_MODE_UNIFIED:
        result, cause = test_cgroup_unified(config)
    elif Cgroup.get_cgroup_mode(config) == Mode.CGROUP_MODE_HYBRID:
        result, cause = test_cgroup_hybrid(config)
    else:
        result, cause = test_cgroup_legacy(config)

    return result, cause


def teardown(config):
    Cgroup.delete(config, CONTROLLERS[0], PARENT, recursive=True)
    if (is_hybrid_with_ctrl(config)):
        Cgroup.delete(config, None, PARENT, recursive=True)


def main(config):
    prereqs(config)

    setup(config)
    [result, cause] = test(config)
    teardown(config)

    return [result, cause]


if __name__ == '__main__':
    config = ftests.parse_args()
    # this test was invoked directly.  run only it
    config.args.num = int(os.path.basename(__file__).split('-')[0])
    sys.exit(ftests.main(config))

# vim: set et ts=4 sw=4:
