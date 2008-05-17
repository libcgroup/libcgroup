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
 * Basic acceptance test for libcg - Written one late night by Balbir Singh
 */
using namespace std;

#include <string>
#include <stdexcept>
#include <iostream>
#include <libcg.h>
#include <sys/types.h>
#include <pwd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

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
};

cg::cg(void)
{
	int ret;

	ret = cg_init();
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
	struct cgroup *ccg;
	struct passwd *passwd;
	struct group *grp;
	int ret;

	ccg = (struct cgroup *)malloc(sizeof(*ccg));

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

	dbg("tuid %d, tgid %d, cuid %d, cgid %d\n", tuid, tgid, cuid, cgid);

	strcpy(ccg->name, name.c_str());
	ccg->controller[0] = (struct controller *)
				calloc(1, sizeof(struct controller));
	strcpy(ccg->controller[0]->name,"cpu");

	ccg->controller[0]->values[0] = (struct control_value *)
					calloc(1, sizeof(struct control_value));
	strcpy(ccg->controller[0]->values[0]->name,"cpu.shares");
	strcpy(ccg->controller[0]->values[0]->value, "100");

	ccg->controller[1] = (struct controller *)
				calloc(1, sizeof(struct controller));
	strcpy(ccg->controller[1]->name, "memory");
	ccg->controller[1]->values[0] = (struct control_value *)
					calloc(1, sizeof(struct control_value));
	strcpy(ccg->controller[1]->values[0]->name,"memory.limit_in_bytes");
	strcpy(ccg->controller[1]->values[0]->value, "102400");

	strcpy(ccg->controller[2]->name, "cpuacct");
	ccg->controller[2]->values[0] = (struct control_value *)
					calloc(1, sizeof(struct control_value));
	strcpy(ccg->controller[2]->values[0]->name,"cpuacct.usage");
	strcpy(ccg->controller[2]->values[0]->value, "0");
	ccg->tasks_uid = tuid;
	ccg->tasks_gid = tgid;
	ccg->control_uid = cuid;
	ccg->control_gid = cgid;

	ret = cg_create_cgroup(ccg, 1);
	if (ret) {
		cout << "cg create group failed " << errno << endl;
		ret = cg_delete_cgroup(ccg, 1);
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
		struct cgroup *ccg;
		ccg = app->makenode("database", "root", "root", "balbir",
					"balbir");
		delete app;
	} catch (exception &e) {
		cout << e.what() << endl;
		exit(1);
	}
	return 0;
}
