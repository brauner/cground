#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>

#include <ifaddrs.h>

extern void print_ifaddrs(struct ifaddrs *ifaddrs_ptr);
