#include "util.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *DEBUG_FP;

void debug_init(void) {
  DEBUG_FP = fopen("log.txt", "w");
}

FILE *debug_fp(void) {
  return DEBUG_FP;
}

void debug(const char *format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(DEBUG_FP, format, args);
  va_end(args);
  fflush(DEBUG_FP);
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
