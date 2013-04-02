/*
 * Copyright IBM Corporation. 2007
 *
 * Author:	Balbir Singh <balbir@linux.vnet.ibm.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2.1 of the GNU Lesser General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#ifndef _LIBCGROUP_H
#define _LIBCGROUP_H

#define _LIBCGROUP_H_INSIDE

#include <libcgroup/error.h>
#include <libcgroup/init.h>
#include <libcgroup/iterators.h>
#include <libcgroup/groups.h>
#include <libcgroup/tasks.h>
#include <libcgroup/config.h>
#include <libcgroup/log.h>

#undef _LIBCGROUP_H_INSIDE

/*! \mainpage libcgroup
 *
 * \section intro_sec Introduction
 *
 * @c libcgroup is a library that abstracts the control group file system in Linux.
 * It comes with various command-line tools and configuration files, see
 * their man pages for details.
 *
 * This documentation provides description of @c libcgroup API. Read following
 * sections, preferably in this order:
 * -# @ref group_init "Initialization"
 * -# @ref group_groups "Control Groups"
 * -# @ref group_iterators "Iterators"
 * -# @ref group_tasks "Manipulation with tasks"
 * -# @ref group_config "Configuration"
 * -# @ref group_errors "Error Handling"
 */

#endif /* _LIBCGROUP_H  */
