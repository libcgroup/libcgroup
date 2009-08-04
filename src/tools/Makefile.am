INCLUDES = -I$(top_srcdir)/src -I$(top_srcdir)/include
LDADD = $(top_srcdir)/src/.libs/libcgroup.la

if WITH_TOOLS

bin_PROGRAMS = cgexec cgclassify cgcreate cgset cgdelete

sbin_PROGRAMS = cgconfigparser cgclear

cgexec_SOURCES = cgexec.c tools-common.c tools-common.h

cgclassify_SOURCES = cgclassify.c tools-common.c tools-common.h

cgcreate_SOURCES = cgcreate.c tools-common.c tools-common.h

cgset_SOURCES = cgset.c tools-common.c tools-common.h

cgconfigparser_SOURCES = cgconfig.c

cgclear_SOURCES = cgclear.c

cgdelete_SOURCES = cgdelete.c tools-common.c tools-common.h

install-exec-hook:
	chmod u+s $(DESTDIR)$(bindir)/cgexec

endif
