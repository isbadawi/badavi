#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

char *read_file(char *filename) {
  FILE *fp = fopen(filename, "r");
  if (!fp) {
    return NULL;
  }

  struct stat info;
  fstat(fileno(fp), &info);
  char *buf = malloc(info.st_size + 1);
  fread(buf, 1, info.st_size, fp);
  buf[info.st_size] = '\0';

  fclose(fp);
  return buf;
}

int split_lines(char *buf) {
  int nlines = 0;
  while (*buf) {
    if (*buf == '\n') {
      nlines++;
      *buf = '\0';
    }
    buf++;
  }
  return nlines;
}
