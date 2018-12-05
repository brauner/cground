#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

static inline int do_procfd_send_signal(int procfd, int sig, siginfo_t *info,
					unsigned int flags)
{
#ifdef __NR_procfd_send_signal
	return syscall(__NR_procfd_send_signal, procfd, sig, info, flags);
#else
	return -ENOSYS;
#endif
}

int main(int argc, char *argv[])
{
	int fd, ret, saved_errno, sig;

	if (argc < 3)
		exit(EXIT_FAILURE);

	fd = open(argv[1], O_DIRECTORY | O_CLOEXEC);
	if (fd < 0) {
		printf("%s - Failed to open \"%s\"\n", strerror(errno), argv[1]);
		exit(EXIT_FAILURE);
	}

	sleep(10);

	sig = atoi(argv[2]);

	printf("Sending signal %d to process %s\n", sig, argv[1]);
	ret = do_procfd_send_signal(fd, sig, NULL, 0);

	saved_errno = errno;
	close(fd);
	errno = saved_errno;

	if (ret < 0) {
		printf("%s - Failed to send signal %d to process %s\n",
		       strerror(errno), sig, argv[1]);
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
