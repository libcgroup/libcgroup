#
# Cgroup class for the libcgroup functional tests
#
# Copyright (c) 2020-2021 Oracle and/or its affiliates.
# Author: Tom Hromatka <tom.hromatka@oracle.com>
#

#
# This library is free software; you can redistribute it and/or modify it
# under the terms of version 2.1 of the GNU Lesser General Public License as
# published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
# for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this library; if not, see <http://www.gnu.org/licenses>.
#

from cgroup import Cgroup
import multiprocessing as mp
from run import Run
import time

children = list()

class Process(object):
    @staticmethod
    def __infinite_loop(config, sleep_time=1):
        cmd = ['nohup', 'perl', '-e', '\'while(1){{sleep({})}};\''.format(sleep_time), '&']

        if config.args.container:
            config.container.run(cmd, shell_bool=True)
        else:
            Run.run(cmd, shell_bool=True)

    @staticmethod
    def create_process(config):
        # To allow for multiple processes to be created, each new process
        # sleeps for a different amount of time.  This lets us uniquely find
        # each process later in this function
        sleep_time = len(children) + 1

        p = mp.Process(target=Process.__infinite_loop,
                       args=(config, sleep_time, ))
        p.start()

        # wait for the process to start.  If we don't wait, then the getpid
        # logic below may not find the process
        time.sleep(2)

        # get the PID of the newly spawned infinite loop
        cmd = 'ps x | grep perl | grep "sleep({})" | awk \'{{print $1}}\''.format(sleep_time)
        if config.args.container:
            pid = config.container.run(cmd, shell_bool=True)
        else:
            pid = Run.run(cmd, shell_bool=True)

        if pid == "" or int(pid) <= 0:
            raise ValueException('Failed to get the pid of the child process: {}'.format(pid))

        children.append(p)
        return pid

    # Create a simple process in the requested cgroup
    @staticmethod
    def create_process_in_cgroup(config, controller, cgname):
        child_pid = Process.create_process(config)
        Cgroup.classify(config, controller, cgname, child_pid)

    # The caller will block until all children are stopped.
    @staticmethod
    def join_children():
        for child in children:
             child.join(1)
