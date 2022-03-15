# SPDX-License-Identifier: LGPL-2.1-only
#
# libcgroup functional tests Makefile.am
#
# Copyright (c) 2019-2021 Oracle and/or its affiliates.
# Author: Tom Hromatka <tom.hromatka@oracle.com>
#

TESTS = ftests.sh ftests-nocontainer.sh

EXTRA_DIST = *.py README.md ftests.sh ftests-nocontainer.sh

clean-local: clean-local-check
.PHONY: clean-local-check
clean-local-check:
	-rm -f *.pyc
