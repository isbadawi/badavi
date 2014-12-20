#ifndef _editor_h_included
#define _editor_h_included

#include "buf.h"
#include "file.h"

typedef struct {
  // The line the cursor is on.
  line_t *line;
  // The offset of the cursor within that line.
  int offset;
} cursor_t;

struct editing_mode_t;
typedef struct editing_mode_t editing_mode_t;

// The "editor" holds the main state of the program.
typedef struct {
  // The file being edited (only one file for now).
  file_t *file;
  // The path to the file being edited.
  char *path;
  // The top line visible on screen.
  line_t *top;
  // The leftmost column visible on screen.
  int left;
  // The editing mode (e.g. normal mode, insert mode, etc.)
  editing_mode_t* mode;
  // The cursor.
  cursor_t* cursor;
  // What's written to the status bar.
  buf_t* status;
} editor_t;

void editor_init(editor_t *editor, cursor_t *cursor, char *path);
void editor_open(char *path);

void editor_save_file(editor_t *editor, char *path);
void editor_execute_command(editor_t *editor, char *command);
void editor_draw(editor_t *editor);

struct tb_event;
void editor_handle_key_press(editor_t *editor, struct tb_event *ev);

void editor_move_left(editor_t *editor);
void editor_move_right(editor_t *editor);
void editor_move_down(editor_t *editor);
void editor_move_up(editor_t *editor);

void editor_send_keys(editor_t *editor, const char *keys);

#endif
