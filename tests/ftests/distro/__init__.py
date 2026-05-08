# SPDX-License-Identifier: LGPL-2.1-only
#
# Constants for the libcgroup functional tests
#
# Copyright (c) 2019-2021, 2026 Oracle and/or its affiliates.
#
# Authors:
#   Tom Hromatka <tom.hromatka@oracle.com>
#     Originally written.
#
#   Kamalesh Babulal <kamalesh.babulal@oracle.com>
#     Added support for additional distributions.
#

from .consts_distro import DistroConstsSelector
from .consts_oracle import OracleConsts
from .consts_ubuntu import UbuntuConsts
import os


DEFAULT_LOG_FILE = 'libcgroup-ftests.log'

LOG_CRITICAL = 1
LOG_WARNING = 5
LOG_DEBUG = 8
DEFAULT_LOG_LEVEL = 5

ftest_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
tests_dir = os.path.dirname(ftest_dir)
LIBCG_MOUNT_POINT = os.path.dirname(tests_dir)

DEFAULT_CONTAINER_NAME = 'TestLibcg'
DEFAULT_CONTAINER_DISTRO = 'ubuntu'
DEFAULT_CONTAINER_RELEASE = '22.04'
DEFAULT_CONTAINER_ARCH = 'amd64'
DEFAULT_CONTAINER_STOP_TIMEOUT = 5

TESTS_RUN_ALL = -1
TESTS_RUN_ALL_SUITES = 'allsuites'
TEST_PASSED = 'passed'
TEST_FAILED = 'failed'
TEST_SKIPPED = 'skipped'

CGRULES_FILE = '/etc/cgrules.conf'

SUPPORTED_DISTROS = {
    'ubuntu': UbuntuConsts(),
    'oracle linux': OracleConsts(),
}

DISTRO_SELECTOR = DistroConstsSelector(
    SUPPORTED_DISTROS,
    default_distro='ubuntu',
    id_map={'ol': 'oracle linux'},
)

_active_distro = DISTRO_SELECTOR.default_distro
_active_profile = SUPPORTED_DISTROS[_active_distro]

EXPECTED_CPU_OUT_V1 = []
EXPECTED_CPU_OUT_V2 = []
EXPECTED_PIDS_OUT = []


def _set_profile(profile):
    global EXPECTED_CPU_OUT_V1
    global EXPECTED_CPU_OUT_V2
    global EXPECTED_PIDS_OUT

    EXPECTED_CPU_OUT_V1 = list(profile.expected_cpu_out_v1)
    EXPECTED_CPU_OUT_V2 = list(profile.expected_cpu_out_v2)
    EXPECTED_PIDS_OUT = list(profile.expected_pids_out)


def configure_expected_profile(config=None):
    """Configure expected output constants for the detected distro profile."""
    global _active_distro
    global _active_profile

    profile, distro = DISTRO_SELECTOR.resolve(config=config)
    _active_profile = profile
    _active_distro = distro
    _set_profile(profile)

    return _active_distro


def get_active_profile():
    """Return the active distro profile name."""
    return _active_distro


_set_profile(_active_profile)

# vim: set et ts=4 sw=4:
