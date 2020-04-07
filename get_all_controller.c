#include <libcgroup.h>
#include <stdio.h>
#include <stdlib.h>

int main()
{
	int error;
	void *handle;
	struct controller_data info;

	error = cgroup_init();

	if (error) {
		printf("cgroup_init failed with %s\n", cgroup_strerror(error));
		exit(1);
	}

	error = cgroup_get_all_controller_begin(&handle, &info);

	while (error != ECGEOF) {
		printf("Controller %10s %5d %5d %5d\n", info.name,
			info.hierarchy, info.num_cgroups, info.enabled);
		error = cgroup_get_all_controller_next(&handle, &info);
		if (error && error != ECGEOF) {
			printf("cgroup_get_controller_next failed with %s\n",
							cgroup_strerror(error));
			exit(1);
		}
	}

	error = cgroup_get_all_controller_end(&handle);

	return 0;
}
