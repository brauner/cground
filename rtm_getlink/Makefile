CC=clang
DEPS=nl.h
CFLAGS=-fpic

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

all: rtm_getlink

rtm_getlink: rtm_getlink.o nl.o
	gcc -o rtm_getlink rtm_getlink.o nl.o

clean:
	rm -f rtm_getlink *.o
