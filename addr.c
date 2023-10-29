#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dlfcn.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

#include "debugp.h"

#define US(X) (_Generic((X), \
  char: (unsigned char)(X), \
  short: (unsigned short)(X), \
  int: (unsigned int)(X), \
  long int: (unsigned long int)(X), \
  long long int: (unsigned long long int)(X) \
))

#pragma GCC visibility push(internal)

static int parse_port(const char *str) {
  char *end;
  errno = 0;
  unsigned long int x = strtoul(str, &end, 10);
  if (errno == 0 && x < 65536 && str != end && end[0] == '\0') {
    return x;
  }
  return -1;
}

static int getsockinfo(int fd, int *domain, int *type, int *protocol) {
  int ret;
  socklen_t optlen;

  *domain = -1; optlen = sizeof(int);
  ret = getsockopt(fd, SOL_SOCKET, SO_DOMAIN, domain, &optlen);
  if (ret < 0) { return ret; } else if (optlen > sizeof(int)) { return ENOBUFS; }

  *type = -1; optlen = sizeof(int);
  ret = getsockopt(fd, SOL_SOCKET, SO_TYPE, type, &optlen);
  if (ret < 0) { return ret; } else if (optlen > sizeof(int)) { return ENOBUFS; }

  *protocol = -1; optlen = sizeof(int);
  ret = getsockopt(fd, SOL_SOCKET, SO_PROTOCOL, protocol, &optlen);
  if (ret < 0) { return ret; } else if (optlen > sizeof(int)) { return ENOBUFS; }

  return 0;
}

// allocate a new addrinfo struct
static struct addrinfo * newaddrinfo() {
  struct addrinfo *ai = malloc(sizeof(struct addrinfo));
  if (ai == NULL) return NULL;

  ai->ai_canonname = NULL;
  ai->ai_next = NULL;

  // allocate sockaddr
  ai->ai_addrlen = sizeof(struct sockaddr_storage);
  if ((ai->ai_addr = malloc(ai->ai_addrlen)) == NULL) {
    free(ai);
    return NULL;
  }

  return ai;
}

// get addrinfo struct based on file descriptor
int fdaddrinfo(int fd, const struct sockaddr *sa, struct addrinfo **res) {
  int err;

  struct addrinfo *ai = newaddrinfo();
  if ((*res = ai) == NULL) { errno = ENOMEM; return -1; }

  if (sa != NULL) {
    // copy the supplied sockaddr
    memcpy(ai->ai_addr, sa, ai->ai_addrlen);
  } else {
    // populate sockaddr from fd
    if ((err = getsockname(fd, ai->ai_addr, &(ai->ai_addrlen))) != 0) {
      free(ai->ai_addr); free(ai);
      *res = NULL;
      return err;
    }
  }

  // populate other fields
  if ((err = getsockinfo(fd, &(ai->ai_family), &(ai->ai_socktype), &(ai->ai_protocol))) != 0) {
    free(ai->ai_addr); free(ai);
    *res = NULL;
    return err;
  }

  return 0;
}

// like inet_pton, but with protocol and port
struct addrinfo * ai_pton(const char *str) {
  struct addrinfo *ai = newaddrinfo();
  if (ai == NULL) { errno = ENOMEM; return NULL; }
  struct sockaddr *sa = ai->ai_addr;
  if (sa == NULL) { errno = ENOMEM; goto ai_pton_fail; }

  const char *sep = strstr(str, "://");
  if (sep == NULL) return NULL;
  const char *proto = str;
  size_t proto_sz = sep - str;
  const char *port, *addr = sep + 3;
  char ip[64];

  if (proto_sz == 4 && memcmp(proto, "unix", 4) == 0) {
    ai->ai_family = sa->sa_family = AF_UNIX;
    ai->ai_socktype = SOCK_STREAM;
    ai->ai_protocol = 0;
    struct sockaddr_un *sun = (struct sockaddr_un *)sa;
    const char *path = realpath(addr, NULL);
    if (path == NULL) path = addr;
    size_t n = strnlen(path, sizeof(sun->sun_path));
    // check whether path is too long
    if (n >= sizeof(sun->sun_path)) { errno = ENOBUFS; goto ai_pton_fail; }
    // we just checked buffer size
    strcpy(sun->sun_path, path);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
    if (path != addr) free(path);
#pragma GCC diagnostic pop
  } else if (proto_sz == 3 && (memcmp(proto, "udp", 3) == 0 || memcmp(proto, "tcp", 3) == 0)) {
    int p;
    if (memcmp(proto, "tcp", 3) == 0) {
      ai->ai_socktype = SOCK_STREAM;
      ai->ai_protocol = IPPROTO_TCP;
    } else if (memcmp(proto, "udp", 3) == 0) {
      ai->ai_socktype = SOCK_DGRAM;
      ai->ai_protocol = IPPROTO_UDP;
    }

    if (addr[0] == '[') {
      sep = strstr(addr, "]:");
      if (sep == NULL) { errno = EINVAL; goto ai_pton_fail; }
      // check whether ip string is too long
      size_t n = (sep - addr) - 1;
      if (n > (sizeof(ip)-1)) { errno = EINVAL; goto ai_pton_fail; }
      // we just checked the size
      memcpy(ip, addr+1, n); ip[n] = '\0';
      port = addr + n + 3;

      ai->ai_family = sa->sa_family = AF_INET6;
      struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)sa;
      if (inet_pton(AF_INET6, ip, (void *)&(sin6->sin6_addr)) != 1) {
        errno = EINVAL;
        goto ai_pton_fail;
      }

      if ((p = parse_port(port)) < 0) { errno = EINVAL; goto ai_pton_fail; }
      sin6->sin6_port = htons(p);
    } else {
      sep = strchr(addr, ':');
      if (sep == NULL) { errno = EINVAL; goto ai_pton_fail; }
      // check whether ip string is too long
      size_t n = sep - addr;
      if (n > (sizeof(ip)-1)) { errno = EINVAL; goto ai_pton_fail; }
      // we just checked the size
      memcpy(ip, addr, n); ip[n] = '\0';
      port = addr + n + 1;

      ai->ai_family = sa->sa_family = AF_INET;
      struct sockaddr_in *sin = (struct sockaddr_in *)sa;
      if (inet_pton(AF_INET, ip, (void *)&(sin->sin_addr)) != 1) {
        errno = EINVAL;
        goto ai_pton_fail;
      }

      if ((p = parse_port(port)) < 0) { errno = EINVAL; goto ai_pton_fail; }
      sin->sin_port = htons(p);
    }
  } else {
    errno = EINVAL;
    goto ai_pton_fail;
  }

  return ai;

ai_pton_fail:
  freeaddrinfo(ai);
  return NULL;
}

// like inet_ntop, but with protocol and port
char * ai_ntop(char *dst, socklen_t size, const struct addrinfo *ai) {
  // set up output
  size_t n;
  char *path, *str = dst;
  dst[size-1] = '\0';

  const struct sockaddr *sa = ai->ai_addr;
  if (sa == NULL) return NULL;

  int family = ai->ai_family;
  int socktype = ai->ai_socktype;
  int protocol = ai->ai_protocol;

  if (family != sa->sa_family) {
    snprintf(dst, size, "Address family mismatch: ai %d != sa %d", family, sa->sa_family);
    return NULL;
  }

  if (family == AF_INET || family == AF_INET6) {
    // get the protcol info
    if (socktype == SOCK_DGRAM && (protocol == 0 || protocol == IPPROTO_UDP)) {
      protocol = IPPROTO_UDP;
      n = snprintf(str, size, "udp://");
      str += n; size -= n;
    } else if (socktype == SOCK_STREAM && (protocol == 0 || protocol == IPPROTO_TCP)) {
      protocol = IPPROTO_TCP;
      n = snprintf(str, size, "tcp://");
      str += n; size -= n;
    } else {
      n = snprintf(str, size, "unknown(%d,%d,%d)://", family, socktype, protocol);
      str += n; size -= n;
    }

    // get the ip address and port
    uint16_t port;
    if (family == AF_INET) {
      struct sockaddr_in *sin = (struct sockaddr_in *)sa;
      inet_ntop(AF_INET, &(sin->sin_addr), str, size);
      n = strnlen(str, size);
      str += n; size -= n;
      port = ntohs(sin->sin_port);
    } else if (family == AF_INET6) {
      struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)sa;
      if (size > 1) { str[0] = '[';  ++str; --size; }
      inet_ntop(AF_INET6, &(sin6->sin6_addr), str, size);
      n = strnlen(str, size);
      str += n; size -= n;
      if (size > 1) { str[0] = ']';  ++str; --size; }
      port = ntohs(sin6->sin6_port);
    }

    if (protocol == IPPROTO_TCP || protocol == IPPROTO_UDP) {
      snprintf(str, size, ":%u", port);
    }
  } else if (family == AF_UNIX) {
    if (socktype == SOCK_STREAM) {
      struct sockaddr_un *sun = (struct sockaddr_un *)sa;
      n = snprintf(str, size, "unix://");
      str += n; size -= n;
      path = realpath((const char *)&(sun->sun_path), NULL);
      if (path == NULL) path = (char *)&(sun->sun_path);
      strncpy(str, path, size);
      if (path != (char *)&(sun->sun_path)) free(path);
    } else {
      snprintf(dst, size, "Unknown unix socket type %d (0x%4x)", socktype, US(socktype));
      return NULL;
    }
  } else {
    snprintf(dst, size, "Unknown address family %d (0x%4x)", family, US(family));
    return NULL;
  }

  // detect buffer out of space
  if (str[size-1] != '\0') { str[size-1] = '\0'; return NULL; }
  return dst;
}

// like inet_ntop, but with protocol and port
char * fd_ntop(char *dst, socklen_t size, int sockfd, const struct sockaddr *sa) {
  int err;
  struct addrinfo *ai;
  if ((err = fdaddrinfo(sockfd, sa, &ai)) != 0) return NULL;
  char *ret = ai_ntop(dst, size, ai);
  freeaddrinfo(ai);
  return ret;
}
#pragma GCC visibility pop
