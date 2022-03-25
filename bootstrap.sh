#!/bin/bash
# SPDX-License-Identifier: LGPL-2.1-only

set -ex

if [ -f .git/hooks/pre-commit.sample ] && [ ! -f .git/hooks/pre-commit ] ; then
	cp -p .git/hooks/pre-commit.sample .git/hooks/pre-commit && \
	chmod +x .git/hooks/pre-commit && \
	echo "Activated pre-commit hook."
fi

# update the git submodules - libcgroup-tests and googletest
git submodule update --init --recursive

# configure libcgroup-tests
pushd tests
git checkout main
popd

# configure googletest
pushd googletest/googletest
git checkout release-1.8.0
cmake -DBUILD_SHARED_LIBS=ON .
make
popd

test -d m4 || mkdir m4
autoreconf -fi
rm -fr autom4te.cache

CFLAGS="$CFLAGS -g -O0" ./configure --sysconfdir=/etc --localstatedir=/var \
	--enable-opaque-hierarchy="name=systemd"

make clean
