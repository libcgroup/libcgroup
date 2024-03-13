#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-only
#
# Display a list of cgroups and their specified PSI metrics
#
# Copyright (c) 2021-2024 Oracle and/or its affiliates.
# Author: Tom Hromatka <tom.hromatka@oracle.com>
#

from libcgrouplist import LibcgroupPsiList
import argparse
import os

def parse_args():
    parser = argparse.ArgumentParser('Libcgroup PSI List')
    parser.add_argument('-C', '--cgroup', type=str, required=False, default=None,
                        help='Relative path to the cgroup of interest, e.g. machine.slice/foo.scope')
    parser.add_argument('-c', '--controller', required=True,
                        help='PSI controller data to display.  cpu, io, or memory')
    parser.add_argument('-f', '--field', required=False, default='some-avg10',
                        help='Which PSI field to display, e.g. some-avg10, full-avg60, ...')
    parser.add_argument('-d', '--depth', type=int, required=False, default=None,
                        help='Depth to recurse into the cgroup path.  0 == only this cgroup, 1 == this cgroup and its children, ...')
    parser.add_argument('-t', '--threshold', type=float, required=False, default=1.0,
                        help='Only list cgroups whose PSI exceeds this percentage')
    parser.add_argument('-l', '--limit', type=int, required=False, default=None,
                        help='Only display the first N cgroups. If not provided, all cgroups that match are displayed')

    args = parser.parse_args()

    return args

def main(args):
    cglist = LibcgroupPsiList(args.cgroup, controller=args.controller, depth=args.depth,
                              psi_field=args.field, threshold=args.threshold, limit=args.limit)

    cglist.walk()
    cglist.show()

if __name__ == '__main__':
    args = parse_args()
    main(args)
