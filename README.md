# libsdsock

## Description

libsdsock is an LD_PRELOAD hack that allows (or at least tries to...) many
pieces of software that listen for network connections to be started on demand
via systemd’s “Socket Activation” feature. TCP, UDP, Unix stream sockets are
supported.

The motivating use case was to have SSH dynamic port forwarding that starts
(and restarts) on demand. Examples ([jump.socket](jump.socket),
[jump.service](jump.service) and [jump.sh](jump.sh)) are provided for that,
though the tool is generic.

## Getting Started

### Dependencies

* libsystemd development files
* libdl development files
* gcc

### Installation

```
git clone https://github.com/ryancdotorg/libsdsock.git
cd libsdsock
make
make install
```

Run as root (`sudo make install`) to install system-wide.

### Usage

* Create systemd .socket and .service files as required (see examples)
* Use `systemctl enable` on the .socket unit - this is *not* required for the
  corresponding .service unit.
* Use `systemctl start` on the .socket unit - again, this is *not* required
  for the corresponding .service unit.

### Debugging

Run `make debug` to build libsdsock-debug.so version, it will print debug
infomation to stderr.

## Help

You can file an issue on GitHub, however I may not respond. This software is
being provided without warranty in the hopes that it may be useful.

## Security

This software has not been independently audited and may contain bugs. That
said, the exposure surface of the code is minimal, and no untrusted data is
processed. The only functions shimmed are `bind`, `listen`, `close`,
`closefrom` and `close_range`.

## To Do

Tests would probably be a good idea.

## Authors

[Ryan Castellucci](https://rya.nc/about.html) ([@ryancdotorg](https://github.com/ryancdotorg)),
Ivan ([@vanym](https://github.com/vanym))

## License

It’s CC0, do what you want.
