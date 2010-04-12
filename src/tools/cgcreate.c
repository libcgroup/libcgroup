/*
 * Copyright Red Hat, Inc. 2009
 *
 * Authors:	Ivana Hutarova Varekova <varekova@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2.1 of the GNU Lesser General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#include <libcgroup.h>
#include <libcgroup-internal.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <grp.h>

#include "tools-common.h"

int main(int argc, char *argv[])
{
	int ret = 0;
	int i, j;
	int c;

	/* Structure to get GID from group name */
	struct group *grp = NULL;
	char *grp_string = NULL;

	/* Structure to get UID from user name */
	struct passwd *pwd = NULL;
	char *pwd_string = NULL;

	uid_t tuid = CGRULE_INVALID, auid = CGRULE_INVALID;
	gid_t tgid = CGRULE_INVALID, agid = CGRULE_INVALID;

	struct cgroup_group_spec **cgroup_list;
	struct cgroup *cgroup;
	struct cgroup_controller *cgc;

	/* approximation of max. numbers of groups that will be created */
	int capacity = argc;

	/* no parametr on input */
	if (argc < 2) {
		fprintf(stderr, "Usage is %s "
			"-t <tuid>:<tgid> -a <agid>:<auid> "
			"-g <list of controllers>:<relative path to cgroup>\n",
			argv[0]);
		return -1;
	}
	cgroup_list = calloc(capacity, sizeof(struct cgroup_group_spec *));
	if (cgroup_list == NULL) {
		fprintf(stderr, "%s: out of memory\n", argv[0]);
		return -1;
	}

	/* parse arguments */
	while ((c = getopt(argc, argv, "a:t:g:")) > 0) {
		switch (c) {
		case 'a':
			/* set admin uid/gid */
			if (optarg[0] == ':')
				grp_string = strtok(optarg, ":");
			else {
				pwd_string = strtok(optarg, ":");
				if (pwd_string != NULL)
					grp_string = strtok(NULL, ":");
			}

			if (pwd_string != NULL) {
				pwd = getpwnam(pwd_string);
				if (pwd != NULL) {
					auid = pwd->pw_uid;
				} else {
					fprintf(stderr, "%s: "
						"can't find uid of user %s.\n",
						argv[0], pwd_string);
					return -1;
				}
			}
			if (grp_string != NULL) {
				grp = getgrnam(grp_string);
				if (grp != NULL)
					agid = grp->gr_gid;
				else {
					fprintf(stderr, "%s: "
						"can't find gid of group %s.\n",
						argv[0], grp_string);
					return -1;
				}
			}

			break;
		case 't':
			/* set task uid/gid */
			if (optarg[0] == ':')
				grp_string = strtok(optarg, ":");
			else {
				pwd_string = strtok(optarg, ":");
				if (pwd_string != NULL)
					grp_string = strtok(NULL, ":");
			}

			if (pwd_string != NULL) {
				pwd = getpwnam(pwd_string);
				if (pwd != NULL) {
					tuid = pwd->pw_uid;
				} else {
					fprintf(stderr, "%s: "
						"can't find uid of user %s.\n",
						argv[0], pwd_string);
					return -1;
				}
			}
			if (grp_string != NULL) {
				grp = getgrnam(grp_string);
				if (grp != NULL)
					tgid = grp->gr_gid;
				else {
					fprintf(stderr, "%s: "
						"can't find gid of group %s.\n",
						argv[0], grp_string);
					return -1;
				}
			}
			break;
		case 'g':
			ret = parse_cgroup_spec(cgroup_list, optarg, capacity);
			if (ret) {
				fprintf(stderr, "%s: "
					"cgroup controller and path"
					"parsing failed (%s)\n",
					argv[0], argv[optind]);
				return -1;
			}
			break;
		default:
			fprintf(stderr, "%s: "
				"invalid command line option\n",
				argv[0]);
			return -1;
			break;
		}
	}

	/* no cgroup name */
	if (argv[optind]) {
		fprintf(stderr, "%s: "
			"wrong arguments (%s)\n",
			argv[0], argv[optind]);
		ret = -1;
		goto err;
	}

	/* initialize libcg */
	ret = cgroup_init();
	if (ret) {
		fprintf(stderr, "%s: "
			"libcgroup initialization failed: %s\n",
			argv[0], cgroup_strerror(ret));
		goto err;
	}

	/* for each new cgroup */
	for (i = 0; i < capacity; i++) {
		if (!cgroup_list[i])
			break;

		/* create the new cgroup structure */
		cgroup = cgroup_new_cgroup(cgroup_list[i]->path);
		if (!cgroup) {
			ret = ECGFAIL;
			fprintf(stderr, "%s: can't add new cgroup: %s\n",
				argv[0], cgroup_strerror(ret));
			goto err;
		}

		/* set uid and gid for the new cgroup based on input options */
		ret = cgroup_set_uid_gid(cgroup, tuid, tgid, auid, agid);
		if (ret)
			goto err;

		/* add controllers to the new cgroup */
		j = 0;
		while (cgroup_list[i]->controllers[j]) {
			cgc = cgroup_add_controller(cgroup,
				cgroup_list[i]->controllers[j]);
			if (!cgc) {
				ret = ECGINVAL;
				fprintf(stderr, "%s: "
					"controller %s can't be add\n",
					argv[0],
					cgroup_list[i]->controllers[j]);
				cgroup_free(&cgroup);
				goto err;
			}
			j++;
		}

		/* all variables set so create cgroup */
		ret = cgroup_create_cgroup(cgroup, 0);
		if (ret) {
			fprintf(stderr, "%s: "
				"can't create cgroup %s: %s\n",
				argv[0], cgroup->name, cgroup_strerror(ret));
			cgroup_free(&cgroup);
			goto err;
		}
		cgroup_free(&cgroup);
	}
err:
	if (cgroup_list) {
		for (i = 0; i < capacity; i++) {
			if (cgroup_list[i])
				cgroup_free_group_spec(cgroup_list[i]);
		}
		free(cgroup_list);
	}
	return ret;
}
