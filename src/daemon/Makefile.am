INCLUDES = -I $(top_srcdir)/include
sbin_PROGRAMS = cgrulesengd

cgrulesengd_SOURCES = cgrulesengd.c cgrulesengd.h
cgrulesengd_LDADD = $(top_srcdir)/src/.libs/libcgroup.la
cgrulesengd_LDFLAGS = -L$(top_srcdir)/src/.libs
