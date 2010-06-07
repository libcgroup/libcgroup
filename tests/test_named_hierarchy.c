#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libcgroup.h>

/*
 * Assumes cgroup is mounted at /cgroup using
 *
 * mount -t cgroup -o none,name=test none /cgroup
 */
int main()
{
	int ret;
	struct cgroup *cgroup;
	struct cgroup_controller *cgc;

	ret = cgroup_init();
	if (ret) {
		printf("FAIL: cgroup_init failed with %s\n", cgroup_strerror(ret));
		exit(3);
	}

	cgroup = cgroup_new_cgroup("test");
	if (!cgroup) {
		printf("FAIL: cgroup_new_cgroup failed\n");
		exit(3);
	}

	cgc = cgroup_add_controller(cgroup, "name=test");
	if (!cgc) {
		printf("FAIL: cgroup_add_controller failed\n");
		exit(3);
	}

	ret = cgroup_create_cgroup(cgroup, 1);
	if (ret) {
		printf("FAIL: cgroup_create_cgroup failed with %s\n", cgroup_strerror(ret));
		exit(3);
	}

	if (access("/cgroup/test", F_OK))
		printf("PASS\n");
	else
		printf("Failed to create cgroup\n");

	return 0;
}
