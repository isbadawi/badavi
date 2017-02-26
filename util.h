#pragma once

#include <stdio.h>
#include <stddef.h>

#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))

#ifndef __unused
#define __unused __attribute__((unused))
#endif

__attribute__((__format__(__printf__, 1, 2)))
void debug(const char *format, ...);

void *xmalloc(size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *s);

struct region_t {
  size_t start;
  size_t end;
};

struct region_t *region_create(size_t start, size_t end);
struct region_t *region_set(struct region_t *region, size_t start, size_t end);
