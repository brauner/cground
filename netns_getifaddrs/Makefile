DEPS=netns_getifaddrs.h print_getifaddrs.h
CFLAGS=-fpic

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

all: getifaddrs_perf

getifaddrs_perf: getifaddrs_perf.o netns_getifaddrs.o print_getifaddrs.o
	$(CC) -o getifaddrs_perf netns_getifaddrs.o print_getifaddrs.o getifaddrs_perf.o

clean:
	rm -f getifaddrs_perf *.o

