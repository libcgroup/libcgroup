INCLUDES = -I$(top_srcdir)/src -I$(top_srcdir)/include
LDADD = $(top_builddir)/src/.libs/libcgroup.la

if WITH_TOOLS

bin_PROGRAMS = cgexec cgclassify cgcreate cgset cgget cgdelete lssubsys\
		lscgroup cgsnapshot

sbin_PROGRAMS = cgconfigparser cgclear

cgexec_SOURCES = cgexec.c tools-common.c tools-common.h

cgclassify_SOURCES = cgclassify.c tools-common.c tools-common.h

cgcreate_SOURCES = cgcreate.c tools-common.c tools-common.h

cgset_SOURCES = cgset.c tools-common.c tools-common.h

cgget_SOURCES = cgget.c tools-common.c tools-common.h

cgconfigparser_SOURCES = cgconfig.c tools-common.c tools-common.h

cgclear_SOURCES = cgclear.c tools-common.c tools-common.h

cgdelete_SOURCES = cgdelete.c tools-common.c tools-common.h

lssubsys_SOURCES = lssubsys.c

lscgroup_SOURCES = tools-common.c lscgroup.c

cgsnapshot_SOURCES = cgsnapshot.c

install-exec-hook:
	chmod u+s $(DESTDIR)$(bindir)/cgexec

endif
