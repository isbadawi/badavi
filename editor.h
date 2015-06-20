#pragma once

#include <stdbool.h>
#include <stddef.h>

struct editor_register_t {
  char name;
  struct buf_t *buf;
};

// The "editor" holds the main state of the program.
struct editor_t {
  // The loaded buffers.
  struct list_t *buffers;

  // The open windows.
  struct list_t *windows;
  // The "selected" window -- the one being edited. An element of windows.
  struct window_t *window;

  // A stack of editing modes.
  struct editing_mode_t* mode;

  // What's written to the status bar.
  struct buf_t* status;
  // Whether the status is an error.
  bool status_error;
  // Whether the status should be displayed.
  bool status_silence;

  // An array of registers.
  struct editor_register_t *registers;

  // List of loaded ctags.
  struct tags_t *tags;

  // The width and height of the screen.
  size_t width;
  size_t height;

  // Temporary input state.
  // TODO(isbadawi): This feels like a kludge but I don't know...
  unsigned int count;
  struct motion_t *motion;
  char register_;
};

void editor_init(struct editor_t *editor);

void editor_open(struct editor_t *editor, char *path);

void editor_push_mode(struct editor_t *editor, struct editing_mode_t *mode);
void editor_pop_mode(struct editor_t *editor);

void editor_save_buffer(struct editor_t *editor, char *path);
void editor_execute_command(struct editor_t *editor, char *command);
void editor_draw(struct editor_t *editor);

struct buf_t *editor_get_register(struct editor_t *editor, char name);

struct tb_event;
void editor_handle_key_press(struct editor_t *editor, struct tb_event *ev);

__attribute__((__format__(__printf__, 2, 3)))
void editor_status_msg(struct editor_t *editor, const char *format, ...);
__attribute__((__format__(__printf__, 2, 3)))
void editor_status_err(struct editor_t *editor, const char *format, ...);

void editor_send_keys(struct editor_t *editor, char *keys);

void editor_equalize_windows(struct editor_t *editor);

struct window_t *editor_left_window(struct editor_t *editor, struct window_t *window);
struct window_t *editor_right_window(struct editor_t *editor, struct window_t *window);

void editor_undo(struct editor_t *editor);
void editor_redo(struct editor_t *editor);
