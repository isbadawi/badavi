#include "gap.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <regex.h>

#include "options.h"

#define GAPSIZE 1024

gapbuf_t *gb_create(void) {
  gapbuf_t *gb = malloc(sizeof(*gb));
  if (!gb) {
    return NULL;
  }

  gb->bufstart = malloc(1 + GAPSIZE);
  if (!gb->bufstart) {
    free(gb);
    return NULL;
  }

  gb->gapstart = gb->bufstart;
  gb->gapend = gb->gapstart + GAPSIZE;
  gb->bufend = gb->bufstart + GAPSIZE + 1;
  gb->bufend[-1] = '\n';

  gb->lines = intbuf_create(10);
  intbuf_add(gb->lines, 0);
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

  gb->gapstart = gb->bufstart;
  gb->gapend = gb->bufstart + GAPSIZE;
  gb->bufend = gb->bufstart + bufsize;

  fread(gb->gapend, 1, filesize, fp);

  gb->lines = intbuf_create(10);
  int last = -1;
  for (int i = 0; i < filesize; ++i) {
    if (gb->gapend[i] == '\n') {
      intbuf_add(gb->lines, i - last - 1);
      last = i;
    }
  }

  return gb;
}

void gb_free(gapbuf_t *gb) {
  free(gb->bufstart);
  intbuf_free(gb->lines);
}

int gb_size(gapbuf_t *gb) {
  return (gb->gapstart - gb->bufstart) + (gb->bufend - gb->gapend);
}

int gb_nlines(gapbuf_t *gb) {
  return gb->lines->len;
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

void gb_getstring(gapbuf_t *gb, int pos, int n, char *buf) {
  char *start = gb->bufstart + gb_index(gb, pos);
  char *end = gb->bufstart + gb_index(gb, pos + n);
  if (end < gb->gapstart || start >= gb->gapend) {
    memcpy(buf, start, n);
  } else {
    int l = gb->gapstart - start;
    int r = end - gb->gapend;
    memcpy(buf, start, l);
    memcpy(buf + l, gb->gapend, r);
  }
  buf[n] = '\0';
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

  int line, col;
  gb_pos_to_linecol(gb, pos, &line, &col);

  // Adjust the line lengths.

  // Where we started inserting on this line
  int start = col;
  // Characters since last newline
  int last = 0;
  for (int i = 0; i < n; ++i) {
    if (gb->gapstart[i] == '\n') {
      int oldlen = gb->lines->buf[line];
      gb->lines->buf[line] = start + last;
      intbuf_insert(gb->lines, oldlen - (start + last), ++line);
      start = last = 0;
    } else {
      gb->lines->buf[line]++;
      last++;
    }
  }
  gb->gapstart += n;
}

void gb_del(gapbuf_t *gb, int n, int pos) {
  gb_mvgap(gb, pos);

  int line, col;
  gb_pos_to_linecol(gb, pos, &line, &col);

  for (int i = 0; i < n; ++i) {
    if (gb->gapstart[-i - 1] == '\n') {
      gb->lines->buf[line - 1] += gb->lines->buf[line];
      intbuf_remove(gb->lines, line);
      line--;
    } else {
      gb->lines->buf[line]--;
    }
  }

  gb->gapstart -= n;

  // Empty files are tricky for us, so insert a newline if needed...
  if (!gb_size(gb)) {
    *(gb->gapstart++) = '\n';
    intbuf_add(gb->lines, 0);
  }
}

int gb_indexof(gapbuf_t *gb, char c, int start) {
  int size = gb_size(gb);
  for (int i = start; i < size; ++i) {
    if (gb_getchar(gb, i) == c) {
      return i;
    }
  }
  return size;
}

int gb_lastindexof(gapbuf_t *gb, char c, int start) {
  for (int i = start; i >= 0; --i) {
    if (gb_getchar(gb, i) == c) {
      return i;
    }
  }
  return -1;
}

void gb_pos_to_linecol(gapbuf_t *gb, int pos, int *line, int *column) {
  int offset = 0;
  *line = *column = 0;
  for (int i = 0; i < gb->lines->len; ++i) {
    int len = gb->lines->buf[i];
    *line = i;
    *column = pos - offset;
    if (offset <= pos && pos <= offset + len) {
      return;
    }
    offset += len + 1;
  }
  *column = pos - offset;
}

int gb_linecol_to_pos(gapbuf_t *gb, int line, int column) {
  int offset = 0;
  for (int i = 0; i < line; ++i) {
    offset += gb->lines->buf[i] + 1;
  }
  return offset + column;
}

void gb_search_forwards(gapbuf_t *gb, char *pattern, int start,
                        gb_search_result_t *result) {
  // Move the gap so the searched region is contiguous.
  gb_mvgap(gb, start);

  regex_t regex;
  int flags = REG_EXTENDED | REG_NEWLINE;
  if (option_get_bool("ignorecase")) {
    flags |= REG_ICASE;
  }
  int err = regcomp(&regex, pattern, flags);
  if (err) {
    result->status = GB_SEARCH_BAD_REGEX;
    regerror(err, &regex, result->v.error, sizeof result->v.error);
    return;
  }

  // regexec assumes the string is at the beginning of a line unless told
  // otherwise. This affects regexes that use ^.
  flags = 0;
  if (start > 0 && gb_getchar(gb, start - 1) != '\n') {
    flags |= REG_NOTBOL;
  }

  regmatch_t match;
  int nomatch = regexec(&regex, gb->gapend, 1, &match, flags);
  regfree(&regex);

  if (nomatch) {
    result->status = GB_SEARCH_NO_MATCH;
  } else {
    result->status = GB_SEARCH_MATCH;
    result->v.match.start = (gb->gapstart - gb->bufstart) + match.rm_so;
    result->v.match.len = match.rm_eo - match.rm_so;
  }
}
