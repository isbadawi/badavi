#ifndef _editor_h_included
#define _editor_h_included

#include "buf.h"
#include "buffer.h"
#include "window.h"

struct editing_mode_t;
typedef struct editing_mode_t editing_mode_t;

typedef struct {
  char name;
  buf_t *buf;
} editor_register_t;

// The "editor" holds the main state of the program.
typedef struct {
  // The loaded buffers.
  buffer_t *buffers;

  // The open windows.
  window_t *windows;
  // The "selected" window -- the one being edited. An element of windows.
  window_t *window;

  // The editing mode (e.g. normal mode, insert mode, etc.)
  editing_mode_t* mode;

  // What's written to the status bar.
  buf_t* status;
  // Whether the status is an error.
  int status_error;

  // An array of registers.
  editor_register_t *registers;
} editor_t;

void editor_init(editor_t *editor);

void editor_open(editor_t *editor, char *path);
void editor_open_empty(editor_t *editor);

void editor_save_buffer(editor_t *editor, char *path);
void editor_execute_command(editor_t *editor, char *command);
void editor_draw(editor_t *editor);

buf_t *editor_get_register(editor_t *editor, char name);

struct tb_event;
void editor_handle_key_press(editor_t *editor, struct tb_event *ev);

void editor_status_msg(editor_t *editor, const char *format, ...);
void editor_status_err(editor_t *editor, const char *format, ...);

void editor_send_keys(editor_t *editor, const char *keys);

#endif
