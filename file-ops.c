/*
 * Copyright IBM Corporation. 2007
 *
 * Authors:	Balbir Singh <balbir@linux.vnet.ibm.com>
 *		Dhaval Giani <dhaval@linux.vnet.ibm.com>
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

#include <dirent.h>
#include <errno.h>
#include <fts.h>
#include <grp.h>
#include <libcg.h>
#include <limits.h>
#include <pwd.h>
#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>

extern const char cg_filesystem[];
extern struct mount_table *mount_table;

int cg_chown_file(FTS *fts, FTSENT *ent, uid_t owner, gid_t group)
{
	int ret = 1;
	const char *filename = fts->fts_path;
	dbg("seeing file %s\n", filename);
	switch (ent->fts_info) {
	case FTS_ERR:
		errno = ent->fts_errno;
		break;
	case FTS_D:
	case FTS_DC:
	case FTS_NSOK:
	case FTS_NS:
	case FTS_DNR:
	case FTS_DP:
	case FTS_F:
	case FTS_DEFAULT:
		ret = chown(filename, owner, group);
		break;
	}
	return ret;
}

/*
 * TODO: Need to decide a better place to put this function.
 */
int cg_chown_recursive(const char *path, uid_t owner, gid_t group)
{
	int ret = 1;
	FTS *fts = fts_open((char **)&path, FTS_PHYSICAL | FTS_NOCHDIR |
				FTS_NOSTAT, NULL);
	while (1) {
		FTSENT *ent;
		ent = fts_read(fts);
		if (!ent) {
			dbg("fts_read failed\n");
			break;
		}
		cg_chown_file(fts, ent, owner, group);
	}
	fts_close(fts);
	return ret;
}

char *cg_build_group_path(struct cg_group *cg_group,
					struct mount_table *mount_info)
{
	char *group_path;

	group_path = malloc(strlen(mount_info->mount_point) +
						strlen(cg_group->name) + 2);
	if (!group_path)
		return NULL;
	strncpy(group_path, mount_info->mount_point,
		strlen(mount_info->mount_point) + 1);
	dbg("group path is %s\n", group_path);
	strncat(group_path, "/", strlen("/"));
	strncat(group_path, cg_group->name, strlen(cg_group->name));
	dbg("group path is %s\n", group_path);
	return group_path;
}

int cg_make_directory(struct cg_group *cg_group, const char *group_path)
{
	int ret;
	ret = mkdir(group_path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	if (ret < 0)
		return 0;
	/*
	 * Recursively change all files under the directory
	 */
	ret = cg_chown_recursive(group_path, cg_group->admin_uid,
						cg_group->admin_gid);
	return ret;
}

/*
 * After having parsed the configuration file, walk through the mount_table
 * and mount the specified controllers. This routine might create directories
 * specified as mount_point.
 */
int cg_mount_controllers(void)
{
	int ret;
	struct mount_table *curr = mount_table;
	struct stat buf;

	while (curr) {
		ret = stat(curr->mount_point, &buf);
		if (ret < 0 && errno != ENOENT)
			return 0;

		/*
		 * Check if path needs to be created before mounting
		 */
		if (errno == ENOENT) {
			ret = mkdir(curr->mount_point, S_IRWXU |
						S_IRWXG | S_IROTH | S_IXOTH);
			if (ret < 0)
				return 0;
		} else if (!S_ISDIR(buf.st_mode)) {
			errno = ENOTDIR;
			return 0;
		}

		ret = mount(cg_filesystem, curr->mount_point,
					cg_filesystem, 0, curr->options);

		if (ret < 0)
			return 0;
		curr = curr->next;
	}
	return 1;
}

/*
 * Called during end of WLM configuration to unmount all controllers or
 * on failure, to cleanup mounted controllers
 */
int cg_unmount_controllers(void)
{
	struct mount_table *curr = mount_table;
	int ret;

	while (curr) {
		/*
		 * We ignore failures and ensure that all mounted
		 * containers are unmounted
		 */
		ret = umount(curr->mount_point);
		if (ret < 0)
			printf("Unmount failed\n");
		ret = rmdir(curr->mount_point);
		if (ret < 0)
			printf("rmdir failed\n");
		curr = curr->next;
	}
	return 1;
}

