#ifndef _buffer_h_included
#define _buffer_h_included

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

// buffer_t is the in-memory text of a file.
typedef struct {
  // Doubly linked list of lines.
  // The first node is always dummy node with buf == NULL.
  line_t *head;
  // Number of lines in the buffer.
  int nlines;
} buffer_t;

// All functions returns 0 on success, negative number on error.

// Reads the given path into a buffer_t object. The path must exist.
// Returns NULL if buffer can't be opened or we're out of memory.
buffer_t *buffer_open(char *path);

// Returns an empty buffer (i.e. with a single empty line).
buffer_t *buffer_create(void);

// Returns the size of the buffer (by adding up the sizes of the lines).
int buffer_size(buffer_t *buffer);

// Writes the contents of the given buffer to the path (replacing any
// existing buffer).
int buffer_write(buffer_t *buffer, char *path);

// Inserts a line consisting of s at position pos (starting from 0).
// Returns the newly created line, or NULL.
line_t *buffer_insert_line(buffer_t *buffer, char *s, int pos);

// Inserts a line consisting of s after line.
// Returns the newly created line, or NULL.
line_t *buffer_insert_line_after(buffer_t *buffer, char *s, line_t *line);

// Removes the given line from the buffer.
void buffer_remove_line(buffer_t *buffer, line_t *line);

int buffer_index_of_line(buffer_t *buffer, line_t *line);

#endif
