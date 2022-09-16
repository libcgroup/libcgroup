#include <libcgroup.h>
#include <stdio.h>
#include <stdlib.h>

int main()
{
	int error;
	void *handle;
	struct cgroup_mount_point info;

	error = cgroup_init();

	if (error) {
		printf("cgroup_init failed with %s\n", cgroup_strerror(error));
		exit(1);
	}

	error = cgroup_get_controller_begin(&handle, &info);

	while (error != ECGEOF) {
		printf("Controller %s is mounted at %s\n", info.name,
								info.path);
		error = cgroup_get_controller_next(&handle, &info);
		if (error && error != ECGEOF) {
			printf("cgroup_get_contrller_next failed with %s",
							cgroup_strerror(error));
			exit(1);
		}
	}

	error = cgroup_get_controller_end(&handle);

	return 0;
}
