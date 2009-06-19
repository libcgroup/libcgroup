INCLUDES = -I$(top_srcdir)/src -I$(top_srcdir)/include
LDADD = $(top_srcdir)/src/.libs/libcgroup.la

if WITH_TOOLS

bin_PROGRAMS = cgexec cgclassify cgcreate cgset

sbin_PROGRAMS = cgconfigparser cgclear

cgexec_SOURCES = cgexec.c tools-common.c tools-common.h

cgclassify_SOURCES = cgclassify.c tools-common.c tools-common.h

cgcreate_SOURCES = cgcreate.c tools-common.c tools-common.h

cgset_SOURCES = cgset.c tools-common.c tools-common.h

cgconfigparser_SOURCES = cgconfig.c

cgclear_SOURCES = cgclear.c

install-exec-hook:
	chmod u+s $(DESTDIR)$(bindir)/cgexec

endif
