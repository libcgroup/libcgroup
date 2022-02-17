#
# Constants for the libcgroup functional tests
#
# Copyright (c) 2019-2021 Oracle and/or its affiliates.
# Author: Tom Hromatka <tom.hromatka@oracle.com>
#

#
# This library is free software; you can redistribute it and/or modify it
# under the terms of version 2.1 of the GNU Lesser General Public License as
# published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
# for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this library; if not, see <http://www.gnu.org/licenses>.
#

import os

DEFAULT_LOG_FILE = 'libcgroup-ftests.log'

LOG_CRITICAL = 1
LOG_WARNING = 5
LOG_DEBUG = 8
DEFAULT_LOG_LEVEL = 5

ftest_dir = os.path.dirname(os.path.abspath(__file__))
tests_dir = os.path.dirname(ftest_dir)
LIBCG_MOUNT_POINT = os.path.dirname(tests_dir)

DEFAULT_CONTAINER_NAME = 'TestLibcg'
DEFAULT_CONTAINER_DISTRO = 'ubuntu'
DEFAULT_CONTAINER_RELEASE = '18.04'
DEFAULT_CONTAINER_ARCH = 'amd64'
DEFAULT_CONTAINER_STOP_TIMEOUT = 5

TESTS_RUN_ALL = -1
TESTS_RUN_ALL_SUITES = 'allsuites'
TEST_PASSED = 'passed'
TEST_FAILED = 'failed'
TEST_SKIPPED = 'skipped'

CGRULES_FILE = '/etc/cgrules.conf'

# vim: set et ts=4 sw=4:
