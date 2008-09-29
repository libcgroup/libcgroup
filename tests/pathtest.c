#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <libcgroup.h>

int main(int argc, char *argv[])
{
	char *path;
	char *expected_path, *controller;
	int ret;

	if (argc < 2) {
		fprintf(stderr, "Usage %s: <controller name> <path>\n",
			argv[0]);
		exit(EXIT_FAILURE);
	}

	controller = argv[1];
	expected_path = argv[2];

	cgroup_init();

	ret = cgroup_get_current_controller_path(getpid(), controller, &path);
	if (ret)
		printf("Test FAIL, get path failed for controller %s\n",
			controller);
	else {
		if (strcmp(path, expected_path))
			printf("Test FAIL, expected_path %s, got path %s\n",
				expected_path, path);
		else
			printf("Test PASS, controller %s path %s\n",
				controller, path);
		free(path);
	}

	return EXIT_SUCCESS;
}
