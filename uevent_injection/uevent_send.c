#define _GNU_SOURCE
#include <asm/types.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "nl.h"

#ifndef UEVENT_SEND
#define UEVENT_SEND 16
#endif

#ifndef NS_GET_USERNS
#define NSIO 0xb7
#define NS_GET_USERNS _IO(NSIO, 0x1)
#define NS_GET_PARENT _IO(NSIO, 0x2)
#endif

int preserve_ns(const int pid, const char *ns)
{
	int ret;
/* 5 /proc + 21 /int_as_str + 3 /ns + 20 /NS_NAME + 1 \0 */
#define __NS_PATH_LEN 50
	char path[__NS_PATH_LEN];

	/* This way we can use this function to also check whether namespaces
	 * are supported by the kernel by passing in the NULL or the empty
	 * string.
	 */
	ret = snprintf(path, __NS_PATH_LEN, "/proc/%d/ns%s%s", pid,
		       !ns || strcmp(ns, "") == 0 ? "" : "/",
		       !ns || strcmp(ns, "") == 0 ? "" : ns);
	errno = EFBIG;
	if (ret < 0 || (size_t)ret >= __NS_PATH_LEN)
		return -EFBIG;

	return open(path, O_RDONLY | O_CLOEXEC);
}

static int fds_in_same_namespace(int ns_fd1, int ns_fd2)
{
	int ret = -1;
	struct stat ns_st1, ns_st2;

	ret = fstat(ns_fd1, &ns_st1);
	if (ret < 0)
		return -1;

	ret = fstat(ns_fd2, &ns_st2);
	if (ret < 0)
		return -1;

	/* processes are in the same namespace */
	ret = -EINVAL;
	if ((ns_st1.st_dev == ns_st2.st_dev ) && (ns_st1.st_ino == ns_st2.st_ino))
		return -EINVAL;

	return ns_fd2;
}

/**
 * in_same_namespace - Check whether two processes are in the same namespace.
 * @pid1 - PID of the first process.
 * @pid2 - PID of the second process.
 * @ns   - Name of the namespace to check. Must correspond to one of the names
 *         for the namespaces as shown in /proc/<pid/ns/
 *
 * If the two processes are not in the same namespace returns an fd to the
 * namespace of the second process identified by @pid2. If the two processes are
 * in the same namespace returns -EINVAL, -1 if an error occurred.
 */
static int pids_in_same_namespace(pid_t pid1, pid_t pid2, const char *ns)
{
	int ns_fd1 = -1, ns_fd2 = -1, ret = -1;

	ns_fd1 = preserve_ns(pid1, ns);
	if (ns_fd1 < 0) {
		/* The kernel does not support this namespace. This is not an
		 * error.
		 */
		if (errno == ENOENT)
			return -EINVAL;

		goto out;
	}

	ns_fd2 = preserve_ns(pid2, ns);
	if (ns_fd2 < 0)
		goto out;

	ret = fds_in_same_namespace(ns_fd1, ns_fd2);
	if (ret < 0)
		goto out;

	/* processes are in different namespaces */
	ret = ns_fd2;
	ns_fd2 = -1;

out:

	if (ns_fd1 >= 0)
		close(ns_fd1);
	if (ns_fd2 >= 0)
		close(ns_fd2);

	return ret;
}

static int inject_uevent_by_pid(pid_t pid)
{
	int ret;
	char *arg_umsg, *umsg;
	pid_t self_pid;
	struct nl_handler nlh;
	int netns_fd = -1, userns_fd = -1;
	char *uevent[] = {
	    "add@/dev/tty63",
	    "ACTION=add",
	    "DEVNAME=/dev/tty63",
	    "DEVPATH=/devices/virtual/tty/tty63",
	    "SUBSYSTEM=tty",
	    "MAJOR=4",
	    "MINOR=63",
	    "SEQNUM=3716",
	    NULL,
	};
	struct nlmsg *nlmsg = NULL;

	/* Check whether we need to attach to any namespaces before opening the
	 * netlink socket.
	 */
	self_pid = getpid();
	netns_fd = pids_in_same_namespace(self_pid, pid, "net");
	if (netns_fd == -1) {
		fprintf(stderr, "Failed to determine whether we are in the "
				"same network namespace\n");
		return -1;
	}

	if (netns_fd >= 0) {
		int current_userns_fd;

		current_userns_fd = preserve_ns(self_pid, "user");
		if (current_userns_fd < 0) {
			close(netns_fd);
			fprintf(stderr, "%s - Failed to preserve current user "
					"namespace \n", strerror(errno));
			return -1;
		}

		userns_fd = ioctl(netns_fd, NS_GET_USERNS);
		if (userns_fd < 0) {
			close(netns_fd);
			close(current_userns_fd);
			fprintf(stderr, "%s - Failed to get owning user "
					"namespace of network namespace\n",
					strerror(errno));
			return -1;
		}

		ret = fds_in_same_namespace(current_userns_fd, userns_fd);
		close(current_userns_fd);
		if (ret == -1) {
			close(netns_fd);
			close(userns_fd);
			fprintf(stderr, "Failed to determine whether fds refer "
					"to the same user namespace\n");
			return -1;
		}

		if (ret == -EINVAL) {
			close(userns_fd);
			userns_fd = -EINVAL;
		}
	}

	if (userns_fd >= 0) {
		ret = setns(userns_fd, CLONE_NEWUSER);
		close(userns_fd);
		if (ret < 0) {
			fprintf(stderr, "%s - Failed to attach to user "
					"namespace\n", strerror(errno));
			if (netns_fd >= 0)
				close(netns_fd);
			return -1;
		}

		fprintf(stderr, "Attached to user namespace of %d\n", pid);

		/* udev will only accept messages from uid 0 so become root in
		 * the user namespace we attached to.
		 */
		ret = setgid(0);
		if (ret < 0) {
			fprintf(stderr, "Failed to switch to gid 0\n");
			if (netns_fd >= 0)
				close(netns_fd);
			return -1;
		}

		ret = setuid(0);
		if (ret < 0) {
			fprintf(stderr, "Failed to switch to uid 0\n");
			if (netns_fd >= 0)
				close(netns_fd);
			return -1;
		}
	}

	if (netns_fd >= 0) {
		ret = setns(netns_fd, CLONE_NEWNET);
		close(netns_fd);
		if (ret < 0) {
			fprintf(stderr, "%s - Failed to attach to network "
					"namespace\n", strerror(errno));
			if (userns_fd >= 0)
				close(userns_fd);
			return -1;
		}
		fprintf(stderr, "Attached to network namespace of %d\n", pid);
	}

	ret = netlink_open(&nlh, NETLINK_KOBJECT_UEVENT, 0);
	if (ret < 0) {
		fprintf(stderr, "Failed to open netlink uevent socket\n");
		return -1;
	}

	nlmsg = nlmsg_alloc(NLMSG_GOOD_SIZE);
	if (!nlmsg) {
		printf("Failed to allocate memory\n");
		ret = -1;
		goto on_error;
	}

	nlmsg->nlmsghdr->nlmsg_flags = NLM_F_ACK | NLM_F_REQUEST;
	nlmsg->nlmsghdr->nlmsg_type = UEVENT_SEND;
	nlmsg->nlmsghdr->nlmsg_pid = 0;

	fprintf(stderr, "Injecting:\n");
	for (char **it = uevent; it && *it; it++) {
		umsg = nlmsg_reserve_unaligned(nlmsg, strlen(*it) + 1);
		if (!umsg) {
			fprintf(stderr, "Failed to allocate memory\n");
			ret = -1;
			goto on_error;
		}

		/* Place uevent message in buffer. */
		memcpy(umsg, *it, strlen(*it) + 1);
		fprintf(stderr, "%s\n", *it);
	}

	ret = netlink_transaction(&nlh, nlmsg, nlmsg);
	if (ret < 0) {
		fprintf(stderr, "Failed to send netlink message: %s\n",
			strerror(-ret));
		ret = -1;
		goto on_error;
	}

	ret = 0;

on_error:
	netlink_close(&nlh);
	return ret;
}

int main(int argc, char *argv[])
{
	int ret, status;
	pid_t arg_pid, helper_pid;

	if (argc < 2) {
		fprintf(stderr, "Missing arguments\n");
		exit(EXIT_FAILURE);
	}

	/* Check whether we need to attach to any namespaces before opening the
	 * netlink socket.
	 */
	arg_pid = atoi(argv[1]);

	/* fork() helper task in case we need to attach to the owning user
	 * namespace of the target network namespace. If we do we can't
	 * reattach to the ancestor user namespace.
	 */
	helper_pid = fork();
	if (helper_pid < 0)
		_exit(EXIT_FAILURE);

	if (helper_pid == 0) {
		ret = inject_uevent_by_pid(arg_pid);
		if (ret < 0)
			_exit(EXIT_FAILURE);

		_exit(EXIT_SUCCESS);
	}

again:
	ret = waitpid(helper_pid, &status, 0);
	if (ret < 0) {
		if (errno == EINTR)
			goto again;

		_exit(EXIT_FAILURE);
	}

	if (ret != helper_pid)
		goto again;

	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		_exit(EXIT_FAILURE);

	_exit(EXIT_SUCCESS);
}
