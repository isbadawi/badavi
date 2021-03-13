#pragma once

#include <stdio.h>
#include <stddef.h>

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

struct region {
  size_t start;
  size_t end;
};

struct region *region_create(size_t start, size_t end);
struct region *region_set(struct region *region, size_t start, size_t end);
