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
typedef struct buffer_t {
  // The name of the file this buffer was loaded from (possibly empty).
  buf_t *name;
  // Doubly linked list of lines.
  // The first node is always dummy node with buf == NULL.
  line_t *head;
  // Number of lines in the buffer.
  int nlines;

  struct buffer_t *next;
} buffer_t;

// All functions returns 0 on success, negative number on error.

// Reads the given path into a buffer_t object. The path must exist.
// Returns NULL if buffer can't be opened or we're out of memory.
buffer_t *buffer_open(char *path);

// Returns an empty buffer (i.e. with a single empty line).
buffer_t *buffer_create(char *path);

// Returns the size of the buffer (by adding up the sizes of the lines).
int buffer_size(buffer_t *buffer);

// Writes the contents of the given buffer to buffer->name.
// Returns -1 if this buffer has no name.
int buffer_write(buffer_t *buffer);

// Writes to the contents of the given buffer to the path.
int buffer_saveas(buffer_t *buffer, char *path);

// Inserts a line consisting of s at position pos (starting from 0).
// Returns the newly created line, or NULL.
line_t *buffer_insert_line(buffer_t *buffer, char *s, int pos);

// Inserts a line consisting of s after line.
// Returns the newly created line, or NULL.
line_t *buffer_insert_line_after(buffer_t *buffer, char *s, line_t *line);

// Removes the given line from the buffer.
void buffer_remove_line(buffer_t *buffer, line_t *line);

int buffer_index_of_line(buffer_t *buffer, line_t *line);

line_t *buffer_get_line(buffer_t *buffer, int pos);

#endif
