#include "gap.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <regex.h>

#include "buf.h"
#include "list.h"
#include "options.h"

#define GAPSIZE 1024

struct gapbuf_t *gb_create(void) {
  struct gapbuf_t *gb = malloc(sizeof(*gb));
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

struct gapbuf_t *gb_load(FILE *fp) {
  struct gapbuf_t *gb = malloc(sizeof(*gb));
  if (!gb) {
    return NULL;
  }

  struct stat info;
  fstat(fileno(fp), &info);
  size_t filesize = (size_t) info.st_size;
  size_t bufsize = filesize + GAPSIZE;

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
  ssize_t last = -1;
  for (ssize_t i = 0; (size_t) i < filesize; ++i) {
    if (gb->gapend[i] == '\n') {
      unsigned int len = (unsigned int) (i - last) - 1;
      intbuf_add(gb->lines, len);
      last = (ssize_t) i;
    }
  }

  return gb;
}

void gb_free(struct gapbuf_t *gb) {
  free(gb->bufstart);
  intbuf_free(gb->lines);
}

size_t gb_size(struct gapbuf_t *gb) {
  return (size_t) ((gb->gapstart - gb->bufstart) + (gb->bufend - gb->gapend));
}

size_t gb_nlines(struct gapbuf_t *gb) {
  return gb->lines->len;
}

void gb_save(struct gapbuf_t *gb, FILE *fp) {
  fwrite(gb->bufstart, 1, (size_t) (gb->gapstart - gb->bufstart), fp);
  fwrite(gb->gapend, 1, (size_t) (gb->bufend - gb->gapend), fp);
}

// Returns the real index of the logical offset pos.
static size_t gb_index(struct gapbuf_t *gb, size_t pos) {
  ptrdiff_t gapsize = gb->gapend - gb->gapstart;
  return gb->bufstart + pos < gb->gapstart ? pos : pos + (size_t) gapsize;
}

char gb_getchar(struct gapbuf_t *gb, size_t pos) {
  return gb->bufstart[gb_index(gb, pos)];
}

void gb_getstring(struct gapbuf_t *gb, size_t pos, size_t n, char *buf) {
  char *start = gb->bufstart + gb_index(gb, pos);
  char *end = gb->bufstart + gb_index(gb, pos + n);
  if (end < gb->gapstart || start >= gb->gapend) {
    memcpy(buf, start, n);
  } else {
    ptrdiff_t l = gb->gapstart - start;
    ptrdiff_t r = end - gb->gapend;
    memcpy(buf, start, l);
    memcpy(buf + l, gb->gapend, r);
  }
  buf[n] = '\0';
}

// Moves the gap so that gb->bufstart + pos == gb->gapstart.
static void gb_mvgap(struct gapbuf_t *gb, size_t pos) {
  char *point = gb->bufstart + gb_index(gb, pos);
  if (gb->gapend <= point) {
    ptrdiff_t n = point - gb->gapend;
    memcpy(gb->gapstart, gb->gapend, n);
    gb->gapstart += n;
    gb->gapend += n;
  } else if (point < gb->gapstart) {
    ptrdiff_t n = gb->gapstart - point;
    memcpy(gb->gapend - n, point, n);
    gb->gapstart -= n;
    gb->gapend -= n;
  }
}

// Ensure the gap fits at least n new characters.
static void gb_growgap(struct gapbuf_t *gb, size_t n) {
  ptrdiff_t gapsize = gb->gapend - gb->gapstart;
  if (n <= (size_t) gapsize) {
    return;
  }

  size_t newgapsize = 0;
  while (newgapsize < 2*n) {
    newgapsize += GAPSIZE;
  }
  // Pointers will be obsoleted so remember offsets...
  ptrdiff_t leftsize = gb->gapstart - gb->bufstart;
  ptrdiff_t rightsize = gb->bufend - gb->gapend;

  size_t newsize = (size_t) (leftsize + rightsize) + newgapsize;

  gb->bufstart = realloc(gb->bufstart, newsize);
  gb->gapstart = gb->bufstart + leftsize;
  gb->gapend = gb->gapstart + newgapsize;
  // Move the bit after the gap right by gapsize positions...
  memcpy(gb->gapend, gb->gapstart + gapsize, rightsize);
  gb->bufend = gb->bufstart + newsize;
}

void gb_putchar(struct gapbuf_t *gb, char c, size_t pos) {
  gb_putstring(gb, &c, 1, pos);
}

void gb_putstring(struct gapbuf_t *gb, char *buf, size_t n, size_t pos) {
  gb_growgap(gb, n);
  gb_mvgap(gb, pos);
  memcpy(gb->gapstart, buf, n);

  size_t line, col;
  gb_pos_to_linecol(gb, pos, &line, &col);

  // Adjust the line lengths.

  // Where we started inserting on this line
  size_t start = col;
  // Characters since last newline
  size_t last = 0;
  for (size_t i = 0; i < n; ++i) {
    if (gb->gapstart[i] == '\n') {
      size_t oldlen = gb->lines->buf[line];
      size_t newlen = start + last;
      gb->lines->buf[line] = (unsigned int) newlen;
      intbuf_insert(gb->lines, (unsigned int) (oldlen - newlen), ++line);
      start = last = 0;
    } else {
      gb->lines->buf[line]++;
      last++;
    }
  }
  gb->gapstart += n;
}

void gb_del(struct gapbuf_t *gb, size_t n, size_t pos) {
  gb_mvgap(gb, pos);

  size_t line, col;
  gb_pos_to_linecol(gb, pos, &line, &col);

  for (size_t i = 0; i < n; ++i) {
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

size_t gb_indexof(struct gapbuf_t *gb, char c, size_t start) {
  size_t size = gb_size(gb);
  for (size_t i = start; i < size; ++i) {
    if (gb_getchar(gb, i) == c) {
      return i;
    }
  }
  return size;
}

ssize_t gb_lastindexof(struct gapbuf_t *gb, char c, size_t start) {
  for (ssize_t i = (ssize_t) start; i >= 0; --i) {
    if (gb_getchar(gb, (size_t) i) == c) {
      return i;
    }
  }
  return -1;
}

void gb_pos_to_linecol(struct gapbuf_t *gb, size_t pos, size_t *line, size_t *column) {
  size_t offset = 0;
  *line = *column = 0;
  for (size_t i = 0; i < gb->lines->len; ++i) {
    size_t len = gb->lines->buf[i];
    *line = i;
    *column = pos - offset;
    if (offset <= pos && pos <= offset + len) {
      return;
    }
    offset += len + 1;
  }
  *column = pos - offset;
}

size_t gb_linecol_to_pos(struct gapbuf_t *gb, size_t line, size_t column) {
  size_t offset = 0;
  for (size_t i = 0; i < line; ++i) {
    offset += gb->lines->buf[i] + 1;
  }
  return offset + column;
}

static bool gb_should_ignore_case(char *pattern) {
  if (!option_get_bool("ignorecase")) {
    return false;
  }
  if (!option_get_bool("smartcase")) {
    return true;
  }

  for (char *p = pattern; *p; ++p) {
    if (isupper((int) *p)) {
      return false;
    }
  }
  return true;
}

void gb_search(struct gapbuf_t *gb, char *pattern, gb_search_result_t *result) {
  // Move the gap so the searched region is contiguous.
  gb_mvgap(gb, 0);

  regex_t regex;
  int flags = REG_EXTENDED | REG_NEWLINE;
  if (gb_should_ignore_case(pattern)) {
    flags |= REG_ICASE;
  }
  int err = regcomp(&regex, pattern, flags);
  if (err) {
    result->matches = NULL;
    regerror(err, &regex, result->error, sizeof(result->error));
    return;
  }

  result->matches = list_create();

  int nomatch = 0;
  size_t start = 0;
  while (!nomatch) {
    regmatch_t match;
    // regexec assumes the string is at the beginning of a line unless told
    // otherwise. This affects regexes that use ^.
    flags = 0;
    if (start > 0 && gb_getchar(gb, start - 1) != '\n') {
      flags |= REG_NOTBOL;
    }

    nomatch = regexec(&regex, gb->gapend + start, 1, &match, flags);
    if (!nomatch) {
      gb_match_t *region = malloc(sizeof(*region));
      region->start = start + (size_t) match.rm_so;
      region->len = (size_t) (match.rm_eo - match.rm_so);
      list_append(result->matches, region);
      start = start + (size_t) match.rm_eo;
    }
  }
  regfree(&regex);
}
