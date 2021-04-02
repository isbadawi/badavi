#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include <sys/queue.h>

#include "attrs.h"

#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))

// TAILQ_FOREACH_SAFE and TAILQ_FOREACH_REVERSE_SAFE, which allow elements to
// be removed or freed during the loop, are available on BSDs but not in glibc,
// so shim them here. The implementation is copied from sys/queue.h on macOS.

#ifndef TAILQ_FOREACH_SAFE
#define TAILQ_FOREACH_SAFE(var, head, field, tvar)                      \
  for ((var) = TAILQ_FIRST((head));                               \
      (var) && ((tvar) = TAILQ_NEXT((var), field), 1);            \
      (var) = (tvar))
#endif

#ifndef TAILQ_FOREACH_REVERSE_SAFE
#define TAILQ_FOREACH_REVERSE_SAFE(var, head, headname, field, tvar)    \
  for ((var) = TAILQ_LAST((head), headname);                      \
      (var) && ((tvar) = TAILQ_PREV((var), headname, field), 1);  \
      (var) = (tvar))
#endif

ATTR_PRINTFLIKE(1, 2)
void debug(const char *format, ...);

void *xmalloc(size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *s);
bool strtoi(char *s, int *result);

// Returns absolute path, with unnecessary //, ./ or ../ removed.
// Also expands a leading ~ to the home directory.
// Unlike realpath(3), does not resolve symlinks.
char *abspath(const char *path);
const char *relpath(const char *path, const char *start);
const char *homedir(void);

struct region {
  size_t start;
  size_t end;
};

struct region *region_create(size_t start, size_t end);
struct region *region_set(struct region *region, size_t start, size_t end);

