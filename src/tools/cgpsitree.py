#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-only
#
# Display the PSI usage in a cgroup hierarchy
#
# Copyright (c) 2021-2024 Oracle and/or its affiliates.
# Author: Tom Hromatka <tom.hromatka@oracle.com>
#

from libcgrouptree import LibcgroupPsiTree
import argparse
import os

def parse_args():
    parser = argparse.ArgumentParser("Libcgroup PSI Tree")
    parser.add_argument('-C', '--cgroup', type=str, required=False, default=None,
                        help='Relative path to the cgroup of interest, e.g. machine.slice/foo.scope')
    parser.add_argument('-c', '--controller', required=True,
                        help='PSI controller data to display.  cpu, io, or memory')
    parser.add_argument('-f', '--field', required=False, default='some-avg10',
                        help='Which PSI field to display, e.g. some-avg10, full-avg60, ...')
    parser.add_argument('-d', '--depth', type=int, required=False, default=None,
                        help='Depth to recurse into the cgroup path.  0 == only this cgroup, 1 == this cgroup and its children, ...')

    args = parser.parse_args()

    return args

def main(args):
    cgtree = LibcgroupPsiTree(args.cgroup, controller=args.controller, depth=args.depth,
                              psi_field=args.field)

    cgtree.walk()
    cgtree.build()

    print('{} {} PSI data'.format(args.controller, args.field))
    cgtree.show()

if __name__ == '__main__':
    args = parse_args()
    main(args)
