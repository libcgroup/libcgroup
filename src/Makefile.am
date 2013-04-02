if WITH_BINDINGS
BINDINGS_SUBDIR = bindings
endif

SUBDIRS = . daemon pam tools $(BINDINGS_SUBDIR)

# generate parse.h  from parse.y
AM_YFLAGS = -d

CLEANFILES = lex.c parse.c parse.h

INCLUDES = -I$(top_srcdir)/include
lib_LTLIBRARIES = libcgroup.la
libcgroup_la_SOURCES = parse.h parse.y lex.l api.c config.c libcgroup-internal.h libcgroup.map wrapper.c log.c
libcgroup_la_LIBADD = -lpthread
libcgroup_la_LDFLAGS = -Wl,--version-script,$(srcdir)/libcgroup.map \
	-version-number $(LIBRARY_VERSION_MAJOR):$(LIBRARY_VERSION_MINOR):$(LIBRARY_VERSION_RELEASE)

