#pragma once

#include <stddef.h>

struct editor_t;

// A single edit action -- either an insert or delete.
struct edit_action_t {
  enum { EDIT_ACTION_INSERT, EDIT_ACTION_DELETE } type;
  // The position at which the action occurred.
  size_t pos;
  // The text added (for insertions) or removed (for deletions).
  struct buf_t *buf;
};

void editor_undo(struct editor_t *editor);
void editor_redo(struct editor_t *editor);

// Start a new action group, clearing the redo stack as a side effect.
// Subsequent calls to editor_add_action will add actions to this group,
// which will be the target of the next editor_undo call.
void editor_start_action_group(struct editor_t *editor);
// Add a new action to the current action group.
void editor_add_action(struct editor_t *editor, struct edit_action_t action);
