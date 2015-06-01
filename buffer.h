#pragma once

#include <stdbool.h>

#include "gap.h"
#include "list.h"

#define BUFFER_NAME_MAXLEN 255

// buffer_t is the in-memory text of a file.
typedef struct buffer_t {
  // The name of the file this buffer was loaded from (possibly empty).
  char name[BUFFER_NAME_MAXLEN];
  // The text proper.
  gapbuf_t *text;
  // True if this buffer has unsaved changes.
  bool dirty;

  // Undo and redo stacks.
  // The elements are lists of actions.
  list_t *undo_stack;
  list_t *redo_stack;
} buffer_t;

// All functions returns 0 on success, negative number on error.

// Reads the given path into a buffer_t object. The path must exist.
// Returns NULL if buffer can't be opened or we're out of memory.
buffer_t *buffer_open(char *path);

// Returns an empty buffer (i.e. with a single empty line).
buffer_t *buffer_create(char *path);

// Writes the contents of the given buffer to buffer->name.
// Returns -1 if this buffer has no name.
int buffer_write(buffer_t *buffer);

// Writes to the contents of the given buffer to the path.
int buffer_saveas(buffer_t *buffer, char *path);
