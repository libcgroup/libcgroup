# SPDX-License-Identifier: LGPL-2.1-only
#
# Libcgroup Python Bindings
#
# Copyright (c) 2021-2022 Oracle and/or its affiliates.
# Author: Tom Hromatka <tom.hromatka@oracle.com>
#

# cython: language_level = 3str

from posix.types cimport pid_t

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

    cdef enum cg_setup_mode_t:
        CGROUP_MODE_UNK
        CGROUP_MODE_LEGACY
        CGROUP_MODE_HYBRID
        CGROUP_MODE_UNIFIED

    cdef struct cgroup_library_version:
        unsigned int major
        unsigned int minor
        unsigned int release

    cdef enum cgroup_systemd_mode_t:
        CGROUP_SYSTEMD_MODE_FAIL
        CGROUP_SYSTEMD_MODE_REPLACE
        CGROUP_SYSTEMD_MODE_ISOLATE
        CGROUP_SYSTEMD_MODE_IGNORE_DEPS
        CGROUP_SYSTEMD_MODE_IGNORE_REQS

    cdef struct cgroup_systemd_scope_opts:
        int delegated
        cgroup_systemd_mode_t mode
        pid_t pid

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

    int cgroup_list_mount_points(const cg_version_t cgrp_version,
                                 char ***mount_paths)

    cg_setup_mode_t cgroup_setup_mode()

    int cgroup_create_scope(const char * const scope_name, const char * const slice_name,
                            const cgroup_systemd_scope_opts * const opts)

    int cgroup_get_cgroup(cgroup *cg)

    int cgroup_delete_cgroup(cgroup *cg, int ignore_migration)

    int cgroup_get_controller_count(cgroup *cgroup)

    cgroup_controller *cgroup_get_controller_by_index(cgroup *cgroup, int index)

    char *cgroup_get_controller_name(cgroup_controller *controller)

    int cgroup_attach_task(cgroup * cgroup)
    int cgroup_attach_task_pid(cgroup * cgroup, pid_t pid)

# vim: set et ts=4 sw=4:
