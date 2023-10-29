# libsdsock

ifeq ($(shell id -u),0)
	PREFIX?=/usr/local
	INSTALL=install -D
else
	PREFIX?=$(HOME)/.local
	INSTALL=mkdir -p $(LIBDIR) && cp
endif

LIBDIR=$(PREFIX)/lib

CC=gcc
CFLAGS=-O2 --std=gnu11 -Wall -Wextra -Wformat -Wformat-signedness -pedantic -Wno-unused-function
SOFLAGS=-shared -fPIC
LIBS=-lsystemd -ldl

lib: libsdsock.so

debug: libsdsock-debug.so norm

all: lib debug

libsdsock.so: sdsock.c addr.c
	$(CC) $(CFLAGS) $(SOFLAGS) -s -DNDEBUG $^ $(LIBS) -o $@

libsdsock-debug.so: sdsock.c addr.c
	$(CC) $(CFLAGS) $(SOFLAGS) -ggdb $^ $(LIBS) -o $@

norm: norm.c addr.c
	$(CC) $(CFLAGS) -ggdb $^ -o $@

install: libsdsock.so
	$(INSTALL) libsdsock.so $(DESTDIR)$(LIBDIR)/libsdsock.so

install-debug: libsdsock-debug.so
	$(INSTALL) libsdsock-debug.so $(DESTDIR)$(LIBDIR)/libsdsock.so

uninstall:
	$(RM) $(DESTDIR)$(LIBDIR)/libsdsock.so

.PHONY: lib debug all install install-debug uninstall clean _clean _nop

# hack to force clean to run first *to completion* even for parallel builds
# note that $(info ...) prints everything on one line
clean: _nop $(foreach _,$(filter clean,$(MAKECMDGOALS)),$(info $(shell $(MAKE) _clean)))
_clean:
	$(RM) libsdsock.so libsdsock-debug.so norm
_nop:
	@true
