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
from run import RunError
import time

class Process(object):
    def __init__(self):
        self.children = list()
        self.children_pids = list()

    def __str__(self):
        out_str = "Process Class\n"
        out_str += "\tchildren = {}\n".format(self.children)
        out_str += "\tchildren_pids = {}\n".format(self.children_pids)

        return out_str

    @staticmethod
    def __infinite_loop(config, sleep_time=1):
        cmd = ['/usr/bin/perl', '-e', '\'while(1){{sleep({})}};\''.format(sleep_time)]

        try:
            if config.args.container:
                config.container.run(cmd, shell_bool=True)
            else:
                Run.run(cmd, shell_bool=True)
        except RunError as re:
            # when the process is killed, a RunError will be thrown.  let's
            # catch and suppress it
            pass

    def create_process(self, config):
        # To allow for multiple processes to be created, each new process
        # sleeps for a different amount of time.  This lets us uniquely find
        # each process later in this function
        sleep_time = len(self.children) + 1

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
            pid = Run.run(cmd, shell_bool=True).decode('ascii')

            for _pid in pid.splitlines():
                self.children_pids.append(_pid)

            if pid.find('\n') >= 0:
                # The second pid in the list contains the actual perl process
                pid = pid.splitlines()[1]

        if pid == "" or int(pid) <= 0:
            raise ValueException('Failed to get the pid of the child process: {}'.format(pid))

        self.children.append(p)
        return pid

    # Create a simple process in the requested cgroup
    def create_process_in_cgroup(self, config, controller, cgname):
        child_pid = self.create_process(config)
        Cgroup.classify(config, controller, cgname, child_pid)

    # The caller will block until all children are stopped.
    def join_children(self, config):
        for child in self.children:
             child.join(1)

        for child in self.children_pids:
            try:
                if config.args.container:
                    config.container.run(['kill', child])
                else:
                    Run.run(['kill', child])
            except:
                # ignore any errors during the kill command.  this is belt
                # and suspenders code
                pass
