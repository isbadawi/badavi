#pragma once

#include <stdio.h>
#include <stddef.h>

#include "attrs.h"

#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))

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
