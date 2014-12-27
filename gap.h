#ifndef _gap_h_included
#define _gap_h_included

#include <stdio.h>
#include "buf.h"

// A "gap buffer" or "split buffer". It's a big buffer that internally
// is separated into two buffers with a gap in the middle -- this allows
// edits at the gap to be efficient.
typedef struct {
  // Points to the start of the buffer.
  char *bufstart;
  // Points to one past the end of the buffer.
  char *bufend;
  // Points to the start of the gap.
  char *gapstart;
  // Points to one past the end of the gap.
  char *gapend;

  // An array of line lengths, with one element per line.
  intbuf_t *lines;
} gapbuf_t;

// The gap is just an implementation detail. In what follows, "the buffer"
// refers to the "logical" buffer, without the gap.

// Create an empty buffer.
gapbuf_t *gb_create(void);
// Read fp into memory and create a buffer from the contents.
gapbuf_t *gb_load(FILE *fp);
// Frees the given buffer.
void gb_free(gapbuf_t *gb);

// Returns the size of the buffer.
int gb_size(gapbuf_t *gb);
// Returns the number of lines.
int gb_nlines(gapbuf_t *gb);

// Writes the contents of the buffer into fp.
void gb_save(gapbuf_t *gb, FILE *fp);

// Returns the character at offset pos from the start of the buffer.
char gb_getchar(gapbuf_t *gb, int pos);
// Reads n characters starting at offset pos into buf.
void gb_getstring(gapbuf_t *gb, int pos, int n, char *buf);

// Insert a single character after offset pos.
void gb_putchar(gapbuf_t *gb, char c, int pos);
// Insert a string of n characters after offset pos.
void gb_putstring(gapbuf_t *gb, char *buf, int n, int pos);

// Remove the n characters before offset pos.
void gb_del(gapbuf_t *gb, int n, int pos);


// Return the offset of the first occurrence of c in the buffer, starting at
// offset start, or gb_size(gb) if c is not found.
int gb_indexof(gapbuf_t *gb, char c, int start);
// Return the offset of the last occurrence of c in the buffer, starting at
// offset start, or -1 if c is not found.
int gb_lastindexof(gapbuf_t *gb, char c, int start);

// Converts a buffer offset into a line number, and offset within that line.
void gb_pos_to_linecol(gapbuf_t *gb, int pos, int *line, int *offset);
// Vice versa.
int gb_linecol_to_pos(gapbuf_t *gb, int line, int offset);

#endif
