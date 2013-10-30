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
 *	1. Add more APIs for the control groups.
 *	2. Handle the configuration related APIs.
 *
 * Code initiated and designed by Dhaval Giani. All faults are most likely
 * his mistake.
 *
 * Bharata B Rao <bharata@linux.vnet.ibm.com> is willing is take blame
 * for mistakes in APIs for reading statistics.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

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
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <fts.h>
#include <ctype.h>
#include <pwd.h>
#include <libgen.h>
#include <assert.h>
#include <linux/un.h>
#include <grp.h>

/*
 * The errno which happend the last time (have to be thread specific)
 */
__thread int last_errno;

#define MAXLEN 256

/* the value have to be thread specific */
static __thread char errtext[MAXLEN];

/* Task command name length */
#define TASK_COMM_LEN 16

/* Check if cgroup_init has been called or not. */
static int cgroup_initialized;

/* List of configuration rules */
static struct cgroup_rule_list rl;

/* Temporary list of configuration rules (for non-cache apps) */
static struct cgroup_rule_list trl;

/* Lock for the list of rules (rl) */
static pthread_rwlock_t rl_lock = PTHREAD_RWLOCK_INITIALIZER;

/* Namespace */
__thread char *cg_namespace_table[CG_CONTROLLER_MAX];

pthread_rwlock_t cg_mount_table_lock = PTHREAD_RWLOCK_INITIALIZER;
struct cg_mount_table_s cg_mount_table[CG_CONTROLLER_MAX];

const char const *cgroup_strerror_codes[] = {
	"Cgroup is not compiled in",
	"Cgroup is not mounted",
	"Cgroup does not exist",
	"Cgroup has not been created",
	"Cgroup one of the needed subsystems is not mounted",
	"Cgroup, request came in from non owner",
	"Cgroup controllers are bound to different mount points",
	"Cgroup, operation not allowed",
	"Cgroup value set exceeds maximum",
	"Cgroup controller already exists",
	"Cgroup value already exists",
	"Cgroup invalid operation",
	"Cgroup, creation of controller failed",
	"Cgroup operation failed",
	"Cgroup not initialized",
	"Cgroup, requested group parameter does not exist",
	"Cgroup generic error",
	"Cgroup values are not equal",
	"Cgroup controllers are different",
	"Cgroup parsing failed",
	"Cgroup, rules file does not exist",
	"Cgroup mounting failed",
	"End of File or iterator",
	"Failed to parse config file",
	"Have multiple paths for the same namespace",
	"Controller in namespace does not exist",
	"Either mount or namespace keyword has to be specified in the configuration file",
	"This kernel does not support this feature",
	"Value setting does not succeed",
	"Failed to remove a non-empty group",
};

static const char const *cgroup_ignored_tasks_files[] = { "tasks", NULL };

static int cg_chown(const char *filename, uid_t owner, gid_t group)
{
	if (owner == NO_UID_GID)
		owner = getuid();
	if (group == NO_UID_GID)
		group = getgid();
	return chown(filename, owner, group);
}
static int cg_chown_file(FTS *fts, FTSENT *ent, uid_t owner, gid_t group)
{
	int ret = 0;
	const char *filename = fts->fts_path;
	cgroup_dbg("chown: seeing file %s\n", filename);
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
		ret = cg_chown(filename, owner, group);
		break;
	}
	if (ret < 0) {
		cgroup_warn("Warning: cannot change owner of file %s: %s\n",
				filename, strerror(errno));
		last_errno = errno;
		ret = ECGOTHER;
	}
	return ret;
}

/*
 * TODO: Need to decide a better place to put this function.
 */
static int cg_chown_recursive(char **path, uid_t owner, gid_t group)
{
	int ret = 0;
	FTS *fts;

	cgroup_dbg("chown: path is %s\n", *path);
	fts = fts_open(path, FTS_PHYSICAL | FTS_NOCHDIR |
				FTS_NOSTAT, NULL);
	if (fts == NULL) {
		cgroup_warn("Warning: cannot open directory %s: %s\n",
				path, strerror(errno));
		last_errno = errno;
		return ECGOTHER;
	}
	while (1) {
		FTSENT *ent;
		ent = fts_read(fts);
		if (!ent) {
			cgroup_warn("Warning: fts_read failed\n");
			break;
		}
		ret = cg_chown_file(fts, ent, owner, group);
	}
	fts_close(fts);
	return ret;
}

int cg_chmod_path(const char *path, mode_t mode, int owner_is_umask)
{
	struct stat buf;
	mode_t mask = -1U;

	if (owner_is_umask) {
		mode_t umask, gmask, omask;

		/*
		 * Use owner permissions as an umask for group and others
		 * permissions because we trust kernel to initialize owner
		 * permissions to something useful.
		 * Keep SUID and SGID bits.
		 */
		if (stat(path, &buf) == -1)
			goto fail;
		umask = S_IRWXU & buf.st_mode;
		gmask = umask >> 3;
		omask = gmask >> 3;

		mask = umask|gmask|omask|S_ISUID|S_ISGID|S_ISVTX;
	}

	if (chmod(path, mode & mask))
		goto fail;

	return 0;

fail:
	cgroup_warn("Warning: cannot change permissions of file %s: %s\n", path,
			strerror(errno));
	last_errno = errno;
	return ECGOTHER;
}

int cg_chmod_file(FTS *fts, FTSENT *ent, mode_t dir_mode,
	int dirm_change, mode_t file_mode, int filem_change,
	int owner_is_umask)
{
	int ret = 0;
	const char *filename = fts->fts_path;

	cgroup_dbg("chmod: seeing file %s\n", filename);

	switch (ent->fts_info) {
	case FTS_ERR:
		errno = ent->fts_errno;
		break;
	case FTS_D:
	case FTS_DC:
	case FTS_DNR:
	case FTS_DP:
		if (dirm_change)
			ret = cg_chmod_path(filename, dir_mode, owner_is_umask);
		break;
	case FTS_F:
	case FTS_NSOK:
	case FTS_NS:
	case FTS_DEFAULT:
		if (filem_change)
			ret = cg_chmod_path(filename, file_mode,
					owner_is_umask);
		break;
	}
	return ret;
}


/**
 * Changes permissions of all directories and control files (i.e. all
 * files except files named in ignore_list. The list must be terminated with
 * NULL.
 */
static int cg_chmod_recursive_controller(char *path, mode_t dir_mode,
		int dirm_change, mode_t file_mode, int filem_change,
		int owner_is_umask, const char const **ignore_list)
{
	int ret = 0;
	int final_ret =0;
	FTS *fts;
	char *fts_path[2];
	int i, ignored;

	fts_path[0] = path;
	fts_path[1] = NULL;
	cgroup_dbg("chmod: path is %s\n", path);

	fts = fts_open(fts_path, FTS_PHYSICAL | FTS_NOCHDIR |
			FTS_NOSTAT, NULL);
	if (fts == NULL) {
		cgroup_warn("Warning: cannot open directory %s: %s\n",
				fts_path, strerror(errno));
		last_errno = errno;
		return ECGOTHER;
	}
	while (1) {
		FTSENT *ent;
		ent = fts_read(fts);
		if (!ent) {
			if (errno != 0) {
				cgroup_dbg("fts_read failed\n");
				last_errno = errno;
				final_ret = ECGOTHER;
			}
			break;
		}
		ignored = 0;
		if (ignore_list != NULL)
			for (i = 0; ignore_list[i] != NULL; i++)
				if (!strcmp(ignore_list[i], ent->fts_name)) {
					ignored = 1;
					break;
				}
		if (ignored)
			continue;

		ret = cg_chmod_file(fts, ent, dir_mode, dirm_change,
				file_mode, filem_change,
				owner_is_umask);
		if (ret) {
			cgroup_warn("Warning: cannot change file mode %s: %s\n",
					fts_path, strerror(errno));
			last_errno = errno;
			final_ret = ECGOTHER;
		}
	}
	fts_close(fts);
	return final_ret;
}

int cg_chmod_recursive(struct cgroup *cgroup, mode_t dir_mode,
		int dirm_change, mode_t file_mode, int filem_change)
{
	int i;
	char *path;
	int final_ret = 0;
	int ret;

	path = malloc(FILENAME_MAX);
	if (!path) {
		last_errno = errno;
		return ECGOTHER;
	}
	for (i = 0; i < cgroup->index; i++) {
		if (!cg_build_path(cgroup->name, path,
				cgroup->controller[i]->name)) {
			final_ret = ECGFAIL;
			break;
		}
		ret = cg_chmod_recursive_controller(path, dir_mode, dirm_change,
				file_mode, filem_change, 0, NULL);
		if (ret)
			final_ret = ret;
	}
	free(path);
	return final_ret;
}

void cgroup_set_permissions(struct cgroup *cgroup,
		mode_t control_dperm, mode_t control_fperm,
		mode_t task_fperm)
{
	cgroup->control_dperm = control_dperm;
	cgroup->control_fperm = control_fperm;
	cgroup->task_fperm = task_fperm;
}

static char *cgroup_basename(const char *path)
{
	char *base;
	char *tmp_string;

	tmp_string = strdup(path);

	if (!tmp_string)
		return NULL;

	base = strdup(basename(tmp_string));

	free(tmp_string);

	return base;
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
 *	@param r The rule to free from memory
 */
static void cgroup_free_rule(struct cgroup_rule *r)
{
	/* Loop variable */
	int i = 0;

	/* Make sure our rule is not NULL, first. */
	if (!r) {
		cgroup_warn("Warning: attempted to free NULL rule\n");
		return;
	}
	if (r->procname) {
		free(r->procname);
		r->procname = NULL;
	}
	/* We must free any used controller strings, too. */
	for (i = 0; i < MAX_MNT_ELEMENTS; i++) {
		if (r->controllers[i])
			free(r->controllers[i]);
	}

	free(r);
}

/**
 * Free a list of cgroup_rule structs.  If rl is the main list of rules,
 * the lock must be taken for writing before calling this function!
 *	@param rl Pointer to the list of rules to free from memory
 */
static void cgroup_free_rule_list(struct cgroup_rule_list *cg_rl)
{
	/* Temporary pointer */
	struct cgroup_rule *tmp = NULL;

	/* Make sure we're not freeing NULL memory! */
	if (!(cg_rl->head)) {
		cgroup_warn("Warning: attempted to free NULL list\n");
		return;
	}

	while (cg_rl->head) {
		tmp = cg_rl->head;
		cg_rl->head = tmp->next;
		cgroup_free_rule(tmp);
	}

	/* Don't leave wild pointers around! */
	cg_rl->head = NULL;
	cg_rl->tail = NULL;
}

static char *cg_skip_unused_charactors_in_rule(char *rule)
{
	char *itr;

	/* We ignore anything after a # sign as comments. */
	itr = strchr(rule, '#');
	if (itr)
		*itr = '\0';

	/* We also need to remove the newline character. */
	itr = strchr(rule, '\n');
	if (itr)
		*itr = '\0';

	/* Now, skip any leading tabs and spaces. */
	itr = rule;
	while (itr && isblank(*itr))
		itr++;

	/* If there's nothing left, we can ignore this line. */
	if (!strlen(itr))
		return NULL;

	return itr;
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
 *	@param cache True to cache rules, else false
 *	@param muid If cache is false, the UID to match against
 *	@param mgid If cache is false, the GID to match against
 *	@return 0 on success, -1 if no cache and match found, > 0 on error.
 * TODO: Make this function thread safe!
 */
static int cgroup_parse_rules(bool cache, uid_t muid,
					  gid_t mgid, const char *mprocname)
{
	/* File descriptor for the configuration file */
	FILE *fp = NULL;

	/* Buffer to store the line we're working on */
	char buff[CGROUP_RULE_MAXLINE] = { '\0' };

	/* Iterator for the line we're working on */
	char *itr = NULL;

	/* Pointer to process name in a line of the configuration file */
	char *procname = NULL;

	/* Pointer to the list that we're using */
	struct cgroup_rule_list *lst = NULL;

	/* Rule to add to the list */
	struct cgroup_rule *newrule = NULL;

	/* Structure to get GID from group name */
	struct group *grp = NULL;

	/* Structure to get UID from user name */
	struct passwd *pwd = NULL;

	/* Temporary storage for a configuration rule */
	char key[CGROUP_RULE_MAXKEY] = { '\0' };
	char user[LOGIN_NAME_MAX] = { '\0' };
	char controllers[CG_CONTROLLER_MAX] = { '\0' };
	char destination[FILENAME_MAX] = { '\0' };
	uid_t uid = CGRULE_INVALID;
	gid_t gid = CGRULE_INVALID;
	size_t len_username;
	int len_procname;

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

	/* Determine which list we're using. */
	if (cache)
		lst = &rl;
	else
		lst = &trl;

	/* If our list already exists, clean it. */
	if (lst->head)
		cgroup_free_rule_list(lst);

	/* Open the configuration file. */
	pthread_rwlock_wrlock(&rl_lock);
	fp = fopen(CGRULES_CONF_FILE, "re");
	if (!fp) {
		cgroup_warn("Warning: failed to open configuration file %s: %s\n",
				CGRULES_CONF_FILE, strerror(errno));
		goto unlock;
	}

	/* Now, parse the configuration file one line at a time. */
	cgroup_dbg("Parsing configuration file.\n");
	while (fgets(buff, sizeof(buff), fp) != NULL) {
		linenum++;

		itr = cg_skip_unused_charactors_in_rule(buff);
		if (!itr)
			continue;

		/*
		 * If we skipped the last rule and this rule is a continuation
		 * of it (begins with %), then we should skip this rule too.
		 */
		if (skipped && *itr == '%') {
			cgroup_warn("Warning: skipped child of invalid rule,"
					" line %d.\n", linenum);
			continue;
		}

		/*
		 * If there is something left, it should be a rule.  Otherwise,
		 * there's an error in the configuration file.
		 */
		skipped = false;
		i = sscanf(itr, "%s%s%s", key, controllers, destination);
		if (i != 3) {
			cgroup_err(
					"Error: failed to parse configuration file on line %d\n",
					linenum);
			goto parsefail;
		}
		procname = strchr(key, ':');
		if (procname) {
			/* <user>:<procname>  <subsystem>  <destination> */
			procname++;	/* skip ':' */
			len_username = procname - key - 1;
			len_procname = strlen(procname);
			if (len_procname < 0) {
				cgroup_err(
						"Error: failed to parse configuration file on line %d\n",
						linenum);
				goto parsefail;
			}
		} else {
			len_username = strlen(key);
			len_procname = 0;
		}
		len_username = min(len_username, sizeof(user) - 1);
		memset(user, '\0', sizeof(user));
		strncpy(user, key, len_username);

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
			cgroup_dbg("Parsing of configuration file"
				" complete.\n\n");
			ret = -1;
			goto close;
		}
		if (strncmp(user, "@", 1) == 0) {
			/* New GID rule. */
			itr = &(user[1]);
			grp = getgrnam(itr);
			if (grp) {
				uid = CGRULE_INVALID;
				gid = grp->gr_gid;
			} else {
				cgroup_dbg("Warning: Entry for %s not"
						"found.  Skipping rule on line"
						" %d.\n", itr, linenum);
				skipped = true;
				continue;
			}
		} else if (strncmp(user, "*", 1) == 0) {
			/* Special wildcard rule. */
			uid = CGRULE_WILD;
			gid = CGRULE_WILD;
		} else if (*itr != '%') {
			/* New UID rule. */
			pwd = getpwnam(user);
			if (pwd) {
				uid = pwd->pw_uid;
				gid = CGRULE_INVALID;
			} else {
				cgroup_dbg("Warning: Entry for %s not"
						"found.  Skipping rule on line"
						" %d.\n", user, linenum);
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
			if (!pwd) {
				continue;
			}
			for (i = 0; grp->gr_mem[i]; i++) {
				if (!(strcmp(pwd->pw_name, grp->gr_mem[i])))
					matched = true;
			}
		}

		if (uid == muid || gid == mgid || uid == CGRULE_WILD)
			matched = true;

		if (!cache) {
			if (!matched)
				continue;
			if (len_procname) {
				char *mproc_base;
				/*
				 * If there is a rule based on process name,
				 * it should be matched with mprocname.
				 */
				if (!mprocname) {
					uid = CGRULE_INVALID;
					gid = CGRULE_INVALID;
					matched = false;
					continue;
				}

				mproc_base = cgroup_basename(mprocname);
				if (strcmp(mprocname, procname) &&
					strcmp(mproc_base, procname)) {
					uid = CGRULE_INVALID;
					gid = CGRULE_INVALID;
					matched = false;
					free(mproc_base);
					continue;
				}
				free(mproc_base);
			}
		}

		/*
		 * Now, we're either caching rules or we found a match.  Either
		 * way, copy everything into a new rule and push it into the
		 * list.
		 */
		newrule = calloc(1, sizeof(struct cgroup_rule));
		if (!newrule) {
			cgroup_err("Error: out of memory? Error was: %s\n",
				strerror(errno));
			last_errno = errno;
			ret = ECGOTHER;
			goto close;
		}

		newrule->uid = uid;
		newrule->gid = gid;
		len_username = min(len_username,
					sizeof(newrule->username) - 1);
		strncpy(newrule->username, user, len_username);
		if (len_procname) {
			newrule->procname = strdup(procname);
			if (!newrule->procname) {
				cgroup_err("Error: strdup failed to allocate memory %s\n",
						strerror(errno));
				free(newrule);
				last_errno = errno;
				ret = ECGOTHER;
				goto close;
			}
		} else {
			newrule->procname = NULL;
		}
		strncpy(newrule->destination, destination,
			sizeof(newrule->destination) - 1);
		newrule->next = NULL;

		/* Parse the controller list, and add that to newrule too. */
		stok_buff = strtok(controllers, ",");
		if (!stok_buff) {
			cgroup_err("Error: failed to parse controllers on line %d\n",
					linenum);
			goto destroyrule;
		}

		i = 0;
		do {
			if (i >= MAX_MNT_ELEMENTS) {
				cgroup_err("Error: too many controllers listed on line %d\n",
						linenum);
				goto destroyrule;
			}

			newrule->controllers[i] = strndup(stok_buff,
							strlen(stok_buff) + 1);
			if (!(newrule->controllers[i])) {
				cgroup_err("Error: out of memory? Error was: %s\n",
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

		cgroup_dbg("Added rule %s (UID: %d, GID: %d) -> %s for"
			" controllers:", lst->tail->username, lst->tail->uid,
			lst->tail->gid, lst->tail->destination);
		for (i = 0; lst->tail->controllers[i]; i++)
			cgroup_dbg(" %s", lst->tail->controllers[i]);
		cgroup_dbg("\n");

		/* Finally, clear the buffer. */
		grp = NULL;
		pwd = NULL;
	}

	/* If we make it here, there were no errors. */
	cgroup_dbg("Parsing of configuration file complete.\n\n");
	ret = (matched && !cache) ? -1 : 0;
	goto close;

destroyrule:
	cgroup_free_rule(newrule);

parsefail:
	ret = ECGRULESPARSEFAIL;

close:
	fclose(fp);
unlock:
	pthread_rwlock_unlock(&rl_lock);
	return ret;
}

int cg_add_duplicate_mount(struct cg_mount_table_s *item, const char *path)
{
	struct cg_mount_point *mount, *it;

	mount = malloc(sizeof(struct cg_mount_point));
	if (!mount) {
		last_errno = errno;
		return ECGOTHER;
	}
	mount->next = NULL;
	strncpy(mount->path, path, sizeof(mount->path));
	mount->path[sizeof(mount->path)-1] = '\0';

	/*
	 * Add the mount point to the end of the list.
	 * Assuming the list is short, no optimization is done.
	 */
	it = &item->mount;
	while (it->next)
		it = it->next;

	it->next = mount;
	return 0;
}

/**
 * cgroup_init(), initializes the MOUNT_POINT.
 *
 * This code is theoretically thread safe now. Its not really tested
 * so it can blow up. If does for you, please let us know with your
 * test case and we can really make it thread safe.
 *
 */
int cgroup_init(void)
{
	FILE *proc_mount = NULL;
	struct mntent *ent = NULL;
	struct mntent *temp_ent = NULL;
	int found_mnt = 0;
	int ret = 0;
	static char *controllers[CG_CONTROLLER_MAX];
	FILE *proc_cgroup = NULL;
	char subsys_name[FILENAME_MAX];
	int hierarchy, num_cgroups, enabled;
	int i = 0;
	int j;
	int duplicate = 0;
	char *mntopt = NULL;
	int err;
	char *buf = NULL;
	char mntent_buffer[4 * FILENAME_MAX];
	char *strtok_buffer = NULL;

	cgroup_set_default_logger(-1);

	pthread_rwlock_wrlock(&cg_mount_table_lock);

	/* free global variables filled by previous cgroup_init() */
	for (i = 0; cg_mount_table[i].name[0] != '\0'; i++) {
		struct cg_mount_point *mount = cg_mount_table[i].mount.next;
		while (mount) {
			struct cg_mount_point *tmp = mount;
			mount = mount->next;
			free(tmp);
		}
	}
	memset(&cg_mount_table, 0, sizeof(cg_mount_table));

	proc_cgroup = fopen("/proc/cgroups", "re");

	if (!proc_cgroup) {
		cgroup_err("Error: cannot open /proc/cgroups: %s\n",
				strerror(errno));
		last_errno = errno;
		ret = ECGOTHER;
		goto unlock_exit;
	}

	/*
	 * The first line of the file has stuff we are not interested in.
	 * So just read it and discard the information.
	 *
	 * XX: fix the size for fgets
	 */
	buf = malloc(FILENAME_MAX);
	if (!buf) {
		last_errno = errno;
		ret = ECGOTHER;
		goto unlock_exit;
	}
	if (!fgets(buf, FILENAME_MAX, proc_cgroup)) {
		free(buf);
		cgroup_err("Error: cannot read /proc/cgroups: %s\n",
				strerror(errno));
		last_errno = errno;
		ret = ECGOTHER;
		goto unlock_exit;
	}
	free(buf);

	i = 0;
	while (!feof(proc_cgroup)) {
		err = fscanf(proc_cgroup, "%s %d %d %d", subsys_name,
				&hierarchy, &num_cgroups, &enabled);
		if (err < 0)
			break;
		controllers[i] = strdup(subsys_name);
		i++;
	}
	controllers[i] = NULL;

	proc_mount = fopen("/proc/mounts", "re");
	if (proc_mount == NULL) {
		cgroup_err("Error: cannot open /proc/mounts: %s\n",
				strerror(errno));
		last_errno = errno;
		ret = ECGOTHER;
		goto unlock_exit;
	}

	temp_ent = (struct mntent *) malloc(sizeof(struct mntent));

	if (!temp_ent) {
		last_errno = errno;
		ret = ECGOTHER;
		goto unlock_exit;
	}

	while ((ent = getmntent_r(proc_mount, temp_ent,
					mntent_buffer,
					sizeof(mntent_buffer))) != NULL) {
		if (strcmp(ent->mnt_type, "cgroup"))
			continue;

		for (i = 0; controllers[i] != NULL; i++) {
			mntopt = hasmntopt(ent, controllers[i]);

			if (!mntopt)
				continue;

			cgroup_dbg("found %s in %s\n", controllers[i], ent->mnt_opts);

			/* do not have duplicates in mount table */
			duplicate = 0;
			for  (j = 0; j < found_mnt; j++) {
				if (strncmp(controllers[i],
							cg_mount_table[j].name,
							FILENAME_MAX) == 0) {
					duplicate = 1;
					break;
				}
			}
			if (duplicate) {
				cgroup_dbg("controller %s is already mounted on %s\n",
					mntopt, cg_mount_table[j].mount.path);
				ret = cg_add_duplicate_mount(&cg_mount_table[j],
						ent->mnt_dir);
				if (ret)
					goto unlock_exit;
				/* continue with next controller */
				continue;
			}

			strncpy(cg_mount_table[found_mnt].name,
				controllers[i], FILENAME_MAX);
			cg_mount_table[found_mnt].name[FILENAME_MAX-1] = '\0';
			strncpy(cg_mount_table[found_mnt].mount.path,
				ent->mnt_dir, FILENAME_MAX);
			cg_mount_table[found_mnt].mount.path[FILENAME_MAX-1] =
				'\0';
			cg_mount_table[found_mnt].mount.next = NULL;
			cgroup_dbg("Found cgroup option %s, count %d\n",
				ent->mnt_opts, found_mnt);
			found_mnt++;
		}

		/*
		 * Doesn't match the controller.
		 * Check if it is a named hierarchy.
		 */
		mntopt = hasmntopt(ent, "name");

		if (mntopt) {
			mntopt = strtok_r(mntopt, ",", &strtok_buffer);
			if (!mntopt)
				continue;
			/*
			 * Check if it is a duplicate
			 */
			duplicate = 0;

#ifdef OPAQUE_HIERARCHY
			/*
			 * Ignore the opaque hierarchy.
			 */
			if (strcmp(mntopt, OPAQUE_HIERARCHY) == 0)
					continue;
#endif

			for (j = 0; j < found_mnt; j++) {
				if (strncmp(mntopt, cg_mount_table[j].name,
							FILENAME_MAX) == 0) {
					duplicate = 1;
					break;
				}
			}

			if (duplicate) {
				cgroup_dbg("controller %s is already mounted on %s\n",
					mntopt, cg_mount_table[j].mount.path);
				ret = cg_add_duplicate_mount(&cg_mount_table[j],
						ent->mnt_dir);
				if (ret)
					goto unlock_exit;
				continue;
			}

			strncpy(cg_mount_table[found_mnt].name,
				mntopt, FILENAME_MAX);
			cg_mount_table[found_mnt].name[FILENAME_MAX-1] = '\0';
			strncpy(cg_mount_table[found_mnt].mount.path,
				ent->mnt_dir, FILENAME_MAX);
			cg_mount_table[found_mnt].mount.path[FILENAME_MAX-1] =
				'\0';
			cg_mount_table[found_mnt].mount.next = NULL;
			cgroup_dbg("Found cgroup option %s, count %d\n",
				ent->mnt_opts, found_mnt);
			found_mnt++;
		}
	}

	free(temp_ent);

	if (!found_mnt) {
		cg_mount_table[0].name[0] = '\0';
		ret = ECGROUPNOTMOUNTED;
		goto unlock_exit;
	}

	found_mnt++;
	cg_mount_table[found_mnt].name[0] = '\0';

	cgroup_initialized = 1;

unlock_exit:
	if (proc_cgroup)
		fclose(proc_cgroup);

	if (proc_mount)
		fclose(proc_mount);

	for (i = 0; controllers[i]; i++) {
		free(controllers[i]);
		controllers[i] = NULL;
	}

	pthread_rwlock_unlock(&cg_mount_table_lock);

	return ret;
}

static int cg_test_mounted_fs(void)
{
	FILE *proc_mount = NULL;
	struct mntent *ent = NULL;
	struct mntent *temp_ent = NULL;
	char mntent_buff[4 * FILENAME_MAX];
	int ret = 1;

	proc_mount = fopen("/proc/mounts", "re");
	if (proc_mount == NULL)
		return 0;

	temp_ent = (struct mntent *) malloc(sizeof(struct mntent));
	if (!temp_ent) {
		/* We just fail at the moment. */
		fclose(proc_mount);
		return 0;
	}

	ent = getmntent_r(proc_mount, temp_ent, mntent_buff,
						sizeof(mntent_buff));

	if (!ent) {
		ret = 0;
		goto done;
	}

	while (strcmp(ent->mnt_type, "cgroup") != 0) {
		ent = getmntent_r(proc_mount, temp_ent, mntent_buff,
						sizeof(mntent_buff));
		if (ent == NULL) {
			ret = 0;
			goto done;
		}
	}
done:
	fclose(proc_mount);
	free(temp_ent);
	return ret;
}

static inline pid_t cg_gettid(void)
{
	return syscall(__NR_gettid);
}

static char *cg_concat_path(const char *pref, const char *suf, char *path)
{
	if ((suf[strlen(suf)-1] == '/') ||
		((strlen(suf) == 0) && (pref[strlen(pref)-1] == '/'))) {
		snprintf(path, FILENAME_MAX, "%s%s", pref,
			suf+((suf[0] == '/') ? 1 : 0));
	} else {
		snprintf(path, FILENAME_MAX, "%s%s/", pref,
			suf+((suf[0] == '/') ? 1 : 0));
	}
	path[FILENAME_MAX-1] = '\0';
	return path;
}


/* Call with cg_mount_table_lock taken */
/* path value have to have size at least FILENAME_MAX */
static char *cg_build_path_locked(const char *name, char *path,
						const char *type)
{
	int i;
	for (i = 0; cg_mount_table[i].name[0] != '\0'; i++) {
		if (strcmp(cg_mount_table[i].name, type) == 0) {
			if (cg_namespace_table[i]) {
				snprintf(path, FILENAME_MAX, "%s/%s/",
						cg_mount_table[i].mount.path,
						cg_namespace_table[i]);
				path[FILENAME_MAX-1] = '\0';
			} else {
				snprintf(path, FILENAME_MAX, "%s/",
						cg_mount_table[i].mount.path);
				path[FILENAME_MAX-1] = '\0';
			}

			if (name) {
				char *tmp;
				tmp = strdup(path);

				/* FIXME: missing OOM check here! */

				cg_concat_path(tmp, name, path);
				free(tmp);
			}
			return path;
		}
	}
	return NULL;
}

char *cg_build_path(const char *name, char *path, const char *type)
{
	pthread_rwlock_rdlock(&cg_mount_table_lock);
	path = cg_build_path_locked(name, path, type);
	pthread_rwlock_unlock(&cg_mount_table_lock);

	return path;
}

static int __cgroup_attach_task_pid(char *path, pid_t tid)
{
	int ret = 0;
	FILE *tasks = NULL;

	tasks = fopen(path, "we");
	if (!tasks) {
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
		last_errno = errno;
		ret = ECGOTHER;
		goto err;
	}
	ret = fflush(tasks);
	if (ret) {
		last_errno = errno;
		ret = ECGOTHER;
		goto err;
	}
	fclose(tasks);
	return 0;
err:
	cgroup_warn("Warning: cannot write tid %d to %s:%s\n",
			tid, path, strerror(errno));
	fclose(tasks);
	return ret;
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
	int i, ret = 0;

	if (!cgroup_initialized) {
		cgroup_warn("Warning: libcgroup is not initialized\n");
		return ECGROUPNOTINITIALIZED;
	}
	if (!cgroup) {
		pthread_rwlock_rdlock(&cg_mount_table_lock);
		for (i = 0; i < CG_CONTROLLER_MAX &&
				cg_mount_table[i].name[0] != '\0'; i++) {
			if (!cg_build_path_locked(NULL, path,
						cg_mount_table[i].name))
				continue;
			strncat(path, "/tasks", sizeof(path) - strlen(path));
			ret = __cgroup_attach_task_pid(path, tid);
			if (ret) {
				pthread_rwlock_unlock(&cg_mount_table_lock);
				return ret;
			}
		}
		pthread_rwlock_unlock(&cg_mount_table_lock);
	} else {
		for (i = 0; i < cgroup->index; i++) {
			if (!cgroup_test_subsys_mounted(cgroup->controller[i]->name)) {
				cgroup_warn("Warning: subsystem %s is not mounted\n",
						cgroup->controller[i]->name);
				return ECGROUPSUBSYSNOTMOUNTED;
			}
		}

		for (i = 0; i < cgroup->index; i++) {
			if (!cg_build_path(cgroup->name, path,
					cgroup->controller[i]->name))
				continue;
			strncat(path, "/tasks", sizeof(path) - strlen(path));
			ret = __cgroup_attach_task_pid(path, tid);
			if (ret)
				return ret;
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

/**
 * cg_mkdir_p, emulate the mkdir -p command (recursively creating paths)
 * @path: path to create
 */
int cg_mkdir_p(const char *path)
{
	char *real_path = NULL;
	int i = 0;
	char pos;
	int ret = 0, stat_ret;
	struct stat st;

	real_path = strdup(path);
	if (!real_path) {
		last_errno = errno;
		return ECGOTHER;
	}

	do {
		while (real_path[i] != '\0' && real_path[i] == '/')
			i++;
		if (real_path[i] == '\0')
			break; /* The path ends with '/', ignore it. */
		while (real_path[i] != '\0' && real_path[i] != '/')
			i++;
		pos = real_path[i];
		real_path[i] = '\0';		/* Temporarily overwrite "/" */
		ret = mkdir(real_path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
		real_path[i] = pos;
		if (ret) {
			switch (errno) {
			case EEXIST:
				ret = 0;	/* Not fatal really */
				break;
			case EPERM:
				ret = ECGROUPNOTOWNER;
				goto done;
			default:
				/* Check if path exists */
				real_path[i] = '\0';
				stat_ret = stat(real_path, &st);
				real_path[i] = pos;
				if (stat_ret == 0) {
					ret = 0;	/* Path exists */
					break;
				}
				ret = ECGROUPNOTALLOWED;
				goto done;
			}
		}
	} while (real_path[i]);

done:
	free(real_path);
	return ret;
}

/*
 * create_control_group()
 * This is the basic function used to create the control group. This function
 * just makes the group. It does not set any permissions, or any control values.
 * The argument path is the fully qualified path name to make it generic.
 */
static int cg_create_control_group(const char *path)
{
	int error;
	if (!cg_test_mounted_fs())
		return ECGROUPNOTMOUNTED;
	error = cg_mkdir_p(path);
	return error;
}

/*
 * set_control_value()
 * This is the low level function for putting in a value in a control file.
 * This function takes in the complete path and sets the value in val in that
 * file.
 */
static int cg_set_control_value(char *path, const char *val)
{
	FILE *control_file = NULL;
	if (!cg_test_mounted_fs())
		return ECGROUPNOTMOUNTED;

	control_file = fopen(path, "r+e");

	if (!control_file) {
		if (errno == EPERM) {
			/*
			 * We need to set the correct error value, does the
			 * group exist but we don't have the subsystem
			 * mounted at that point, or is it that the group
			 * does not exist. So we check if the tasks file
			 * exist. Before that, we need to extract the path.
			 */
			char *path_dir_end;
			char *tasks_path;

			path_dir_end = strrchr(path, '/');
			if (path_dir_end == NULL)
				return ECGROUPVALUENOTEXIST;
			path_dir_end = '\0';

			/* task_path contain: $path/tasks */
			tasks_path = (char *)malloc(strlen(path) + 6 + 1);
			if (tasks_path == NULL) {
				last_errno = errno;
				return ECGOTHER;
			}
			strcpy(tasks_path, path);
			strcat(tasks_path, "/tasks");

			/* test tasks file for read flag */
			control_file = fopen(tasks_path, "re");
			if (!control_file) {
				if (errno == ENOENT) {
					free(tasks_path);
					return ECGROUPSUBSYSNOTMOUNTED;
				}
			} else {
				fclose(control_file);
			}
			free(tasks_path);
			return ECGROUPNOTALLOWED;
		}
		return ECGROUPVALUENOTEXIST;
	}

	if (fprintf(control_file, "%s", val) < 0) {
		last_errno = errno;
		fclose(control_file);
		return ECGOTHER;
	}
	if (fclose(control_file) < 0) {
		last_errno = errno;
		return ECGOTHER;
	}
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
	char *path, base[FILENAME_MAX];
	int i;
	int error = 0;
	int ret;

	if (!cgroup_initialized)
		return ECGROUPNOTINITIALIZED;

	if (!cgroup)
		return ECGROUPNOTALLOWED;

	for (i = 0; i < cgroup->index; i++) {
		if (!cgroup_test_subsys_mounted(cgroup->controller[i]->name)) {
			cgroup_warn("Warning: subsystem %s is not mounted\n",
				cgroup->controller[i]->name);
			return ECGROUPSUBSYSNOTMOUNTED;
		}
	}

	for (i = 0; i < cgroup->index; i++) {
		int j;
		if (!cg_build_path(cgroup->name, base,
			cgroup->controller[i]->name))
			continue;
		for (j = 0; j < cgroup->controller[i]->index; j++) {
			ret = asprintf(&path, "%s%s", base,
				cgroup->controller[i]->values[j]->name);
			if (ret < 0) {
				last_errno = errno;
				error = ECGOTHER;
				goto err;
			}
			error = cg_set_control_value(path,
				cgroup->controller[i]->values[j]->value);
			free(path);
			path = NULL;
			/* don't consider error in files directly written by
			 * the user as fatal */
			if (error && !cgroup->controller[i]->values[j]->dirty) {
				error = 0;
				continue;
			}
			if (error)
				goto err;
			cgroup->controller[i]->values[j]->dirty = false;
		}
	}
err:
	return error;

}

/**
 * @dst: Destination controller
 * @src: Source controller from which values will be copied to dst
 *
 * Create a duplicate copy of values under the specified controller
 */
static int cgroup_copy_controller_values(struct cgroup_controller *dst,
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
			last_errno = errno;
			ret = ECGOTHER;
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
			last_errno = errno;
			ret = ECGOTHER;
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
 * If ECGCANTSETVALUE is returned, the group was created successfully
 * but not all controller parameters were successfully set.
 */
int cgroup_create_cgroup(struct cgroup *cgroup, int ignore_ownership)
{
	char *fts_path[2];
	char *base = NULL;
	char *path = NULL;
	int i, j, k;
	int error = 0;
	int retval = 0;
	int ret;

	if (!cgroup_initialized)
		return ECGROUPNOTINITIALIZED;

	if (!cgroup)
		return ECGROUPNOTALLOWED;

	for (i = 0; i < cgroup->index;	i++) {
		if (!cgroup_test_subsys_mounted(cgroup->controller[i]->name))
			return ECGROUPSUBSYSNOTMOUNTED;
	}

	fts_path[0] = (char *)malloc(FILENAME_MAX);
	if (!fts_path[0]) {
		last_errno = errno;
		return ECGOTHER;
	}
	fts_path[1] = NULL;
	path = fts_path[0];

	/*
	 * XX: One important test to be done is to check, if you have multiple
	 * subsystems mounted at one point, all of them *have* be on the cgroup
	 * data structure. If not, we fail.
	 */
	for (k = 0; k < cgroup->index; k++) {
		if (!cg_build_path(cgroup->name, path,
				cgroup->controller[k]->name))
			continue;

		error = cg_create_control_group(path);
		if (error)
			goto err;

		base = strdup(path);

		if (!base) {
			last_errno = errno;
			error = ECGOTHER;
			goto err;
		}

		if (!ignore_ownership) {
			cgroup_dbg("Changing ownership of %s\n", fts_path[0]);
			error = cg_chown_recursive(fts_path,
				cgroup->control_uid, cgroup->control_gid);
			if (!error)
				error = cg_chmod_recursive_controller(fts_path[0],
						cgroup->control_dperm,
						cgroup->control_dperm != NO_PERMS,
						cgroup->control_fperm,
						cgroup->control_fperm != NO_PERMS,
						1, cgroup_ignored_tasks_files);
		}

		if (error)
			goto err;

		for (j = 0; j < cgroup->controller[k]->index; j++) {
			ret = snprintf(path, FILENAME_MAX, "%s%s", base,
					cgroup->controller[k]->values[j]->name);
			cgroup_dbg("setting %s to \"%s\", pathlen %d\n", path,
				cgroup->controller[k]->values[j]->value, ret);
			if (ret < 0 || ret >= FILENAME_MAX) {
				last_errno = errno;
				error = ECGOTHER;
				goto err;
			}
			error = cg_set_control_value(path,
				cgroup->controller[k]->values[j]->value);
			/*
			 * Should we undo, what we've done in the loops above?
			 * An error should not be treated as fatal, since we
			 * have several read-only files and several files that
			 * are only conditionally created in the child.
			 *
			 * A middle ground would be to track that there
			 * was an error and return a diagnostic value--
			 * callers don't get context for the error, but can
			 * ignore it specifically if they wish.
			 */
			if (error) {
				cgroup_err("Error: failed to set %s: %s\n",
					path, cgroup_strerror(error));
				retval = ECGCANTSETVALUE;
				continue;
			}
		}

		if (!ignore_ownership) {
			ret = snprintf(path, FILENAME_MAX, "%s/tasks", base);
			if (ret < 0 || ret >= FILENAME_MAX) {
				last_errno = errno;
				error = ECGOTHER;
				goto err;
			}
			error = cg_chown(path, cgroup->tasks_uid,
							cgroup->tasks_gid);
			if (!error && cgroup->task_fperm != NO_PERMS)
				error = cg_chmod_path(path, cgroup->task_fperm,
						1);

			if (error) {
				last_errno = errno;
				error = ECGOTHER;
				goto err;
			}

		}
		free(base);
		base = NULL;
	}

err:
	if (path)
		free(path);
	if (base)
		free(base);
	if (retval && !error)
		error = retval;
	return error;
}

/**
 * Obtain the calculated parent name of specified cgroup; no validation
 * of the existence of the child or parent group is performed.
 *
 * Given the path-like hierarchy of cgroup names, this function returns
 * the dirname() of the cgroup name as the likely parent name; the caller
 * is responsible for validating parent as appropriate.
 *
 * @param cgroup The cgroup to query for parent's name
 * @param parent Output, name of parent's group, or NULL if the
 * 	provided cgroup is the root group.
 *	Caller is responsible to free the returned string.
 * @return 0 on success, > 0 on error
 */
static int cgroup_get_parent_name(struct cgroup *cgroup, char **parent)
{
	int ret = 0;
	char *dir = NULL;
	char *pdir = NULL;

	dir = strdup(cgroup->name);
	if (!dir) {
		last_errno = errno;
		return ECGOTHER;
	}
	cgroup_dbg("group name is %s\n", dir);

	pdir = dirname(dir);
	cgroup_dbg("parent's group name is %s\n", pdir);

	/* check for root group */
	if (strlen(cgroup->name) == 0 || !strcmp(cgroup->name, pdir)) {
		cgroup_dbg("specified cgroup \"%s\" is root group\n",
			cgroup->name);
		*parent = NULL;
	}
	else {
		*parent = strdup(pdir);
		if (*parent == NULL) {
			last_errno = errno;
			ret = ECGOTHER;
		}
	}
	free(dir);

	return ret;
}

/**
 * Find the parent of the specified directory. It returns the parent in
 * hierarchy of given controller (the parent is usually name/.. unless name is
 * a mount point.  It is assumed both the cgroup (and, therefore, parent)
 * already exist, and will fail otherwise.
 *
 * When namespaces are used, a group can have different parents for different
 * controllers.
 *
 * @param cgroup The cgroup
 * @param controller The controller
 * @param parent Output, name of parent's group (if the group has parent) or
 *	NULL, if the provided cgroup is the root group and has no parent.
 *	Caller is responsible to free the returned string!
 * @return 0 on success, >0 on error.
 */
static int cgroup_find_parent(struct cgroup *cgroup, char *controller,
		char **parent)
{
	char child_path[FILENAME_MAX];
	char *parent_path = NULL;
	struct stat stat_child, stat_parent;
	int ret = 0;

	*parent = NULL;

	pthread_rwlock_rdlock(&cg_mount_table_lock);
	if (!cg_build_path_locked(cgroup->name, child_path, controller)) {
		pthread_rwlock_unlock(&cg_mount_table_lock);
		return ECGFAIL;
	}
	pthread_rwlock_unlock(&cg_mount_table_lock);

	cgroup_dbg("path is %s\n", child_path);

	if (asprintf(&parent_path, "%s/..", child_path) < 0)
		return ECGFAIL;

	cgroup_dbg("parent's name is %s\n", parent_path);

	if (stat(child_path, &stat_child) < 0) {
		last_errno = errno;
		ret = ECGOTHER;
		goto free_parent;
	}

	if (stat(parent_path, &stat_parent) < 0) {
		last_errno = errno;
		ret = ECGOTHER;
		goto free_parent;
	}

	/*
	 * Is the specified "name" a mount point?
	 */
	if (stat_parent.st_dev != stat_child.st_dev) {
		*parent = NULL;
		ret = 0;
		cgroup_dbg("Parent is on different device\n");
	} else {
		ret = cgroup_get_parent_name(cgroup, parent);
	}

free_parent:
	free(parent_path);
	return ret;
}

/**
 * @cgroup: cgroup data structure to be filled with parent values and then
 *	  passed down for creation
 * @ignore_ownership: Ignore doing a chown on the newly created cgroup
 * @return 0 on success, > 0 on failure.  If  ECGCANTSETVALUE is returned,
 * the group was created successfully, but not all controller parameters
 * were copied from the parent successfully; unfortunately, this is expected...
 */
int cgroup_create_cgroup_from_parent(struct cgroup *cgroup,
					int ignore_ownership)
{
	char *parent = NULL;
	struct cgroup *parent_cgroup = NULL;
	int ret = ECGFAIL;

	if (!cgroup_initialized)
		return ECGROUPNOTINITIALIZED;

	ret = cgroup_get_parent_name(cgroup, &parent);
	if (ret)
		return ret;

	if (parent == NULL) {
		/*
		 * The group to create is root group!
		 * TODO: find better error code?
		 */
		return ECGFAIL;
	}

	cgroup_dbg("parent is %s\n", parent);
	parent_cgroup = cgroup_new_cgroup(parent);
	if (!parent_cgroup) {
		ret = ECGFAIL;
		goto err_nomem;
	}

	if (cgroup_get_cgroup(parent_cgroup)) {
		ret = ECGFAIL;
		goto err_parent;
	}

	cgroup_dbg("got parent group for %s\n", parent_cgroup->name);
	ret = cgroup_copy_cgroup(cgroup, parent_cgroup);
	if (ret) {
		goto err_parent;
	}

	cgroup_dbg("copied parent group %s to %s\n", parent_cgroup->name,
							cgroup->name);
	ret = cgroup_create_cgroup(cgroup, ignore_ownership);

err_parent:
	cgroup_free(&parent_cgroup);
err_nomem:
	free(parent);
	return ret;
}

/**
 * Move all processes from one task file to another.
 * @param input_tasks Pre-opened file to read tasks from.
 * @param output_tasks Pre-opened file to write tasks to.
 * @return 0 on succes, >0 on error.
 */
static int cg_move_task_files(FILE *input_tasks, FILE *output_tasks)
{
	int tids;
	int ret = 0;

	while (!feof(input_tasks)) {
		ret = fscanf(input_tasks, "%d", &tids);
		if (ret == EOF || ret == 0) {
			ret = 0;
			break;
		}
		if (ret < 0)
			break;

		ret = fprintf(output_tasks, "%d", tids);
		if (ret < 0)
			break;

		/*
		 * Flush the file, we need only one process per write() call.
		 */
		ret = fflush(output_tasks);
		if (ret < 0)
			break;
	}

	if (ret < 0) {
		last_errno = errno;
		return ECGOTHER;
	}
	return 0;
}

/**
 * Remove one cgroup from specific controller. The function  moves all
 * processes from it to given target group.
 *
 * The function succeeds if the group to remove is already removed - when
 * cgroup_delete_cgroup is called with group with two controllers mounted
 * to the same hierarchy, this function is called once for each of these
 * controllers. And during the second call the group is already removed...
 *
 * @param cgroup_name Name of the group to remove.
 * @param controller  Name of the controller.
 * @param target_tasks Opened tasks file of the target group, where all
 *	processes should be moved.
 * @param flags Flag indicating whether the errors from task
 *	migration should be ignored (CGROUP_DELETE_IGNORE_MIGRATION) or not (0).
 * @returns 0 on success, >0 on error.
 */
static int cg_delete_cgroup_controller(char *cgroup_name, char *controller,
		FILE *target_tasks, int flags)
{
	FILE *delete_tasks;
	char path[FILENAME_MAX];
	int ret = 0;

	cgroup_dbg("Removing group %s:%s\n", controller, cgroup_name);

	if (!(flags & CGFLAG_DELETE_EMPTY_ONLY)) {
		/*
		 * Open tasks file of the group to delete.
		 */
		if (!cg_build_path(cgroup_name, path, controller))
			return ECGROUPSUBSYSNOTMOUNTED;
		strncat(path, "tasks", sizeof(path) - strlen(path));

		delete_tasks = fopen(path, "re");
		if (delete_tasks) {
			ret = cg_move_task_files(delete_tasks, target_tasks);
			if (ret != 0)
				cgroup_warn("Warning: removing tasks from %s failed: %s\n",
						path, cgroup_strerror(ret));
			fclose(delete_tasks);
		} else {
			/*
			 * Can't open the tasks file. If the file does not
			 * exist, ignore it - the group has been already
			 * removed.
			 */
			if (errno != ENOENT) {
				cgroup_err("Error: cannot open %s: %s\n",
						path, strerror(errno));
				last_errno = errno;
				ret = ECGOTHER;
			}
		}

		if (ret != 0 && !(flags & CGFLAG_DELETE_IGNORE_MIGRATION))
			return ret;
	}

	/*
	 * Remove the group.
	 */
	if (!cg_build_path(cgroup_name, path, controller))
		return ECGROUPSUBSYSNOTMOUNTED;

	ret = rmdir(path);
	if (ret == 0 || errno == ENOENT)
		return 0;

	if ((flags & CGFLAG_DELETE_EMPTY_ONLY) && (errno == EBUSY))
		return ECGNONEMPTY;

	cgroup_warn("Warning: cannot remove directory %s: %s\n",
			path, strerror(errno));
	last_errno = errno;
	return ECGOTHER;
}

/**
 * Recursively delete one control group. Moves all tasks from the group and
 * its subgroups to given task file.
 *
 * @param cgroup_name The group to delete.
 * @param controller The controller, where to delete.
 * @param target_tasks Opened file, where all tasks should be moved.
 * @param flags Combination of CGFLAG_DELETE_* flags. The function assumes
 *	that CGFLAG_DELETE_RECURSIVE is set.
 * @param delete_root Whether the group itself should be removed(1) or not(0).
 */
static int cg_delete_cgroup_controller_recursive(char *cgroup_name,
		char *controller, FILE *target_tasks, int flags,
		int delete_root)
{
	int ret;
	void *handle;
	struct cgroup_file_info info;
	int level, group_len;
	char child_name[FILENAME_MAX];

	cgroup_dbg("Recursively removing %s:%s\n", controller, cgroup_name);

	ret = cgroup_walk_tree_begin(controller, cgroup_name, 0, &handle,
			&info, &level);

	if (ret == 0)
		ret = cgroup_walk_tree_set_flags(&handle,
				CGROUP_WALK_TYPE_POST_DIR);

	if (ret != 0) {
		cgroup_walk_tree_end(&handle);
		return ret;
	}

	group_len = strlen(info.full_path);

	/*
	 * Skip the root group, it will be handled explicitly at the end.
	 */
	ret = cgroup_walk_tree_next(0, &handle, &info, level);

	while (ret == 0) {
		if (info.type == CGROUP_FILE_TYPE_DIR && info.depth > 0) {
			snprintf(child_name, sizeof(child_name), "%s/%s",
					cgroup_name,
					info.full_path + group_len);

			ret = cg_delete_cgroup_controller(child_name,
					controller, target_tasks,
					flags);
			if (ret != 0)
				break;
		}

		ret = cgroup_walk_tree_next(0, &handle, &info, level);
	}
	if (ret == ECGEOF) {
		/*
		 * Iteration finished successfully, remove the root group.
		 */
		ret = 0;
		if (delete_root)
			ret = cg_delete_cgroup_controller(cgroup_name,
					controller, target_tasks,
					flags);
	}

	cgroup_walk_tree_end(&handle);
	return ret;
}

/** cgroup_delete cgroup deletes a control group.
 *  struct cgroup *cgroup takes the group which is to be deleted.
 *
 *  returns 0 on success.
 */
int cgroup_delete_cgroup(struct cgroup *cgroup, int ignore_migration)
{
	int flags = ignore_migration ? CGFLAG_DELETE_IGNORE_MIGRATION : 0;
	return cgroup_delete_cgroup_ext(cgroup, flags);
}

int cgroup_delete_cgroup_ext(struct cgroup *cgroup, int flags)
{
	FILE *parent_tasks = NULL;
	char parent_path[FILENAME_MAX];
	int first_error = 0, first_errno = 0;
	int i, ret;
	char *parent_name = NULL;
	int delete_group = 1;

	if (!cgroup_initialized)
		return ECGROUPNOTINITIALIZED;

	if (!cgroup)
		return ECGROUPNOTALLOWED;

	if ((flags & CGFLAG_DELETE_RECURSIVE)
			&& (flags & CGFLAG_DELETE_EMPTY_ONLY))
		return ECGINVAL;

	for (i = 0; i < cgroup->index; i++) {
		if (!cgroup_test_subsys_mounted(cgroup->controller[i]->name))
			return ECGROUPSUBSYSNOTMOUNTED;
	}

	/*
	 * Remove the group from all controllers.
	 */
	for (i = 0; i < cgroup->index; i++) {
		ret = 0;

		/* find parent, it can be different for each controller */
		if (!(flags & CGFLAG_DELETE_EMPTY_ONLY)) {
			ret = cgroup_find_parent(cgroup,
					cgroup->controller[i]->name,
					&parent_name);
			if (ret) {
				if (first_error == 0) {
					first_errno = last_errno;
					first_error = ret;
				}
				continue;
			}
			if (parent_name == NULL) {
				/*
				 * Root group is being deleted.
				 */
				if (flags & CGFLAG_DELETE_RECURSIVE) {
					/*
					 * Move all tasks to the root group and
					 * do not delete it afterwards.
					 */
					parent_name = strdup(".");
					if (parent_name == NULL) {
						if (first_error == 0) {
							first_errno = errno;
							first_error = ECGOTHER;
						}
						continue;
					}
					delete_group = 0;
				} else
					/*
					 * root group is being deleted in non-
					 * recursive mode
					 */
					continue;
			}
		}

		if (parent_name) {
			/* tasks need to be moved, pre-open target tasks file */
			if (!cg_build_path(parent_name, parent_path,
					cgroup->controller[i]->name)) {
				if (first_error == 0)
					first_error = ECGFAIL;
				free(parent_name);
				continue;
			}
			strncat(parent_path, "/tasks", sizeof(parent_path)
					- strlen(parent_path));

			parent_tasks = fopen(parent_path, "we");
			if (!parent_tasks) {
				if (first_error == 0) {
					cgroup_warn("Warning: cannot open tasks file %s: %s\n",
							parent_path,
							strerror(errno));
					first_errno = errno;
					first_error = ECGOTHER;
				}
				free(parent_name);
				continue;
			}
		}
		if (flags & CGFLAG_DELETE_RECURSIVE) {
			ret = cg_delete_cgroup_controller_recursive(
					cgroup->name,
					cgroup->controller[i]->name,
					parent_tasks, flags,
					delete_group);
		} else {
			ret = cg_delete_cgroup_controller(cgroup->name,
					cgroup->controller[i]->name,
					parent_tasks, flags);
		}

		if (parent_tasks) {
			fclose(parent_tasks);
			parent_tasks = NULL;
		}
		free(parent_name);
		parent_name = NULL;
		/*
		 * If any of the controller delete fails, remember the first
		 * error code, but continue with next controller and try remove
		 * the group from all of them.
		 */
		if (ret != 0 && first_error == 0) {
			/*
			 * ECGNONEMPTY is more or less not an error, but an
			 * indication that something was not removed.
			 * Therefore it should be replaced by any other error.
			 */
			if (ret != ECGNONEMPTY || first_error == ECGNONEMPTY) {
				first_errno = last_errno;
				first_error = ret;
			}
		}
	}

	/*
	 * Restore the last_errno to the first errno from
	 * cg_delete_cgroup_controller[_ext].
	 */
	if (first_errno != 0)
		last_errno = first_errno;

	return first_error;
}

/*
 * This function should really have more checks, but this version
 * will assume that the callers have taken care of everything.
 * Including the locking.
 */
static int cg_rd_ctrl_file(const char *subsys, const char *cgroup,
					const char *file, char **value)
{
	char path[FILENAME_MAX];
	FILE *ctrl_file = NULL;
	int ret;

	if (!cg_build_path_locked(cgroup, path, subsys))
		return ECGFAIL;

	strncat(path, file, sizeof(path) - strlen(path));
	ctrl_file = fopen(path, "re");
	if (!ctrl_file)
		return ECGROUPVALUENOTEXIST;

	*value = calloc(CG_VALUE_MAX, 1);
	if (!*value) {
		fclose(ctrl_file);
		last_errno = errno;
		return ECGOTHER;
	}

	/*
	 * using %as crashes when we try to read from files like
	 * memory.stat
	 */
	ret = fread(*value, 1, CG_VALUE_MAX-1, ctrl_file);
	if (ret < 0) {
		free(*value);
		*value = NULL;
	} else {
		/* remove trailing \n */
		if (ret > 0 && (*value)[ret-1] == '\n')
			(*value)[ret-1] = '\0';
	}

	fclose(ctrl_file);

	return 0;
}

/*
 * Call this function with required locks taken.
 */
static int cgroup_fill_cgc(struct dirent *ctrl_dir, struct cgroup *cgroup,
			struct cgroup_controller *cgc, int cg_index)
{
	char *ctrl_name = NULL;
	char *ctrl_file = NULL;
	char *ctrl_value = NULL;
	char *d_name = NULL;
	char *tmp_path = NULL;
	int tmp_len = 0;
	char path[FILENAME_MAX+1];
	char *buffer = NULL;
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

	cg_build_path_locked(cgroup->name, path, cg_mount_table[cg_index].name);
	strncat(path, d_name, sizeof(path) - strlen(path));

	error = stat(path, &stat_buffer);

	if (error) {
		error = ECGFAIL;
		goto fill_error;
	}

	/*
	 * We have already stored the tasks_uid & tasks_gid.
	 * This check is to avoid the overwriting of the values
	 * stored in control_uid & cotrol_gid. tasks file will
	 * have the uid and gid of the user who is capable of
	 * putting a task to this cgroup. control_uid and control_gid
	 * is meant for the users who are capable of managing the
	 * cgroup shares.
	 *
	 * The strstr() function will return the pointer to the
	 * beginning of the sub string "/tasks".
	 */
	tmp_len = strlen(path) - strlen("/tasks");

	/*
	 * tmp_path would be pointing to the last six characters
	 */
	tmp_path = (char *)path + tmp_len;

	/*
	 * Checking to see, if this is actually a 'tasks' file
	 * We need to compare the last 6 bytes
	 */
	if (strcmp(tmp_path, "/tasks")){
		cgroup->control_uid = stat_buffer.st_uid;
		cgroup->control_gid = stat_buffer.st_gid;
	}

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

	if (strcmp(ctrl_name, cg_mount_table[cg_index].name) == 0) {
		error = cg_rd_ctrl_file(cg_mount_table[cg_index].name,
				cgroup->name, ctrl_dir->d_name, &ctrl_value);
		if (error || !ctrl_value)
			goto fill_error;

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
 * cgroup_get_cgroup reads the cgroup data from the filesystem.
 * struct cgroup has the name of the group to be populated
 *
 * return 0 on success.
 */
int cgroup_get_cgroup(struct cgroup *cgroup)
{
	int i, j;
	char path[FILENAME_MAX];
	DIR *dir = NULL;
	struct dirent *ctrl_dir = NULL;
	char *control_path = NULL;
	int error;
	int ret;

	if (!cgroup_initialized) {
		/* ECGROUPNOTINITIALIZED */
		return ECGROUPNOTINITIALIZED;
	}

	if (!cgroup) {
		/* ECGROUPNOTALLOWED */
		return ECGROUPNOTALLOWED;
	}

	pthread_rwlock_rdlock(&cg_mount_table_lock);
	for (i = 0; i < CG_CONTROLLER_MAX &&
			cg_mount_table[i].name[0] != '\0'; i++) {
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

		ret = asprintf(&control_path, "%s/tasks", path);

		if (ret < 0) {
			last_errno = errno;
			error = ECGOTHER;
			goto unlock_error;
		}

		if (stat(control_path, &stat_buffer)) {
			last_errno = errno;
			free(control_path);
			error = ECGOTHER;
			goto unlock_error;
		}

		cgroup->tasks_uid = stat_buffer.st_uid;
		cgroup->tasks_gid = stat_buffer.st_gid;

		free(control_path);

		cgc = cgroup_add_controller(cgroup,
				cg_mount_table[i].name);
		if (!cgc) {
			error = ECGINVAL;
			goto unlock_error;
		}

		dir = opendir(path);
		if (!dir) {
			last_errno = errno;
			error = ECGOTHER;
			goto unlock_error;
		}

		while ((ctrl_dir = readdir(dir)) != NULL) {
			/*
			 * Skip over non regular files
			 */
			if (ctrl_dir->d_type != DT_REG)
				continue;

			error = cgroup_fill_cgc(ctrl_dir, cgroup, cgc, i);
			for (j = 0; j < cgc->index; j++)
				cgc->values[j]->dirty = false;

			if (error == ECGFAIL) {
				closedir(dir);
				goto unlock_error;
			}
		}
		closedir(dir);
	}

	/* Check if the group really exists or not */
	if (!cgroup->index) {
		error = ECGROUPNOTEXIST;
		goto unlock_error;
	}

	pthread_rwlock_unlock(&cg_mount_table_lock);
	return 0;

unlock_error:
	pthread_rwlock_unlock(&cg_mount_table_lock);
	/*
	 * XX: Need to figure out how to cleanup? Cleanup just the stuff
	 * we added, or the whole structure.
	 */
	cgroup_free_controllers(cgroup);
	cgroup = NULL;
	return error;
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
					const char * const controllers[])
{
	int ret = 0, i;
	const char *controller = NULL;
	struct cgroup_controller *cptr = NULL;

	/* Fill in cgroup details.  */
	cgroup_dbg("Will move pid %d to cgroup '%s'\n", pid, dest);

	strncpy(cgroup->name, dest, FILENAME_MAX);
	cgroup->name[FILENAME_MAX-1] = '\0';

	/* Scan all the controllers */
	for (i = 0; i < CG_CONTROLLER_MAX; i++) {
		int j = 0;
		if (!controllers[i])
			return 0;
		controller = controllers[i];

		/* If first string is "*" that means all the mounted
		 * controllers. */
		if (strcmp(controller, "*") == 0) {
			pthread_rwlock_rdlock(&cg_mount_table_lock);
			for (j = 0; j < CG_CONTROLLER_MAX &&
				cg_mount_table[j].name[0] != '\0'; j++) {
				cgroup_dbg("Adding controller %s\n",
					cg_mount_table[j].name);
				cptr = cgroup_add_controller(cgroup,
						cg_mount_table[j].name);
				if (!cptr) {
					cgroup_warn("Warning: adding controller '%s' failed\n",
						cg_mount_table[j].name);
					pthread_rwlock_unlock(&cg_mount_table_lock);
					cgroup_free_controllers(cgroup);
					return ECGROUPNOTALLOWED;
				}
			}
			pthread_rwlock_unlock(&cg_mount_table_lock);
			return ret;
		}

		/* it is individual controller names and not "*" */
		cgroup_dbg("Adding controller %s\n", controller);
		cptr = cgroup_add_controller(cgroup, controller);
		if (!cptr) {
			cgroup_warn("Warning: adding controller '%s' failed\n",
				controller);
			cgroup_free_controllers(cgroup);
			return ECGROUPNOTALLOWED;
		}
	}

	return ret;
}

static struct cgroup_rule *cgroup_find_matching_rule_uid_gid(uid_t uid,
				gid_t gid, struct cgroup_rule *rule)
{
	/* Temporary user data */
	struct passwd *usr = NULL;

	/* Temporary group data */
	struct group *grp = NULL;

	/* Temporary string pointer */
	char *sp = NULL;

	/* Loop variable */
	int i = 0;

	while (rule) {
		/* Skip "%" which indicates continuation of previous rule. */
		if (rule->username[0] == '%') {
			rule = rule->next;
			continue;
		}
		/* The wildcard rule always matches. */
		if ((rule->uid == CGRULE_WILD) && (rule->gid == CGRULE_WILD))
			return rule;

		/* This is the simple case of the UID matching. */
		if (rule->uid == uid)
			return rule;

		/* This is the simple case of the GID matching. */
		if (rule->gid == gid)
			return rule;

		/* If this is a group rule, the UID might be a member. */
		if (rule->username[0] == '@') {
			/* Get the group data. */
			sp = &(rule->username[1]);
			grp = getgrnam(sp);
			if (!grp)
				continue;

			/* Get the data for UID. */
			usr = getpwuid(uid);
			if (!usr)
				continue;

			/* If UID is a member of group, we matched. */
			for (i = 0; grp->gr_mem[i]; i++) {
				if (!(strcmp(usr->pw_name, grp->gr_mem[i])))
					return rule;
			}
		}

		/* If we haven't matched, try the next rule. */
		rule = rule->next;
	}

	/* If we get here, no rules matched. */
	return NULL;
}

/**
 * Finds the first rule in the cached list that matches the given UID, GID
 * or PROCESS NAME, and returns a pointer to that rule.
 * This function uses rl_lock.
 *
 * This function may NOT be thread safe.
 *	@param uid The UID to match
 *	@param gid The GID to match
 *	@param procname The PROCESS NAME to match
 *	@return Pointer to the first matching rule, or NULL if no match
 * TODO: Determine thread-safeness and fix if not safe.
 */
static struct cgroup_rule *cgroup_find_matching_rule(uid_t uid,
				gid_t gid, const char *procname)
{
	/* Return value */
	struct cgroup_rule *ret = rl.head;
	char *base = NULL;

	pthread_rwlock_wrlock(&rl_lock);
	while (ret) {
		ret = cgroup_find_matching_rule_uid_gid(uid, gid, ret);
		if (!ret)
			break;
		if (!procname)
			/* If procname is NULL, return a rule matching
			 * UID or GID */
			break;
		if (!ret->procname)
			/* If no process name in a rule, that means wildcard */
			break;
		if (!strcmp(ret->procname, procname))
			break;

		base = cgroup_basename(procname);
		if (!strcmp(ret->procname, base))
			/* Check a rule of basename. */
			break;
		ret = ret->next;
		free(base);
		base = NULL;
	}
	pthread_rwlock_unlock(&rl_lock);

	if (base)
		free(base);

	return ret;
}

/* Procedure the existence of cgroup "prefix" is in subsystem controller_name
 * return 0 on success
 */
int cgroup_exist_in_subsystem(char *controller_name, char *prefix)
{
	DIR *dir;
	char path[FILENAME_MAX];
	char *ret_path;
	int ret;

	pthread_rwlock_rdlock(&cg_mount_table_lock);
	ret_path = cg_build_path_locked(prefix, path, controller_name);
	pthread_rwlock_unlock(&cg_mount_table_lock);
	if (!ret_path) {
		ret = 1;
		goto end;
	}

	dir = opendir(path);
	if (dir == NULL) {
		/* cgroup in wanted subsystem does not exist */
		ret = 1;
	} else {
		/* cgroup in wanted subsystem exists */
		ret = 0;
		closedir(dir);
	}
end:
	return ret;
}

/* auxiliary function return a pointer to the string
 * which is copy of input string and end with the slash
 */
char *cgroup_copy_with_slash(char *input)
{
	char *output;
	int len = strlen(input);

	/* if input does not end with '/', allocate one more space for it */
	if ((input[len-1]) != '/')
		len = len+1;

	output = (char *)malloc(sizeof(char)*(len+1));
	if (output == NULL)
		return NULL;

	strcpy(output, input);
	output[len-1] = '/';
	output[len] = '\0';

	return output;
}

/* add controller to a group if it is not exists create it */
static int add_controller(struct cgroup **pgroup, char *group_name,
	char controller_name[FILENAME_MAX])
{
	int ret = 0;
	struct cgroup_controller *controller = NULL;
	struct cgroup *group = pgroup[0];

	if  (group == NULL) {
		/* it is the first controllerc the group have to be created */
		group = cgroup_new_cgroup(group_name);
		if (group == NULL) {
			ret = ECGFAIL;
			goto end;
		}
		pgroup[0] = group;
	}

	controller = cgroup_add_controller(
		group, controller_name);
	if (controller == NULL) {
		cgroup_free(&group);
		ret = ECGFAIL;
	}
end:
	return ret;
}



/* create control group based given template
 * if the group already don't exist
 * dest is template name with substitute variables
 * tmp is used cgrules rule
 */
static int cgroup_create_template_group(char *orig_group_name,
	struct cgroup_rule *tmp, int flags)
{

	char *template_name = NULL;	/* name of the template */
	char *group_name = NULL;	/* name of the group based on template -
					   variables are substituted */
	char *template_position;	/* denotes directory in template path
					   which is investigated */
	char *group_position;		/* denotes directory in cgroup path
					   which is investigated */

	struct cgroup *template_group = NULL;
	int ret = 0;
	int i;
	int exist;

	/* template name and group name have to have '/' sign at the end */
	template_name = cgroup_copy_with_slash(tmp->destination);
	if (template_name == NULL) {
		ret = ECGOTHER;
		last_errno = errno;
		goto end;
	}
	group_name = cgroup_copy_with_slash(orig_group_name);
	if (group_name == NULL) {
		ret = ECGOTHER;
		last_errno = errno;
		free(template_name);
		goto end;
	}

	/* set start positions */
	template_position = strchr(template_name, '/');
	group_position = strchr(group_name, '/');

	/* go recursively through whole path to template group and create given
	 * directory if it does not exist yet
	 */
	while ((group_position != NULL) && (template_position != NULL)) {
		/* set new subpath */
		group_position[0] = '\0';
		template_position[0] = '\0';
		template_group = NULL;

		/* test for which controllers wanted group does not exist */
		i = 0;
		while (tmp->controllers[i] != NULL) {
			exist = cgroup_exist_in_subsystem(tmp->controllers[i],
				group_name);

			if (exist != 0) {
				/* the cgroup does not exist */
				ret = add_controller(&template_group, group_name,
					tmp->controllers[i]);
				if  (ret != 0)
					goto while_end;
			}
			i++;
		}

		if (template_group != NULL) {
			/*  new group have to be created */
			if (strcmp(group_name, template_name) == 0) {
				/* the prefix cgroup without template */
				ret = cgroup_create_cgroup(template_group, 0);
			} else {
				/* use template to create relevant cgroup */
				ret = cgroup_config_create_template_group(
					template_group, template_name,
					flags);
			}

			if (ret != 0) {
				cgroup_free(&template_group);
				goto while_end;
			}
			cgroup_dbg("Group %s created - based on template %s\n",
				group_name, template_name);

			cgroup_free(&template_group);
		}
		template_position[0] = '/';
		group_position[0] = '/';
		template_position = strchr(++template_position, '/');
		group_position = strchr(++group_position, '/');
	}

while_end:
	if ((template_position != NULL ) && (template_position[0] == '\0'))
		template_position[0] = '/';
	if ((group_position != NULL) && (group_position[0] == '\0'))
		group_position[0] = '/';

end:
	if (group_name != NULL)
		free(group_name);
	if (template_name != NULL)
		free(template_name);
	return ret;
}

int cgroup_change_cgroup_flags(uid_t uid, gid_t gid,
		const char *procname, pid_t pid, int flags)
{
	/* Temporary pointer to a rule */
	struct cgroup_rule *tmp = NULL;

	/* Temporary variables for destination substitution */
	char newdest[FILENAME_MAX];
	int i, j;
	int written;
	int available;
	struct passwd *user_info;
	struct group *group_info;

	/* Return codes */
	int ret = 0;

	/* We need to check this before doing anything else! */
	if (!cgroup_initialized) {
		cgroup_warn("Warning: libcgroup is not initialized\n");
		ret = ECGROUPNOTINITIALIZED;
		goto finished;
	}

	/*
	 * If the user did not ask for cached rules, we must parse the
	 * configuration to find a matching rule (if one exists).  Else, we'll
	 * find the first match in the cached list (rl).
	 */
	if (!(flags & CGFLAG_USECACHE)) {
		cgroup_dbg("Not using cached rules for PID %d.\n", pid);
		ret = cgroup_parse_rules(false, uid, gid, procname);

		/* The configuration file has an error!  We must exit now. */
		if (ret != -1 && ret != 0) {
			cgroup_err("Error: failed to parse the configuration rules\n");
			goto finished;
		}

		/* We did not find a matching rule, so we're done. */
		if (ret == 0) {
			cgroup_dbg("No rule found to match PID: %d, UID: %d, "
				"GID: %d\n", pid, uid, gid);
			goto finished;
		}

		/* Otherwise, we did match a rule and it's in trl. */
		tmp = trl.head;
	} else {
		/* Find the first matching rule in the cached list. */
		tmp = cgroup_find_matching_rule(uid, gid, procname);
		if (!tmp) {
			cgroup_dbg("No rule found to match PID: %d, UID: %d, "
				"GID: %d\n", pid, uid, gid);
			ret = 0;
			goto finished;
		}
	}
	cgroup_dbg("Found matching rule %s for PID: %d, UID: %d, GID: %d\n",
			tmp->username, pid, uid, gid);

	/* If we are here, then we found a matching rule, so execute it. */
	do {
		cgroup_dbg("Executing rule %s for PID %d... ", tmp->username,
								pid);
		/* Destination substitutions */
		for(j = i = 0; i < strlen(tmp->destination) &&
			(j < FILENAME_MAX - 2); ++i, ++j) {
			if(tmp->destination[i] == '%') {
				/* How many bytes did we write / error check */
				written = 0;
				/* How many bytes can we write */
				available = FILENAME_MAX - j - 2;
				/* Substitution */
				switch(tmp->destination[++i]) {
				case 'U':
					written = snprintf(newdest+j, available,
						"%d", uid);
					break;
				case 'u':
					user_info = getpwuid(uid);
					if(user_info) {
						written = snprintf(newdest + j,
							available, "%s",
							user_info -> pw_name);
					} else {
						written = snprintf(newdest + j,
							available, "%d", uid);
					}
					break;
				case 'G':
					written = snprintf(newdest + j,
						available, "%d", gid);
					break;
				case 'g':
					group_info = getgrgid(gid);
					if(group_info) {
						written = snprintf(newdest + j,
							available, "%s",
							group_info -> gr_name);
					} else {
						written = snprintf(newdest + j,
							available, "%d", gid);
					}
					break;
				case 'P':
					written = snprintf(newdest + j,
						available, "%d", pid);
					break;
				case 'p':
					if(procname) {
						written = snprintf(newdest + j,
							available, "%s",
							procname);
					} else {
						written = snprintf(newdest + j,
							available, "%d", pid);
					}
					break;
				}
				written = min(written, available);
				/*
				 * written<1 only when either error occurred
				 * during snprintf or if no substitution was
				 * made at all. In both cases, we want to just
				 * copy input string.
				 */
				if(written<1) {
					newdest[j] = '%';
					if(available>1)
						newdest[++j] =
							tmp->destination[i];
				} else {
					/*
					 * In next iteration, we will write
					 * just after the substitution, but j
					 * will get incremented in the
					 * meantime.
					 */
					j += written - 1;
				}
			} else {
				if(tmp->destination[i] == '\\')
					++i;
				newdest[j] = tmp->destination[i];
			}
		}

		newdest[j] = 0;
		if (strcmp(newdest, tmp->destination) != 0) {
			/* destination tag contains templates */

			cgroup_dbg("control group %s is template\n", newdest);
			ret = cgroup_create_template_group(newdest, tmp, flags);
		}

		/* Apply the rule */
		ret = cgroup_change_cgroup_path(newdest,
				pid, (const char * const *)tmp->controllers);
		if (ret) {
			cgroup_warn("Warning: failed to apply the rule. Error was: %d\n",
					ret);
			goto finished;
		}
		cgroup_dbg("OK!\n");

		/* Now, check for multi-line rules.  As long as the "next"
		 * rule starts with '%', it's actually part of the rule that
		 * we just executed.
		 */
		tmp = tmp->next;
	} while (tmp && (tmp->username[0] == '%'));

finished:
	return ret;
}

int cgroup_change_cgroup_uid_gid_flags(uid_t uid, gid_t gid,
				pid_t pid, int flags)
{
	return cgroup_change_cgroup_flags(uid, gid, NULL, pid, flags);
}

/**
 * Provides backwards-compatibility with older versions of the API.  This
 * function is deprecated, and cgroup_change_cgroup_uid_gid_flags() should be
 * used instead.  In fact, this function simply calls the newer one with flags
 * set to 0 (none).
 *	@param uid The UID to match
 *	@param gid The GID to match
 *	@param pid The PID of the process to move
 *	@return 0 on success, > 0 on error
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
int cgroup_change_cgroup_path(const char *dest, pid_t pid,
				const char *const controllers[])
{
	int ret;
	struct cgroup cgroup;

	if (!cgroup_initialized) {
		cgroup_warn("Warning: libcgroup is not initialized\n");
		return ECGROUPNOTINITIALIZED;
	}
	memset(&cgroup, 0, sizeof(struct cgroup));

	ret = cg_prepare_cgroup(&cgroup, pid, dest, controllers);
	if (ret)
		return ret;
	/* Add task to cgroup */
	ret = cgroup_attach_task_pid(&cgroup, pid);
	if (ret)
		cgroup_warn("Warning: cgroup_attach_task_pid failed: %d\n",
				ret);
	cgroup_free_controllers(&cgroup);
	return ret;
}

/**
 * Changes the cgroup of all running PIDs based on the rules in the config
 * file. If a rules exists for a PID, then the PID is placed in the correct
 * group.
 *
 * This function may be called after creating new control groups to move
 * running PIDs into the newly created control groups.
 *	@return 0 on success, < 0 on error
 */
int cgroup_change_all_cgroups(void)
{
	DIR *dir;
	struct dirent *pid_dir = NULL;
	char *path = "/proc/";

	dir = opendir(path);
	if (!dir)
		return -ECGOTHER;

	while ((pid_dir = readdir(dir)) != NULL) {
		int err, pid;
		uid_t euid;
		gid_t egid;
		char *procname = NULL;

		err = sscanf(pid_dir->d_name, "%i", &pid);
		if (err < 1)
			continue;

		err = cgroup_get_uid_gid_from_procfs(pid, &euid, &egid);
		if (err)
			continue;

		err = cgroup_get_procname_from_procfs(pid, &procname);
		if (err)
			continue;

		err = cgroup_change_cgroup_flags(euid,
				egid, procname, pid, CGFLAG_USECACHE);
		if (err)
			cgroup_dbg("cgroup change pid %i failed\n", pid);

		free(procname);
	}

	closedir(dir);
	return 0;
}

/**
 * Print the cached rules table.  This function should be called only after
 * first calling cgroup_parse_config(), but it will work with an empty rule
 * list.
 *	@param fp The file stream to print to
 */
void cgroup_print_rules_config(FILE *fp)
{
	/* Iterator */
	struct cgroup_rule *itr = NULL;

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
		fprintf(fp, "Rule: %s", itr->username);
		if (itr->procname)
			fprintf(fp, ":%s", itr->procname);
		fprintf(fp, "\n");

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
			if (itr->controllers[i])
				fprintf(fp, "    %s\n", itr->controllers[i]);
		}
		fprintf(fp, "\n");
		itr = itr->next;
	}
	pthread_rwlock_unlock(&rl_lock);
}

/**
 * Reloads the rules list, using the given configuration file.  This function
 * is probably NOT thread safe (calls cgroup_parse_rules()).
 *	@return 0 on success, > 0 on failure
 */
int cgroup_reload_cached_rules(void)
{
	/* Return codes */
	int ret = 0;

	cgroup_dbg("Reloading cached rules from %s.\n", CGRULES_CONF_FILE);
	ret = cgroup_parse_rules(true, CGRULE_INVALID, CGRULE_INVALID, NULL);
	if (ret) {
		cgroup_warn("Warning: error parsing configuration file '%s': %d\n",
			CGRULES_CONF_FILE, ret);
		ret = ECGRULESPARSEFAIL;
		goto finished;
	}

	#ifdef CGROUP_DEBUG
		cgroup_print_rules_config(stdout);
	#endif

finished:
	return ret;
}

/**
 * Initializes the rules cache.
 *	@return 0 on success, > 0 on error
 */
int cgroup_init_rules_cache(void)
{
	/* Return codes */
	int ret = 0;

	/* Attempt to read the configuration file and cache the rules. */
	ret = cgroup_parse_rules(true, CGRULE_INVALID, CGRULE_INVALID, NULL);
	if (ret) {
		cgroup_dbg("Could not initialize rule cache, error was: %d\n",
			ret);
	}

	return ret;
}

/**
 * cgroup_get_current_controller_path
 * @pid: pid of the current process for which the path is to be determined
 * @controller: name of the controller for which to determine current path
 * @current_path: a pointer that is filled with the value of the current
 *		path as seen in /proc/<pid>/cgroup
 */
int cgroup_get_current_controller_path(pid_t pid, const char *controller,
					char **current_path)
{
	char *path = NULL;
	int ret;
	FILE *pid_cgroup_fd = NULL;

	if (!controller)
		return ECGOTHER;

	if (!cgroup_initialized) {
		cgroup_warn("Warning: libcgroup is not initialized\n");
		return ECGROUPNOTINITIALIZED;
	}

	ret = asprintf(&path, "/proc/%d/cgroup", pid);
	if (ret <= 0) {
		cgroup_warn(
				"Warning: cannot allocate memory (/proc/pid/cgroup) ret %d\n",
				ret);
		return ret;
	}

	ret = ECGROUPNOTEXIST;
	pid_cgroup_fd = fopen(path, "re");
	if (!pid_cgroup_fd)
		goto cleanup_path;

	/*
	 * Why do we grab the cg_mount_table_lock?, the reason is that
	 * the cgroup of a pid can change via the cgroup_attach_task_pid()
	 * call. To make sure, we return consitent and safe results,
	 * we acquire the lock upfront. We can optimize by acquiring
	 * and releasing the lock in the while loop, but that
	 * will be more expensive.
	 */
	pthread_rwlock_rdlock(&cg_mount_table_lock);
	while (!feof(pid_cgroup_fd)) {
		char controllers[FILENAME_MAX];
		char cgroup_path[FILENAME_MAX];
		int num;
		char *savedptr;
		char *token;

		ret = fscanf(pid_cgroup_fd, "%d:%[^:]:%s\n", &num, controllers,
				cgroup_path);
		/*
		 * Magic numbers like "3" seem to be integrating into
		 * my daily life, I need some magic to help make them
		 * disappear :)
		 */
		if (ret != 3) {
			cgroup_warn("Warning: read failed for pid_cgroup_fd ret %d\n",
					ret);
			last_errno = errno;
			ret = ECGOTHER;
			goto done;
		}

		token = strtok_r(controllers, ",", &savedptr);
		while (token) {
			if (strncmp(controller, token, strlen(controller) + 1)
								== 0) {
				*current_path = strdup(cgroup_path);
				if (!*current_path) {
					last_errno = errno;
					ret = ECGOTHER;
					goto done;
				}
				ret = 0;
				goto done;
			}
			token = strtok_r(NULL, ",", &savedptr);
		}
	}

done:
	pthread_rwlock_unlock(&cg_mount_table_lock);
	fclose(pid_cgroup_fd);
cleanup_path:
	free(path);
	return ret;
}

const char *cgroup_strerror(int code)
{
	if (code == ECGOTHER)
		return strerror_r(cgroup_get_last_errno(), errtext, MAXLEN);

	return cgroup_strerror_codes[code % ECGROUPNOTCOMPILED];
}

/**
 * Return last errno, which caused ECGOTHER error.
 */
int cgroup_get_last_errno(void)
{
    return last_errno;
}


static int cg_walk_node(FTS *fts, FTSENT *ent, const int depth,
			struct cgroup_file_info *info, int dir)
{
	int ret = 0;

	if (!cgroup_initialized)
		return ECGROUPNOTINITIALIZED;

	cgroup_dbg("seeing file %s\n", ent->fts_path);

	info->path = ent->fts_name;
	info->parent = ent->fts_parent->fts_name;
	info->full_path = ent->fts_path;
	info->depth = ent->fts_level;
	info->type = CGROUP_FILE_TYPE_OTHER;

	if (depth && (info->depth > depth))
		return 0;

	switch (ent->fts_info) {
	case FTS_DNR:
	case FTS_ERR:
		errno = ent->fts_errno;
		break;
	case FTS_D:
		if (dir & CGROUP_WALK_TYPE_PRE_DIR)
			info->type = CGROUP_FILE_TYPE_DIR;
		break;
	case FTS_DC:
	case FTS_NSOK:
	case FTS_NS:
	case FTS_DP:
		if (dir & CGROUP_WALK_TYPE_POST_DIR)
			info->type = CGROUP_FILE_TYPE_DIR;
		break;
	case FTS_F:
		info->type = CGROUP_FILE_TYPE_FILE;
		break;
	case FTS_DEFAULT:
		break;
	}
	return ret;
}

int cgroup_walk_tree_next(int depth, void **handle,
				struct cgroup_file_info *info, int base_level)
{
	int ret = 0;
	struct cgroup_tree_handle *entry;
	FTSENT *ent;

	if (!cgroup_initialized)
		return ECGROUPNOTINITIALIZED;

	if (!handle)
		return ECGINVAL;

	entry = (struct cgroup_tree_handle *) *handle;

	ent = fts_read(entry->fts);
	if (!ent)
		return ECGEOF;
	if (!base_level && depth)
		base_level = ent->fts_level + depth;

	ret = cg_walk_node(entry->fts, ent, base_level, info, entry->flags);

	*handle = entry;
	return ret;
}

int cgroup_walk_tree_end(void **handle)
{
	struct cgroup_tree_handle *entry;

	if (!cgroup_initialized)
		return ECGROUPNOTINITIALIZED;

	if (!handle)
		return ECGINVAL;

	entry = (struct cgroup_tree_handle *) *handle;

	fts_close(entry->fts);
	free(entry);
	*handle = NULL;
	return 0;
}

/*
 * TODO: Need to decide a better place to put this function.
 */
int cgroup_walk_tree_begin(const char *controller, const char *base_path,
		int depth, void **handle, struct cgroup_file_info *info,
							int *base_level)
{
	int ret = 0;
	char *cg_path[2];
	char full_path[FILENAME_MAX];
	FTSENT *ent;
	struct cgroup_tree_handle *entry;

	if (!cgroup_initialized)
		return ECGROUPNOTINITIALIZED;

	if (!handle)
		return ECGINVAL;

	cgroup_dbg("path is %s\n", base_path);

	if (!cg_build_path(base_path, full_path, controller))
		return ECGOTHER;

	entry = calloc(sizeof(struct cgroup_tree_handle), 1);

	if (!entry) {
		last_errno = errno;
		*handle = NULL;
		return ECGOTHER;
	}

	entry->flags |= CGROUP_WALK_TYPE_PRE_DIR;

	*base_level = 0;
	cg_path[0] = full_path;
	cg_path[1] = NULL;

	entry->fts = fts_open(cg_path, FTS_LOGICAL | FTS_NOCHDIR |
				FTS_NOSTAT, NULL);
	if (entry->fts == NULL) {
		free(entry);
		last_errno = errno;
		*handle = NULL;
		return ECGOTHER;
	}
	ent = fts_read(entry->fts);
	if (!ent) {
		cgroup_warn("Warning: fts_read failed\n");
		fts_close(entry->fts);
		free(entry);
		*handle = NULL;
		return ECGINVAL;
	}
	if (!*base_level && depth)
		*base_level = ent->fts_level + depth;

	ret = cg_walk_node(entry->fts, ent, *base_level, info, entry->flags);
	if (ret != 0) {
		fts_close(entry->fts);
		free(entry);
		*handle = NULL;
	} else {
		*handle = entry;
	}
	return ret;
}

int cgroup_walk_tree_set_flags(void **handle, int flags)
{
	struct cgroup_tree_handle *entry;

	if (!cgroup_initialized)
		return ECGROUPNOTINITIALIZED;

	if (!handle)
		return ECGINVAL;

	if ((flags & CGROUP_WALK_TYPE_PRE_DIR) &&
			(flags & CGROUP_WALK_TYPE_POST_DIR))
		return ECGINVAL;

	entry = (struct cgroup_tree_handle *) *handle;
	entry->flags = flags;

	*handle = entry;
	return 0;
}

/*
 * This parses a stat line which is in the form of (name value) pair
 * separated by a space.
 */
static int cg_read_stat(FILE *fp, struct cgroup_stat *cgroup_stat)
{
	int ret = 0;
	char *line = NULL;
	size_t len = 0;
	ssize_t read_bytes;
	char *token;
	char *saveptr = NULL;

	read_bytes = getline(&line, &len, fp);
	if (read_bytes == -1) {
		ret = ECGEOF;
		goto out_free;
	}

	token = strtok_r(line, " ", &saveptr);
	if (!token) {
		ret = ECGINVAL;
		goto out_free;
	}
	strncpy(cgroup_stat->name, token, FILENAME_MAX);

	token = strtok_r(NULL, " ", &saveptr);
	if (!token) {
		ret = ECGINVAL;
		goto out_free;
	}
	strncpy(cgroup_stat->value, token, CG_VALUE_MAX);

out_free:
	free(line);
	return ret;
}


int cgroup_read_value_end(void **handle)
{
	FILE *fp;

	if (!cgroup_initialized)
		return ECGROUPNOTINITIALIZED;

	if (!handle)
		return ECGINVAL;

	fp = (FILE *)*handle;
	fclose(fp);

	return 0;
}

int cgroup_read_value_next(void **handle, char *buffer, int max)
{
	int ret = 0;
	char *ret_c;
	FILE *fp;

	if (!cgroup_initialized)
		return ECGROUPNOTINITIALIZED;

	if (!buffer || !handle)
		return ECGINVAL;

	fp = (FILE *)*handle;
	ret_c = fgets(buffer, max, fp);
	if (ret_c == NULL)
		ret = ECGEOF;

	return ret;
}

int cgroup_read_value_begin(const char *controller, const char *path,
	char *name, void **handle, char *buffer, int max)
{
	int ret = 0;
	char *ret_c = NULL;
	char stat_file[FILENAME_MAX];
	char stat_path[FILENAME_MAX];
	FILE *fp;

	if (!cgroup_initialized)
		return ECGROUPNOTINITIALIZED;

	if (!buffer || !handle)
		return ECGINVAL;

	if (!cg_build_path(path, stat_path, controller))
		return ECGOTHER;

	snprintf(stat_file, sizeof(stat_file), "%s/%s", stat_path,
		name);
	fp = fopen(stat_file, "re");
	if (!fp) {
		cgroup_warn("Warning: fopen failed\n");
		last_errno = errno;
		*handle = NULL;
		return ECGOTHER;
	}

	ret_c = fgets(buffer, max, fp);
	if (ret_c == NULL)
		ret = ECGEOF;

	*handle = fp;
	return ret;
}



int cgroup_read_stats_end(void **handle)
{
	FILE *fp;

	if (!cgroup_initialized)
		return ECGROUPNOTINITIALIZED;

	if (!handle)
		return ECGINVAL;

	fp = (FILE *)*handle;
	if (fp == NULL)
		return ECGINVAL;

	fclose(fp);
	return 0;
}

int cgroup_read_stats_next(void **handle, struct cgroup_stat *cgroup_stat)
{
	int ret = 0;
	FILE *fp;

	if (!cgroup_initialized)
		return ECGROUPNOTINITIALIZED;

	if (!handle || !cgroup_stat)
		return ECGINVAL;

	fp = (FILE *)*handle;
	ret = cg_read_stat(fp, cgroup_stat);
	*handle = fp;
	return ret;
}

/*
 * TODO: Need to decide a better place to put this function.
 */
int cgroup_read_stats_begin(const char *controller, const char *path,
				void **handle, struct cgroup_stat *cgroup_stat)
{
	int ret = 0;
	char stat_file[FILENAME_MAX];
	char stat_path[FILENAME_MAX];
	FILE *fp;

	if (!cgroup_initialized)
		return ECGROUPNOTINITIALIZED;

	if (!cgroup_stat || !handle)
		return ECGINVAL;

	if (!cg_build_path(path, stat_path, controller))
		return ECGOTHER;

	snprintf(stat_file, sizeof(stat_file), "%s/%s.stat", stat_path,
			controller);

	fp = fopen(stat_file, "re");
	if (!fp) {
		cgroup_warn("Warning: fopen failed\n");
		return ECGINVAL;
	}

	ret = cg_read_stat(fp, cgroup_stat);
	*handle = fp;
	return ret;
}

int cgroup_get_task_end(void **handle)
{
	if (!cgroup_initialized)
		return ECGROUPNOTINITIALIZED;

	if (!*handle)
		return ECGINVAL;

	fclose((FILE *) *handle);
	*handle = NULL;

	return 0;
}

int cgroup_get_task_next(void **handle, pid_t *pid)
{
	int ret;

	if (!cgroup_initialized)
		return ECGROUPNOTINITIALIZED;

	if (!handle)
		return ECGINVAL;

	ret = fscanf((FILE *) *handle, "%u", pid);

	if (ret != 1) {
		if (ret == EOF)
			return ECGEOF;
		last_errno = errno;
		return ECGOTHER;
	}

	return 0;
}

int cgroup_get_task_begin(const char *cgroup, const char *controller,
					void **handle, pid_t *pid)
{
	int ret = 0;
	char path[FILENAME_MAX];
	char *fullpath = NULL;

	if (!cgroup_initialized)
		return ECGROUPNOTINITIALIZED;

	if (!cg_build_path(cgroup, path, controller))
		return ECGOTHER;

	ret = asprintf(&fullpath, "%s/tasks", path);

	if (ret < 0) {
		last_errno = errno;
		return ECGOTHER;
	}

	*handle = (void *) fopen(fullpath, "re");
	free(fullpath);

	if (!*handle) {
		last_errno = errno;
		return ECGOTHER;
	}
	ret = cgroup_get_task_next(handle, pid);

	return ret;
}


int cgroup_get_controller_end(void **handle)
{
	int *pos = (int *) *handle;

	if (!cgroup_initialized)
		return ECGROUPNOTINITIALIZED;

	if (!pos)
		return ECGINVAL;

	free(pos);
	*handle = NULL;

	return 0;
}

int cgroup_get_controller_next(void **handle, struct cgroup_mount_point *info)
{
	int *pos = (int *) *handle;
	int ret = 0;

	if (!cgroup_initialized)
		return ECGROUPNOTINITIALIZED;

	if (!pos)
		return ECGINVAL;

	if (!info)
		return ECGINVAL;

	pthread_rwlock_rdlock(&cg_mount_table_lock);

	if (cg_mount_table[*pos].name[0] == '\0') {
		ret = ECGEOF;
		goto out_unlock;
	}

	strncpy(info->name, cg_mount_table[*pos].name, FILENAME_MAX);

	strncpy(info->path, cg_mount_table[*pos].mount.path, FILENAME_MAX);

	(*pos)++;
	*handle = pos;

out_unlock:
	pthread_rwlock_unlock(&cg_mount_table_lock);
	return ret;
}

int cgroup_get_controller_begin(void **handle, struct cgroup_mount_point *info)
{
	int *pos;

	if (!cgroup_initialized)
		return ECGROUPNOTINITIALIZED;

	if (!info)
		return ECGINVAL;

	pos = malloc(sizeof(int));

	if (!pos) {
		last_errno = errno;
		return ECGOTHER;
	}

	*pos = 0;

	*handle = pos;

	return cgroup_get_controller_next(handle, info);
}

/**
 * Get process data (euid and egid) from /proc/<pid>/status file.
 * @param pid: The process id
 * @param euid: The uid of param pid
 * @param egid: The gid of param pid
 * @return 0 on success, > 0 on error.
 */
int cgroup_get_uid_gid_from_procfs(pid_t pid, uid_t *euid, gid_t *egid)
{
	FILE *f;
	char path[FILENAME_MAX];
	char buf[4092];
	uid_t ruid, suid, fsuid;
	gid_t rgid, sgid, fsgid;
	bool found_euid = false;
	bool found_egid = false;

	sprintf(path, "/proc/%d/status", pid);
	f = fopen(path, "re");
	if (!f)
		return ECGROUPNOTEXIST;

	while (fgets(buf, sizeof(buf), f)) {
		if (!strncmp(buf, "Uid:", 4)) {
			if (sscanf((buf + strlen("Uid:") + 1), "%d%d%d%d",
					&ruid, euid, &suid, &fsuid) != 4)
				break;
			cgroup_dbg("Scanned proc values are %d %d %d %d\n",
				ruid, *euid, suid, fsuid);
			found_euid = true;
		} else if (!strncmp(buf, "Gid:", 4)) {
			if (sscanf((buf + strlen("Gid:") + 1), "%d%d%d%d",
					&rgid, egid, &sgid, &fsgid) != 4)
				break;
			cgroup_dbg("Scanned proc values are %d %d %d %d\n",
				rgid, *egid, sgid, fsgid);
			found_egid = true;
		}
		if (found_euid && found_egid)
			break;
	}
	fclose(f);
	if (!found_euid || !found_egid) {
		/*
		 * This method doesn't match the file format of
		 * /proc/<pid>/status. The format has been changed
		 * and we should catch up the change.
		 */
		cgroup_warn("Warning: invalid file format of /proc/%d/status\n",
				pid);
		return ECGFAIL;
	}
	return 0;
}

/**
 * Get process name from /proc/<pid>/status file.
 * @param pid: The process id
 * @param pname_status : The process name
 * @return 0 on success, > 0 on error.
 */
static int cg_get_procname_from_proc_status(pid_t pid, char **procname_status)
{
	int ret = ECGFAIL;
	int len;
	FILE *f;
	char path[FILENAME_MAX];
	char buf[4092];

	sprintf(path, "/proc/%d/status", pid);
	f = fopen(path, "re");
	if (!f)
		return ECGROUPNOTEXIST;

	while (fgets(buf, sizeof(buf), f)) {
		if (!strncmp(buf, "Name:", 5)) {
			len = strlen(buf);
			if (buf[len - 1] == '\n')
				buf[len - 1] = '\0';
			*procname_status = strdup(buf + strlen("Name:") + 1);
			if (*procname_status == NULL) {
				last_errno = errno;
				ret = ECGOTHER;
				break;
			}
			ret = 0;
			break;
		}
	}
	fclose(f);
	return ret;
}

/**
 * Get process name from /proc/<pid>/cmdline file.
 * This function is mainly for getting a script name (shell, perl,
 * etc). A script name is written into the second or later argument
 * of /proc/<pid>/cmdline. This function gets each argument and
 * compares it to a process name taken from /proc/<pid>/status.
 * @param pid: The process id
 * @param pname_status : The process name taken from /proc/<pid>/status
 * @param pname_cmdline: The process name taken from /proc/<pid>/cmdline
 * @return 0 on success, > 0 on error.
 */
static int cg_get_procname_from_proc_cmdline(pid_t pid,
			const char *pname_status, char **pname_cmdline)
{
	FILE *f;
	int ret = ECGFAIL;
	int c = 0;
	int len = 0;
	char path[FILENAME_MAX];
	char buf_pname[FILENAME_MAX];
	char buf_cwd[FILENAME_MAX];

	memset(buf_cwd, '\0', sizeof(buf_cwd));
	sprintf(path, "/proc/%d/cwd", pid);
	if (readlink(path, buf_cwd, sizeof(buf_cwd)) < 0)
		return ECGROUPNOTEXIST;

	sprintf(path, "/proc/%d/cmdline", pid);
	f = fopen(path, "re");
	if (!f)
		return ECGROUPNOTEXIST;

	while (c != EOF) {
		c = fgetc(f);
		if ((c != EOF) && (c != '\0')) {
			buf_pname[len] = c;
			len++;
			continue;
		}
		buf_pname[len] = '\0';

		/*
		 * The taken process name from /proc/<pid>/status is
		 * shortened to 15 characters if it is over. So the
		 * name should be compared by its length.
		 */
		if (strncmp(pname_status, basename(buf_pname),
						TASK_COMM_LEN - 1)) {
			len = 0;
			continue;
		}
		if (buf_pname[0] == '/') {
			*pname_cmdline = strdup(buf_pname);
			if (*pname_cmdline == NULL) {
				last_errno = errno;
				ret = ECGOTHER;
				break;
			}
			ret = 0;
			break;
		} else {
			strcat(buf_cwd, "/");
			strcat(buf_cwd, buf_pname);
			if (!realpath(buf_cwd, path)) {
				last_errno = errno;
				ret = ECGOTHER;
				break;
			}
			*pname_cmdline = strdup(path);
			if (*pname_cmdline == NULL) {
				last_errno = errno;
				ret = ECGOTHER;
				break;
			}
			ret = 0;
			break;
		}
	}
	fclose(f);
	return ret;
}

/**
 * Get a process name from /proc file system.
 * This function allocates memory for a process name, writes a process
 * name onto it. So a caller should free the memory when unusing it.
 * @param pid: The process id
 * @param procname: The process name
 * @return 0 on success, > 0 on error.
 */
int cgroup_get_procname_from_procfs(pid_t pid, char **procname)
{
	int ret;
	char *pname_status;
	char *pname_cmdline;
	char path[FILENAME_MAX];
	char buf[FILENAME_MAX];

	ret = cg_get_procname_from_proc_status(pid, &pname_status);
	if (ret)
		return ret;

	/*
	 * Get the full patch of process name from /proc/<pid>/exe.
	 */
	memset(buf, '\0', sizeof(buf));
	sprintf(path, "/proc/%d/exe", pid);
	if (readlink(path, buf, sizeof(buf)) < 0) {
		/*
		 * readlink() fails if a kernel thread, and a process
		 * name is taken from /proc/<pid>/status.
		 */
		*procname = pname_status;
		return 0;
	}
	if (!strncmp(pname_status, basename(buf), TASK_COMM_LEN - 1)) {
		/*
		 * The taken process name from /proc/<pid>/status is
		 * shortened to 15 characters if it is over. So the
		 * name should be compared by its length.
		 */
		free(pname_status);
		*procname = strdup(buf);
		if (*procname == NULL) {
			last_errno = errno;
			return ECGOTHER;
		}
		return 0;
	}

	/*
	 * The above strncmp() is not 0 if a shell script, because
	 * /proc/<pid>/exe links a shell command (/bin/bash etc.)
	 * and the pname_status represents a shell script name.
	 * Then the full path of a shell script is taken from
	 * /proc/<pid>/cmdline.
	 */
	ret = cg_get_procname_from_proc_cmdline(pid, pname_status,
						    &pname_cmdline);
	if (!ret) {
		*procname = pname_cmdline;
		free(pname_status);
		return 0;
	}

	/*
	 * The above strncmp() is not 0 also if executing a symbolic link,
	 * /proc/pid/exe points to real executable name then.
	 * Return it as the last resort.
	 */
	free(pname_status);
	*procname = strdup(buf);
	if (*procname == NULL) {
		last_errno = errno;
		return ECGOTHER;
	}
	return 0;
}

int cgroup_register_unchanged_process(pid_t pid, int flags)
{
	int sk;
	int ret = 1;
	char buff[sizeof(CGRULE_SUCCESS_STORE_PID)];
	struct sockaddr_un addr;

	sk = socket(PF_UNIX, SOCK_STREAM, 0);
	if (sk < 0)
		return 1;

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, CGRULE_CGRED_SOCKET_PATH);

	if (connect(sk, (struct sockaddr *)&addr,
	    sizeof(addr.sun_family) + strlen(CGRULE_CGRED_SOCKET_PATH)) < 0) {
		/* If the daemon does not work, this function returns 0
		 * as success. */
		ret = 0;
		goto close;
	}
	if (write(sk, &pid, sizeof(pid)) < 0)
		goto close;

	if (write(sk, &flags, sizeof(flags)) < 0)
		goto close;

	if (read(sk, buff, sizeof(buff)) < 0)
		goto close;

	if (strncmp(buff, CGRULE_SUCCESS_STORE_PID, sizeof(buff)))
		goto close;

	ret = 0;
close:
	close(sk);
	return ret;
}

int cgroup_get_subsys_mount_point(const char *controller, char **mount_point)
{
	int i;
	int ret = ECGROUPNOTEXIST;

	if (!cgroup_initialized)
		return ECGROUPNOTINITIALIZED;

	if (!controller)
		return ECGINVAL;

	pthread_rwlock_rdlock(&cg_mount_table_lock);
	for (i = 0; cg_mount_table[i].name[0] != '\0'; i++) {
		if (strncmp(cg_mount_table[i].name, controller, FILENAME_MAX))
			continue;

		*mount_point = strdup(cg_mount_table[i].mount.path);

		if (!*mount_point) {
			last_errno = errno;
			ret = ECGOTHER;
			goto out_exit;
		}

		ret = 0;
		break;
	}
out_exit:
	pthread_rwlock_unlock(&cg_mount_table_lock);
	return ret;
}


int cgroup_get_all_controller_end(void **handle)
{
	FILE *proc_cgroup = (FILE *) *handle;

	if (!proc_cgroup)
		return ECGINVAL;

	fclose(proc_cgroup);
	*handle = NULL;

	return 0;
}


int cgroup_get_all_controller_next(void **handle, struct controller_data *info)
{
	FILE *proc_cgroup = (FILE *) *handle;
	int err = 0;
	int hierarchy, num_cgroups, enabled;
	char subsys_name[FILENAME_MAX];

	if (!proc_cgroup)
		return ECGINVAL;

	if (!info)
		return ECGINVAL;

	err = fscanf(proc_cgroup, "%s %d %d %d\n", subsys_name,
			&hierarchy, &num_cgroups, &enabled);

	if (err != 4)
		return ECGEOF;

	strncpy(info->name, subsys_name, FILENAME_MAX);
	info->name[FILENAME_MAX-1] = '\0';
	info->hierarchy = hierarchy;
	info->num_cgroups = num_cgroups;
	info->enabled = enabled;

	return 0;
}


int cgroup_get_all_controller_begin(void **handle, struct controller_data *info)
{
	FILE *proc_cgroup = NULL;
	char buf[FILENAME_MAX];
	int ret;

	if (!info)
		return ECGINVAL;

	proc_cgroup = fopen("/proc/cgroups", "re");
	if (!proc_cgroup) {
		last_errno = errno;
		return ECGOTHER;
	}

	if (!fgets(buf, FILENAME_MAX, proc_cgroup)) {
		last_errno = errno;
		fclose(proc_cgroup);
		*handle = NULL;
		return ECGOTHER;
	}
	*handle = proc_cgroup;

	ret = cgroup_get_all_controller_next(handle, info);
	if (ret != 0) {
		fclose(proc_cgroup);
		*handle = NULL;
	}
	return ret;
}

static int pid_compare(const void *a, const void *b)
{
	const pid_t *pid1, *pid2;

	pid1 = (pid_t *) a;
	pid2 = (pid_t *) b;

	return (*pid1 - *pid2);
}

/*
 *pids needs to be completely uninitialized so that we can set it up
 *
 * Caller must free up pids.
 */
int cgroup_get_procs(char *name, char *controller, pid_t **pids, int *size)
{
	char cgroup_path[FILENAME_MAX];
	FILE *procs;
	pid_t *tmp_list;
	int tot_procs = 16;
	int n = 0;
	int err;

	cg_build_path(name, cgroup_path, controller);
	strncat(cgroup_path, "/cgroup.procs", FILENAME_MAX-strlen(cgroup_path));

	/*
	 * This kernel does have support for cgroup.procs
	 */
	if (access(cgroup_path, F_OK))
		return ECGROUPUNSUPP;

	/*
	 * Keep doubling the memory allocated if needed
	 */
	tmp_list= malloc(sizeof(pid_t) * tot_procs);
	if (!tmp_list) {
		last_errno = errno;
		return ECGOTHER;
	}

	procs = fopen(cgroup_path, "r");
	if (!procs) {
		last_errno = errno;
		free(tmp_list);
		*pids = NULL;
		*size = 0;
		return ECGOTHER;
	}

	while (!feof(procs)) {
		while (!feof(procs) && n < tot_procs) {
			pid_t pid;
			err = fscanf(procs, "%u", &pid);
			if (err == EOF)
				break;
			tmp_list[n] = pid;
			n++;
		}
		if (!feof(procs)) {
			pid_t *orig_list = tmp_list;
			tot_procs *= 2;
			tmp_list = realloc(tmp_list, sizeof(pid_t) * tot_procs);
			if (!tmp_list) {
				last_errno = errno;
				fclose(procs);
				free(orig_list);
				*pids = NULL;
				*size = 0;
				return ECGOTHER;
			}
		}
	}

	fclose(procs);

	*size = n;

	qsort(tmp_list, n, sizeof(pid_t), &pid_compare);

	*pids = tmp_list;

	return 0;
}


int cgroup_dictionary_create(struct cgroup_dictionary **dict,
		int flags)
{
	if (!dict)
		return ECGINVAL;
	*dict = (struct cgroup_dictionary *) calloc(
			1, sizeof(struct cgroup_dictionary));

	if (!*dict) {
		last_errno = errno;
		return ECGOTHER;
	}
	(*dict)->flags = flags;
	return 0;
}


int cgroup_dictionary_add(struct cgroup_dictionary *dict,
		const char *name, const char *value)
{
	struct cgroup_dictionary_item *it;

	if (!dict)
		return ECGINVAL;

	it = (struct cgroup_dictionary_item *) malloc(
			sizeof(struct cgroup_dictionary_item));
	if (!it) {
		last_errno = errno;
		return ECGOTHER;
	}

	it->next = NULL;
	it->name = name;
	it->value = value;

	if (dict->tail) {
		dict->tail->next = it;
		dict->tail = it;
	} else {
		/* it is the first item */
		dict->tail = it;
		dict->head = it;
	}
	return 0;
}

int cgroup_dictionary_free(struct cgroup_dictionary *dict)
{
	struct cgroup_dictionary_item *it;

	if (!dict)
		return ECGINVAL;

	it = dict->head;
	while (it) {
		struct cgroup_dictionary_item *del = it;
		it = it->next;
		if (!(dict->flags & CG_DICT_DONT_FREE_ITEMS)) {
			free((void *)del->value);
			free((void *)del->name);
		}
		free(del);
	}

	free(dict);
	return 0;
}

int cgroup_dictionary_iterator_begin(struct cgroup_dictionary *dict,
		void **handle, const char **name, const char **value)
{
	struct cgroup_dictionary_iterator *iter;

	*handle = NULL;

	if (!dict)
		return ECGINVAL;

	iter = (struct cgroup_dictionary_iterator *) malloc(
			sizeof(struct cgroup_dictionary_iterator));
	if (!iter) {
		last_errno = errno;
		return ECGOTHER;
	}

	iter->item = dict->head;
	*handle = iter;
	return cgroup_dictionary_iterator_next(handle, name, value);
}

int cgroup_dictionary_iterator_next(void **handle,
		const char **name, const char **value)
{
	struct cgroup_dictionary_iterator *iter;

	if (!handle)
		return ECGINVAL;

	iter = *handle;

	if (!iter)
		return ECGINVAL;

	if (!iter->item)
		return ECGEOF;

	*name = iter->item->name;
	*value = iter->item->value;
	iter->item = iter->item->next;
	return 0;
}

void cgroup_dictionary_iterator_end(void **handle)
{
	if (!handle)
		return;

	free(*handle);
	*handle = NULL;
}

int cgroup_get_subsys_mount_point_begin(const char *controller, void **handle,
		char *path)
{
	int i;

	if (!cgroup_initialized)
		return ECGROUPNOTINITIALIZED;
	if (!handle || !path || !controller)
		return ECGINVAL;


	for (i = 0; cg_mount_table[i].name[0] != '\0'; i++)
		if (strcmp(controller, cg_mount_table[i].name) == 0)
			break;

	if (cg_mount_table[i].name[0] == '\0') {
		/* the controller is not mounted at all */
		*handle = NULL;
		*path = '\0';
		return ECGEOF;
	}

	/*
	 * 'handle' is pointer to struct cg_mount_point, which should be
	 * returned next.
	 */
	*handle = cg_mount_table[i].mount.next;
	strcpy(path, cg_mount_table[i].mount.path);
	return 0;
}

int cgroup_get_subsys_mount_point_next(void **handle,
		char *path)
{
	struct cg_mount_point *it;

	if (!cgroup_initialized)
		return ECGROUPNOTINITIALIZED;
	if (!handle || !path)
		return ECGINVAL;

	it = *handle;
	if (!it) {
		*handle = NULL;
		*path = '\0';
		return ECGEOF;
	}

	*handle = it->next;
	strcpy(path, it->path);
	return 0;
}

int cgroup_get_subsys_mount_point_end(void **handle)
{
	return 0;
}


