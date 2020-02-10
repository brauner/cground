#define __GNU_SOURCE
#define __STDC_FORMAT_MACROS
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>

#define TEST_NR_FILES 1000000
#define TEST_ITER 50
int main(int argc, char *argv[])
{
	int ret;
	size_t i, k;
	int fd[TEST_NR_FILES];
	int times[1000];
	char pathname[4096];
	struct stat st;
	struct timeval t1, t2;
	uint64_t time_in_mcs;
	uint64_t sum = 0;

	if (argc != 2) {
		fprintf(stderr, "Please specify a directory where to create "
				"the test files\n");
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < sizeof(fd) / sizeof(fd[0]); i++) {
		sprintf(pathname, "%s/idmap_test_%zu", argv[1], i);
		fd[i]= open(pathname, O_RDWR | O_CREAT, S_IXUSR | S_IXGRP | S_IXOTH);
		if (fd[i] < 0) {
			ssize_t j;
			fprintf(stderr, "%s - Failed to create file \"%s\"", strerror(errno), pathname);
			for (j = i; j >= 0; j--)
				close(fd[j]);
			exit(EXIT_FAILURE);
		}
	}

	for (i = 0; i < sizeof(fd) / sizeof(fd[0]); i++) {
		ret = syscall(__NR_fstat, fd[i], &st);
		if (ret)
			exit(EXIT_FAILURE);
	}

	printf("%d\n", getpid());
	// raise(SIGSTOP);
	for (k = 0; k < TEST_ITER; k++) {
		ret = syscall(__NR_gettimeofday, &t1, NULL);
		if (ret < 0) {
			fprintf(stderr, "%s - Failed to get time of day", strerror(errno));
			goto close_all;
		}

		for (i = 0; i < sizeof(fd) / sizeof(fd[0]); i++) {
			ret = syscall(__NR_fstat, fd[i], &st);
			if (ret < 0) {
				fprintf(stderr, "%s - Failed to stat", strerror(errno));
				goto close_all;
			}
		}

		ret = syscall(__NR_gettimeofday, &t2, NULL);
		if (ret < 0) {
			fprintf(stderr, "%s - Failed to get time of day", strerror(errno));
			goto close_all;
		}

		time_in_mcs = (1000000 * t2.tv_sec + t2.tv_usec);
		time_in_mcs -= (1000000 * t1.tv_sec + t1.tv_usec);
		printf("%zu: Total time in micro seconds:       %" PRIu64 "\n",
		       k, time_in_mcs);
		printf("%zu: Total time in nanoseconds:         %" PRIu64 "\n",
		       k, time_in_mcs * 1000);
		printf("%zu: Time per file in nanoseconds:      %" PRIu64 "\n",
		       k, (time_in_mcs * 1000) / TEST_NR_FILES);
		times[k] = (time_in_mcs * 1000) / TEST_NR_FILES;
	}

close_all:
	for (i = 0; i < sizeof(fd) / sizeof(fd[0]); i++)
		close(fd[i]);

	if (ret < 0)
		exit(EXIT_FAILURE);

	for (k = 0; k < TEST_ITER; k++) {
		sum += times[k];
	}

	printf("Mean time per file in nanoseconds: %" PRIu64 "\n", sum / TEST_ITER);

	exit(EXIT_SUCCESS);;
}
