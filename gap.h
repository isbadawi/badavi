#pragma once

#include <stddef.h>
#include <stdio.h>

// A "gap buffer" or "split buffer". It's a big buffer that internally
// is separated into two buffers with a gap in the middle -- this allows
// edits at the gap to be efficient.
struct gapbuf_t {
  // Points to the start of the buffer.
  char *bufstart;
  // Points to one past the end of the buffer.
  char *bufend;
  // Points to the start of the gap.
  char *gapstart;
  // Points to one past the end of the gap.
  char *gapend;

  // An array of line lengths, with one element per line.
  struct intbuf_t *lines;
};

// The gap is just an implementation detail. In what follows, "the buffer"
// refers to the "logical" buffer, without the gap.

// Create an empty buffer.
struct gapbuf_t *gb_create(void);
// Read fp into memory and create a buffer from the contents.
struct gapbuf_t *gb_load(FILE *fp);
// Frees the given buffer.
void gb_free(struct gapbuf_t *gb);

// Returns the size of the buffer.
size_t gb_size(struct gapbuf_t *gb);
// Returns the number of lines.
size_t gb_nlines(struct gapbuf_t *gb);

// Writes the contents of the buffer into fp.
void gb_save(struct gapbuf_t *gb, FILE *fp);

// Returns the character at offset pos from the start of the buffer.
char gb_getchar(struct gapbuf_t *gb, size_t pos);
// Reads n characters starting at offset pos into buf.
void gb_getstring(struct gapbuf_t *gb, size_t pos, size_t n, char *buf);

// Insert a single character after offset pos.
void gb_putchar(struct gapbuf_t *gb, char c, size_t pos);
// Insert a string of n characters after offset pos.
void gb_putstring(struct gapbuf_t *gb, char *buf, size_t n, size_t pos);

// Remove the n characters before offset pos.
void gb_del(struct gapbuf_t *gb, size_t n, size_t pos);

// Return the offset of the first occurrence of c in the buffer, starting at
// offset start, or gb_size(gb) if c is not found.
size_t gb_indexof(struct gapbuf_t *gb, char c, size_t start);
// Return the offset of the last occurrence of c in the buffer, starting at
// offset start, or -1 if c is not found.
ssize_t gb_lastindexof(struct gapbuf_t *gb, char c, size_t start);

// Converts a buffer offset into a line number, and offset within that line.
void gb_pos_to_linecol(struct gapbuf_t *gb, size_t pos, size_t *line, size_t *offset);
// Vice versa.
size_t gb_linecol_to_pos(struct gapbuf_t *gb, size_t line, size_t offset);

typedef struct {
  size_t start;
  size_t len;
} gb_match_t;

typedef struct {
  char error[48];
  struct list_t *matches;
} gb_search_result_t;

// Finds all matches for the given regex in the buffer. Fills in a
// gb_search_result_t structure describing the outcome.
void gb_search(struct gapbuf_t *gb, char *regex, gb_search_result_t *result);
