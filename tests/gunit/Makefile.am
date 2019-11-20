#
# libcgroup googletests Makefile.am
#
# Copyright (c) 2019 Oracle and/or its affiliates.  All rights reserved.
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

AM_CPPFLAGS = -I$(top_srcdir)/include \
	      -I$(top_srcdir)/src \
	      -I$(top_builddir)/googletest/googletest/include \
	      -I$(top_builddir)/googletest/googletest \
	      -std=c++11 \
	      -DSTATIC= \
	      -DUNIT_TEST
LDADD = ../../src/.libs/libcgroupfortesting.la \
	libgtest.la

libgtest_la_SOURCES = ../../googletest/googletest/src/gtest-all.cc
libgtest_la_CPPFLAGS = -I$(top_builddir)/googletest/googletest/include \
		       -I$(top_builddir)/googletest/googletest
libgtest_la_LDFLAGS = -lpthread

check_LTLIBRARIES = libgtest.la
check_PROGRAMS = gtest
TESTS = gtest

gtest_SOURCES = gtest.cpp \
		001-path.cpp \
		002-cgroup_parse_rules_options.cpp \
		003-cg_get_cgroups_from_proc_cgroups.cpp \
		004-cgroup_compare_ignore_rule.cpp \
		005-cgroup_compare_wildcard_procname.cpp
