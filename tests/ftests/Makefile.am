# SPDX-License-Identifier: LGPL-2.1-only
#
# libcgroup functional tests Makefile.am
#
# Copyright (c) 2019-2021 Oracle and/or its affiliates.
# Author: Tom Hromatka <tom.hromatka@oracle.com>
#

TESTS = ftests.sh ftests-nocontainer.sh

EXTRA_DIST_PYTHON_UTILS = \
			  cgroup.py \
			  config.py \
			  consts.py \
			  container.py \
			  controller.py \
			  ftests.py \
			  __init__.py \
			  log.py \
			  process.py \
			  run.py \
			  systemd.py \
			  utils.py

EXTRA_DIST_PYTHON_TESTS = \
			  001-cgget-basic_cgget_v1.py \
			  002-cgdelete-recursive_delete.py \
			  003-cgget-basic_cgget_v2.py \
			  004-cgsnapshot-basic_snapshot_v1.py \
			  005-cgsnapshot-basic_snapshot_v2.py \
			  006-cgrules-basic_cgrules_v1.py \
			  007-cgrules-basic_cgrules_v2.py \
			  008-cgget-multiple_r_flags.py \
			  009-cgget-g_flag_controller_only.py \
			  010-cgget-g_flag_controller_and_cgroup.py \
			  011-cgget-r_flag_two_cgroups.py \
			  012-cgget-multiple_r_flags2.py \
			  013-cgget-multiple_g_flags.py \
			  014-cgget-a_flag.py \
			  015-cgget-multiline_r_flag.py \
			  016-cgget-invalid_options.py \
			  017-cgconfig-load_file.py \
			  018-cgconfig-load_dir.py \
			  019-cgconfig-uidgid_dperm_fperm.py \
			  020-cgconfig-tasks_perms_owner.py \
			  021-cgconfig-invalid_options.py \
			  022-cgset-multiple_r_flag.py \
			  023-cgset-copy_from.py \
			  025-cgset-multiple_cgroups.py \
			  026-cgset-multiple_r_multiple_cgroup.py \
			  027-cgset-invalid_options.py \
			  029-lssubsys-basic_lssubsys.py \
			  030-lssubsys-lssubsys_all.py \
			  031-lscgroup-g_flag.py \
			  032-lscgroup-multiple_g_flags.py \
			  033-cgget-no_flags.py \
			  034-cgexec-basic_cgexec.py \
			  035-cgset-set_cgroup_type.py \
			  036-cgset-multi_thread.py \
			  037-cgxget-cpu_settings.py \
			  038-cgxget-cpuset_settings.py \
			  039-pybindings-cgxget.py \
			  040-pybindings-cgxset.py \
			  041-pybindings-library_version.py \
			  042-cgxget-unmappable.py \
			  043-cgcreate-empty_controller.py \
			  044-pybindings-cgcreate_empty_controller.py \
			  045-pybindings-list_mount_points.py \
			  046-cgexec-empty_controller.py \
			  047-cgcreate-delete_cgrp_shared_mnt.py \
			  048-pybindings-get_cgroup_mode.py \
			  049-sudo-systemd_create_scope.py \
			  050-sudo-systemd_create_scope2.py \
			  051-sudo-cgroup_get_cgroup.py
# Intentionally omit the stress test from the extra dist
# 999-stress-cgroup_init.py

EXTRA_DIST = README.md ftests.sh ftests-nocontainer.sh \
	     ${EXTRA_DIST_PYTHON_UTILS} ${EXTRA_DIST_PYTHON_TESTS}

clean-local: clean-local-check
.PHONY: clean-local-check
clean-local-check:
	-rm -f *.pyc
