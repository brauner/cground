#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>

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

int main(int argc, char *argv[])
{
	int ret;
	size_t umsg_len;
	char *arg_umsg, *umsg;
	pid_t arg_pid, self_pid;
	struct nl_handler nlh;
	int netns_fd = -1, userns_fd = -1;
	struct nlmsg *nlmsg = NULL;

	if (argc < 3) {
		fprintf(stderr, "Missing arguments\n");
		exit(EXIT_FAILURE);
	}

	/* Check whether we need to attach to any namespaces before opening the
	 * netlink socket.
	 */
	arg_pid = atoi(argv[1]);
	self_pid = getpid();
	netns_fd = pids_in_same_namespace(self_pid, arg_pid, "net");
	if (netns_fd == -1) {
		fprintf(stderr, "Failed to determine whether we are in the "
				"same network namespace\n");
		exit(EXIT_FAILURE);
	}

	if (netns_fd >= 0) {
		int current_userns_fd;

		current_userns_fd = preserve_ns(self_pid, "user");
		if (current_userns_fd < 0) {
			close(netns_fd);
			fprintf(stderr, "%s - Failed to preserve current user "
					"namespace \n", strerror(errno));
			exit(EXIT_FAILURE);
		}

		userns_fd = ioctl(netns_fd, NS_GET_USERNS);
		if (userns_fd < 0) {
			close(netns_fd);
			close(current_userns_fd);
			fprintf(stderr, "%s - Failed to get owning user "
					"namespace of network namespace\n",
					strerror(errno));
			exit(EXIT_FAILURE);
		}

		ret = fds_in_same_namespace(current_userns_fd, userns_fd);
		close(current_userns_fd);
		if (ret == -1) {
			close(netns_fd);
			close(userns_fd);
			fprintf(stderr, "Failed to determine whether fds refer "
					"to the same user namespace\n");
			exit(EXIT_FAILURE);
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
			exit(EXIT_FAILURE);
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
			exit(EXIT_FAILURE);
		}
	}

	ret = netlink_open(&nlh, NETLINK_KOBJECT_UEVENT, 0);
	if (ret < 0) {
		fprintf(stderr, "Failed to open netlink uevent socket\n");
		exit(EXIT_FAILURE);
	}

	nlmsg = nlmsg_alloc(NLMSG_GOOD_SIZE);
	if (!nlmsg) {
		printf("Failed to allocate memory\n");
		ret = EXIT_FAILURE;
		goto on_error;
	}

	nlmsg->nlmsghdr->nlmsg_flags = NLM_F_ACK | NLM_F_REQUEST;
	nlmsg->nlmsghdr->nlmsg_type = UEVENT_SEND;

	arg_umsg = argv[2];
	umsg_len = strlen(arg_umsg) + 1;
	umsg = nlmsg_reserve(nlmsg, umsg_len);
	if (!umsg) {
		fprintf(stderr, "Failed to allocate memory\n");
		ret = EXIT_FAILURE;
		goto on_error;
	}
	/* Place uevent message in buffer. */
	memcpy(umsg, arg_umsg, umsg_len + 1);

	ret = netlink_transaction(&nlh, nlmsg, nlmsg);
	if (ret < 0) {
		fprintf(stderr, "Failed to send netlink message: %s\n",
			strerror(-ret));
		ret = EXIT_FAILURE;
		goto on_error;
	}

	ret = EXIT_SUCCESS;

on_error:
	netlink_close(&nlh);
	exit(ret);
}
