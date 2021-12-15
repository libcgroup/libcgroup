#
# Libcgroup Python Bindings
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
@CODE_COVERAGE_RULES@

PY_DISTUTILS = \
	VERSION_RELEASE="@PACKAGE_VERSION@" \
	CPPFLAGS="-I\${top_srcdir}/include ${AM_CPPFLAGS} ${CPPFLAGS}" \
	CFLAGS="$(CODE_COVERAGE_CFLAGS) ${AM_CFLAGS} ${CFLAGS}" \
	LDFLAGS="$(CODE_COVERAGE_LIBS) ${AM_LDFLAGS} ${LDFLAGS}" \
	${PYTHON} ${srcdir}/setup.py

# support silent builds
PY_BUILD_0 = @echo "  PYTHON  " $@; ${PY_DISTUTILS} -q build
PY_BUILD_1 = ${PY_DISTUTILS} build
PY_BUILD_ = ${PY_BUILD_0}
PY_BUILD = ${PY_BUILD_@AM_V@}

PY_INSTALL = ${PY_DISTUTILS} install

EXTRA_DIST = cgroup.pxd libcgroup.pyx setup.py

all-local: build

build: ../libcgroup.la cgroup.pxd libcgroup.pyx setup.py
	[ ${srcdir} == ${builddir} ] || cp ${srcdir}/libcgroup.pyx ${builddir}
	${PY_BUILD} && touch build

install-exec-local: build
	${PY_INSTALL} --install-lib=${DESTDIR}/${pyexecdir} \
		--record=${DESTDIR}/${pyexecdir}/install_files.txt

uninstall-local:
	cat ${DESTDIR}/${pyexecdir}/install_files.txt | xargs ${RM} -f
	${RM} -f ${DESTDIR}/${pyexecdir}/install_files.txt

clean-local:
	[ ${srcdir} == ${builddir} ] || ${RM} -f ${builddir}/libcgroup.pyx
	${RM} -rf libcgroup.c build
