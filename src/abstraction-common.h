/**
 * Libcgroup abstraction layer prototypes and structs
 *
 * Copyright (c) 2021-2022 Oracle and/or its affiliates.
 * Author: Tom Hromatka <tom.hromatka@oracle.com>
 */

/*
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of version 2.1 of the GNU Lesser General Public License as
 * published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses>.
 */
#ifndef __ABSTRACTION_COMMON
#define __ABSTRACTION_COMMON

#ifdef __cplusplus
extern "C" {
#endif

#include "config.h"
#include <libcgroup.h>
#include "libcgroup-internal.h"

/**
 * Convert a string to a long
 *
 * @param in_str String to be converted
 * @param base Integer base
 * @param out_value Pointer to hold the output long value
 *
 * @return 0 on success,
 * 	   ECGFAIL if the conversion to long failed,
 * 	   ECGINVAL upon an invalid parameter
 */
int cgroup_strtol(const char * const in_str, int base,
		  long int * const out_value);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __ABSTRACTION_COMMON */
