#ifndef _file_h_included
#define _file_h_included

#include "buf.h"

// line_t is a single line of text.
typedef struct line_t {
  // The characters in the line (not including the newline).
  buf_t *buf;
  // The previous line.
  struct line_t *prev;
  // The next line, or NULL if this is the last line.
  struct line_t *next;
} line_t;

// file_t is all the text in a file.
typedef struct {
  // Doubly linked list of lines.
  // The first node is always dummy node with buf == NULL.
  line_t *head;
  // Number of lines in the file.
  int nlines;
  // Number of characters in the file.
  int size;
} file_t;

// All functions returns 0 on success, negative number on error.

// Reads the given path into a file_t object.
// Returns NULL if file can't be opened or we're out of memory.
file_t *file_read(char *path);

// Writes the contents of the given file to the path (replacing any
// existing file).
int file_write(file_t *file, char *path);

// Inserts a line consisting of the given s at position pos (starting from 0).
// Returns the newly created line, or NULL.
line_t *file_insert_line(file_t *file, const char *s, int pos);

#endif
