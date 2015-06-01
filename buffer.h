#pragma once

#include <stdbool.h>

#define BUFFER_NAME_MAXLEN 255

// struct buffer_t is the in-memory text of a file.
struct buffer_t {
  // The name of the file this buffer was loaded from (possibly empty).
  char name[BUFFER_NAME_MAXLEN];
  // The text proper.
  struct gapbuf_t *text;
  // True if this buffer has unsaved changes.
  bool dirty;

  // Undo and redo stacks.
  // The elements are lists of actions.
  struct list_t *undo_stack;
  struct list_t *redo_stack;
};

// All functions returns 0 on success, negative number on error.

// Reads the given path into a struct buffer_t object. The path must exist.
// Returns NULL if buffer can't be opened or we're out of memory.
struct buffer_t *buffer_open(char *path);

// Returns an empty buffer (i.e. with a single empty line).
struct buffer_t *buffer_create(char *path);

// Writes the contents of the given buffer to buffer->name.
// Returns -1 if this buffer has no name.
int buffer_write(struct buffer_t *buffer);

// Writes to the contents of the given buffer to the path.
int buffer_saveas(struct buffer_t *buffer, char *path);
