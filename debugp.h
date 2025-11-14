#pragma once

#ifndef NDEBUG
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#define debugp(...) _debugp(__FILE__, __func__, __LINE__, __VA_ARGS__)
static void _debugp(const char *file, const char *func, unsigned int line, const char *fmt, ...) {
  char f[256];
  va_list args;
  va_start(args, fmt);

  // treat fmt as an empty string if the pointer is null
  size_t n = fmt == NULL ? 0 : strnlen(fmt, sizeof(f));
  // check if fmt with null terminator will fit in f
  if (sizeof(f) == n) {
    fprintf(stderr, "%s(%s:%u,%d): bad format, aborting\n", file, func, line, errno);
    abort();
  } else if (n > 0) {
    strcpy(f, fmt);
    // remove newline from format string and print the debug message
    f[n-(f[n-1] == '\n' ? 1 : 0)] = 0;
    fprintf(stderr, "%s(%s:%u,%d): ", file, func, line, errno);
    vfprintf(stderr, f, args);
    fprintf(stderr, "\n");
  } else {
    // no format string, we'll just print the call site and errno
    fprintf(stderr, "%s(%s:%u,%d)\n", file, func, line, errno);
  }

  va_end(args);
}
#else
#define debugp(...) do {} while (0)
#endif
