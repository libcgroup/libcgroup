INCLUDES = -I $(top_srcdir)/include

if WITH_PAM

pamlib_LTLIBRARIES = pam_cgroup.la
pam_cgroup_la_SOURCES = pam_cgroup.c
pam_cgroup_la_LDFLAGS = -module
pam_cgroup_la_LIBADD = $(top_builddir)/src/.libs/libcgroup.la -lpam

endif
