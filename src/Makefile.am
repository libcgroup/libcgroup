@CODE_COVERAGE_RULES@

BUILT_SOURCES = parse.c parse.h

if WITH_BINDINGS
BINDINGS_SUBDIR = bindings
endif

SUBDIRS = . daemon pam tools $(BINDINGS_SUBDIR)

# generate parse.h  from parse.y
AM_YFLAGS = -d

CLEANFILES = lex.c parse.c parse.h

AM_CPPFLAGS = -I$(top_srcdir)/include

lib_LTLIBRARIES = libcgroup.la
libcgroup_la_SOURCES = parse.h parse.y lex.l api.c config.c libcgroup-internal.h libcgroup.map wrapper.c log.c
libcgroup_la_LIBADD = -lpthread $(CODE_COVERAGE_LIBS)
libcgroup_la_CFLAGS = $(CODE_COVERAGE_CFLAGS) -DSTATIC=static
libcgroup_la_LDFLAGS = -Wl,--version-script,$(srcdir)/libcgroup.map \
	-version-number $(LIBRARY_VERSION_MAJOR):$(LIBRARY_VERSION_MINOR):$(LIBRARY_VERSION_RELEASE)

noinst_LTLIBRARIES = libcgroupfortesting.la
libcgroupfortesting_la_SOURCES = $(libcgroup_la_SOURCES)
libcgroupfortesting_la_LIBADD = -lpthread $(CODE_COVERAGE_LIBS)
libcgroupfortesting_la_CFLAGS = $(CODE_COVERAGE_CFLAGS) -DSTATIC= -DUNIT_TEST
libcgroupfortesting_la_LDFLAGS = -Wl,--version-script,$(top_srcdir)/tests/gunit/libcgroup_unittest.map \
	-version-number $(LIBRARY_VERSION_MAJOR):$(LIBRARY_VERSION_MINOR):$(LIBRARY_VERSION_RELEASE)
