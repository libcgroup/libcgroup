INCLUDES = -I$(top_srcdir)/src -I$(top_srcdir)/include
LDADD = $(top_srcdir)/src/.libs/libcgroup.la

bin_PROGRAMS = cgexec cgclassify
sbin_PROGRAMS = cgconfigparser

cgexec_SOURCES = cgexec.c tools-common.c tools-common.h

cgclassify_SOURCES = cgclassify.c tools-common.c tools-common.h

cgconfigparser_SOURCES = cgconfig.c

