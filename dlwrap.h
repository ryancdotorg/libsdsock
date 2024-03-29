#pragma once

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

// NAME calls _impl_NAME
//
// _impl_NAME initially points to _wrap_NAME
//
// _wrap_NAME tries to save wrap_NAME to _impl_NAME, falling back to NAME,
// then calls _impl_NAME
//
// wrap_NAME is our wrapper function
//
// _real_NAME initially points to _load_NAME
//
// _load_NAME saves the real function as _real_NAME then calls _real_NAME
#define DLWRAP(NAME, TYPE, ARGS, ARGN) \
TYPE wrap_##NAME ARGS; \
static TYPE _load_##NAME ARGS; \
static TYPE _wrap_##NAME ARGS; \
static TYPE (*_real_##NAME)ARGS = _load_##NAME; \
static TYPE (*_impl_##NAME)ARGS = _wrap_##NAME; \
static TYPE _wrap_##NAME ARGS { \
  _impl_##NAME = dlsym(RTLD_DEFAULT, "wrap_" #NAME); \
  if (_impl_##NAME == NULL) _impl_##NAME = dlsym(RTLD_NEXT, #NAME); \
  return _impl_##NAME ARGN; \
} \
static TYPE _load_##NAME ARGS { \
  _real_##NAME = dlsym(RTLD_NEXT, #NAME); \
  if (_real_##NAME == NULL) { \
    fprintf(stderr, "libsdsock: " #NAME " function not found, aborting\n"); \
    abort(); \
  } \
  return _real_##NAME ARGN; \
} \
TYPE NAME ARGS { return _impl_##NAME ARGN; }
