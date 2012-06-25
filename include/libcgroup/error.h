#ifndef _LIBCGROUP_ERROR_H
#define _LIBCGROUP_ERROR_H

#ifndef _LIBCGROUP_H_INSIDE
#error "Only <libcgroup.h> should be included directly."
#endif

#ifndef SWIG
#include <features.h>
#endif

__BEGIN_DECLS

/**
 * @defgroup group_errors 6. Error handling
 * @{
 *
 * @name Error handling
 * @{
 * Unless states otherwise in documentation of a function, all functions
 * return @c int, which is zero (0) when the function succeeds, and positive
 * number if the function fails.
 *
 * The returned integer is one of the ECG* values described below. Value
 * #ECGOTHER means that the error was caused by underlying OS and the real
 * cause can be found by calling cgroup_get_last_errno().
 */

enum {
	ECGROUPNOTCOMPILED = 50000,
	ECGROUPNOTMOUNTED,
	ECGROUPNOTEXIST,
	ECGROUPNOTCREATED,
	ECGROUPSUBSYSNOTMOUNTED,
	ECGROUPNOTOWNER,
	/** Controllers bound to different mount points */
	ECGROUPMULTIMOUNTED,
	/* This is the stock error. Default error. @todo really? */
	ECGROUPNOTALLOWED,
	ECGMAXVALUESEXCEEDED,
	ECGCONTROLLEREXISTS,
	ECGVALUEEXISTS,
	ECGINVAL,
	ECGCONTROLLERCREATEFAILED,
	ECGFAIL,
	ECGROUPNOTINITIALIZED,
	ECGROUPVALUENOTEXIST,
	/**
	 * Represents error coming from other libraries like glibc. @c libcgroup
	 * users need to check cgroup_get_last_errno() upon encountering this
	 * error.
	 */
	ECGOTHER,
	ECGROUPNOTEQUAL,
	ECGCONTROLLERNOTEQUAL,
	/** Failed to parse rules configuration file. */
	ECGROUPPARSEFAIL,
	/** Rules list does not exist. */
	ECGROUPNORULES,
	ECGMOUNTFAIL,
	/**
	 * Not an real error, it just indicates that iterator has come to end
	 * of sequence and no more items are left.
	 */
	ECGEOF = 50023,
	/** Failed to parse config file (cgconfig.conf). */
	ECGCONFIGPARSEFAIL,
	ECGNAMESPACEPATHS,
	ECGNAMESPACECONTROLLER,
	ECGMOUNTNAMESPACE,
	ECGROUPUNSUPP,
	ECGCANTSETVALUE,
	/** Removing of a group failed because it was not empty. */
	ECGNONEMPTY,
};

/**
 * Legacy definition of ECGRULESPARSEFAIL error code.
 */
#define ECGRULESPARSEFAIL	ECGROUPPARSEFAIL

/**
 * Format error code to a human-readable English string. No internationalization
 * is currently done. Returned pointer leads to @c libcgroup memory and
 * must not be freed nor modified. The memory is rewritten by subsequent
 * call to this function.
 * @param code Error code for which the corresponding error string is
 * returned. When #ECGOTHER is used, text with glibc's description of
 * cgroup_get_last_errno() value is returned.
 */
const char *cgroup_strerror(int code);

/**
 * Return last errno, which caused ECGOTHER error.
 */
int cgroup_get_last_errno(void);

/**
 * @}
 * @}
 */
__END_DECLS

#endif /* _LIBCGROUP_INIT_H */
