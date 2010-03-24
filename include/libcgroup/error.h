#ifndef _LIBCGROUP_ERROR_H
#define _LIBCGROUP_ERROR_H

#ifndef _LIBCGROUP_H_INSIDE
#error "Only <libcgroup.h> should be included directly."
#endif

#include <features.h>

__BEGIN_DECLS

enum {
	ECGROUPNOTCOMPILED = 50000,
	ECGROUPNOTMOUNTED,
	ECGROUPNOTEXIST,
	ECGROUPNOTCREATED,
	ECGROUPSUBSYSNOTMOUNTED,
	ECGROUPNOTOWNER,
	ECGROUPMULTIMOUNTED,/* Controllers bound to different mount points */
	ECGROUPNOTALLOWED,  /* This is the stock error. Default error. */
	ECGMAXVALUESEXCEEDED,
	ECGCONTROLLEREXISTS,
	ECGVALUEEXISTS,
	ECGINVAL,
	ECGCONTROLLERCREATEFAILED,
	ECGFAIL,
	ECGROUPNOTINITIALIZED,
	ECGROUPVALUENOTEXIST,
	/* Represents error coming from other libraries like glibc. libcgroup
	 * users need to check errno upon encoutering ECGOTHER.
	 */
	ECGOTHER,	/* OS error, see errno */
	ECGROUPNOTEQUAL,
	ECGCONTROLLERNOTEQUAL,
	ECGROUPPARSEFAIL, /* Failed to parse rules configuration file. */
	ECGROUPNORULES, /* Rules list does not exist. */
	ECGMOUNTFAIL,
	ECGSENTINEL,	/* Please insert further error codes above this */
	ECGEOF,		/* End of file, iterator */
	ECGCONFIGPARSEFAIL,/* Failed to parse config file (cgconfig.conf). */
	ECGNAMESPACEPATHS,
	ECGNAMESPACECONTROLLER,
	ECGMOUNTNAMESPACE,
};

#define ECGRULESPARSEFAIL	ECGROUPPARSEFAIL

/**
 * Return error corresponding to @code in human readable format.
 * @code: error code for which the corresponding error string is to be
 * returned
 */
const char *cgroup_strerror(int code);

/**
 * Return last errno, which caused ECGOTHER error.
 */
int cgroup_get_last_errno(void);

__END_DECLS

#endif /* _LIBCGROUP_INIT_H */
