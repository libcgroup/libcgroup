DIST_SUBDIRS = ftests gunit tools
if WITH_TESTS
SUBDIRS = $(DIST_SUBDIRS)
endif

INCLUDES = -I$(top_srcdir)/include
LDADD = $(top_builddir)/src/.libs/libcgroup.la

libcgrouptest01_SOURCES=libcgrouptest01.c test_functions.c libcgrouptest.h
libcg_ba_SOURCES=libcg_ba.cpp
setuid_SOURCES=setuid.c
walk_test_SOURCES=walk_test.c
read_stats_SOURCES=read_stats.c
walk_task_SOURCES=walk_task.c
get_controller_SOURCES=get_controller.c
get_mount_point_SOURCES=get_mount_point.c
proctest_SOURCES=proctest.c
get_all_controller_SOURCES=get_all_controller.c
get_variable_names_SOURCES=get_variable_names.c
test_named_hierarchy_SOURCES=test_named_hierarchy.c
get_procs_SOURCES=get_procs.c
wrapper_test_SOURCES=wrapper_test.c
logger_SOURCES=logger.c

EXTRA_DIST = runlibcgrouptest.sh logger.sh

# The legacy tests have been disabled because they do not successfully run
# on Travis CI.
#TESTS = wrapper_test runlibcgrouptest.sh logger.sh
