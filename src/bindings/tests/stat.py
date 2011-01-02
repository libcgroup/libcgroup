#!/usr/bin/python

from libcgroup import *
from ctypes import *

ret = cgroup_init()
if (ret != 0):
	print cgroup_strerror(ret)
	exit(1)

#
# Add the correct controllers based on the mount point
#
cg_stat = cgroup_stat()
tree_handle = c_void_p()
info = cgroup_file_info()
lvl = new_intp()
ret1, tree_handle = cgroup_walk_tree_begin("memory", "/", 0, tree_handle, info, lvl)
root_len = len(info.full_path) - 1
while ret1 != ECGEOF:
        if (info.type == CGROUP_FILE_TYPE_DIR):
                dir = info.full_path[root_len:]
                print "\nDirectory %s\n" %(dir)

                p = c_void_p()
                ret, p = cgroup_read_stats_begin("memory", dir, p, cg_stat)
                while ret != ECGEOF:
                        print "%s:%s" %(cg_stat.name, cg_stat.value.strip('\n'))
                        ret, p = cgroup_read_stats_next(p, cg_stat)

                cgroup_read_stats_end(p)
        ret1, tree_handle = cgroup_walk_tree_next(0, tree_handle, info, intp_value(lvl))

cgroup_walk_tree_end(tree_handle)
delete_intp(lvl)
