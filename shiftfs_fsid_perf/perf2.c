#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include <limits.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <linux/capability.h>
#include <linux/magic.h>
#include <linux/sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/fsuid.h>

static void *readdir_thread(void *arg)
{
	DIR *d = arg;
	struct dirent *dent;

	for (;;) {
		errno = 0;
		dent = readdir(d);
		if (!dent) {
			if (errno)
				perror("readdir");
			break;
		}
		rewinddir(d);
	}

	return NULL;
}

	struct a {
		int n;
		int m;
		char s[100];
	};

static void foo(struct a *a, struct a *b)
{
	*b = *a;
	printf("%d - %d - %s\n", a->n, a->m, a->s);
	printf("%d - %d - %s\n", b->n, b->m, b->s);
}

#define __MS_RDONLY	 1	/* Mount read-only */
#define __MS_NOSUID	 2	/* Ignore suid and sgid bits */
#define __MS_NODEV	 4	/* Disallow access to device special files */
#define __MS_NOEXEC	 8	/* Disallow program execution */
#define __MS_SYNCHRONOUS	16	/* Writes are synced at once */
// #defi__ne MS_REMOUNT	32	/* Alter flags of a mounted FS */
#define __MS_MANDLOCK	64	/* Allow mandatory locks on an FS */
#define __MS_DIRSYNC	128	/* Directory modifications are synchronous */
#define __MS_NOATIME	1024	/* Do not update access times. */
#define __MS_NODIRATIME	2048	/* Do not update directory access times */
// #defi__ne MS_BIND		4096
// #defi__ne MS_MOVE		8192
#define __MS_REC		16384
#define __MS_VERBOSE	32768	/* War is peace. Verbosity is silence.
	__			   MS_VERBOSE is deprecated. */
#define __MS_SILENT	32768
#define __MS_POSIXACL	(1<<16)	/* VFS does not apply the umask */
// #defi__ne MS_UNBINDABLE	(1<<17)	/* change to unbindable */
// #defi__ne MS_PRIVATE	(1<<18)	/* change to private */
// #defi__ne MS_SLAVE	(1<<19)	/* change to slave */
// #defi__ne MS_SHARED	(1<<20)	/* change to shared */
#define __MS_RELATIME	(1<<21)	/* Update atime relative to mtime/ctime. */
#define __MS_KERNMOUNT	(1<<22) /* this is a kern_mount call */
#define __MS_I_VERSION	(1<<23) /* Update inode I_version field */
#define __MS_STRICTATIME	(1<<24) /* Always perform atime updates */
#define __MS_LAZYTIME	(1<<25) /* Update the on-disk [acm]times lazily */

#ifndef __NR_clone3
#define __NR_clone3 435
#endif

#define ptr_to_u64(ptr) ((__u64)((uintptr_t)(ptr)))

static pid_t sys_clone3(struct clone_args *args)
{
	return syscall(__NR_clone3, args, sizeof(struct clone_args));
}

#define CGROUP_CREATE_RETRY "-NNNN"
#define CGROUP_CREATE_RETRY_LEN strlen(CGROUP_CREATE_RETRY)

void foo_init(struct a *b)
{
	*b = (struct a) {
		.n = 2,
	};
}

struct seccomp_data {
	int nr;
	__u32 arch;
	__u64 instruction_pointer;
	__u64 args[6];
};

struct seccomp_notif {
	__u64 id;
	__u32 pid;
	__u32 flags;
	struct seccomp_data data;
};

struct monitor_netlink_header {
        /* "libudev" prefix to distinguish libudev and kernel messages */
        char prefix[8];
        /* Magic to protect against daemon <-> Library message format mismatch
         * Used in the kernel from socket filter rules; needs to be stored in network order */
        unsigned magic;
        /* Total length of header structure known to the sender */
        unsigned header_size;
        /* Properties string buffer */
        unsigned properties_off;
        unsigned properties_len;
        /* Hashes of primary device properties strings, to let libudev subscribers
         * use in-kernel socket filters; values need to be stored in network order */
        unsigned filter_subsystem_hash;
        unsigned filter_devtype_hash;
        unsigned filter_tag_bloom_hi;
        unsigned filter_tag_bloom_lo;
};

int main(void)
{
	uid_t ruid = -1, euid = -1, suid = -1, fsuid = -1;
	gid_t rgid = -1, egid = -1, sgid = -1, fsgid = -1;

	if (getresuid(&ruid, &euid, &suid) || getresgid(&rgid, &egid, &sgid))
		exit(1);

	fsuid = setfsuid(-1);
	fsgid = setfsgid(-1);

	printf("ruid:  %d\n", ruid);
	printf("rgid:  %d\n", rgid);
	printf("euid:  %d\n", euid);
	printf("egid:  %d\n", egid);
	printf("suid:  %d\n", suid);
	printf("sgid:  %d\n", sgid);
	printf("fsuid: %d\n", fsuid);
	printf("fsgid: %d\n", fsgid);

	printf("%d\n", getpid());
	if (unshare(CLONE_NEWUSER))
		exit(2);

	raise(SIGSTOP);

	printf("%d = setuid(0)\n", setuid(0));
	printf("%d = setgid(0)\n", setgid(0));
	printf("%d = setfsuid(0)\n", setfsuid(0));
	printf("%d = setfsgid(0)\n", setfsgid(0));
	printf("%d = setfsuid(-1)\n", setfsuid(-1));
	printf("%d = setfsgid(-1)\n", setfsgid(-1));

	execlp("bash", "bash", "-i", (char *)NULL);
	exit(EXIT_SUCCESS);
}
