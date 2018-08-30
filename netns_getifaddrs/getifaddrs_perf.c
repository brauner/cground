#define _GNU_SOURCE
#define __STDC_FORMAT_MACROS
#include <fcntl.h>
#include <inttypes.h>
#include <linux/types.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "netns_getifaddrs.h"
#include "print_getifaddrs.h"

#define ITERATIONS 1000000
#define SEC_TO_MICROSEC(x) (1000000 * (x))

int main(int argc, char *argv[])
{
	int i, ret;
	__s32 netns_id;
	pid_t netns_pid;
	char path[1024];
	intmax_t times[ITERATIONS];
	struct timeval t1, t2;
	intmax_t time_in_mcs;
	int fret = EXIT_FAILURE;
	intmax_t sum = 0;
	int host_netns_fd = -1, netns_fd = -1;

	struct ifaddrs *ifaddrs = NULL;

	if (argc != 3)
		goto on_error;

	netns_id = atoi(argv[1]);
	netns_pid = atoi(argv[2]);
	printf("%d\n", netns_id);
	printf("%d\n", netns_pid);

	for (i = 0; i < ITERATIONS; i++) {
		ret = gettimeofday(&t1, NULL);
		if (ret < 0)
			goto on_error;

		ret = netns_getifaddrs(&ifaddrs, netns_id);
		freeifaddrs(ifaddrs);
		if (ret < 0)
			goto on_error;

		ret = gettimeofday(&t2, NULL);
		if (ret < 0)
			goto on_error;

		time_in_mcs = (SEC_TO_MICROSEC(t2.tv_sec) + t2.tv_usec) -
			      (SEC_TO_MICROSEC(t1.tv_sec) + t1.tv_usec);
		times[i] = time_in_mcs;
	}

	for (i = 0; i < ITERATIONS; i++)
		sum += times[i];

	printf("Mean time per file in microseconds (netnsid): %ju\n",
	       sum / ITERATIONS);

	ret = snprintf(path, sizeof(path), "/proc/%d/ns/net", netns_pid);
	if (ret < 0 || (size_t)ret >= sizeof(path))
		goto on_error;

	netns_fd = open(path, O_RDONLY | O_CLOEXEC);
	if (netns_fd < 0)
		goto on_error;

	host_netns_fd = open("/proc/self/ns/net", O_RDONLY | O_CLOEXEC);
	if (host_netns_fd < 0)
		goto on_error;

	memset(times, 0, sizeof(times));
	for (i = 0; i < ITERATIONS; i++) {
		ret = gettimeofday(&t1, NULL);
		if (ret < 0)
			goto on_error;

		ret = setns(netns_fd, CLONE_NEWNET);
		if (ret < 0)
			goto on_error;

		ret = getifaddrs(&ifaddrs);
		freeifaddrs(ifaddrs);
		if (ret < 0)
			goto on_error;

		ret = setns(host_netns_fd, CLONE_NEWNET);
		if (ret < 0)
			goto on_error;

		ret = gettimeofday(&t2, NULL);
		if (ret < 0)
			goto on_error;

		time_in_mcs = (SEC_TO_MICROSEC(t2.tv_sec) + t2.tv_usec) -
			      (SEC_TO_MICROSEC(t1.tv_sec) + t1.tv_usec);
		times[i] = time_in_mcs;
	}

	for (i = 0; i < ITERATIONS; i++)
		sum += times[i];

	printf("Mean time per file in microseconds (setns): %ju\n",
	       sum / ITERATIONS);

	fret = EXIT_SUCCESS;

on_error:
	if (netns_fd >= 0)
		close(netns_fd);

	if (host_netns_fd >= 0)
		close(host_netns_fd);

	exit(fret);
}
