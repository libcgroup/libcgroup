AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS= -I m4
SUBDIRS = dist doc include samples scripts src
if WITH_TESTS
SUBDIRS += tests
endif

EXTRA_DIST = README_daemon libcgroup.doxyfile README_systemd CONTRIBUTING.md

AM_DISTCHECK_CONFIGURE_FLAGS = --sysconfdir=/etc --localstatedir=/var \
			       --enable-opaque-hierarchy="name=systemd" \
			       --enable-python

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libcgroup.pc
