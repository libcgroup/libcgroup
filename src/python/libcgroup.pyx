# SPDX-License-Identifier: LGPL-2.1-only
#
# Libcgroup Python Bindings
#
# Copyright (c) 2021-2022 Oracle and/or its affiliates.
# Author: Tom Hromatka <tom.hromatka@oracle.com>
#

# cython: language_level = 3str

""" Python bindings for the libcgroup library
"""

__author__ =  'Tom Hromatka <tom.hromatka@oracle.com>'
__date__ = "25 October 2021"

cimport cgroup

cdef class Version:
    CGROUP_UNK = cgroup.CGROUP_UNK
    CGROUP_V1 = cgroup.CGROUP_V1
    CGROUP_V2 = cgroup.CGROUP_V2
    CGROUP_DISK = cgroup.CGROUP_DISK

def c_str(string):
    return bytes(string, "ascii")

def indent(in_str, cnt):
    leading_indent = cnt * ' '
    return ''.join(leading_indent + line for line in in_str.splitlines(True))

class Controller:
    def __init__(self, name):
        self.name = name
        # self.settings maps to
        # struct control_value *values[CG_NV_MAX];
        self.settings = dict()

    def __str__(self):
        out_str = "Controller {}\n".format(self.name)

        for setting_key in self.settings:
            out_str += indent("{} = {}\n".format(setting_key,
                              self.settings[setting_key]), 4)

        return out_str

cdef class Cgroup:
    """ Python object representing a libcgroup cgroup """
    cdef cgroup.cgroup * _cgp
    cdef public:
        object name, controllers, version

    def __cinit__(self, name, version):
        ret = cgroup.cgroup_init()
        if ret != 0:
            raise RuntimeError("Failed to initialize libcgroup: {}".format(ret))

        self._cgp = cgroup.cgroup_new_cgroup(c_str(name))
        if self._cgp == NULL:
            raise RuntimeError("Failed to create cgroup {}".format(name))

    def __init__(self, name, version):
        """Initialize this cgroup instance

        Arguments:
        name - Name of this cgroup
        version - Version of this cgroup

        Note:
        Does not modify the cgroup sysfs.  Does not read from the cgroup sysfs
        """
        self.name = name
        self.controllers = dict()
        self.version = version

    def __str__(self):
        out_str = "Cgroup {}\n".format(self.name)
        for ctrl_key in self.controllers:
            out_str += indent(str(self.controllers[ctrl_key]), 4)

        return out_str

    @staticmethod
    def library_version():
        cdef const cgroup.cgroup_library_version * version

        version = cgroup.cgroup_version()
        return [version.major, version.minor, version.release]

    def add_controller(self, ctrl_name):
        """Add a controller to the Cgroup instance

        Arguments:
        ctrl_name - name of the controller

        Description:
        Adds a controller to the Cgroup instance

        Note:
        Does not modify the cgroup sysfs
        """
        cdef cgroup.cgroup_controller * cgcp
        cdef cgroup.cgroup * cgp

        cgcp = cgroup.cgroup_add_controller(self._cgp,
                                            c_str(ctrl_name))
        if cgcp == NULL:
            raise RuntimeError("Failed to add controller {} to cgroup".format(
                               ctrl_name))

        self.controllers[ctrl_name] = Controller(ctrl_name)

    def add_setting(self, setting_name, setting_value=None):
        """Add a setting to the Cgroup/Controller instance

        Arguments:
        setting_name - name of the cgroup setting, e.g. 'cpu.shares'
        setting_value (optional) - value

        Description:
        Adds a setting/value pair to the Cgroup/Controller instance

        Note:
        Does not modify the cgroup sysfs
        """
        cdef cgroup.cgroup_controller *cgcp
        cdef char * value

        ctrl_name = setting_name.split('.')[0]

        cgcp = cgroup.cgroup_get_controller(self._cgp,
                                            c_str(ctrl_name))
        if cgcp == NULL:
            self.add_controller(ctrl_name)

            cgcp = cgroup.cgroup_get_controller(self._cgp,
                                                c_str(ctrl_name))
            if cgcp == NULL:
                raise RuntimeError("Failed to get controller {}".format(
                                   ctrl_name))

        if setting_value == None:
            ret = cgroup.cgroup_add_value_string(cgcp,
                      c_str(setting_name), NULL)
        else:
            ret = cgroup.cgroup_add_value_string(cgcp,
                      c_str(setting_name), c_str(setting_value))
        if ret != 0:
            raise RuntimeError("Failed to add setting {}: {}".format(
                               setting_name, ret))

    def _pythonize_cgroup(self):
        """
        Given a populated self._cgp, populate the equivalent Python fields
        """
        cdef char *setting_name
        cdef char *setting_value

        for ctrlr_key in self.controllers:
            cgcp = cgroup.cgroup_get_controller(self._cgp,
                       c_str(self.controllers[ctrlr_key].name))
            if cgcp == NULL:
                raise RuntimeError("Failed to get controller {}".format(
                                   ctrlr_key))

            self.controllers[ctrlr_key] = Controller(ctrlr_key)
            setting_cnt = cgroup.cgroup_get_value_name_count(cgcp)

            for i in range(0, setting_cnt):
                setting_name = cgroup.cgroup_get_value_name(cgcp, i)

                ret = cgroup.cgroup_get_value_string(cgcp,
                          setting_name, &setting_value)
                if ret != 0:
                    raise RuntimeError("Failed to get value {}: {}".format(
                                       setting_name, ret))

                name = setting_name.decode("ascii")
                value = setting_value.decode("ascii").strip()
                self.controllers[ctrlr_key].settings[name] = value

    def convert(self, out_version):
        """Convert this cgroup to another cgroup version

        Arguments:
        out_version - Version to convert to

        Return:
        Returns the converted cgroup instance

        Description:
        Convert this cgroup instance to a cgroup instance of a different
        cgroup version

        Note:
        Does not modify the cgroup sysfs.  Does not read from the cgroup sysfs
        """
        out_cgp = Cgroup(self.name, out_version)
        ret = cgroup.cgroup_convert_cgroup(out_cgp._cgp,
                  out_version, self._cgp, self.version)
        if ret != 0:
            raise RuntimeError("Failed to convert cgroup: {}".format(ret))

        for ctrlr_key in self.controllers:
            out_cgp.controllers[ctrlr_key] = Controller(ctrlr_key)

        out_cgp._pythonize_cgroup()

        return out_cgp

    def cgxget(self, ignore_unmappable=False):
        """Read the requested settings from the cgroup sysfs

        Arguments:
        ignore_unmappable - Ignore cgroup settings that can't be converted
                            from one version to another

        Return:
        Returns the cgroup instance that represents the settings read from
        sysfs

        Description:
        Given this cgroup instance, read the settings/values from the
        cgroup sysfs.  If the read was successful, the settings are
        returned in the return value

        Note:
        Reads from the cgroup sysfs
        """
        cdef bint ignore

        if ignore_unmappable:
            ignore = 1
        else:
            ignore = 0

        ret = cgroup.cgroup_cgxget(&self._cgp, self.version, ignore)
        if ret != 0:
            raise RuntimeError("cgxget failed: {}".format(ret))

        self._pythonize_cgroup()

    def cgxset(self, ignore_unmappable=False):
        """Write this cgroup to the cgroup sysfs

        Arguments:
        ignore_unmappable - Ignore cgroup settings that can't be converted
                            from one version to another

        Description:
        Write the settings/values in this cgroup instance to the cgroup sysfs

        Note:
        Writes to the cgroup sysfs
        """
        if ignore_unmappable:
            ignore = 1
        else:
            ignore = 0

        ret = cgroup.cgroup_cgxset(self._cgp, self.version, ignore)
        if ret != 0:
            raise RuntimeError("cgxset failed: {}".format(ret))

    def create(self, ignore_ownership=True):
        """Write this cgroup to the cgroup sysfs

        Arguments:
        ignore_ownership - if true, all errors are ignored when setting ownership
                           of the group and its tasks file

        Return:
        None
        """
        ret = cgroup.cgroup_create_cgroup(self._cgp, ignore_ownership)
        if ret != 0:
            raise RuntimeError("Failed to create cgroup: {}".format(ret))

    def __dealloc__(self):
        cgroup.cgroup_free(&self._cgp);

# vim: set et ts=4 sw=4:
