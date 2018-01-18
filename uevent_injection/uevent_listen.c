#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <asm/types.h>
#include <linux/netlink.h>
#include <sys/socket.h>

#include "nl.h"

int main(int argc, char *argv[])
{
	int ret;
	char buf[4096];
	struct nl_handler nlh;

	ret = netlink_open(&nlh, NETLINK_KOBJECT_UEVENT, -1);
	if (ret < 0) {
		fprintf(stderr, "Failed to open netlink uevent socket\n");
		exit(EXIT_FAILURE);
	}

	ret = EXIT_SUCCESS;
	for (;;) {
		ssize_t r, w;

		r = read(nlh.fd, &buf, 4096);
		if (r <= 0) {
			ret = EXIT_FAILURE;
			goto on_error;
		}

		w = write(STDOUT_FILENO, &buf, r);
		if (r != w) {
			ret = EXIT_FAILURE;
			goto on_error;
		}
		write(STDOUT_FILENO, "\n", 1);
	}

on_error:
	netlink_close(&nlh);
	exit(ret);
}
