_SPDX-License-Identifier: LGPL-2.1-only_

_Copyright (c) 2023 Oracle and/or its affiliates._

_Author: Tom Hromatka <<tom.hromatka@oracle.com>>_


# Creating a Systemd Scope and Child Hierarchy via Libcgroup Command Line

The goal of this document is to outline the steps required to create
a systemd scope and a child cgroup hierarchy using the libcgroup
command line tools.

The following steps are encapsulated in a
[libcgroup automated test](../../tests/ftests/086-sudo-systemd_cmdline_example.py).

## Requirements:
1. Create a cgroup hierarchy that obeys the
   [single-writer rule](https://systemd.io/CGROUP_DELEGATION/)
1. The hierarchy should be cgroup v2
1. The hierarchy should be of the form
   ```
   database.scope
   ├── high-priority
   ├── low-priority
   └── medium-priority
   ```
1. The memory controller should be enabled for the entire tree

   1. The `high-priority` cgroup should be guaranteed at least 1GB of RAM
   1. The `low-priority` cgroup should be hard-limited to 2GB of RAM
   1. The `medium-prioirty` cgroup should have a soft memory limit of 3 GB

1. The cpu controller should be enabled for the entire tree

   1. The `high-priority` cgroup should be able to consume 60% of CPU cycles
   1. The `medium-priority` cgroup should be able to consume 30% of CPU cycles
   1. The `low-priority` cgroup should be able to consume 10% of CPU cycles

## Steps

1. Create a delegated
   [scope](https://www.freedesktop.org/software/systemd/man/systemd.scope.html)
   cgroup
   ```
   sudo cgcreate -c -S -g cpu,memory:mycompany.slice/database.scope
   ```

   This will create a
   [transient](https://github.com/systemd/systemd/blob/main/docs/TRANSIENT-SETTINGS.md),
   [delegated](https://systemd.io/CGROUP_DELEGATION/) scope.  The `-c` flag
   instructs libcgroup to create a systemd scope; libcgroup then instructs
   systemd that this hierarchy is delegated, i.e. it is to be managed by
   another process and _not_ by systemd.  The `-S` flag notifies libcgroup
   that we want `mycompany.scope/database.slice` to be the default base
   path in libcgroup; this will significantly help in reducing typing in
   follow-on commands.  The `-g` flag tells libcgroup to create a cgroup
   named `mycompany.slice/database.scope` and enable the cpu and memory
   controllers within it.

   <details>
      <summary>Problems during this step?</summary>

      Systemd should automatically remove scopes with no active processes
      running within them.  So, the first step would be to kill any processes
      in the scope, wait to see if systemd removes the scope, and then try the
      `cgcreate` operation again.

      - Remove all processes in the scope
        ```
        $ for PID in $(cgget -nvb -r cgroup.procs mycompany.slice/database.scope); do sudo kill -9 $PID;done
        ```

        The above command could be simplified as
        `for PID in $(cgget -nv -r cgroup.procs /); do sudo kill -9 $PID;done`,
        but introduces some risk.  If there is a typo or the default scope path
        isn't set, then unconditionally killing processes in `/` could be
        catastrophic.

      Sometimes systemd's internal list of scopes gets out of sync with the
      filesystem.  You can purge the `database.scope` from its list by running
      the following commands
      - Remove `database.scope` from systemd's internal list
        ```
        sudo systemctl kill database.scope
        sudo systemctl stop database.scope
        ```
   </details>

1. Create the child cgroups
   ```
   $ sudo cgcreate -g cpu,memory:mycompany.slice/database.scope/high-priority
   cgcreate: can't create cgroup mycompany.slice/database.scope/high-priority: Operation not supported
   ```

   But... but... I did everything right.  Why can't I create the
   `high-priority` child cgroup?  This operation failed due to the
   [no-processes-in-inner-nodes](https://systemd.io/CGROUP_DELEGATION/) rule.
   Since a process, `libcgroup_systemd_idle_thread`, resides in
   `database.scope`, we are subject to the no-process-in-inner-nodes rule.
   The kernel will let us create a child cgroup, but it will fail when we try
   to enable controllers in `database.scope`'s `cgroup.subtree_control` file.
   There are a few different ways to solve this failure; the easiest is
   probably to create a temporary cgroup under `database.scope` and move the
   `libcgroup_systemd_idle_thread` to this temporary cgroup.  This allows
   `database.scope` to operate as a legal inner node, and we can then create
   the entire hierarchy.

   1. Temporarily disable the cpu and memory controllers at the scope level
      ```
      sudo cgset -r cgroup.subtree_control="-cpu -memory" /
      ```

      Since we informed libcgroup that `mycompany.slice/database.scope` is the
      default path, we can use `/`.  Otherwise, we would have had to specify
      the entire path.  This pattern continues throughout this example.

   1. Create a temporary cgroup and move the idle process
      ```
      sudo cgcreate -g :tmp
      sudo cgclassify -g :tmp $(cgget -nv -r cgroup.procs /)
      ```

   1. Re-enable the cpu and memory controllers at the scope level
      ```
      sudo cgset -r cgroup.subtree_control="+cpu +memory" /
      ```

   Now we can finally get back to creating our child cgroups
   ```
   sudo cgcreate -g cpu,memory:high-priority -g cpu,memory:medium-priority -g cpu,memory:low-priority
   ```

1. Configure the cgroups per the requirements
   1. The `high-priority` cgroup should be guaranteed at least 1GB of RAM
      ```
      sudo cgset -r memory.low=1G high-priority
      ```

   1. The `low-priority` cgroup should be hard-limited to 2GB of RAM
      ```
      sudo cgset -r memory.max=2G low-priority
      ```

   1. The `medium-prioirty` cgroup should have a soft memory limit of 3 GB
      ```
      sudo cgset -r memory.high=3G medium-priority
      ```

   1. The `high-priority` cgroup should be able to consume 60% of CPU cycles
      ```
      sudo cgset -r cpu.weight=600 high-priority
      ```

      Note that I've (somewhat arbitrarily) chosen a total `cpu.weight` within
      `database.scope` to be 1000.  Thus, to meet the 60% requirement, we need
      to allocate 600 shares to the `high-priority` cgroup.

   1. The `medium-priority` cgroup should be able to consume 30% of CPU cycles
      ```
      sudo cgset -r cpu.weight=300 medium-priority
      ```

   1. The `low-priority` cgroup should be able to consume 10% of CPU cycles
      ```
      sudo cgset -r cpu.weight=100 low-priority
      ```

1. Start up the application

   - If the application is already running, then you can use `cgclassify` to
     move the process(es) to the appropriate cgroups

   - To start a fresh application, it is recommended to use `cgexec` to place
     the application in the desired cgroup

   - Finally, consider using `cgrulesengd` to automatically move processes to
     the correct cgroups

1. Clean up

   1. Now that there are other processes running within the scope, we can
      remove the `libcgroup_systemd_idle_thread`
      ```
      $ for PID in $(cgget -nv -r cgroup.procs tmp); do sudo kill -9 $PID;done
      ```

   1. Delete the `tmp` cgroup
      ```
      sudo cgdelete -g :tmp
      ```

1. Verify the cgroups were configured per the requirements
   ```
   $ cgget -r cpu.weight -r memory.low -r memory.high -r memory.max high-priority medium-priority low-priority
   high-priority:
   cpu.weight: 600
   memory.low: 1073741824
   memory.high: max
   memory.max: max

   medium-priority:
   cpu.weight: 300
   memory.low: 0
   memory.high: 3221225472
   memory.max: max

   low-priority:
   cpu.weight: 100
   memory.low: 0
   memory.high: max
   memory.max: 2147483648
   ```

1. Summary

This document outlines the steps for creating a delegated systemd scope and
configuring its child cgroups on a cgroup v2 system.  Systemd and libcgroup
provide powerful tools to simplify these steps.
