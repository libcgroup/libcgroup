/*
 * Copyright IBM Corporation. 2007
 *
 * Author:	Balbir Singh <balbir@linux.vnet.ibm.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2.1 of the GNU Lesser General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Basic acceptance test for libcgroup - Written one late night by Balbir Singh
 */
using namespace std;

#include <string>
#include <stdexcept>
#include <iostream>
#include <libcgroup.h>
#include <sys/types.h>
#include <pwd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <grp.h>

#include "../config.h"

#ifdef CGROUP_DBG
#define cgroup_dbg(p...)	printf(p...)
#else
#define cgroup_dbg(p...)	do {} while (0)
#endif

namespace cgtest {

class cg {
private:
public:
	cg();
	~cg()
	{ }
	struct cgroup *makenode(const string &name, const string &task_uid,
			const string &task_gid, const string &control_uid,
			const string &control_gid);
	struct cgroup *makenodefromparent(const string &name);
};

cg::cg(void)
{
	int ret;

	ret = cgroup_init();
	if (ret)
		throw logic_error("Control Group Initialization failed..."
				"Please check that cgroups are mounted and\n"
				"at a single place");
}

struct cgroup *cg::makenode(const string &name, const string &task_uid,
		const string &task_gid, const string &control_uid,
		const string &control_gid)
{
	uid_t tuid, cuid;
	gid_t tgid, cgid;
	char *cgroup_name;
	struct cgroup *ccg;
	struct cgroup_controller *cpu, *cpuacct;
	struct passwd *passwd;
	struct group *grp;
	int ret;

	passwd = getpwnam(task_uid.c_str());
	if (!passwd)
		return NULL;
	tuid = passwd->pw_uid;

	grp = getgrnam(task_gid.c_str());
	if (!grp)
		return NULL;
	tgid = grp->gr_gid;

	passwd = getpwnam(control_uid.c_str());
	if (!passwd)
		return NULL;
	cuid = passwd->pw_uid;

	grp = getgrnam(control_gid.c_str());
	if (!grp)
		return NULL;
	cgid = grp->gr_gid;

	cgroup_dbg("tuid %d, tgid %d, cuid %d, cgid %d\n", tuid, tgid, cuid, cgid);

	cgroup_name = (char *) malloc(name.length());
	strncpy(cgroup_name, name.c_str(), name.length() + 1);

	ccg = cgroup_new_cgroup(cgroup_name);
	cgroup_set_uid_gid(ccg, tuid, tgid, cuid, cgid);
	cpu = cgroup_add_controller(ccg, "cpu");
	cgroup_add_value_uint64(cpu, "cpu.shares", 2048);
	cpuacct = cgroup_add_controller(ccg, "cpuacct");
	cgroup_add_value_uint64(cpuacct, "cpuacct.usage", 0);


	ret = cgroup_create_cgroup(ccg, 1);
	if (ret) {
		cout << "cg create group failed " << errno << endl;
		ret = cgroup_delete_cgroup(ccg, 1);
		if (ret)
			cout << "cg delete group failed " << errno << endl;
	}
	return ccg;
}

struct cgroup *cg::makenodefromparent(const string &name)
{
	char *cgroup_name;
	struct cgroup *ccg;
	int ret;

	cgroup_name = (char *) malloc(name.length());
	memset(cgroup_name, '\0', name.length());
	strcpy(cgroup_name, name.c_str());

	ccg = cgroup_new_cgroup(cgroup_name);
	ret = cgroup_create_cgroup_from_parent(ccg, 1);
	if (ret) {
		cout << "cg create group failed " << errno << endl;
		ret = cgroup_delete_cgroup(ccg, 1);
		if (ret)
			cout << "cg delete group failed " << errno << endl;
	}
	return ccg;
}

} // namespace

using namespace cgtest;
int main(int argc, char *argv[])
{
	try {
		cg *app = new cg();
		struct cgroup *ccg, *ccg_child1, *ccg_child2;
		ccg = app->makenode("database", "root", "root", "balbir",
					"balbir");
		ccg_child1 = app->makenodefromparent("mysql");
		ccg_child2 = app->makenodefromparent("mysql/db1");
		cgroup_free(&ccg);
		cgroup_free(&ccg_child1);
		cgroup_free(&ccg_child2);
		delete app;
	} catch (exception &e) {
		cout << e.what() << endl;
		exit(1);
	}
	return 0;
}
