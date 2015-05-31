#ifndef _editor_h_included
#define _editor_h_included

#include <stdbool.h>
#include <stddef.h>

#include "buf.h"
#include "window.h"
#include "list.h"
#include "tags.h"

struct editing_mode_t;
typedef struct editing_mode_t editing_mode_t;

struct motion_t;
typedef struct motion_t motion_t;

typedef struct {
  char name;
  buf_t *buf;
} editor_register_t;

// A single edit action -- either an insert or delete.
typedef struct edit_action_t {
  enum { EDIT_ACTION_INSERT, EDIT_ACTION_DELETE } type;
  // The position at which the action occurred.
  size_t pos;
  // The text added (for insertions) or removed (for deletions).
  buf_t *buf;
} edit_action_t;

// The "editor" holds the main state of the program.
typedef struct {
  // The loaded buffers.
  list_t *buffers;

  // The open windows.
  list_t *windows;
  // The "selected" window -- the one being edited. An element of windows.
  window_t *window;

  // A stack of editing modes.
  editing_mode_t* mode;

  // What's written to the status bar.
  buf_t* status;
  // Whether the status is an error.
  bool status_error;

  // An array of registers.
  editor_register_t *registers;

  // List of loaded ctags.
  tags_t *tags;

  // The width and height of the screen.
  size_t width;
  size_t height;

  // Temporary input state.
  // TODO(isbadawi): This feels like a kludge but I don't know...
  unsigned int count;
  motion_t *motion;
  char register_;
} editor_t;

void editor_init(editor_t *editor);

void editor_open(editor_t *editor, char *path);

void editor_push_mode(editor_t *editor, editing_mode_t *mode);
void editor_pop_mode(editor_t *editor);

void editor_save_buffer(editor_t *editor, char *path);
void editor_execute_command(editor_t *editor, char *command);
void editor_draw(editor_t *editor);

buf_t *editor_get_register(editor_t *editor, char name);
void editor_search(editor_t *editor);

struct tb_event;
void editor_handle_key_press(editor_t *editor, struct tb_event *ev);

__attribute__((__format__(__printf__, 2, 3)))
void editor_status_msg(editor_t *editor, const char *format, ...);
__attribute__((__format__(__printf__, 2, 3)))
void editor_status_err(editor_t *editor, const char *format, ...);

void editor_send_keys(editor_t *editor, char *keys);

void editor_undo(editor_t *editor);
void editor_redo(editor_t *editor);

void editor_equalize_windows(editor_t *editor);

// Start a new action group, clearing the redo stack as a side effect.
// Subsequent calls to editor_add_action will add actions to this group,
// which will be the target of the next editor_undo call.
void editor_start_action_group(editor_t *editor);
// Add a new action to the current action group.
void editor_add_action(editor_t *editor, edit_action_t action);

window_t *editor_left_window(editor_t *editor, window_t *window);
window_t *editor_right_window(editor_t *editor, window_t *window);

void editor_jump_to_tag(editor_t *editor, char *tag);

void editor_tag_stack_prev(editor_t *editor);
void editor_tag_stack_next(editor_t *editor);

#endif
