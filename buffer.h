#pragma once

#include <stdbool.h>
#include <stddef.h>

#define BUFFER_NAME_MAXLEN 255

struct buf_t;

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

  // Marked regions, whose positions are updated as edits are made via
  // buffer_do_insert and buffer_do_delete.
  struct list_t *marks;
};

// Reads the given path into a struct buffer_t object. The path must exist.
// Returns NULL if buffer can't be opened or we're out of memory.
struct buffer_t *buffer_open(char *path);

// Returns an empty buffer (i.e. with a single empty line).
struct buffer_t *buffer_create(char *path);

// Writes the contents of the given buffer to buffer->name.
// Returns false if this buffer has no name.
bool buffer_write(struct buffer_t *buffer);

// Writes to the contents of the given buffer to the path.
// Returns false if the file couldn't be opened for writing.
bool buffer_saveas(struct buffer_t *buffer, char *path);

// Insert the given buf into the buffer's text at offset pos,
// updating the undo information along the way.
void buffer_do_insert(struct buffer_t *buffer, struct buf_t *buf, size_t pos);
// Delete n characters from the buffer's text starting at offset pos,
// updating the undo information along the way.
void buffer_do_delete(struct buffer_t *buffer, size_t n, size_t pos);

// Undo the last action group. Return false if there is nothing to undo.
bool buffer_undo(struct buffer_t *buffer);
// Redo the last undone action group. Return false if there is nothing to redo.
bool buffer_redo(struct buffer_t *buffer);

// Start a new action group, clearing the redo stack as a side effect.
// Subsequent calls to buffer_do_insert or buffer_do_delete will add actions to
// this group, which will be the target of the next buffer_undo call.
void buffer_start_action_group(struct buffer_t *buffer);
