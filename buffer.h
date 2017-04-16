#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "options.h"

#define BUFFER_NAME_MAXLEN 255

struct buf;

// struct buffer is the in-memory text of a file.
struct buffer {
  // The name of the file this buffer was loaded from (possibly empty).
  char name[BUFFER_NAME_MAXLEN];
  // The text proper.
  struct gapbuf *text;
  // True if this buffer has unsaved changes.
  bool dirty;

  // Undo and redo stacks.
  // The elements are lists of actions.
  struct list *undo_stack;
  struct list *redo_stack;

  // Marked regions, whose positions are updated as edits are made via
  // buffer_do_insert and buffer_do_delete.
  struct list *marks;

  struct {
#define OPTION(name, type, _) type name;
  BUFFER_OPTIONS
#undef OPTION
  } opt;
};

// Reads the given path into a struct buffer object. The path must exist.
// Returns NULL if buffer can't be opened or we're out of memory.
struct buffer *buffer_open(char *path);

// Returns an empty buffer (i.e. with a single empty line).
struct buffer *buffer_create(char *path);

// Writes the contents of the given buffer to buffer->name.
// Returns false if this buffer has no name.
bool buffer_write(struct buffer *buffer);

// Writes to the contents of the given buffer to the path.
// Returns false if the file couldn't be opened for writing.
bool buffer_saveas(struct buffer *buffer, char *path);

// Insert the given buf into the buffer's text at offset pos,
// updating the undo information along the way.
void buffer_do_insert(struct buffer *buffer, struct buf *buf, size_t pos);
// Delete n characters from the buffer's text starting at offset pos,
// updating the undo information along the way.
void buffer_do_delete(struct buffer *buffer, size_t n, size_t pos);

// Undo the last action group. Return false if there is nothing to undo.
bool buffer_undo(struct buffer *buffer);
// Redo the last undone action group. Return false if there is nothing to redo.
bool buffer_redo(struct buffer *buffer);

// Start a new action group, clearing the redo stack as a side effect.
// Subsequent calls to buffer_do_insert or buffer_do_delete will add actions to
// this group, which will be the target of the next buffer_undo call.
void buffer_start_action_group(struct buffer *buffer);
