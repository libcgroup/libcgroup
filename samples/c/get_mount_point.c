#include <libcgroup.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main()
{
	int ret;
	char *mount_point;
	char string[100];

	strcpy(string, "cpu");

	ret = cgroup_init();
	if (ret) {
		printf("cgroup_init failed with %s\n", cgroup_strerror(ret));
		exit(3);
	}

	ret = cgroup_get_subsys_mount_point(string, &mount_point);
	if (ret) {
		printf("get_mount_point failed with %s\n",
					cgroup_strerror(ret));
		exit(3);
	}

	printf("The mount point is %s\n", mount_point);
	free(mount_point);

	strcpy(string, "obviouslynonexistsubsys");

	ret = cgroup_get_subsys_mount_point(string, &mount_point);

	if (!ret) {
		printf("get_mount_point failed as it got a "
					"non existant subsys\n");
		exit(3);
	}

	if (ret == ECGROUPNOTEXIST) {
		printf("get_mount_point worked as expected\n");
		return 0;
	}

	printf("get_mount_point failed with %s\n", cgroup_strerror(ret));

	return 3;
}
