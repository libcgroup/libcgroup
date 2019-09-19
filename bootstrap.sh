#!/bin/bash

set -ex

if [ -f .git/hooks/pre-commit.sample -a ! -f .git/hooks/pre-commit ] ; then
	cp -p .git/hooks/pre-commit.sample .git/hooks/pre-commit && \
	chmod +x .git/hooks/pre-commit && \
	echo "Activated pre-commit hook."
fi

# configure googletest
git submodule update --init --recursive
pushd googletest
git checkout release-1.8.1
cmake .
make
popd

aclocal
libtoolize -c
autoconf
autoheader
automake --foreign --add-missing --copy

CFLAGS="$CFLAGS -g -O0" ./configure --sysconfdir=/etc --localstatedir=/var

make clean
