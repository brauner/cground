#ifndef _IFADDRS_H
#define _IFADDRS_H

#include <linux/types.h>

struct ifaddrs {
	struct ifaddrs *ifa_next;
	char *ifa_name;
	unsigned ifa_flags;
	struct sockaddr *ifa_addr;
	struct sockaddr *ifa_netmask;
	union {
		struct sockaddr *ifu_broadaddr;
		struct sockaddr *ifu_dstaddr;
	} ifa_ifu;
	void *ifa_data;
};
#define ifa_broadaddr ifa_ifu.ifu_broadaddr
#define ifa_dstaddr ifa_ifu.ifu_dstaddr

extern void freeifaddrs(struct ifaddrs *);
extern int getifaddrs(struct ifaddrs **);
extern int netns_getifaddrs(struct ifaddrs **, __s32);

#endif /* _IFADDRS_H */
