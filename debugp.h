#pragma once

#ifndef NDEBUG
#define debugp(...) _debugp(__FILE__, __func__, __LINE__, __VA_ARGS__)
static void _debugp(const char *file, const char *func, unsigned int line, const char *fmt, ...) {
  char f[256];
  va_list args;
  va_start(args, fmt);
  size_t n = strnlen(fmt, sizeof(f) - 1);
  strncpy(f, fmt, n);
  // remove newline from format string
  f[n-(f[n-1] == '\n' ? 1 : 0)] = 0;
  fprintf(stderr, "%s(%s:%u,%d): ", file, func, line, errno);
  vfprintf(stderr, f, args);
  fprintf(stderr, "\n");
}
#else
#define debugp(...) do {} while (0)
#endif
