SUBDIRS = . daemon pam tools

# generate parse.h  from parse.y
AM_YFLAGS = -d

INCLUDES = -I$(top_srcdir)/include
lib_LTLIBRARIES = libcgroup.la
libcgroup_la_SOURCES = parse.y lex.l api.c config.c libcgroup-internal.h libcgroup.map wrapper.c
libcgroup_la_LIBADD = -lpthread
libcgroup_la_LDFLAGS = -Wl,--version-script,libcgroup.map \
	-version-number $(LIBRARY_VERSION_MAJOR):$(LIBRARY_VERSION_MINOR):$(LIBRARY_VERSION_RELEASE)

