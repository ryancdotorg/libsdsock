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

#include <systemd/sd-daemon.h>

#include "debugp.h"
#include "dlwrap.h"

#define dbgfd(sockfd, sa) \
do { \
  int val = -2; socklen_t len = sizeof(val); \
  getsockopt(sockfd, SOL_SOCKET, SO_ACCEPTCONN, &val, &len); \
  char _desc[256]; \
  debugp("dbgfd %s (%d) %d", fd_ntop(_desc, sizeof(_desc), sockfd, sa), sockfd, val); \
} while (0)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
DLWRAP(bind, int, (int a, const struct sockaddr *b, socklen_t c), (a, b, c));
DLWRAP(listen, int, (int a, int b), (a, b));
DLWRAP(close, int, (int a), (a));
DLWRAP(close_range, int, (unsigned int a, unsigned int b, unsigned int c), (a, b, c));
#pragma GCC diagnostic pop

char * fd_ntop(char *dst, socklen_t size, int sockfd, const struct sockaddr *sa);

typedef struct sockmap_s {
  const char *desc;
  const char *name;
  struct sockmap_s *next;
} sockmap_t;

static sockmap_t *sockmap = NULL;

static int setmap(const char *desc, const char *name) {
  sockmap_t *prev = NULL, *curr = sockmap;

  // find the last entry in the list
  while (curr != NULL) { prev = curr; curr = curr->next; }

  // allocate
  curr = malloc(sizeof(sockmap_t));
  if (curr == NULL) { errno = ENOMEM; return -1; }

  // initialize
  prev == NULL ? (sockmap = curr) : (prev->next = curr);
  curr->next = NULL; curr->desc = desc; curr->name = name;

  return 0;
}

static const char * getmapname(const char *desc) {
  for (sockmap_t *curr = sockmap; curr != NULL; curr = curr->next) {
    if (strcmp(curr->desc, desc) == 0) return curr->name;
  }
  return NULL;
}

static char **sd_names = NULL;
static const int sd_min_fd = SD_LISTEN_FDS_START;
static int sd_last_fd = -1;

static int sd_sock_fd(const char *name) {
  for (int i = 0, fd = sd_min_fd; fd <= sd_last_fd; ++i, ++fd) {
    if (strcmp(name, sd_names[i]) == 0) return fd;
  }
  return -1;
}

static char dsock_ready = 0;

// this will be run when the library is loaded
static void __attribute__((constructor)) dsock_init() {
  if (dsock_ready) return;
  debugp("dsock_init()\n");

  // try to populate the socket mapping
  char *s = getenv("LIBSDSOCK_MAP");
  while (s != NULL && s[0] != '\0') {
    char *desc, *name, *p;
    size_t n;

    // get pointer to equals
    if ((p = strchr(s, '=')) == NULL) { debugp("Bad SOCKMAP"); exit(-1); }
    if ((desc = malloc((n = p - s) + 1)) == NULL) { debugp("desc malloc"); exit(-1); }
    memcpy(desc, s, n); desc[n] = '\0';

    // advance pointer
    s = p + 1;

    // get pointer to comma or end of string
    if ((p = strchr(s, ',')) == NULL) p = strchr(s, '\0');
    if ((name = malloc((n = p - s) + 1)) == NULL) { debugp("name malloc"); exit(-1); }
    memcpy(name, s, n); name[n] = '\0';

    setmap(desc, name);

    if ((s = p)[0] == ',') ++s;
  }

  int n = sd_listen_fds_with_names(1, &sd_names);
  if (n < 0) {
    debugp("sd_listen_fds error: %s\n", strerror(-n));
  } else {
    sd_last_fd = (sd_min_fd + n) - 1;
  }

  dsock_ready = 1;
}

// try to swap in a socket from systemd
// return values:
// * error: < 0
// * continue: 0
// * success: > 0
static int sd_dup(int sockfd, const struct sockaddr *addr) {
  int fd, ret = 0;

  char desc[256];
  if (fd_ntop(desc, sizeof(desc), sockfd, addr) != NULL) {
    const char *name = getmapname(desc);
    if (name != NULL) {
      if ((fd = sd_sock_fd(name)) >= sd_min_fd) {
        ret = dup2(fd, sockfd);
        if (ret >= 0) ret = fd;
      } else {
        debugp("mapped socket not found for `%s`", desc);
        errno = EOPNOTSUPP;
        ret = -1;
      }
    }
  }

  if (ret < 0) close(sockfd);

  return ret;
}

int wrap_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
  int ret;

  ret = sd_dup(sockfd, addr);
  debugp(
    "sd_dup(%d, ...) => %d (%s)",
    sockfd, ret,
    ret > 0 ? sd_names[ret-sd_min_fd] : "null"
  );
  if (ret < 0) { return ret; } else if (ret > 0) { return 0; }

  debugp("bind(%d, ...)", sockfd);
  ret = _real_bind(sockfd, addr, addrlen);
  if (ret != 0) debugp("bind: %s", strerror(errno));
  return ret;
}

// prevent trying to listen again on already listening sockets
int wrap_listen(int sockfd, int backlog) {
  int val; socklen_t len = sizeof(val);
  if (getsockopt(sockfd, SOL_SOCKET, SO_ACCEPTCONN, &val, &len) == 0 && val) {
    debugp("ignoring listen(%d, ...)", sockfd);
    return 0;
  }

  debugp("listen(%d, %d)", sockfd, backlog);
  return _real_listen(sockfd, backlog);
}

// prevent the application from closing the systemd sockets
int wrap_close(int sockfd) {
  if (sd_last_fd >= sd_min_fd && sockfd >= sd_min_fd && sockfd <= sd_last_fd) {
    debugp("ignoring close(%d)", sockfd);
    return 0;
  }

  return _real_close(sockfd);
}

// prevent the application from closing range that includes the systemd sockets
int wrap_close_range(unsigned int first, unsigned int last, unsigned int flags) {
  // the compiler will remove this if SD_LISTEN_FDS_START is non-negative
  if (sd_min_fd < 0) {
    return _real_close_range(first, last, flags);
  }

  // checked above
  unsigned int ex_first = (unsigned int)sd_min_fd;
  // checked below before use
  unsigned int ex_last = (unsigned int)sd_last_fd;

  // ranges are invalid or do not intersect
  if (sd_min_fd > sd_last_fd || first > last || last < ex_first || first > ex_last) {
    return _real_close_range(first, last, flags);
  }

  // close any file descriptors before systemd range
  if (first < ex_first) {
    debugp(
        "calling close_range(%u, %u, %u)"
        " instead of close_range(%u, %u, %u)",
        first, ex_first - 1, flags,
        first, last, flags
    );
    if (_real_close_range(first, ex_first - 1, flags) != 0) {
      debugp("close_range: %s", strerror(errno));
      return -1;
    }
  }

  // close any file descriptors after systemd range
  if (last > ex_last) {
    debugp(
        "calling close_range(%u, %u, %u)"
        " instead of close_range(%u, %u, %u)",
        ex_last + 1, last, flags,
        first, last, flags
    );
    if (_real_close_range(ex_last + 1, last, flags) != 0) {
      debugp("close_range: %s", strerror(errno));
      return -1;
    }
  }

  // range entirely inside - compiler should remove this if NDEBUG is defined
  if (ex_first <= first && last <= ex_last) {
    debugp("ignoring close_range(%u, %u, %u)", first, last, flags);
  }

  return 0;
}
