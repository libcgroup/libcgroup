INCLUDES = -I$(top_srcdir)/src -I$(top_srcdir)/include
LDADD = $(top_builddir)/src/.libs/libcgroup.la

if WITH_TOOLS

bin_PROGRAMS = cgexec cgclassify cgcreate cgset cgget cgdelete lssubsys lscgroup

sbin_PROGRAMS = cgconfigparser cgclear

cgexec_SOURCES = cgexec.c tools-common.c tools-common.h

cgclassify_SOURCES = cgclassify.c tools-common.c tools-common.h

cgcreate_SOURCES = cgcreate.c tools-common.c tools-common.h

cgset_SOURCES = cgset.c tools-common.c tools-common.h

cgget_SOURCES = cgget.c tools-common.c tools-common.h

cgconfigparser_SOURCES = cgconfig.c

cgclear_SOURCES = cgclear.c

cgdelete_SOURCES = cgdelete.c tools-common.c tools-common.h

lssubsys_SOURCES = lssubsys.c

lscgroup_SOURCES = tools-common.c lscgroup.c

install-exec-hook:
	chmod u+s $(DESTDIR)$(bindir)/cgexec

endif
