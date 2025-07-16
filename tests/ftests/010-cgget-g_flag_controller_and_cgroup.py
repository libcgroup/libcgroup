#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-only
#
# Advanced cgget functionality test - '-g' <controller>:<path>
#
# Copyright (c) 2021 Oracle and/or its affiliates.
# Author: Tom Hromatka <tom.hromatka@oracle.com>
#

from distro import ConstsCommon as consts
from distro import consts_distro
from cgroup import Cgroup
import ftests
import utils
import sys
import os

CONTROLLER = 'cpu'
CGNAME = '010cgget'


def prereqs(config):
    pass


def setup(config):
    Cgroup.create(config, CONTROLLER, CGNAME)


def test(config):
    result = consts.TEST_FAILED
    cause = None

    out = Cgroup.get(config, controller='{}:{}'.format(CONTROLLER, CGNAME),
                     print_headers=False)

    EXPECTED_OUT = consts_distro.consts.expected_cpu_out_010()

    for expected_out in EXPECTED_OUT:
        if len(out.splitlines()) == len(expected_out.splitlines()):
            result_, tmp_cause = utils.is_output_same(config, out, expected_out)
            if result_ is True:
                result = consts.TEST_PASSED
                cause = None
                break
            else:
                if cause is None:
                    cause = 'Tried Matching:\n==============='

                cause = '\n'.join(filter(None, [cause, expected_out]))

    return result, cause


def teardown(config):
    Cgroup.delete(config, CONTROLLER, CGNAME)


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
