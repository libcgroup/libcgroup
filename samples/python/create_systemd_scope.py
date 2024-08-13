#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-only
#
# Sample program that shows how to use libcgroup to create a systemd scope
#
#  This program is designed to meet the requirements outlined in the systemd
# cmdline example [1] via the libcgroup C APIs.
#
# [1] https://github.com/libcgroup/libcgroup/blob/main/samples/cmdline/systemd-with-idle-process.md
#
#
# Copyright (c) 2023 Oracle and/or its affiliates.
# Author: Tom Hromatka <tom.hromatka@oracle.com>
#

#
# To run this program:
#       1. Compile the libcgroup library
#           $ ./bootstrap.sh
#           $ make
#       2. Note the path to your libcgroup python library
#           $ pushd src/python/build/lib*
#           $ export TMPPATH=$(pwd)
#           $ popd
#       2. Run the program
#           $ # Note that there are more options.  Run `create_systemd_scope.py -h`
#           $ # for more info
#           $ sudo PYTHONPATH=$TMPPATH \
#             ./samples/python/create_systemd_scope.py \
#             --slice <yourslicename> --scope <yourscopename>
#

from libcgroup import Cgroup, Version, LogLevel
import multiprocessing
import argparse
import signal
import time
import sys
import os

TMP_CGNAME = 'tmp'
HIGH_CGNAME = 'high-priority'
MED_CGNAME = 'medium-priority'
LOW_CGNAME = 'low-priority'


def parse_args():
    parser = argparse.ArgumentParser('libcgroup sample program to create a systemd scope')

    delegated_parser = parser.add_mutually_exclusive_group(required=False)
    delegated_parser.add_argument('-d', '--delegated', help='Create a delegated scope',
                                  action='store_true', dest='delegated')
    delegated_parser.add_argument('-n', '--notdelegated',
                                  help='Create a scope that is not delegated',
                                  action='store_false', dest='delegated')
    parser.set_defaults(delegated=True)

    parser.add_argument('-p', '--pid', help='PID to place within the scope. If not provided,'
                        'libcgroup will place a temporary process in the scope',
                        required=False, type=int, default=None)
    parser.add_argument('-s', '--scope', help='Name of the scope to be created',
                        required=True, type=str, default=None)
    parser.add_argument('-t', '--slice', help='Name of the slice where the scope will reside',
                        required=True, type=str, default=None)
    parser.add_argument('-v', '--verbose', help='Enable verbose logging within libcgroup',
                        required=False, action='store_true')

    args = parser.parse_args()

    return args


def create_scope(args):
    print('\n----------------------------------------------------------------')
    print('Creating systemd scope, {}/{},'.format(args.slice, args.scope))
    if args.pid:
        print('and placing PID, {}, in the scope'.format(args.pid))
    else:
        print('and libcgroup will place an idle process in the scope')
    print('----------------------------------------------------------------\n')

    Cgroup.create_scope(args.scope, args.slice, args.delegated, pid=args.pid)
    Cgroup.write_default_systemd_scope(args.slice, args.scope, True)


def create_tmp_cgroup():
    cg = Cgroup(TMP_CGNAME, Version.CGROUP_V2)
    cg.create()


def move_pids_to_tmp_cgroup():
    cg = Cgroup('/', Version.CGROUP_V2)
    pids = cg.get_processes()
    for pid in pids:
        Cgroup.move_process(pid, TMP_CGNAME)


def create_high_priority_cgroup():
    cg = Cgroup(HIGH_CGNAME, Version.CGROUP_V2)
    cg.add_setting('cpu.weight', '600')
    cg.add_setting('memory.low', '1G')
    cg.create()


def create_medium_priority_cgroup():
    cg = Cgroup(MED_CGNAME, Version.CGROUP_V2)
    cg.add_setting('cpu.weight', '300')
    cg.add_setting('memory.high', '3G')
    cg.create()


def create_low_priority_cgroup():
    cg = Cgroup(LOW_CGNAME, Version.CGROUP_V2)
    cg.add_setting('cpu.weight', '100')
    cg.add_setting('memory.max', '2G')
    cg.create()


def __infinite_loop():
    while True:
        time.sleep(10)


def create_process_in_cgroup(cgname):
    p = multiprocessing.Process(target=__infinite_loop, )
    p.start()

    Cgroup.move_process(p.pid, cgname)

    return p.pid


def delete_tmp_cgroup():
    cg = Cgroup(TMP_CGNAME, Version.CGROUP_V2)
    pids = cg.get_processes()

    for pid in pids:
        os.kill(pid, signal.SIGKILL)

    # I have observed "Device or resource busy" errors when deleting the
    # cgroup immediately after the os.kill() instruction.  Give the kill
    # command a little time to succeed
    time.sleep(1)

    cg.delete()


def main(args):
    if args.verbose:
        Cgroup.log_level(LogLevel.CGROUP_LOG_DEBUG)

    create_scope(args)
    create_tmp_cgroup()
    move_pids_to_tmp_cgroup()

    create_high_priority_cgroup()
    create_medium_priority_cgroup()
    create_low_priority_cgroup()

    high_pid = create_process_in_cgroup(HIGH_CGNAME)
    med_pid = create_process_in_cgroup(MED_CGNAME)
    low_pid = create_process_in_cgroup(LOW_CGNAME)

    delete_tmp_cgroup()

    print('\n----------------------------------------------------------------')
    print('Cgroup setup completed successfully')
    print('\t* The scope {} was placed under slice {}'.format(args.scope, args.slice))
    print('\t* Libcgroup initially placed an idle process in the scope,\n'
          '\t  but it has been removed by this program')
    print('\t* PID {} has been placed in the {} cgroup'.format(high_pid, HIGH_CGNAME))
    print('\t* PID {} has been placed in the {} cgroup'.format(med_pid, MED_CGNAME))
    print('\t* PID {} has been placed in the {} cgroup'.format(low_pid, LOW_CGNAME))
    print('\nThis program will wait for the aforementioned child processes to\n'
          'exit before exiting itself. Systemd will automatically delete the\n'
          'scope when there are no longer any processes running within the\n'
          'scope.  Systemd will not automatically delete the slice.')
    print('----------------------------------------------------------------\n')


if __name__ == '__main__':
    args = parse_args()
    sys.exit(main(args))

# vim: set et ts=4 sw=4:
