#include "util.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <limits.h>
#include <pwd.h>
#include <unistd.h>

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

bool strtoi(char *s, int *result) {
  char *nptr;
  long val = strtol(s, &nptr, 10);
  if (*nptr == '\0') {
    *result = (int) val;
    return true;
  }
  return false;
}

char *abspath(const char *path) {
  char buf[PATH_MAX];
  if (*path == '~') {
    strcpy(buf, homedir());
    strcat(buf, path + 1);
  } else if (*path != '/') {
    getcwd(buf, sizeof(buf));
    strcat(buf, "/");
    strcat(buf, path);
  } else {
    strcpy(buf, path);
  }

  char *result = xmalloc(strlen(buf) + 1);
  strcpy(result, "/");

  if (!strcmp(path, "/")) {
    return result;
  }

  char *p = result;
  char *prevslash = result;
  char *component = strtok(buf, "/");
  do {
    if (!strcmp(component, "") ||
        !strcmp(component, ".")) {
      continue;
    }
    if (!strcmp(component, "..")) {
      p = prevslash;
      prevslash = result;
      if (p != result) {
        *p = '\0';
        prevslash = strrchr(result, '/');
      }
      continue;
    }
    prevslash = p;
    *p = '/';
    size_t len = strlen(component);
    memcpy(p + 1, component, len);
    p += len + 1;
    *p = '\0';
  } while ((component = strtok(NULL, "/")));

  p[1] = '\0';
  return result;
}

const char *relpath(const char *path, const char *start) {
  size_t startlen = strlen(start);
  assert(start[startlen - 1] != '/');
  if (!strncmp(path, start, startlen)) {
    return path + startlen + 1;
  }
  return path;
}

const char *homedir(void) {
  const char *home = getenv("HOME");
  return home ? home : getpwuid(getuid())->pw_dir;
}

struct region *region_set(struct region *region, size_t start, size_t end) {
  region->start = min(start, end);
  region->end = max(start, end);
  return region;
}

struct region *region_create(size_t start, size_t end) {
  return region_set(xmalloc(sizeof(struct region)), start, end);
}
