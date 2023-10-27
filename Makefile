PREFIX?=/usr/local
LIBDIR=$(PREFIX)/lib

CC=gcc
CFLAGS=-O2 --std=gnu11 -Wall -Wextra -Wno-unused-function

lib: libsdsock.so

debug: libsdsock-debug.so norm

all: lib debug

libsdsock.so:
	$(CC) $(CFLAGS) -s -DNDEBUG -shared -fPIC sdsock.c addr.c -lsystemd -ldl -o libsdsock.so

libsdsock-debug.so:
	$(CC) $(CFLAGS) -ggdb -shared -fPIC sdsock.c addr.c -lsystemd -ldl -o libsdsock-debug.so

norm:
	$(CC) $(CFLAGS) -ggdb norm.c addr.c -o norm

install: libsdsock.so
	install -Dm755 libsdsock.so $(DESTDIR)$(LIBDIR)/libsdsock.so

install-debug: libsdsock-debug.so
	install -Dm755 libsdsock-debug.so $(DESTDIR)$(LIBDIR)/libsdsock.so

uninstall:
	$(RM) $(DESTDIR)$(LIBDIR)/libsdsock.so

clean:
	$(RM) libsdsock.so libsdsock-debug.so norm

.PHONY: lib debug all install install-debug uninstall clean
