#!/usr/bin/env python

#
# Libcgroup Python Module Build Script
#

#
# Copyright (c) 2021-2022 Oracle and/or its affiliates.
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

from setuptools import Extension, setup
from Cython.Build import cythonize
import os

setup(
    name='libcgroup',
    version=os.environ['VERSION_RELEASE'],
    description='Python bindings for libcgroup',
    url='https://github.com/libcgroup/libcgroup',
    maintainer='Tom Hromatka',
    maintainer_email='tom.hromatka@oracle.com',
    license='LGPLv2.1',
    platforms='Linux',
    ext_modules=cythonize([
            Extension(
                      'libcgroup', ['libcgroup.pyx'],
                      # unable to handle libtool libraries directly
                      extra_objects=['../.libs/libcgroup.a']
                     ),
             ])
)

# vim: set et ts=4 sw=4:
