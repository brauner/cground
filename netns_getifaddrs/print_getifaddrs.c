#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>

#include "netns_getifaddrs.h"

static void print_ip(const char *name, struct ifaddrs *ifaddrs_ptr, void *addr_ptr)
{
	if (addr_ptr) {
		/* This constant is defined in <netinet/in.h> */
		char address[INET6_ADDRSTRLEN];
		inet_ntop(ifaddrs_ptr->ifa_addr->sa_family, addr_ptr, address,
			  sizeof(address));
		printf("%s: %s\n", name, address);
	} else {
		printf("No %s\n", name);
	}
}

/* Get a pointer to the address structure from a sockaddr. */
static void *get_addr_ptr(struct sockaddr *sockaddr_ptr)
{
	void *addr_ptr = 0;
	if (sockaddr_ptr->sa_family == AF_INET)
		addr_ptr = &((struct sockaddr_in *)sockaddr_ptr)->sin_addr;
	else if (sockaddr_ptr->sa_family == AF_INET6)
		addr_ptr = &((struct sockaddr_in6 *)sockaddr_ptr)->sin6_addr;
	return addr_ptr;
}

/* Print the internet address. */
static void print_internet_address(struct ifaddrs *ifaddrs_ptr)
{
	void *addr_ptr;
	if (!ifaddrs_ptr->ifa_addr)
		return;
	addr_ptr = get_addr_ptr(ifaddrs_ptr->ifa_addr);
	print_ip("internet address", ifaddrs_ptr, addr_ptr);
}

/* Print the netmask. */
static void print_netmask(struct ifaddrs *ifaddrs_ptr)
{
	void *addr_ptr;
	if (!ifaddrs_ptr->ifa_netmask)
		return;
	addr_ptr = get_addr_ptr(ifaddrs_ptr->ifa_netmask);
	print_ip("netmask", ifaddrs_ptr, addr_ptr);
}

static void print_internet_interface(struct ifaddrs *ifaddrs_ptr)
{
	print_internet_address(ifaddrs_ptr);
	print_netmask(ifaddrs_ptr);
	if (ifaddrs_ptr->ifa_dstaddr) {
		void *addr_ptr = get_addr_ptr(ifaddrs_ptr->ifa_dstaddr);
		print_ip("destination", ifaddrs_ptr, addr_ptr);
	}
	if (ifaddrs_ptr->ifa_broadaddr) {
		void *addr_ptr = get_addr_ptr(ifaddrs_ptr->ifa_broadaddr);
		print_ip("broadcast", ifaddrs_ptr, addr_ptr);
	}
}

void print_ifaddrs(struct ifaddrs *ifaddrs_ptr)
{
	struct ifaddrs *ifa_next;

	/* Print this one. */
	printf("Name: %s\n"
	       "flags: %x\n",
	       ifaddrs_ptr->ifa_name, ifaddrs_ptr->ifa_flags);
	if (ifaddrs_ptr->ifa_addr->sa_family == AF_INET) {
		/* AF_INET is defined in /usr/include/sys/socket.h. */

		printf("AF_INET\n");
		print_internet_interface(ifaddrs_ptr);
		printf("\n");
	} else if (ifaddrs_ptr->ifa_addr->sa_family == AF_INET6) {
		printf("AF_INET6\n");
		print_internet_interface(ifaddrs_ptr);
		printf("\n");
	}
	/* Do the next one. */
	ifa_next = ifaddrs_ptr->ifa_next;
	if (!ifa_next)
		return;
	print_ifaddrs(ifa_next);
}
