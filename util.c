#include "util.h"

#include <stdio.h>
#include <stdarg.h>

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
