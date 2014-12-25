#include "gap.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "util.h"

#define GAPSIZE 1024

gapbuf_t *gb_create(void) {
  gapbuf_t *gb = malloc(sizeof(*gb));
  if (!gb) {
    return NULL;
  }

  gb->bufstart = malloc(GAPSIZE);
  if (!gb->bufstart) {
    free(gb);
    return NULL;
  }

  gb->gapstart = gb->bufstart;
  gb->gapend = gb->bufend = gb->bufstart + GAPSIZE;
  return gb;
}

gapbuf_t *gb_load(FILE *fp) {
  gapbuf_t *gb = malloc(sizeof(*gb));
  if (!gb) {
    return NULL;
  }

  struct stat info;
  fstat(fileno(fp), &info);
  int filesize = info.st_size;
  int bufsize = filesize + GAPSIZE;

  gb->bufstart = malloc(bufsize);
  if (!gb->bufstart) {
    free(gb);
    return NULL;
  }

  gb->gapend = gb->bufstart + GAPSIZE;
  gb->bufend = gb->bufstart + bufsize;

  fread(gb->gapend, 1, filesize, fp);
  return gb;
}

int gb_size(gapbuf_t *gb) {
  return (gb->gapstart - gb->bufstart) + (gb->bufend - gb->gapend);
}

void gb_save(gapbuf_t *gb, FILE *fp) {
  fwrite(gb->bufstart, 1, gb->gapstart - gb->bufstart, fp);
  fwrite(gb->gapend, 1, gb->bufend - gb->gapend, fp);
}

// Returns the real index of the logical offset pos.
static int gb_index(gapbuf_t *gb, int pos) {
  int gapsize = gb->gapend - gb->gapstart;
  return gb->bufstart + pos < gb->gapstart ? pos : pos + gapsize;
}

char gb_getchar(gapbuf_t *gb, int pos) {
  return gb->bufstart[gb_index(gb, pos)];
}

// Moves the gap so that gb->bufstart + pos == gb->gapstart.
static void gb_mvgap(gapbuf_t *gb, int pos) {
  char *point = gb->bufstart + gb_index(gb, pos);
  if (gb->gapend <= point) {
    int n = point - gb->gapend;
    memcpy(gb->gapstart, gb->gapend, n);
    gb->gapstart += n;
    gb->gapend += n;
  } else if (point < gb->gapstart) {
    int n = gb->gapstart - point;
    memcpy(gb->gapend - n, point, n);
    gb->gapstart -= n;
    gb->gapend -= n;
  }
}

// Ensure the gap fits at least n new characters.
static void gb_growgap(gapbuf_t *gb, int n) {
  int gapsize = gb->gapend - gb->gapstart;
  if (n <= gapsize) {
    return;
  }

  int newgapsize = 0;
  while (newgapsize < 2*n) {
    newgapsize += GAPSIZE;
  }
  // Pointers will be obsoleted so remember offsets...
  int leftsize = gb->gapstart - gb->bufstart;
  int rightsize = gb->bufend - gb->gapend;

  int newsize = leftsize + rightsize + newgapsize;

  gb->bufstart = realloc(gb->bufstart, newsize);
  gb->gapstart = gb->bufstart + leftsize;
  gb->gapend = gb->gapstart + newgapsize;
  // Move the bit after the gap right by gapsize positions...
  memcpy(gb->gapend, gb->gapstart + gapsize, rightsize);
  gb->bufend = gb->bufstart + newsize;
}

void gb_putchar(gapbuf_t *gb, char c, int pos) {
  gb_putstring(gb, &c, 1, pos);
}

void gb_putstring(gapbuf_t *gb, char *buf, int n, int pos) {
  gb_growgap(gb, n);
  gb_mvgap(gb, pos);
  memcpy(gb->gapstart, buf, n);
  gb->gapstart += n;
}

void gb_del(gapbuf_t *gb, int n, int pos) {
  gb_mvgap(gb, pos);
  gb->gapstart -= n;
}
