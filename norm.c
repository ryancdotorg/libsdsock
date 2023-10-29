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

#include <sys/socket.h>

struct addrinfo * ai_pton(const char *str);
char * ai_ntop(char *dst, socklen_t size, const struct addrinfo *ai, const struct sockaddr *a);

int main(int argc, char *argv[]) {
  char buf[256];

  if (argc != 2) return -1;

  struct addrinfo *ai = ai_pton(argv[1]);
  if (ai == NULL) {
    printf("INVALID %s\n", argv[1]);
    return 2;
  }
  ai_ntop(buf, sizeof(buf), ai, NULL);
  printf("OKAY %s\n", buf);

  return 0;
}
