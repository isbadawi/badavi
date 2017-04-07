#pragma once

#include <stddef.h>
#include <stdio.h>

// A "gap buffer" or "split buffer". It's a big buffer that internally
// is separated into two buffers with a gap in the middle -- this allows
// edits at the gap to be efficient.
struct gapbuf {
  // Points to the start of the buffer.
  char *bufstart;
  // Points to one past the end of the buffer.
  char *bufend;
  // Points to the start of the gap.
  char *gapstart;
  // Points to one past the end of the gap.
  char *gapend;

  // An array of line lengths, with one element per line.
  struct intbuf *lines;
};

// The gap is just an implementation detail. In what follows, "the buffer"
// refers to the "logical" buffer, without the gap.

// Create an empty buffer.
struct gapbuf *gb_create(void);
// Read fp into memory and create a buffer from the contents.
struct gapbuf *gb_load(FILE *fp);
// Frees the given buffer.
void gb_free(struct gapbuf *gb);

// Returns the size of the buffer.
size_t gb_size(struct gapbuf *gb);
// Returns the number of lines.
size_t gb_nlines(struct gapbuf *gb);

// Writes the contents of the buffer into fp.
void gb_save(struct gapbuf *gb, FILE *fp);

// Returns the character at offset pos from the start of the buffer.
char gb_getchar(struct gapbuf *gb, size_t pos);
// Reads n characters starting at offset pos.
struct buf *gb_getstring(struct gapbuf *gb, size_t pos, size_t n);

// Moves the gap so that gb->bufstart + pos == gb->gapstart.
void gb_mvgap(struct gapbuf *gb, size_t pos);

// Insert a single character after offset pos.
void gb_putchar(struct gapbuf *gb, char c, size_t pos);
// Insert a string of n characters after offset pos.
void gb_putstring(struct gapbuf *gb, char *buf, size_t n, size_t pos);

// Remove the n characters before offset pos.
void gb_del(struct gapbuf *gb, size_t n, size_t pos);

// Return the offset of the first occurrence of c in the buffer, starting at
// offset start, or gb_size(gb) if c is not found.
size_t gb_indexof(struct gapbuf *gb, char c, size_t start);
// Return the offset of the last occurrence of c in the buffer, starting at
// offset start, or -1 if c is not found.
ssize_t gb_lastindexof(struct gapbuf *gb, char c, size_t start);

// Converts a buffer offset into a line number, and offset within that line.
void gb_pos_to_linecol(struct gapbuf *gb, size_t pos, size_t *line, size_t *offset);
// Vice versa.
size_t gb_linecol_to_pos(struct gapbuf *gb, size_t line, size_t offset);
