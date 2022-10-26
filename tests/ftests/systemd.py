# SPDX-License-Identifier: LGPL-2.1-only
#
# Systemd class for the libcgroup functional tests
#
# Copyright (c) 2022 Oracle and/or its affiliates.
# Author: Tom Hromatka <tom.hromatka@oracle.com>
#

from run import Run, RunError


class Systemd(object):
    @staticmethod
    def is_delegated(config, scope_name):
        cmd = ['systemctl', 'show', '-P', 'Delegate', scope_name]
        try:
            out = Run.run(cmd, shell_bool=True)

            if out == 'yes':
                return True
            else:
                return False
        except RunError as re:
            if re.stderr.find('invalid option') >= 0:
                # This version of systemd is too old for the '-P' flag.  At this time, I don't
                # think there's a way to verify the scope is delegated.  Lie and return true
                # until we figure out something better :(
                return True
            raise re
