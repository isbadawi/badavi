#include "util.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void debug(const char *format, ...) {
  static FILE *debug_fp = NULL;
  if (!debug_fp) {
    time_t timestamp = time(0);
    struct tm *now = localtime(&timestamp);
    char name[64];
    strftime(name, sizeof(name), "/tmp/badavi_log.%Y%m%d-%H%M%S.txt", now);
    debug_fp = fopen(name, "w");
    assert(debug_fp);
  }

  va_list args;
  va_start(args, format);
  vfprintf(debug_fp, format, args);
  va_end(args);
  fflush(debug_fp);
}

void *xmalloc(size_t size) {
  void *mem = malloc(size);
  if (!mem) {
    fprintf(stderr, "failed to allocate memory (%zu bytes)", size);
    abort();
  }
  return mem;
}

void *xrealloc(void *ptr, size_t size) {
  void *mem = realloc(ptr, size);
  if (size && !mem) {
    fprintf(stderr, "failed to allocate memory (%zu bytes)", size);
    abort();
  }
  return mem;
}

char *xstrdup(const char *s) {
  char *copy = strdup(s);
  if (!copy) {
    fprintf(stderr, "failed to allocate memory (%zu bytes)", strlen(s) + 1);
    abort();
  }
  return copy;
}


struct region_t *region_set(struct region_t *region, size_t start, size_t end) {
  region->start = min(start, end);
  region->end = max(start, end);
  return region;
}

struct region_t *region_create(size_t start, size_t end) {
  return region_set(xmalloc(sizeof(struct region_t)), start, end);
}
