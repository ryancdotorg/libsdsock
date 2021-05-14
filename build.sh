#!/bin/sh
gcc -O2 -ggdb -Wall -Wextra --std=gnu11 \
	-Wno-unused-function \
	norm.c addr.c \
	-o norm
gcc -O2 -ggdb -shared -Wall -Wextra --std=gnu11 -fPIC \
	-Wno-unused-function \
	sdsock.c addr.c \
	-lsystemd -ldl \
	-o libsdsock.so
