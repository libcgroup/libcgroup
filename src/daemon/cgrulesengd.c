/*
 * Copyright Red Hat Inc. 2008
 *
 * Author: Steve Olivieri <sjo@redhat.com>
 * Author: Vivek Goyal <vgoyal@redhat.com>
 *
 * Some part of the programs have been derived from Dhaval Giani's posting
 * for daemon to place the task in right container. Original copyright notice
 * follows.
 *
 * Copyright IBM Corporation, 2007
 * Author: Dhaval Giani <dhaval <at> linux.vnet.ibm.com>
 * Derived from test_cn_proc.c by Matt Helsley
 * Original copyright notice follows
 *
 * Copyright (C) Matt Helsley, IBM Corp. 2005
 * Derived from fcctl.c by Guillaume Thouvenin
 * Original copyright notice follows:
 *
 * Copyright (C) 2005 BULL SA.
 * Written by Guillaume Thouvenin <guillaume.thouvenin <at> bull.net>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2.1 of the GNU Lesser General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * TODO Stop using netlink for communication (or at least rewrite that part).
 */

#include "libcgroup.h"
#include "cgrulesengd.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <string.h>
#include <linux/netlink.h>
#include <signal.h>
#include <time.h>
#include <syslog.h>
#include <getopt.h>

#include <sys/stat.h>
#include <unistd.h>
#include <linux/connector.h>
#include <linux/cn_proc.h>

/* Log file, NULL if logging to file is disabled */
FILE* logfile;

/* Log facility, 0 if logging to syslog is disabled */
int logfacility;

/* Current log level */
int loglevel;

/**
 * Prints the usage information for this program and, optionally, an error
 * message.  This function uses vfprintf.
 * 	@param fd The file stream to print to
 * 	@param msg The error message to print (printf style)
 * 	@param ... Any args to msg (printf style)
 */
void usage(FILE* fd, const char* msg, ...)
{
	/* List of args to msg */
	va_list ap;

	/* Put all args after msg into the list. */
	va_start(ap, msg);

	if (msg)
		vfprintf(fd, msg, ap);
	fprintf(fd, "\n");
	fprintf(fd, "cgrulesengd -- a daemon for the cgroups rules engine\n\n");
	fprintf(fd, "Usage : cgrulesengd [options]\n\n");
	fprintf(fd, "  options :\n");
	fprintf(fd, "    -q           | --quiet             quiet mode\n"
		"    -v           | --verbose           verbose mode\n"
		"    -f <path>    | --logfile=<path>    write log to file\n"
		"    -s[facility] | --syslog=[facility] write log to syslog\n"
		"    -n           | --nodaemom          don't fork daemon\n"
		"    -d           | --debug             same as -v -v -n -f -\n"
		"    -Q           | --nolog             disable logging\n"
		"    -h           | --help              show this help\n\n"
		);
	va_end(ap);
}

/**
 * Prints a formatted message (like printf()) to all log destinations.
 * Flushes the file stream's buffer so that the message is immediately
 * readable.
 * 	@param level The log level (LOG_EMERG ... LOG_DEBUG)
 * 	@param format The format for the message (printf style)
 * 	@param ... Any args to format (printf style)
 */
void flog(int level, const char *format, ...)
{
	/* List of args to format */
	va_list ap;

	/* Check the log level */
	if (level > loglevel)
		return;

	if (logfile) {
		/* Print the message to the given stream. */
		va_start(ap, format);
		vfprintf(logfile, format, ap);
		va_end(ap);
		fprintf(logfile, "\n");

		/*
		 * Flush the stream's buffer, so the data is readable
		 * immediately.
		 */
		fflush(logfile);
	}

	if (logfacility) {
		sigset_t sigset;

		sigemptyset(&sigset);
		sigaddset(&sigset, SIGUSR2);
		sigprocmask(SIG_BLOCK, &sigset, NULL);

		va_start(ap, format);
		vsyslog(LOG_MAKEPRI(logfacility, level), format, ap);
		va_end(ap);

		sigprocmask(SIG_UNBLOCK, &sigset, NULL);
	}
}

static int cgre_get_euid_egid_from_status(pid_t pid, uid_t *euid, gid_t *egid)
{
	/* Handle for the /proc/PID/status file */
	FILE *f;

	/* Path for /proc/PID/status file */
	char path[FILENAME_MAX];

	/* Temporary buffer */
	char buf[4092];

	/* UID data */
	uid_t ruid, suid, fsuid;

	/* GID data */
	gid_t rgid, sgid, fsgid;

	/*
	 * First, we need to open the /proc/PID/status file so that we can
	 * get the effective UID and GID for the process that we're working
	 * on.  This process is probably not us, so we can't just call
	 * geteuid() or getegid().
	 */
	sprintf(path, "/proc/%d/status", pid);
	f = fopen(path, "r");
	if (!f) {
		flog(LOG_WARNING, "Failed to open %s", path);
		return 1;
	}

	/* Have the eUID, need to find the eGID. */
	memset(buf, '\0', sizeof(buf));
	while (fgets(buf, sizeof(buf), f)) {
		if (!strncmp(buf, "Uid:", 4)) {
			sscanf((buf + 5), "%d%d%d%d", &ruid, euid,
				&suid, &fsuid);
		} else if (!strncmp(buf, "Gid:", 4)) {
			sscanf((buf + 5), "%d%d%d%d", &rgid, egid,
				&sgid, &fsgid);
		}
		memset(buf, '\0', sizeof(buf));
	}
	fclose(f);
	return 0;
}

struct parent_info {
	__u64 timestamp;
	pid_t pid;
};
struct array_parent_info {
	int index;
	int num_allocation;
	struct parent_info **parent_info;
};
struct array_parent_info array_pi;

static int cgre_store_parent_info(pid_t pid)
{
	__u64 uptime_ns;
	struct timespec tp;
	struct parent_info *info;

	if (clock_gettime(CLOCK_MONOTONIC, &tp) < 0) {
		flog(LOG_WARNING, "Failed to get time");
		return 1;
	}
	uptime_ns = ((__u64)tp.tv_sec * 1000 * 1000 * 1000 ) + tp.tv_nsec;

	if (array_pi.index >= array_pi.num_allocation) {
		array_pi.num_allocation += 100;
		array_pi.parent_info = realloc(array_pi.parent_info,
					sizeof(info) * array_pi.num_allocation);
		if (!array_pi.parent_info) {
			flog(LOG_WARNING, "Failed to allocate memory");
			return 1;
		}
	}
	info = calloc(1, sizeof(struct parent_info));
	if (!info) {
		flog(LOG_WARNING, "Failed to allocate memory");
		return 1;
	}
	info->timestamp = uptime_ns;
	info->pid = pid;

	array_pi.parent_info[array_pi.index] = info;
	array_pi.index++;

	return 0;
}

static void cgre_remove_old_parent_info(__u64 key_timestamp)
{
	int i, j;

	for (i = 0; i < array_pi.index; i++) {
		if (key_timestamp < array_pi.parent_info[i]->timestamp)
			continue;
		free(array_pi.parent_info[i]);
		for (j = i; j < array_pi.index - 1; j++)
			array_pi.parent_info[j] = array_pi.parent_info[j + 1];
		array_pi.index--;
		i--;
	}
	return;
}

static int cgre_was_parent_changed_when_forking(const struct proc_event *ev)
{
	int i;
	pid_t parent_pid;
	__u64 timestamp_child;
	__u64 timestamp_parent;

	parent_pid = ev->event_data.fork.parent_pid;
	timestamp_child = ev->timestamp_ns;

	cgre_remove_old_parent_info(timestamp_child);

	for (i = 0; i < array_pi.index; i++) {
		if (parent_pid != array_pi.parent_info[i]->pid)
			continue;
		timestamp_parent = array_pi.parent_info[i]->timestamp;
		if (timestamp_child > timestamp_parent)
			continue;
		return 1;
	}
	return 0;
}

static int cgre_change_cgroup_uid_gid(const uid_t uid, const gid_t gid,
					const pid_t pid)
{
	int ret;
	sigset_t sigset;

	/*
	 * For avoiding the deadlock, protect cdgroup_change_cgroup_
	 * ~uid_gid_flags() by blocking SIGUSR2 signal.
	 */
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGUSR2);
	sigprocmask(SIG_BLOCK, &sigset, NULL);

	ret = cgroup_change_cgroup_uid_gid_flags(uid, gid, pid,
						 CGFLAG_USECACHE);
	sigprocmask(SIG_UNBLOCK, &sigset, NULL);

	return ret;
}

/**
 * Process an event from the kernel, and determine the correct UID/GID/PID to
 * pass to libcgroup.  Then, libcgroup will decide the cgroup to move the PID
 * to, if any.
 * 	@param ev The event to process
 * 	@param type The type of event to process (part of ev)
 * 	@return 0 on success, > 0 on failure
 */
int cgre_process_event(const struct proc_event *ev, const int type)
{
	pid_t pid = 0, log_pid = 0;
	uid_t euid, log_uid = 0;
	gid_t egid, log_gid = 0;

	int ret = 0;

	switch (type) {
	case PROC_EVENT_UID:
	case PROC_EVENT_GID:
		pid = ev->event_data.id.process_pid;
		break;
	case PROC_EVENT_FORK:
		/*
		 * If this process was forked while changing parent's cgroup,
		 * this process's cgroup also should be changed.
		 */
		if (!cgre_was_parent_changed_when_forking(ev))
			return 0;
		pid = ev->event_data.fork.child_pid;
		break;
	default:
		break;
	}
	if (cgre_get_euid_egid_from_status(pid, &euid, &egid))
		/* cgre_get_euid_egid_from_status() returns 1 if it fails to
		 * open /proc/<pid>/status file and that is not a problem. */
		return 0;

	/*
	 * Now that we have the UID, the GID, and the PID, we can make a call
	 * to libcgroup to change the cgroup for this PID.
	 */
	log_pid = pid;
	switch (type) {
	case PROC_EVENT_UID:
		log_uid = ev->event_data.id.e.euid;
		log_gid = egid;
		ret = cgre_change_cgroup_uid_gid(
					ev->event_data.id.e.euid,
					egid, pid);
		break;
	case PROC_EVENT_GID:
		log_uid = euid;
		log_gid = ev->event_data.id.e.egid;
		ret = cgre_change_cgroup_uid_gid(euid,
					ev->event_data.id.e.egid, pid);
		break;
	case PROC_EVENT_FORK:
		log_uid = euid;
		log_gid = egid;
		ret = cgre_change_cgroup_uid_gid(euid, egid, pid);
		break;
	default:
		break;
	}

	if (ret) {
		/*
		 * TODO: add some supression, do not spam log when every group
		 * change fails
		 */
		flog(LOG_WARNING, "Cgroup change for PID: %d, UID: %d, GID: %d"
			" FAILED! (Error Code: %d)", log_pid, log_uid, log_gid,
			ret);
	} else {
		ret = cgre_store_parent_info(pid);
		flog(LOG_INFO, "Cgroup change for PID: %d, UID: %d, GID: %d OK",
			log_pid, log_uid, log_gid);
	}
	return ret;
}

/**
 * Handle a netlink message.  In the event of PROC_EVENT_UID or PROC_EVENT_GID,
 * we pass the event along to cgre_process_event for further processing.  All
 * other events are ignored.
 * 	@param cn_hdr The netlink message
 * 	@return 0 on success, > 0 on error
 */
int cgre_handle_msg(struct cn_msg *cn_hdr)
{
	/* The event to consider */
	struct proc_event *ev;

	/* Return codes */
	int ret = 0;

	/* Get the event data.  We only care about two event types. */
	ev = (struct proc_event*)cn_hdr->data;
	switch (ev->what) {
	case PROC_EVENT_UID:
		flog(LOG_DEBUG, "UID Event: PID = %d, tGID = %d, rUID = %d,"
				" eUID = %d", ev->event_data.id.process_pid,
				ev->event_data.id.process_tgid,
				ev->event_data.id.r.ruid,
				ev->event_data.id.e.euid);
		ret = cgre_process_event(ev, PROC_EVENT_UID);
		break;
	case PROC_EVENT_GID:
		flog(LOG_DEBUG, "GID Event: PID = %d, tGID = %d, rGID = %d,"
				" eGID = %d", ev->event_data.id.process_pid,
				ev->event_data.id.process_tgid,
				ev->event_data.id.r.rgid,
				ev->event_data.id.e.egid);
		ret = cgre_process_event(ev, PROC_EVENT_GID);
		break;
	case PROC_EVENT_FORK:
		ret = cgre_process_event(ev, PROC_EVENT_FORK);
		break;
	default:
		break;
	}

	return ret;
}

int cgre_create_netlink_socket_process_msg()
{
	int sk_nl;
	int err;
	struct sockaddr_nl my_nla, kern_nla, from_nla;
	socklen_t from_nla_len;
	char buff[BUFF_SIZE];
	int rc = -1;
	struct nlmsghdr *nl_hdr;
	struct cn_msg *cn_hdr;
	enum proc_cn_mcast_op *mcop_msg;
	size_t recv_len = 0;

	/*
	 * Create an endpoint for communication. Use the kernel user
	 * interface device (PF_NETLINK) which is a datagram oriented
	 * service (SOCK_DGRAM). The protocol used is the connector
	 * protocol (NETLINK_CONNECTOR)
	 */
	sk_nl = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_CONNECTOR);
	if (sk_nl == -1) {
		cgroup_dbg("socket sk_nl error");
		return rc;
	}

	my_nla.nl_family = AF_NETLINK;
	my_nla.nl_groups = CN_IDX_PROC;
	my_nla.nl_pid = getpid();
	my_nla.nl_pad = 0;

	kern_nla.nl_family = AF_NETLINK;
	kern_nla.nl_groups = CN_IDX_PROC;
	kern_nla.nl_pid = 1;
	kern_nla.nl_pad = 0;

	err = bind(sk_nl, (struct sockaddr *)&my_nla, sizeof(my_nla));
	if (err == -1) {
		cgroup_dbg("binding sk_nl error");
		goto close_and_exit;
	}

	nl_hdr = (struct nlmsghdr *)buff;
	cn_hdr = (struct cn_msg *)NLMSG_DATA(nl_hdr);
	mcop_msg = (enum proc_cn_mcast_op*)&cn_hdr->data[0];
	cgroup_dbg("sending proc connector: PROC_CN_MCAST_LISTEN... ");
	memset(buff, 0, sizeof(buff));
	*mcop_msg = PROC_CN_MCAST_LISTEN;

	/* fill the netlink header */
	nl_hdr->nlmsg_len = SEND_MESSAGE_LEN;
	nl_hdr->nlmsg_type = NLMSG_DONE;
	nl_hdr->nlmsg_flags = 0;
	nl_hdr->nlmsg_seq = 0;
	nl_hdr->nlmsg_pid = getpid();

	/* fill the connector header */
	cn_hdr->id.idx = CN_IDX_PROC;
	cn_hdr->id.val = CN_VAL_PROC;
	cn_hdr->seq = 0;
	cn_hdr->ack = 0;
	cn_hdr->len = sizeof(enum proc_cn_mcast_op);
	cgroup_dbg("sending netlink message len=%d, cn_msg len=%d\n",
		nl_hdr->nlmsg_len, (int) sizeof(struct cn_msg));
	if (send(sk_nl, nl_hdr, nl_hdr->nlmsg_len, 0) != nl_hdr->nlmsg_len) {
		cgroup_dbg("failed to send proc connector mcast ctl op!\n");
		goto close_and_exit;
	}
	cgroup_dbg("sent\n");

	for(;;) {
		memset(buff, 0, sizeof(buff));
		from_nla_len = sizeof(from_nla);
		struct nlmsghdr *nlh = (struct nlmsghdr*)buff;
		memcpy(&from_nla, &kern_nla, sizeof(from_nla));
		recv_len = recvfrom(sk_nl, buff, BUFF_SIZE, 0,
		(struct sockaddr*)&from_nla, &from_nla_len);
		if (recv_len == ENOBUFS) {
			flog(LOG_ERR, "ERROR: NETLINK BUFFER FULL, MESSAGE "
					"DROPPED!");
			continue;
		}
		if (recv_len < 1)
			continue;
		while (NLMSG_OK(nlh, recv_len)) {
			cn_hdr = NLMSG_DATA(nlh);
			if (nlh->nlmsg_type == NLMSG_NOOP) {
				nlh = NLMSG_NEXT(nlh, recv_len);
				continue;
			}
			if ((nlh->nlmsg_type == NLMSG_ERROR) ||
					(nlh->nlmsg_type == NLMSG_OVERRUN))
				break;
			if(cgre_handle_msg(cn_hdr) < 0) {
				goto close_and_exit;
			}
			if (nlh->nlmsg_type == NLMSG_DONE)
				break;
			nlh = NLMSG_NEXT(nlh, recv_len);
		}
	}

close_and_exit:
	close(sk_nl);
	return rc;
}

/**
 * Start logging. Opens syslog and/or log file and sets log level.
 * 	@param logp Path of the log file, NULL if no log file was specified
 * 	@param logf Syslog facility, NULL if no facility was specified
 * 	@param logv Log verbosity, 2 is the default, 0 = no logging, 4 = everything
 */
static void cgre_start_log(const char *logp, int logf, int logv)
{
	/* Current system time */
	time_t tm;

	/* Log levels */
	int loglevels[] = {
		LOG_EMERG,		/* -qq */
		LOG_ERR,		/* -q */
		LOG_NOTICE,		/* default */
		LOG_INFO,		/* -v */
		LOG_DEBUG		/* -vv */
	};

	/* Set default logging destination if nothing was specified */
	if (!logp && !logf)
		logf = LOG_DAEMON;

	/* Open log file */
	if (logp) {
		if (strcmp("-", logp) == 0) {
			logfile = stdout;
		} else {
			logfile = fopen(logp, "a");
			if (!logfile) {
				fprintf(stderr, "Failed to open log file %s,"
					" error: %s. Continuing anyway.\n",
					logp, strerror(errno));
				logfile = stdout;
			}
		}
	} else
		logfile = NULL;

	/* Open syslog */
	if (logf) {
		openlog("CGRE", LOG_CONS | LOG_PID, logf);
		logfacility = logf;
	} else
		logfacility = 0;

	/* Set the log level */
	if (logv < 0)
		logv = 0;
	if (logv >= sizeof(loglevels)/sizeof(int))
		logv = sizeof(loglevels)/sizeof(int)-1;

	loglevel = loglevels[logv];

	flog(LOG_DEBUG, "CGroup Rules Engine Daemon log started");
	tm = time(0);
	flog(LOG_DEBUG, "Current time: %s", ctime(&tm));
	flog(LOG_DEBUG, "Opened log file: %s, log facility: %d, log level: %d",
			logp, logfacility, loglevel);
}


/**
 * Turns this program into a daemon.  In doing so, we fork() and kill the
 * parent process.  Note too that stdout, stdin, and stderr are closed in
 * daemon mode, and a file descriptor for a log file is opened.
 * 	@param logp Path of the log file, NULL if no log file was specified
 * 	@param logf Syslog facility, 0 if no facility was specified
 * 	@param daemon False to turn off daemon mode (no fork, leave FDs open)
 * 	@param logv Log verbosity, 2 is the default, 0 = no logging, 5 = everything
 * 	@return 0 on success, > 0 on error
 */
int cgre_start_daemon(const char *logp, const int logf,
			const unsigned char daemon, const int logv)
{
	/* PID returned from the fork() */
	pid_t pid;

	/* Fork and die. */
	if (daemon) {
		pid = fork();
		if (pid < 0) {
			openlog("CGRE", LOG_CONS, LOG_DAEMON|LOG_WARNING);
			syslog(LOG_DAEMON|LOG_WARNING, "Failed to fork,"
					" error: %s", strerror(errno));
			closelog();
			fprintf(stderr, "Failed to fork(), %s\n",
					strerror(errno));
			return 1;
		} else if (pid > 0) {
			exit(EXIT_SUCCESS);
		}

		/* Change the file mode mask. */
		umask(0);
	} else {
		cgroup_dbg("Not using daemon mode.\n");
		pid = getpid();
	}

	cgre_start_log(logp, logf, logv);

	if (!daemon) {
		/* We can skip the rest, since we're not becoming a daemon. */
		flog(LOG_INFO, "Proceeding with PID %d", getpid());
		return 0;
	} else {
		/* Get a new SID for the child. */
		if (setsid() < 0) {
			flog(LOG_ERR, "Failed to get a new SID, error: %s",
					strerror(errno));
			return 2;
		}

		/* Change to the root directory. */
		if (chdir("/") < 0) {
			flog(LOG_ERR, "Failed to chdir to /, error: %s",
					strerror(errno));
			return 3;
		}

		/* Close standard file descriptors. */
		close(STDIN_FILENO);
		if (logfile != stdout)
			close(STDOUT_FILENO);
		close(STDERR_FILENO);
	}

	/* If we make it this far, we're a real daemon! Or we chose not to.  */
	flog(LOG_INFO, "Proceeding with PID %d", getpid());
	return 0;
}

/**
 * Catch the SIGUSR2 signal and reload the rules configuration.  This function
 * makes use of the logfile and flog() to print the new rules.
 * 	@param signum The signal that we caught (always SIGUSR2)
 */
void cgre_flash_rules(int signum)
{
	/* Current time */
	time_t tm = time(0);

	flog(LOG_NOTICE, "Reloading rules configuration.");
	flog(LOG_DEBUG, "Current time: %s", ctime(&tm));

	/* Ask libcgroup to reload the rules table. */
	cgroup_reload_cached_rules();

	/* Print the results of the new table to our log file. */
	if (logfile && loglevel >= LOG_INFO) {
		cgroup_print_rules_config(logfile);
		fprintf(logfile, "\n");
	}
}

/**
 * Catch the SIGTERM and SIGINT signals so that we can exit gracefully.  Before
 * exiting, this function makes use of the logfile and flog().
 * 	@param signum The signal that we caught (SIGTERM, SIGINT)
 */
void cgre_catch_term(int signum)
{
	/* Current time */
	time_t tm = time(0);

	flog(LOG_NOTICE, "Stopped CGroup Rules Engine Daemon at %s",
			ctime(&tm));

	/* Close the log file, if we opened one */
	if (logfile && logfile != stdout)
		fclose(logfile);

	/* Close syslog */
	if (logfacility)
		closelog();

	exit(EXIT_SUCCESS);
}

/**
 * Parse the syslog facility as received on command line.
 * 	@param arg Command line argument with the syslog facility
 * 	@return the syslog facility (e.g. LOG_DAEMON) or 0 on error
 */
static int cgre_parse_syslog_facility(const char *arg)
{
    if (arg == NULL)
	return 0;

    if (strlen(arg) > 1)
	return 0;

	switch (arg[0]) {
	case '0':
		return LOG_LOCAL0;
	case '1':
		return LOG_LOCAL1;
	case '2':
		return LOG_LOCAL2;
	case '3':
		return LOG_LOCAL3;
	case '4':
		return LOG_LOCAL4;
	case '5':
		return LOG_LOCAL5;
	case '6':
		return LOG_LOCAL6;
	case '7':
		return LOG_LOCAL7;
	case 'D':
		return LOG_DAEMON;
	default:
		return 0;
	}
}

int main(int argc, char *argv[])
{
	/* Patch to the log file */
	const char *logp = NULL;

	/* Syslog facility */
	int facility = 0;

	/* Verbose level */
	int verbosity = 2;

	/* For catching signals */
	struct sigaction sa;

	/* Should we daemonize? */
	unsigned char daemon = 1;

	/* Return codes */
	int ret = 0;

	/* Command line arguments */
	const char *short_options = "hvqf:s::ndQ";
	struct option long_options[] = {
		{"help", no_argument, NULL, 'h'},
		{"verbose", no_argument, NULL, 'v'},
		{"quiet", no_argument, NULL, 'q'},
		{"logfile", required_argument, NULL, 'f'},
		{"syslog", optional_argument, NULL, 's'},
		{"nodaemon", no_argument, NULL, 'n'},
		{"debug", no_argument, NULL, 'd'},
		{"nolog", no_argument, NULL, 'Q'},
		{NULL, 0, NULL, 0}
	};

	/* Make sure the user is root. */
	if (getuid() != 0) {
		fprintf(stderr, "Error: Only root can start/stop the control"
				" group rules engine daemon\n");
		ret = 1;
		goto finished;
	}

	while (1) {
		int c;

		c = getopt_long(argc, argv, short_options, long_options, NULL);
		if (c == -1)
			break;

		switch (c) {
		case 'h':   /* --help */
			usage(stdout, "Help:\n");
			ret = 0;
			goto finished;

		case 'v':   /* --verbose */
			verbosity++;
			break;

		case 'q':   /* --quiet */
			verbosity--;
			break;

		case 'Q':   /* --nolog */
			verbosity = 0;
			break;

		case 'f':   /* --logfile=<filename> */
			logp = optarg;
			break;

		case 's':   /* --syslog=[facility] */
			if (optarg) {
				facility = cgre_parse_syslog_facility(optarg);
				if (facility == 0) {
					fprintf(stderr,
						"Unknown syslog facility: %s\n",
						optarg);
					ret = 2;
					goto finished;
				}
			} else {
				facility = LOG_DAEMON;
			}
			break;

		case 'n':   /* --no-fork */
			daemon = 0;
			break;

		case 'd':   /* --debug */
			/* same as -vvn */
			daemon = 0;
			verbosity = 4;
			logp = "-";
			break;

		default:
			usage(stderr, "");
			ret = 2;
			goto finished;
		}
	}

	/* Initialize libcgroup. */
	if ((ret = cgroup_init()) != 0) {
		fprintf(stderr, "Error: libcgroup initialization failed, %d\n",
				ret);
		goto finished;
	}

	/* Ask libcgroup to load the configuration rules. */
	if ((ret = cgroup_init_rules_cache()) != 0) {
		fprintf(stderr, "Error: libcgroup failed to initialize rules"
				"cache, %d\n", ret);
		goto finished;
	}

	/* Now, start the daemon. */
	ret = cgre_start_daemon(logp, facility, daemon, verbosity);
	if (ret < 0) {
		fprintf(stderr, "Error: Failed to launch the daemon, %d\n",
			ret);
		goto finished;
	}

	/*
	 * Set up the signal handler to reload the cached rules upon reception
	 * of a SIGUSR2 signal.
	 */
	sa.sa_handler = &cgre_flash_rules;
	sa.sa_flags = 0;
	sa.sa_restorer = NULL;
	sigemptyset(&sa.sa_mask);
	if ((ret = sigaction(SIGUSR2, &sa, NULL))) {
		flog(LOG_ERR, "Failed to set up signal handler for SIGUSR2."
				" Error: %s", strerror(errno));
		goto finished;
	}

	/*
	 * Set up the signal handler to catch SIGINT and SIGTERM so that we
	 * can exit gracefully.
	 */
	sa.sa_handler = &cgre_catch_term;
	ret = sigaction(SIGINT, &sa, NULL);
	ret |= sigaction(SIGTERM, &sa, NULL);
	if (ret) {
		flog(LOG_ERR, "Failed to set up the signal handler.  Error:"
				" %s", strerror(errno));
		goto finished;
	}

	/* Print the configuration to the log file, or stdout. */
	if (logfile && loglevel >= LOG_INFO)
		cgroup_print_rules_config(logfile);

	flog(LOG_NOTICE, "Started the CGroup Rules Engine Daemon.");

	/* We loop endlesly in this function, unless we encounter an error. */
	ret =  cgre_create_netlink_socket_process_msg();

finished:
	if (logfile && logfile != stdout)
		fclose(logfile);

	return ret;
}
