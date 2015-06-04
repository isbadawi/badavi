#pragma once

#include <stdio.h>
#include <stddef.h>

#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))

void debug_init(void);
FILE *debug_fp(void);
__attribute__((__format__(__printf__, 1, 2)))
void debug(const char *format, ...);

void *xmalloc(size_t size);
void *xrealloc(void *ptr, size_t size);
