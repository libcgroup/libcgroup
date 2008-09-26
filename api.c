/*
 * Copyright IBM Corporation. 2007
 *
 * Author:	Dhaval Giani <dhaval@linux.vnet.ibm.com>
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
 * TODOs:
 *	1. Convert comments to Docbook style.
 *	2. Add more APIs for the control groups.
 *	3. Handle the configuration related APIs.
 *	4. Error handling.
 *
 * Code initiated and designed by Dhaval Giani. All faults are most likely
 * his mistake.
 */

#include <dirent.h>
#include <errno.h>
#include <libcgroup.h>
#include <libcgroup-internal.h>
#include <mntent.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <fts.h>
#include <ctype.h>
#include <pwd.h>
#include <libgen.h>

#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION 0.01
#endif

#define VERSION(ver)	#ver

/*
 * Remember to bump this up for major API changes.
 */
const static char cg_version[] = VERSION(PACKAGE_VERSION);

struct cg_mount_table_s cg_mount_table[CG_CONTROLLER_MAX];
static pthread_rwlock_t cg_mount_table_lock = PTHREAD_RWLOCK_INITIALIZER;

/* Check if cgroup_init has been called or not. */
static int cgroup_initialized;

/* Check if the rules cache has been loaded or not. */
static bool cgroup_rules_loaded;

/* List of configuration rules */
static struct cgroup_rule_list rl;

/* Temporary list of configuration rules (for non-cache apps) */
static struct cgroup_rule_list trl;

/* Lock for the list of rules (rl) */
static pthread_rwlock_t rl_lock = PTHREAD_RWLOCK_INITIALIZER;

static int cg_chown_file(FTS *fts, FTSENT *ent, uid_t owner, gid_t group)
{
	int ret = 0;
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
static int cg_chown_recursive(char **path, uid_t owner, gid_t group)
{
	int ret = 0;
	dbg("path is %s\n", *path);
	FTS *fts = fts_open(path, FTS_PHYSICAL | FTS_NOCHDIR |
				FTS_NOSTAT, NULL);
	while (1) {
		FTSENT *ent;
		ent = fts_read(fts);
		if (!ent) {
			dbg("fts_read failed\n");
			break;
		}
		ret = cg_chown_file(fts, ent, owner, group);
	}
	fts_close(fts);
	return ret;
}

static int cgroup_test_subsys_mounted(const char *name)
{
	int i;

	pthread_rwlock_rdlock(&cg_mount_table_lock);

	for (i = 0; cg_mount_table[i].name[0] != '\0'; i++) {
		if (strncmp(cg_mount_table[i].name, name,
				sizeof(cg_mount_table[i].name)) == 0) {
			pthread_rwlock_unlock(&cg_mount_table_lock);
			return 1;
		}
	}
	pthread_rwlock_unlock(&cg_mount_table_lock);
	return 0;
}

/**
 * Free a single cgroup_rule struct.
 * 	@param r The rule to free from memory
 */
static void cgroup_free_rule(struct cgroup_rule *r)
{
	/* Loop variable */
	int i = 0;

	/* Make sure our rule is not NULL, first. */
	if (!r) {
		dbg("Warning: Attempted to free NULL rule.\n");
		return;
	}

	/* We must free any used controller strings, too. */
	for(i = 0; i < MAX_MNT_ELEMENTS; i++) {
		if (r->controllers[i])
			free(r->controllers[i]);
	}

	free(r);
}

/**
 * Free a list of cgroup_rule structs.  If rl is the main list of rules,
 * the lock must be taken for writing before calling this function!
 * 	@param rl Pointer to the list of rules to free from memory
 */
static void cgroup_free_rule_list(struct cgroup_rule_list *rl)
{
	/* Temporary pointer */
	struct cgroup_rule *tmp = NULL;

	/* Make sure we're not freeing NULL memory! */
	if (!(rl->head)) {
		dbg("Warning: Attempted to free NULL list.\n");
		return;
	}

	while (rl->head) {
		tmp = rl->head;
		rl->head = tmp->next;
		cgroup_free_rule(tmp);
	}

	/* Don't leave wild pointers around! */
	rl->head = NULL;
	rl->tail = NULL;
}

/**
 * Parse the configuration file that maps UID/GIDs to cgroups.  If ever the
 * configuration file is modified, applications should call this function to
 * load the new configuration rules.  The function caller is responsible for
 * calling free() on each rule in the list.
 *
 * The cache parameter alters the behavior of this function.  If true, this
 * function will read the entire configuration file and store the results in
 * rl (global rules list).  If false, this function will only parse until it
 * finds a rule matching the given UID or GID.  It will store this rule in rl,
 * as well as any children rules (rules that begin with a %) that it has.
 *
 * This function is NOT thread safe!
 * 	@param cache True to cache rules, else false
 * 	@param muid If cache is false, the UID to match against
 * 	@param mgid If cache is false, the GID to match against
 * 	@return 0 on success, -1 if no cache and match found, > 0 on error.
 * TODO: Make this function thread safe!
 */
static int cgroup_parse_rules(bool cache, uid_t muid, gid_t mgid)
{
	/* File descriptor for the configuration file */
	FILE *fp = NULL;

	/* Buffer to store the line we're working on */
	char *buff = NULL;

	/* Iterator for the line we're working on */
	char *itr = NULL;

	/* Pointer to the list that we're using */
	struct cgroup_rule_list *lst = NULL;

	/* Rule to add to the list */
	struct cgroup_rule *newrule = NULL;

	/* Structure to get GID from group name */
	struct group *grp = NULL;

	/* Structure to get UID from user name */
	struct passwd *pwd = NULL;

	/* Temporary storage for a configuration rule */
	char user[LOGIN_NAME_MAX] = { '\0' };
	char controllers[CG_CONTROLLER_MAX] = { '\0' };
	char destination[FILENAME_MAX] = { '\0' };
	uid_t uid = CGRULE_INVALID;
	gid_t gid = CGRULE_INVALID;

	/* The current line number */
	unsigned int linenum = 0;

	/* Did we skip the previous line? */
	bool skipped = false;

	/* Have we found a matching rule (non-cache mode)? */
	bool matched = false;

	/* Return codes */
	int ret = 0;

	/* Temporary buffer for strtok() */
	char *stok_buff = NULL;

	/* Loop variable. */
	int i = 0;

	/* Open the configuration file. */
	pthread_rwlock_wrlock(&rl_lock);
	fp = fopen(CGRULES_CONF_FILE, "r");
	if (!fp) {
		dbg("Failed to open configuration file %s with"
				" error: %s\n", CGRULES_CONF_FILE,
				strerror(errno));
		ret = errno;
		goto finish;
	}

	buff = calloc(CGROUP_RULE_MAXLINE, sizeof(char));
	if (!buff) {
		dbg("Out of memory?  Error: %s\n", strerror(errno));
		ret = errno;
		goto close_unlock;
	}

	/* Determine which list we're using. */
	if (cache)
		lst = &rl;
	else
		lst = &trl;

	/* If our list already exists, clean it. */
	if (lst->head)
		cgroup_free_rule_list(lst);

	/* Now, parse the configuration file one line at a time. */
	dbg("Parsing configuration file.\n");
	while ((itr = fgets(buff, CGROUP_RULE_MAXLINE, fp)) != NULL) {
		linenum++;

		/* We ignore anything after a # sign as comments. */
		if ((itr = strchr(buff, '#')))
			*itr = '\0';

		/* We also need to remove the newline character. */
		if ((itr = strchr(buff, '\n')))
			*itr = '\0';

		/* Now, skip any leading tabs and spaces. */
		itr = buff;
		while (itr && isblank(*itr))
			itr++;

		/* If there's nothing left, we can ignore this line. */
		if (!strlen(itr))
			continue;

		/*
		 * If we skipped the last rule and this rule is a continuation
		 * of it (begins with %), then we should skip this rule too.
		 */
		if (skipped && *itr == '%') {
			dbg("Warning: Skipped child of invalid rule,"
					" line %d.\n", linenum);
			memset(buff, '\0', CGROUP_RULE_MAXLINE);
			continue;
		}

		/*
		 * If there is something left, it should be a rule.  Otherwise,
		 * there's an error in the configuration file.
		 */
		skipped = false;
		memset(user, '\0', LOGIN_NAME_MAX);
		memset(controllers, '\0', CG_CONTROLLER_MAX);
		memset(destination, '\0', FILENAME_MAX);
		i = sscanf(itr, "%s%s%s", user, controllers, destination);
		if (i != 3) {
			dbg("Failed to parse configuration file on"
					" line %d.\n", linenum);
			goto parsefail;
		}

		/*
		 * Next, check the user/group.  If it's a % sign, then we
		 * are continuing another rule and UID/GID should not be
		 * reset.  If it's a @, we're dealing with a GID rule.  If
		 * it's a *, then we do not need to do a lookup because the
		 * rule always applies (it's a wildcard).  If we're using
		 * non-cache mode and we've found a matching rule, we only
		 * continue to parse if we're looking at a child rule.
		 */
		if ((!cache) && matched && (strncmp(user, "%", 1) != 0)) {
			/* If we make it here, we finished (non-cache). */
			dbg("Parsing of configuration file complete.\n\n");
			ret = -1;
			goto cleanup;
		}
		if (strncmp(user, "@", 1) == 0) {
			/* New GID rule. */
			itr = &(user[1]);
			if ((grp = getgrnam(itr))) {
				uid = CGRULE_INVALID;
				gid = grp->gr_gid;
			} else {
				dbg("Warning: Entry for %s not"
						"found.  Skipping rule on line"
						" %d.\n", itr, linenum);
				memset(buff, '\0', CGROUP_RULE_MAXLINE);
				skipped = true;
				continue;
			}
		} else if (strncmp(user, "*", 1) == 0) {
			/* Special wildcard rule. */
			uid = CGRULE_WILD;
			gid = CGRULE_WILD;
		} else if (*itr != '%') {
			/* New UID rule. */
			if ((pwd = getpwnam(user))) {
				uid = pwd->pw_uid;
				gid = CGRULE_INVALID;
			} else {
				dbg("Warning: Entry for %s not"
						"found.  Skipping rule on line"
						" %d.\n", user, linenum);
				memset(buff, '\0', CGROUP_RULE_MAXLINE);
				skipped = true;
				continue;
			}
		} /* Else, we're continuing another rule (UID/GID are okay). */

		/*
		 * If we are not caching rules, then we need to check for a
		 * match before doing anything else.  We consider four cases:
		 * The UID matches, the GID matches, the UID is a member of the
		 * GID, or we're looking at the wildcard rule, which always
		 * matches.  If none of these are true, we simply continue to
		 * the next line in the file.
		 */
		if (grp && muid != CGRULE_INVALID) {
			pwd = getpwuid(muid);
			for (i = 0; grp->gr_mem[i]; i++) {
				if (!(strcmp(pwd->pw_name, grp->gr_mem[i])))
					matched = true;
			}
		}

		if (uid == muid || gid == mgid || uid == CGRULE_WILD) {
			matched = true;
		}

		if (!cache && !matched)
			continue;

		/*
		 * Now, we're either caching rules or we found a match.  Either
		 * way, copy everything into a new rule and push it into the
		 * list.
		 */
		newrule = calloc(1, sizeof(struct cgroup_rule));
		if (!newrule) {
			dbg("Out of memory?  Error: %s\n", strerror(errno));
			ret = errno;
			goto cleanup;
		}

		newrule->uid = uid;
		newrule->gid = gid;
		strncpy(newrule->name, user, strlen(user));
		strncpy(newrule->destination, destination, strlen(destination));
		newrule->next = NULL;

		/* Parse the controller list, and add that to newrule too. */
		stok_buff = strtok(controllers, ",");
		if (!stok_buff) {
			dbg("Failed to parse controllers on line"
					" %d\n", linenum);
			goto destroyrule;
		}

		i = 0;
		do {
			if (i >= MAX_MNT_ELEMENTS) {
				dbg("Too many controllers listed"
					" on line %d\n", linenum);
				goto destroyrule;
			}

			newrule->controllers[i] = strndup(stok_buff,
							strlen(stok_buff) + 1);
			if (!(newrule->controllers[i])) {
				dbg("Out of memory?  Error was: %s\n",
					strerror(errno));
				goto destroyrule;
			}
			i++;
		} while ((stok_buff = strtok(NULL, ",")));

		/* Now, push the rule. */
		if (lst->head == NULL) {
			lst->head = newrule;
			lst->tail = newrule;
		} else {
			lst->tail->next = newrule;
			lst->tail = newrule;
		}

                dbg("Added rule %s (UID: %d, GID: %d) -> %s for controllers:",
                        lst->tail->name, lst->tail->uid, lst->tail->gid,
			lst->tail->destination);
                for (i = 0; lst->tail->controllers[i]; i++) {
			dbg(" %s", lst->tail->controllers[i]);
		}
		dbg("\n");

		/* Finally, clear the buffer. */
		memset(buff, '\0', CGROUP_RULE_MAXLINE);
		grp = NULL;
		pwd = NULL;
	}

	/* If we make it here, there were no errors. */
	dbg("Parsing of configuration file complete.\n\n");
	ret = (matched && !cache) ? -1 : 0;
	goto cleanup;

destroyrule:
	cgroup_free_rule(newrule);

parsefail:
	ret = ECGROUPPARSEFAIL;

cleanup:
	free(buff);

close_unlock:
	fclose(fp);
	pthread_rwlock_unlock(&rl_lock);
finish:
	return ret;
}

/**
 * cgroup_init(), initializes the MOUNT_POINT.
 *
 * This code is theoretically thread safe now. Its not really tested
 * so it can blow up. If does for you, please let us know with your
 * test case and we can really make it thread safe.
 *
 */
int cgroup_init()
{
	FILE *proc_mount;
	struct mntent *ent, *temp_ent;
	int found_mnt = 0;
	int ret = 0;
	static char *controllers[CG_CONTROLLER_MAX];
	FILE *proc_cgroup;
	char subsys_name[FILENAME_MAX];
	int hierarchy, num_cgroups, enabled;
	int i=0;
	char *mntopt;
	int err;
	char *buf;
	char mntent_buffer[4 * FILENAME_MAX];
	char *strtok_buffer;

	pthread_rwlock_wrlock(&cg_mount_table_lock);

	proc_cgroup = fopen("/proc/cgroups", "r");

	if (!proc_cgroup) {
		ret = EIO;
		goto unlock_exit;
	}

	/*
	 * The first line of the file has stuff we are not interested in.
	 * So just read it and discard the information.
	 *
	 * XX: fix the size for fgets
	 */
	buf = fgets(subsys_name, FILENAME_MAX, proc_cgroup);
	if (!buf) {
		ret = EIO;
		goto unlock_exit;
	}

	while (!feof(proc_cgroup)) {
		err = fscanf(proc_cgroup, "%s %d %d %d", subsys_name,
				&hierarchy, &num_cgroups, &enabled);
		if (err < 0)
			break;
		controllers[i] = (char *)malloc(strlen(subsys_name));
		strcpy(controllers[i], subsys_name);
		i++;
	}
	controllers[i] = NULL;
	fclose(proc_cgroup);

	proc_mount = fopen("/proc/mounts", "r");
	if (proc_mount == NULL) {
		ret = ECGFAIL;
		goto unlock_exit;
	}

	temp_ent = (struct mntent *) malloc(sizeof(struct mntent));

	if (!temp_ent) {
		ret = ECGFAIL;
		goto unlock_exit;
	}

	while ((ent = getmntent_r(proc_mount, temp_ent,
					mntent_buffer,
					sizeof(mntent_buffer))) != NULL) {
		if (!strcmp(ent->mnt_type, "cgroup")) {
			for (i = 0; controllers[i] != NULL; i++) {
				mntopt = hasmntopt(ent, controllers[i]);

				if (!mntopt)
					continue;

				mntopt = strtok_r(mntopt, ",", &strtok_buffer);

				if (strcmp(mntopt, controllers[i]) == 0) {
					dbg("matched %s:%s\n", mntopt,
						controllers[i]);
					strcpy(cg_mount_table[found_mnt].name,
						controllers[i]);
					strcpy(cg_mount_table[found_mnt].path,
						ent->mnt_dir);
					dbg("Found cgroup option %s, "
						" count %d\n",
						ent->mnt_opts, found_mnt);
					found_mnt++;
				}
			}
		}
	}

	if (!found_mnt) {
		cg_mount_table[0].name[0] = '\0';
		ret = ECGROUPNOTMOUNTED;
		goto unlock_exit;
	}

	found_mnt++;
	cg_mount_table[found_mnt].name[0] = '\0';

	fclose(proc_mount);
	cgroup_initialized = 1;

unlock_exit:
	pthread_rwlock_unlock(&cg_mount_table_lock);
	return ret;
}

static int cg_test_mounted_fs()
{
	FILE *proc_mount;
	struct mntent *ent, *temp_ent;
	char mntent_buff[4 * FILENAME_MAX];

	proc_mount = fopen("/proc/mounts", "r");
	if (proc_mount == NULL) {
		return -1;
	}

	temp_ent = (struct mntent *) malloc(sizeof(struct mntent));
	if (!temp_ent) {
		/* We just fail at the moment. */
		return 0;
	}

	ent = getmntent_r(proc_mount, temp_ent, mntent_buff,
						sizeof(mntent_buff));

	if (!ent)
		return 0;

	while (strcmp(ent->mnt_type, "cgroup") !=0) {
		ent = getmntent_r(proc_mount, temp_ent, mntent_buff,
						sizeof(mntent_buff));
		if (ent == NULL)
			return 0;
	}
	fclose(proc_mount);
	return 1;
}

static inline pid_t cg_gettid()
{
	return syscall(__NR_gettid);
}


/* Call with cg_mount_table_lock taken */
static char *cg_build_path_locked(char *name, char *path, char *type)
{
	int i;
	for (i = 0; cg_mount_table[i].name[0] != '\0'; i++) {
		if (strcmp(cg_mount_table[i].name, type) == 0) {
			strcpy(path, cg_mount_table[i].path);
			strcat(path, "/");
			if (name) {
				strcat(path, name);
				strcat(path, "/");
			}
			return path;
		}
	}
	return NULL;
}

static char *cg_build_path(char *name, char *path, char *type)
{
	pthread_rwlock_rdlock(&cg_mount_table_lock);
	path = cg_build_path_locked(name, path, type);
	pthread_rwlock_unlock(&cg_mount_table_lock);

	return path;
}

/** cgroup_attach_task_pid is used to assign tasks to a cgroup.
 *  struct cgroup *cgroup: The cgroup to assign the thread to.
 *  pid_t tid: The thread to be assigned to the cgroup.
 *
 *  returns 0 on success.
 *  returns ECGROUPNOTOWNER if the caller does not have access to the cgroup.
 *  returns ECGROUPNOTALLOWED for other causes of failure.
 */
int cgroup_attach_task_pid(struct cgroup *cgroup, pid_t tid)
{
	char path[FILENAME_MAX];
	FILE *tasks;
	int i, ret = 0;

	if (!cgroup_initialized) {
		dbg ("libcgroup is not initialized\n");
		return ECGROUPNOTINITIALIZED;
	}
	if(!cgroup)
	{
		pthread_rwlock_rdlock(&cg_mount_table_lock);
		for(i = 0; i < CG_CONTROLLER_MAX &&
				cg_mount_table[i].name[0]!='\0'; i++) {
			if (!cg_build_path_locked(NULL, path,
						cg_mount_table[i].name))
				continue;
			strcat(path, "/tasks");

			tasks = fopen(path, "w");
			if (!tasks) {
				pthread_rwlock_unlock(&cg_mount_table_lock);
				switch (errno) {
				case EPERM:
					return ECGROUPNOTOWNER;
				case ENOENT:
					return ECGROUPNOTEXIST;
				default:
					return ECGROUPNOTALLOWED;
				}
			}
			ret = fprintf(tasks, "%d", tid);
			if (ret < 0) {
				dbg("Error writing tid %d to %s:%s\n",
						tid, path, strerror(errno));
				fclose(tasks);
				return ECGOTHER;
			}

			ret = fflush(tasks);
			if (ret) {
				dbg("Error writing tid  %d to %s:%s\n",
						tid, path, strerror(errno));
				fclose(tasks);
				return ECGOTHER;
			}
			fclose(tasks);
		}
		pthread_rwlock_unlock(&cg_mount_table_lock);
	} else {
		for (i = 0; i < cgroup->index; i++) {
			if (!cgroup_test_subsys_mounted(cgroup->controller[i]->name)) {
				dbg("subsystem %s is not mounted\n",
					cgroup->controller[i]->name);
				return ECGROUPSUBSYSNOTMOUNTED;
			}
		}

		for (i = 0; i < cgroup->index; i++) {
			if (!cg_build_path(cgroup->name, path,
					cgroup->controller[i]->name))
				continue;

			strcat(path, "/tasks");

			tasks = fopen(path, "w");
			if (!tasks) {
				dbg("fopen failed for %s:%s", path,
							strerror(errno));

				switch (errno) {
				case EPERM:
					return ECGROUPNOTOWNER;
				case ENOENT:
					return ECGROUPNOTEXIST;
				default:
					return ECGROUPNOTALLOWED;
				}
			}
			ret = fprintf(tasks, "%d", tid);
			if (ret < 0) {
				dbg("Error writing tid %d to %s:%s\n",
						tid, path, strerror(errno));
				fclose(tasks);
				return ECGOTHER;
			}
			ret = fflush(tasks);
			if (ret) {
				dbg("Error writing tid  %d to %s:%s\n",
						tid, path, strerror(errno));
				fclose(tasks);
				return ECGOTHER;
			}
			fclose(tasks);
		}
	}
	return 0;
}

/** cgroup_attach_task is used to attach the current thread to a cgroup.
 *  struct cgroup *cgroup: The cgroup to assign the current thread to.
 *
 *  See cg_attach_task_pid for return values.
 */
int cgroup_attach_task(struct cgroup *cgroup)
{
	pid_t tid = cg_gettid();
	int error;

	error = cgroup_attach_task_pid(cgroup, tid);

	return error;
}

/*
 * create_control_group()
 * This is the basic function used to create the control group. This function
 * just makes the group. It does not set any permissions, or any control values.
 * The argument path is the fully qualified path name to make it generic.
 */
static int cg_create_control_group(char *path)
{
	int error;
	if (!cg_test_mounted_fs())
		return ECGROUPNOTMOUNTED;
	error = mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	if (error) {
		switch(errno) {
		case EEXIST:
			/*
			 * If the directory already exists, it really should
			 * not be an error
			 */
			return 0;
		case EPERM:
			return ECGROUPNOTOWNER;
		default:
			return ECGROUPNOTALLOWED;
		}
	}
	return error;
}

/*
 * set_control_value()
 * This is the low level function for putting in a value in a control file.
 * This function takes in the complete path and sets the value in val in that
 * file.
 */
static int cg_set_control_value(char *path, char *val)
{
	FILE *control_file;
	if (!cg_test_mounted_fs())
		return ECGROUPNOTMOUNTED;

	control_file = fopen(path, "r+");

	if (!control_file) {
		if (errno == EPERM) {
			/*
			 * We need to set the correct error value, does the
			 * group exist but we don't have the subsystem
			 * mounted at that point, or is it that the group
			 * does not exist. So we check if the tasks file
			 * exist. Before that, we need to extract the path.
			 */
			int len = strlen(path);

			while (*(path+len) != '/')
				len--;
			*(path+len+1) = '\0';
			strcat(path, "tasks");
			control_file = fopen(path, "r");
			if (!control_file) {
				if (errno == ENOENT)
					return ECGROUPSUBSYSNOTMOUNTED;
			}
			fclose(control_file);
			return ECGROUPNOTALLOWED;
		}
		return ECGROUPVALUENOTEXIST;
	}

	fprintf(control_file, "%s", val);
	fclose(control_file);
	return 0;
}

/** cgroup_modify_cgroup modifies the cgroup control files.
 * struct cgroup *cgroup: The name will be the cgroup to be modified.
 * The values will be the values to be modified, those not mentioned
 * in the structure will not be modified.
 *
 * The uids cannot be modified yet.
 *
 * returns 0 on success.
 *
 */

int cgroup_modify_cgroup(struct cgroup *cgroup)
{
	char path[FILENAME_MAX], base[FILENAME_MAX];
	int i;
	int error;

	if (!cgroup_initialized)
		return ECGROUPNOTINITIALIZED;

	if (!cgroup)
		return ECGROUPNOTALLOWED;

	for (i = 0; i < cgroup->index; i++) {
		if (!cgroup_test_subsys_mounted(cgroup->controller[i]->name)) {
			dbg("subsystem %s is not mounted\n",
				cgroup->controller[i]->name);
			return ECGROUPSUBSYSNOTMOUNTED;
		}
	}

	strcpy(path, base);
	for (i = 0; i < cgroup->index; i++, strcpy(path, base)) {
		int j;
		if (!cg_build_path(cgroup->name, base,
			cgroup->controller[i]->name))
			continue;
		strcpy(path, base);
		for (j = 0; j < cgroup->controller[i]->index;
				j++, strcpy(path, base)) {
			strcat(path, cgroup->controller[i]->values[j]->name);
			error = cg_set_control_value(path,
				cgroup->controller[i]->values[j]->value);
			if (error)
				goto err;
		}
	}
	return 0;
err:
	return error;

}

/**
 * @dst: Destination controller
 * @src: Source controller from which values will be copied to dst
 *
 * Create a duplicate copy of values under the specified controller
 */
int cgroup_copy_controller_values(struct cgroup_controller *dst,
					struct cgroup_controller *src)
{
	int i, ret = 0;

	if (!dst || !src)
		return ECGFAIL;

	strncpy(dst->name, src->name, FILENAME_MAX);
	for (i = 0; i < src->index; i++, dst->index++) {
		struct control_value *src_val = src->values[i];
		struct control_value *dst_val;

		dst->values[i] = calloc(1, sizeof(struct control_value));
		if (!dst->values[i]) {
			ret = ECGFAIL;
			goto err;
		}

		dst_val = dst->values[i];
		strncpy(dst_val->value, src_val->value, CG_VALUE_MAX);
		strncpy(dst_val->name, src_val->name, FILENAME_MAX);
	}
err:
	return ret;
}

/**
 * @dst: Destination control group
 * @src: Source from which values will be copied to dst
 *
 * Create a duplicate copy of src in dst. This will be useful for those who
 * that intend to create new instances based on an existing control group
 */
int cgroup_copy_cgroup(struct cgroup *dst, struct cgroup *src)
{
	int ret = 0, i;

	if (!dst || !src)
		return ECGROUPNOTEXIST;

	/*
	 * Should we just use the restrict keyword instead?
	 */
	if (dst == src)
		return ECGFAIL;

	cgroup_free_controllers(dst);

	for (i = 0; i < src->index; i++, dst->index++) {
		struct cgroup_controller *src_ctlr = src->controller[i];
		struct cgroup_controller *dst_ctlr;

		dst->controller[i] = calloc(1, sizeof(struct cgroup_controller));
		if (!dst->controller[i]) {
			ret = ECGFAIL;
			goto err;
		}

		dst_ctlr = dst->controller[i];
		ret = cgroup_copy_controller_values(dst_ctlr, src_ctlr);
		if (ret)
			goto err;
	}
err:
	return ret;
}

/** cgroup_create_cgroup creates a new control group.
 * struct cgroup *cgroup: The control group to be created
 *
 * returns 0 on success. We recommend calling cg_delete_cgroup
 * if this routine fails. That should do the cleanup operation.
 */
int cgroup_create_cgroup(struct cgroup *cgroup, int ignore_ownership)
{
	char *fts_path[2], base[FILENAME_MAX], *path;
	int i, j, k;
	int error = 0;

	if (!cgroup_initialized)
		return ECGROUPNOTINITIALIZED;

	if (!cgroup)
		return ECGROUPNOTALLOWED;

	for (i = 0; i < cgroup->index;	i++) {
		if (!cgroup_test_subsys_mounted(cgroup->controller[i]->name))
			return ECGROUPSUBSYSNOTMOUNTED;
	}

	fts_path[0] = (char *)malloc(FILENAME_MAX);
	if (!fts_path[0])
		return ENOMEM;
	fts_path[1] = NULL;
	path = fts_path[0];

	/*
	 * XX: One important test to be done is to check, if you have multiple
	 * subsystems mounted at one point, all of them *have* be on the cgroup
	 * data structure. If not, we fail.
	 */
	for (k = 0; k < cgroup->index; k++) {
		path[0] = '\0';

		if (!cg_build_path(cgroup->name, path,
				cgroup->controller[k]->name))
			continue;

		error = cg_create_control_group(path);
		if (error)
			goto err;

		strcpy(base, path);

		if (!ignore_ownership)
			error = cg_chown_recursive(fts_path,
				cgroup->control_uid, cgroup->control_gid);

		if (error)
			goto err;

		for (j = 0; j < cgroup->controller[k]->index; j++,
							strcpy(path, base)) {
			strcat(path, cgroup->controller[k]->values[j]->name);
			error = cg_set_control_value(path,
				cgroup->controller[k]->values[j]->value);
			/*
			 * Should we undo, what we've done in the loops above?
			 * An error should not be treated as fatal, since we have
			 * several read-only files and several files that
			 * are only conditionally created in the child.
			 */
			if (error)
				continue;
		}

		if (!ignore_ownership) {
			strcpy(path, base);
			strcat(path, "/tasks");
			error = chown(path, cgroup->tasks_uid,
							cgroup->tasks_gid);
			if (!error) {
				error = ECGFAIL;
				goto err;
			}
		}
	}

err:
	free(path);
	return error;
}

/**
 * Find the parent of the specified directory. It returns the parent (the
 * parent is usually name/.. unless name is a mount point.
 */
char *cgroup_find_parent(char *name)
{
	char child[FILENAME_MAX], *parent;
	struct stat stat_child, stat_parent;
	char *type;
	char *dir;

	pthread_rwlock_rdlock(&cg_mount_table_lock);
	type = cg_mount_table[0].name;
	if (!cg_build_path_locked(name, child, type)) {
		pthread_rwlock_unlock(&cg_mount_table_lock);
		return NULL;
	}
	pthread_rwlock_unlock(&cg_mount_table_lock);

	dbg("path is %s\n", child);
	dir = dirname(child);
	dbg("directory name is %s\n", dir);

	if (asprintf(&parent, "%s/..", dir) < 0)
		return NULL;

	dbg("parent's name is %s\n", parent);

	if (stat(dir, &stat_child) < 0)
		goto free_parent;

	if (stat(parent, &stat_parent) < 0)
		goto free_parent;

	/*
	 * Is the specified "name" a mount point?
	 */
	if (stat_parent.st_dev != stat_child.st_dev) {
		dbg("parent is a mount point\n");
		strcpy(parent, ".");
	} else {
		dir = strdup(name);
		if (!dir)
			goto free_parent;
		dir = dirname(dir);
		if (strcmp(dir, ".") == 0)
			strcpy(parent, "..");
		else
			strcpy(parent, dir);
	}

	return parent;

free_parent:
	free(parent);
	return NULL;
}

/**
 * @cgroup: cgroup data structure to be filled with parent values and then
 *	  passed down for creation
 * @ignore_ownership: Ignore doing a chown on the newly created cgroup
 */
int cgroup_create_cgroup_from_parent(struct cgroup *cgroup,
					int ignore_ownership)
{
	char *parent;
	struct cgroup *parent_cgroup;
	int ret = ECGFAIL;

	if (!cgroup_initialized)
		return ECGROUPNOTINITIALIZED;

	parent = cgroup_find_parent(cgroup->name);
	if (!parent)
		return ret;

	dbg("parent is %s\n", parent);
	parent_cgroup = cgroup_new_cgroup(parent);
	if (!parent_cgroup)
		goto err_nomem;

	if (cgroup_get_cgroup(parent_cgroup) == NULL)
		goto err_parent;

	dbg("got parent group for %s\n", parent_cgroup->name);
	ret = cgroup_copy_cgroup(cgroup, parent_cgroup);
	if (ret)
		goto err_parent;

	dbg("copied parent group %s to %s\n", parent_cgroup->name,
							cgroup->name);
	ret = cgroup_create_cgroup(cgroup, ignore_ownership);

err_parent:
	cgroup_free(&parent_cgroup);
err_nomem:
	free(parent);
	return ret;
}

/** cgroup_delete cgroup deletes a control group.
 *  struct cgroup *cgroup takes the group which is to be deleted.
 *
 *  returns 0 on success.
 */
int cgroup_delete_cgroup(struct cgroup *cgroup, int ignore_migration)
{
	FILE *delete_tasks, *base_tasks = NULL;
	int tids;
	char path[FILENAME_MAX];
	int error = ECGROUPNOTALLOWED;
	int i, ret;

	if (!cgroup_initialized)
		return ECGROUPNOTINITIALIZED;

	if (!cgroup)
		return ECGROUPNOTALLOWED;

	for (i = 0; i < cgroup->index; i++) {
		if (!cgroup_test_subsys_mounted(cgroup->controller[i]->name))
			return ECGROUPSUBSYSNOTMOUNTED;
	}

	for (i = 0; i < cgroup->index; i++) {
		if (!cg_build_path(cgroup->name, path,
					cgroup->controller[i]->name))
			continue;
		strcat(path, "../tasks");

		base_tasks = fopen(path, "w");
		if (!base_tasks)
			goto base_open_err;

		if (!cg_build_path(cgroup->name, path,
					cgroup->controller[i]->name))
			continue;

		strcat(path, "tasks");

		delete_tasks = fopen(path, "r");
		if (!delete_tasks)
			goto del_open_err;

		while (!feof(delete_tasks)) {
			ret = fscanf(delete_tasks, "%d", &tids);
			/*
			 * Don't know how to handle EOF yet, so
			 * ignore it
			 */
			fprintf(base_tasks, "%d", tids);
		}

		if (!cg_build_path(cgroup->name, path,
					cgroup->controller[i]->name))
			continue;
		error = rmdir(path);

		fclose(delete_tasks);
	}
del_open_err:
	if (base_tasks)
		fclose(base_tasks);
base_open_err:
	if (ignore_migration) {
		for (i = 0; i < cgroup->index; i++) {
			if (!cg_build_path(cgroup->name, path,
						cgroup->controller[i]->name))
				continue;
			error = rmdir(path);
			if (error < 0 && errno == ENOENT)
				error = 0;
		}
	}
	if (error)
		return ECGOTHER;

	return error;
}

/*
 * This function should really have more checks, but this version
 * will assume that the callers have taken care of everything.
 * Including the locking.
 */
static char *cg_rd_ctrl_file(char *subsys, char *cgroup, char *file)
{
	char *value;
	char path[FILENAME_MAX];
	FILE *ctrl_file;
	int ret;

	if (!cg_build_path_locked(cgroup, path, subsys))
		return NULL;

	strcat(path, file);
	ctrl_file = fopen(path, "r");
	if (!ctrl_file)
		return NULL;

	value = malloc(CG_VALUE_MAX);
	if (!value)
		return NULL;

	/*
	 * using %as crashes when we try to read from files like
	 * memory.stat
	 */
	ret = fscanf(ctrl_file, "%s", value);
	if (ret == 0 || ret == EOF) {
		free(value);
		value = NULL;
	}

	fclose(ctrl_file);

	return value;
}

/*
 * Call this function with required locks taken.
 */
static int cgroup_fill_cgc(struct dirent *ctrl_dir, struct cgroup *cgroup,
			struct cgroup_controller *cgc, int index)
{
	char *ctrl_name;
	char *ctrl_file;
	char *ctrl_value = NULL;
	char *d_name;
	char path[FILENAME_MAX+1];
	char *buffer;
	int error = 0;
	struct stat stat_buffer;

	d_name = strdup(ctrl_dir->d_name);

	if (!strcmp(d_name, ".") || !strcmp(d_name, "..")) {
		error = ECGINVAL;
		goto fill_error;
	}


	/*
	 * This part really needs to be optimized out. Probably use
	 * some sort of a flag, but this is fine for now.
	 */

	cg_build_path_locked(cgroup->name, path, cg_mount_table[index].name);
	strcat(path, d_name);

	error = stat(path, &stat_buffer);

	if (error) {
		error = ECGFAIL;
		goto fill_error;
	}

	cgroup->control_uid = stat_buffer.st_uid;
	cgroup->control_gid = stat_buffer.st_gid;

	ctrl_name = strtok_r(d_name, ".", &buffer);

	if (!ctrl_name) {
		error = ECGFAIL;
		goto fill_error;
	}

	ctrl_file = strtok_r(NULL, ".", &buffer);

	if (!ctrl_file) {
		error = ECGINVAL;
		goto fill_error;
	}

	if (strcmp(ctrl_name, cg_mount_table[index].name) == 0) {
		ctrl_value = cg_rd_ctrl_file(cg_mount_table[index].name,
				cgroup->name, ctrl_dir->d_name);
		if (!ctrl_value) {
			error = ECGFAIL;
			goto fill_error;
		}

		if (cgroup_add_value_string(cgc, ctrl_dir->d_name,
				ctrl_value)) {
			error = ECGFAIL;
			goto fill_error;
		}
	}
fill_error:
	if (ctrl_value)
		free(ctrl_value);
	free(d_name);
	return error;
}

/*
 * cgroup_get_cgroup returns the cgroup data from the filesystem.
 * struct cgroup has the name of the group to be populated
 *
 * return succesfully filled cgroup data structure on success.
 */
struct cgroup *cgroup_get_cgroup(struct cgroup *cgroup)
{
	int i;
	char path[FILENAME_MAX];
	DIR *dir;
	struct dirent *ctrl_dir;
	char *control_path;
	int error;

	if (!cgroup_initialized) {
		/* ECGROUPNOTINITIALIZED */
		return NULL;
	}

	if (!cgroup) {
		/* ECGROUPNOTALLOWED */
		return NULL;
	}

	pthread_rwlock_rdlock(&cg_mount_table_lock);
	for (i = 0; i < CG_CONTROLLER_MAX &&
			cg_mount_table[i].name[0] != '\0'; i++) {
		/*
		 * cgc will not leak, since it has to be freed using
		 * cgroup_free_cgroup
		 */
		struct cgroup_controller *cgc;
		struct stat stat_buffer;
		int path_len;

		if (!cg_build_path_locked(NULL, path,
					cg_mount_table[i].name))
			continue;

		path_len = strlen(path);
		strncat(path, cgroup->name, FILENAME_MAX - path_len - 1);

		if (access(path, F_OK))
			continue;

		if (!cg_build_path_locked(cgroup->name, path,
					cg_mount_table[i].name)) {
			/*
			 * This fails when the cgroup does not exist
			 * for that controller.
			 */
			continue;
		}

		/*
		 * Get the uid and gid information
		 */

		control_path = malloc(strlen(path) + strlen("tasks") + 1);
		strcpy(control_path, path);

		if (!control_path)
			goto unlock_error;

		strcat(control_path, "tasks");

		if (stat(control_path, &stat_buffer)) {
			free(control_path);
			goto unlock_error;
		}

		cgroup->tasks_uid = stat_buffer.st_uid;
		cgroup->tasks_gid = stat_buffer.st_gid;

		free(control_path);

		cgc = cgroup_add_controller(cgroup,
				cg_mount_table[i].name);
		if (!cgc)
			goto unlock_error;

		dir = opendir(path);
		if (!dir) {
			/* error = ECGROUPSTRUCTERROR; */
			goto unlock_error;
		}

		while ((ctrl_dir = readdir(dir)) != NULL) {
			/*
			 * Skip over non regular files
			 */
			if (ctrl_dir->d_type != DT_REG)
				continue;

			error = cgroup_fill_cgc(ctrl_dir, cgroup, cgc, i);
			if (error == ECGFAIL) {
				closedir(dir);
				goto unlock_error;
			}
		}
		closedir(dir);
	}

	/* Check if the group really exists or not */
	if (!cgroup->index)
		goto unlock_error;

	pthread_rwlock_unlock(&cg_mount_table_lock);
	return cgroup;

unlock_error:
	pthread_rwlock_unlock(&cg_mount_table_lock);
	cgroup = NULL;
	return NULL;
}

/** cg_prepare_cgroup
 * Process the selected rule. Prepare the cgroup structure which can be
 * used to add the task to destination cgroup.
 *
 *
 *  returns 0 on success.
 */
static int cg_prepare_cgroup(struct cgroup *cgroup, pid_t pid,
					const char *dest,
					char *controllers[])
{
	int ret = 0, i;
	char *controller;
	struct cgroup_controller *cptr;

	/* Fill in cgroup details.  */
	dbg("Will move pid %d to cgroup '%s'\n", pid, dest);

	strcpy(cgroup->name, dest);

	/* Scan all the controllers */
	for (i = 0; i < CG_CONTROLLER_MAX; i++) {
		if (!controllers[i])
			return 0;
		controller = controllers[i];

		/* If first string is "*" that means all the mounted
		 * controllers. */
		if (strcmp(controller, "*") == 0) {
			pthread_rwlock_rdlock(&cg_mount_table_lock);
			for (i = 0; i < CG_CONTROLLER_MAX &&
				cg_mount_table[i].name[0] != '\0'; i++) {
				dbg("Adding controller %s\n",
					cg_mount_table[i].name);
				cptr = cgroup_add_controller(cgroup,
						cg_mount_table[i].name);
				if (!cptr) {
					dbg("Adding controller '%s' failed\n",
						cg_mount_table[i].name);
					pthread_rwlock_unlock(&cg_mount_table_lock);
					return ECGROUPNOTALLOWED;
				}
			}
			pthread_rwlock_unlock(&cg_mount_table_lock);
			return ret;
		}

		/* it is individual controller names and not "*" */
		dbg("Adding controller %s\n", controller);
		cptr = cgroup_add_controller(cgroup, controller);
		if (!cptr) {
			dbg("Adding controller '%s' failed\n", controller);
			return ECGROUPNOTALLOWED;
		}
	}

	return ret;
}

/**
 * This function takes a string which has got list of controllers separated
 * by commas and it converts it to an array of string pointer where each
 * string contains name of one controller.
 *
 * returns 0 on success.
 */
static int cg_prepare_controller_array(char *cstr, char *controllers[])
{
	int j = 0;
	char *temp, *saveptr = NULL;

	do {
		if (j == 0)
			temp = strtok_r(cstr, ",", &saveptr);
		else
			temp = strtok_r(NULL, ",", &saveptr);

		if (temp) {
			controllers[j] = strdup(temp);
			if (!controllers[j])
				return ECGOTHER;
		}
		j++;
	} while (temp);
	return 0;
}


static void cg_free_controller_array(char *controllers[])
{
	int j = 0;

	/* Free up temporary controllers array */
	for (j = 0; j < CG_CONTROLLER_MAX; j++) {
		if (!controllers[j])
			break;
		free(controllers[j]);
		controllers[j] = 0;
	}
}

/**
 * Finds the first rule in the cached list that matches the given UID or GID,
 * and returns a pointer to that rule.  This function uses rl_lock.
 *
 * This function may NOT be thread safe.
 * 	@param uid The UID to match
 * 	@param gid The GID to match
 * 	@return Pointer to the first matching rule, or NULL if no match
 * TODO: Determine thread-safeness and fix if not safe.
 */
static struct cgroup_rule *cgroup_find_matching_rule_uid_gid(const uid_t uid,
				const gid_t gid)
{
	/* Return value */
	struct cgroup_rule *ret = rl.head;

	/* Temporary user data */
	struct passwd *usr = NULL;

	/* Temporary group data */
	struct group *grp = NULL;

	/* Temporary string pointer */
	char *sp = NULL;

	/* Loop variable */
	int i = 0;

	pthread_rwlock_wrlock(&rl_lock);
	while (ret) {
		/* The wildcard rule always matches. */
		if ((ret->uid == CGRULE_WILD) && (ret->gid == CGRULE_WILD)) {
			goto finished;
		}

		/* This is the simple case of the UID matching. */
		if (ret->uid == uid) {
			goto finished;
		}

		/* This is the simple case of the GID matching. */
		if (ret->gid == gid) {
			goto finished;
		}

		/* If this is a group rule, the UID might be a member. */
		if (ret->name[0] == '@') {
			/* Get the group data. */
			sp = &(ret->name[1]);
			grp = getgrnam(sp);
			if (!grp) {
				continue;
			}

			/* Get the data for UID. */
			usr = getpwuid(uid);
			if (!usr) {
				continue;
			}

			/* If UID is a member of group, we matched. */
			for (i = 0; grp->gr_mem[i]; i++) {
				if (!(strcmp(usr->pw_name, grp->gr_mem[i])))
					goto finished;
			}
		}

		/* If we haven't matched, try the next rule. */
		ret = ret->next;
	}

	/* If we get here, no rules matched. */
	ret = NULL;

finished:
	pthread_rwlock_unlock(&rl_lock);
	return ret;
}

/**
 * Changes the cgroup of a program based on the rules in the config file.  If a
 * rule exists for the given UID or GID, then the given PID is placed into the
 * correct group.  By default, this function parses the configuration file each
 * time it is called.
 * 
 * The flags can alter the behavior of this function:
 * 	CGFLAG_USECACHE: Use cached rules instead of parsing the config file
 *
 * This function may NOT be thread safe. 
 * 	@param uid The UID to match
 * 	@param gid The GID to match
 * 	@param pid The PID of the process to move
 * 	@param flags Bit flags to change the behavior, as defined above
 * 	@return 0 on success, > 0 on error
 * TODO: Determine thread-safeness and fix of not safe.
 */
int cgroup_change_cgroup_uid_gid_flags(const uid_t uid, const gid_t gid,
				const pid_t pid, const int flags)
{
	/* Temporary pointer to a rule */
	struct cgroup_rule *tmp = NULL;

	/* Return codes */
	int ret = 0;

	/* We need to check this before doing anything else! */
	if (!cgroup_initialized) {
		dbg("libcgroup is not initialized\n");
		ret = ECGROUPNOTINITIALIZED;
		goto finished;
	}

	/* 
	 * If the user did not ask for cached rules, we must parse the
	 * configuration to find a matching rule (if one exists).  Else, we'll
	 * find the first match in the cached list (rl).
	 */
	if (!(flags & CGFLAG_USECACHE)) {
		dbg("Not using cached rules for PID %d.\n", pid);
		ret = cgroup_parse_rules(false, uid, gid);

		/* The configuration file has an error!  We must exit now. */
		if (ret != -1 && ret != 0) {
			dbg("Failed to parse the configuration rules.\n");
			goto finished;
		}

		/* We did not find a matching rule, so we're done. */
		if (ret == 0) {
			dbg("No rule found to match PID: %d, UID: %d, "
				"GID: %d\n", pid, uid, gid);
			goto finished;
		}

		/* Otherwise, we did match a rule and it's in trl. */
		tmp = trl.head;
	} else {
		/* Find the first matching rule in the cached list. */
		tmp = cgroup_find_matching_rule_uid_gid(uid, gid);
		if (!tmp) {
			dbg("No rule found to match PID: %d, UID: %d, "
				"GID: %d\n", pid, uid, gid);
			ret = 0;
			goto finished;
		}
	}
	dbg("Found matching rule %s for PID: %d, UID: %d, GID: %d\n",
			tmp->name, pid, uid, gid);

	/* If we are here, then we found a matching rule, so execute it. */
	do {
		dbg("Executing rule %s for PID %d... ", tmp->name, pid);
		ret = cgroup_change_cgroup_path(tmp->destination,
				pid, tmp->controllers);
		if (ret) {
			dbg("FAILED! (Error Code: %d)\n", ret);
			goto finished;
		}
		dbg("OK!\n");

		/* Now, check for multi-line rules.  As long as the "next"
		 * rule starts with '%', it's actually part of the rule that
		 * we just executed.
		 */
		tmp = tmp->next;
	} while (tmp && (tmp->name[0] == '%'));

finished:
	return ret;
}

/**
 * Provides backwards-compatibility with older versions of the API.  This
 * function is deprecated, and cgroup_change_cgroup_uid_gid_flags() should be
 * used instead.  In fact, this function simply calls the newer one with flags
 * set to 0 (none).
 * 	@param uid The UID to match
 * 	@param gid The GID to match
 * 	@param pid The PID of the process to move
 * 	@return 0 on success, > 0 on error
 * 
 */
int cgroup_change_cgroup_uid_gid(uid_t uid, gid_t gid, pid_t pid)
{
	return cgroup_change_cgroup_uid_gid_flags(uid, gid, pid, 0);
}

/**
 * Changes the cgroup of a program based on the path provided.  In this case,
 * the user must already know into which cgroup the task should be placed and
 * no rules will be parsed.
 *
 *  returns 0 on success.
 */
int cgroup_change_cgroup_path(char *dest, pid_t pid, char *controllers[])
{
	int ret;
	struct cgroup cgroup;

	if (!cgroup_initialized) {
		dbg("libcgroup is not initialized\n");
		return ECGROUPNOTINITIALIZED;
	}
	memset(&cgroup, 0, sizeof(struct cgroup));

	ret = cg_prepare_cgroup(&cgroup, pid, dest, controllers);
	if (ret)
		return ret;
	/* Add task to cgroup */
	ret = cgroup_attach_task_pid(&cgroup, pid);
	if (ret) {
		dbg("cgroup_attach_task_pid failed:%d\n", ret);
		return ret;
	}
	return 0;
}

/**
 * Print the cached rules table.  This function should be called only after
 * first calling cgroup_parse_config(), but it will work with an empty rule
 * list.
 * 	@param fp The file stream to print to
 */
void cgroup_print_rules_config(FILE *fp)
{
	/* Iterator */
	struct cgroup_rule *itr;

	/* Loop variable */
	int i = 0;

	pthread_rwlock_rdlock(&rl_lock);

	if (!(rl.head)) {
		fprintf(fp, "The rules table is empty.\n\n");
		pthread_rwlock_unlock(&rl_lock);
		return;
	}

	itr = rl.head;
	while (itr) {
		fprintf(fp, "Rule: %s\n", itr->name);

		if (itr->uid == CGRULE_WILD)
			fprintf(fp, "  UID: any\n");
		else if (itr->uid == CGRULE_INVALID)
			fprintf(fp, "  UID: N/A\n");
		else
			fprintf(fp, "  UID: %d\n", itr->uid);

		if (itr->gid == CGRULE_WILD)
			fprintf(fp, "  GID: any\n");
		else if (itr->gid == CGRULE_INVALID)
			fprintf(fp, "  GID: N/A\n");
		else
			fprintf(fp, "  GID: %d\n", itr->gid);

		fprintf(fp, "  DEST: %s\n", itr->destination);

		fprintf(fp, "  CONTROLLERS:\n");
		for (i = 0; i < MAX_MNT_ELEMENTS; i++) {
			if (itr->controllers[i]) {
				fprintf(fp, "    %s\n", itr->controllers[i]);
			}
		}
		fprintf(fp, "\n");
		itr = itr->next;
	}
	pthread_rwlock_unlock(&rl_lock);
}

/**
 * Reloads the rules list, using the given configuration file.  This function
 * is probably NOT thread safe (calls cgroup_parse_rules()).
 * 	@return 0 on success, > 0 on failure
 */
int cgroup_reload_cached_rules()
{
	/* Return codes */
	int ret = 0;

	dbg("Reloading cached rules from %s.\n", CGRULES_CONF_FILE);
	if ((ret = cgroup_parse_rules(true, CGRULE_INVALID, CGRULE_INVALID))) {
		dbg("Error parsing configuration file \"%s\": %d.\n",
			CGRULES_CONF_FILE, ret);
		ret = ECGROUPPARSEFAIL;
		goto finished;
	}
		
	#ifdef DEBUG
		cgroup_print_rules_config(stdout);
	#endif

finished:
	return ret;
}

/**
 * Initializes the rules cache.
 * 	@return 0 on success, > 0 on error
 */
int cgroup_init_rules_cache()
{
	/* Return codes */
	int ret = 0;

	/* Attempt to read the configuration file and cache the rules. */
	ret = cgroup_parse_rules(true, CGRULE_INVALID, CGRULE_INVALID);
	if (ret) {
		dbg("Could not initialize rule cache, error was: %d\n", ret);
		cgroup_rules_loaded = false;
	} else {
		cgroup_rules_loaded = true;
	}

	return ret;
}
