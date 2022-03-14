# SPDX-License-Identifier: LGPL-2.1-only
#
# Libcgroup Python Bindings
#
# Copyright (c) 2021-2022 Oracle and/or its affiliates.
# Author: Tom Hromatka <tom.hromatka@oracle.com>
#

# cython: language_level = 3str

cdef extern from "libcgroup.h":
    cdef struct cgroup:
        pass

    cdef struct cgroup_controller:
        pass

    cdef enum cg_version_t:
        CGROUP_UNK
        CGROUP_V1
        CGROUP_V2
        CGROUP_DISK

    cdef struct cgroup_library_version:
        unsigned int major
        unsigned int minor
        unsigned int release

    int cgroup_init()
    const cgroup_library_version * cgroup_version()

    cgroup *cgroup_new_cgroup(const char *name)
    int cgroup_create_cgroup(cgroup *cg, int ignore_ownership)
    int cgroup_convert_cgroup(cgroup *out_cg, cg_version_t out_version,
                              cgroup  *in_cg, cg_version_t in_version)
    void cgroup_free(cgroup **cg)

    cgroup_controller *cgroup_add_controller(cgroup *cg, const char *name)
    cgroup_controller *cgroup_get_controller(cgroup *cg, const char *name)

    int cgroup_add_value_string(cgroup_controller *cgc, const char *name,
                                const char *value)
    int cgroup_get_value_string(cgroup_controller *cgc, const char *name,
                                char **value)
    char *cgroup_get_value_name(cgroup_controller *cgc, int index)
    int cgroup_get_value_name_count(cgroup_controller *cgc)

    int cgroup_cgxget(cgroup ** cg, cg_version_t version,
                      bint ignore_unmappable)

    int cgroup_cgxset(const cgroup * const cg, cg_version_t version,
                      bint ignore_unmappable)

# vim: set et ts=4 sw=4:
