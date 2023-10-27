#!/bin/sh

exec systemd-socket-activate -l 127.0.0.1:1080 --fdname 'jump' \
  -E LD_PRELOAD=/usr/local/lib/libsdsock.so \
  -E LIBSDSOCK_MAP=tcp://127.0.0.1:1080=jump \
  ssh -o ExitOnForwardFailure=yes -nNTD 127.0.0.1:1080 jump@<server>
