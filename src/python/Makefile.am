# SPDX-License-Identifier: LGPL-2.1-only
#
# Libcgroup Python Bindings
#
# Copyright (c) 2021-2022 Oracle and/or its affiliates.
# Author: Tom Hromatka <tom.hromatka@oracle.com>
#
@CODE_COVERAGE_RULES@

PY_DISTUTILS = \
	VERSION_RELEASE="@PACKAGE_VERSION@" \
	CPPFLAGS="-I\${top_srcdir}/include -I\${top_builddir}/include ${AM_CPPFLAGS} ${CPPLAGS} -O2" \
	CFLAGS="$(CODE_COVERAGE_CFLAGS) ${AM_CFLAGS} ${CFLAGS}" \
	LDFLAGS="$(CODE_COVERAGE_LIBS) ${AM_LDFLAGS} ${LDFLAGS}" \
	${PYTHON} ${srcdir}/setup.py

# support silent builds
PY_BUILD_0 = @echo "  PYTHON  " $@; ${PY_DISTUTILS} -q build
PY_BUILD_1 = ${PY_DISTUTILS} build
PY_BUILD_ = ${PY_BUILD_0}
PY_BUILD = ${PY_BUILD_@AM_V@}

PY_INSTALL = ${PY_DISTUTILS} install

EXTRA_DIST = cgroup.pxd.m4 libcgroup.pyx.m4 setup.py.m4 \
	     cgroup.pxd libcgroup.pyx setup.py

if WITH_SYSTEMD
M4_FLAGS = -D WITH_SYSTEMD
else
M4_FLAGS = -U WITH_SYSTEMD
endif

all-local: build

buildm4:
	m4 $(M4_FLAGS) ${srcdir}/cgroup.pxd.m4 > ${builddir}/cgroup.pxd
	m4 $(M4_FLAGS) ${srcdir}/libcgroup.pyx.m4 > ${builddir}/libcgroup.pyx
	m4 $(M4_FLAGS) ${srcdir}/setup.py.m4 > ${builddir}/setup.py

cgroup.pxd: cgroup.pxd.m4 buildm4
libcgroup.pyx: libcgroup.pyx.m4 buildm4
setup.py: setup.py.m4 buildm4

build: ../libcgroup.la buildm4
	${PY_BUILD} && touch build

install-exec-local: build
	${PY_INSTALL} --install-lib=${DESTDIR}/${pyexecdir} \
		--record=${DESTDIR}/${pyexecdir}/install_files.txt

uninstall-local:
	cat ${DESTDIR}/${pyexecdir}/install_files.txt | xargs ${RM} -f
	${RM} -f ${DESTDIR}/${pyexecdir}/install_files.txt

clean-local:
	${RM} -f ${builddir}/libcgroup.pyx
	${RM} -f ${builddir}/cgroup.pxd
	${RM} -f ${builddir}/setup.py
	${RM} -rf libcgroup.c build libcgroup.egg-info dist
