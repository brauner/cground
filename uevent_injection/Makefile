CC=clang
DEPS=nl.h
CFLAGS=-fpic

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

all: uevent_listen uevent_send

uevent_listen: uevent_listen.o nl.o
	gcc -o uevent_listen uevent_listen.o nl.o

uevent_send: uevent_send.o nl.o
	gcc -o uevent_send uevent_send.o nl.o

clean:
	rm -f uevent_listen uevent_send *.o
