SUBDIRS = .
INCLUDES = -I$(top_srcdir)/include

lib_LTLIBRARIES = _libcgroup.la
_libcgroup_la_SOURCES = libcgroup.c
_libcgroup_la_LDFLAGS = $(shell python-config --ldflags) -module -avoid-version
_libcgroup_la_CFLAGS = $(shell python-config --cflags)
_libcgroup_la_LIBADD = $(top_builddir)/src/.libs/libcgroup.la
SWIG=swig


libcgroup.c: libcgroup.p $(top_srcdir)/include/libcgroup.h
	cp libcgroup.p libcgroup.i
	$(CC) $(INCLUDES) -DSWIG -E $(top_srcdir)/include/libcgroup.h >> libcgroup.i
	$(SWIG) -python -o libcgroup.c libcgroup.i
