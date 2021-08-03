#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <sys/queue.h>

#include "attrs.h"
#include "history.h"
#include "mode.h"
#include "options.h"

struct editor_event {
  uint8_t type;
  uint16_t key;
  uint32_t ch;

  TAILQ_ENTRY(editor_event) pointers;
};

// The "editor" holds the main state of the program.
struct editor {
  // The loaded buffers.
  TAILQ_HEAD(buffer_list, buffer) buffers;

  // The focused window.
  struct window *window;

  // A stack of editing modes.
  struct editing_mode* mode;

  struct {
#define MODE(name) struct name##_mode name;
    MODES
#undef MODE
  } modes;

  // Global working directory if explicitly set (otherwise NULL).
  // This is not necessarily the current working directory of the process.
  char *pwd;

  // Optional output above the status bar.
  struct buf *message;
  // What's written to the status bar.
  struct buf *status;

  struct editor_popup {
    bool visible;
    size_t pos;
    char **lines;
    int offset;
    int selected;
    int len;
  } popup;

  // Whether the status is an error.
  bool status_error;
  // Whether the status should be displayed.
  bool status_silence;
  // The position of the cursor in cmdline mode.
  size_t status_cursor;

  // An array of registers (a-z, /, ", * and +).
  #define EDITOR_NUM_REGISTERS 30
  struct editor_register {
    char name;
    struct buf *buf;
    // Read from the register. Caller must free the returned buffer.
    char *(*read)(struct editor_register *reg);
    // Write to the register. Makes a copy of the input buffer.
    void (*write)(struct editor_register *reg, char*);
  } registers[EDITOR_NUM_REGISTERS];

  // List of loaded ctags.
  struct tags *tags;

  // The width and height of the screen.
  size_t width;
  size_t height;

  bool highlight_search_matches;

  // Which register is being recorded into (or '\0' if not recording).
  char recording;

  // Temporary input state.
  // TODO(isbadawi): This feels like a kludge but I don't know...
  unsigned int count;
  char register_;

  struct history command_history;
  struct history search_history;

  // Synthetic input events generated by editor_send_keys.
  // These take precedence over real events emitted by termbox.
  TAILQ_HEAD(event_list, editor_event) synthetic_events;

  struct {
#define OPTION(name, type, _) type name;
    BUFFER_OPTIONS
    EDITOR_OPTIONS
#undef OPTION
  } opt;
};

struct editor *editor_create(size_t width, size_t height);
struct editor *editor_create_and_open(size_t width, size_t height, char *path);
void editor_free(struct editor *editor);

void editor_open(struct editor *editor, char *path);

void editor_set_window(struct editor *editor, struct window *window);

void editor_push_mode(struct editor *editor, struct editing_mode *mode);
void editor_pop_mode(struct editor *editor);

bool editor_save_buffer(struct editor *editor, char *path);
void editor_draw(struct editor *editor);

void editor_jump_to_line(struct editor *editor, int line);
void editor_jump_to_end(struct editor *editor);

const char *editor_buffer_name(struct editor *editor, struct buffer *buffer);

const char *editor_relpath(struct editor *editor, const char *path);

struct editor_register *editor_get_register(struct editor *editor, char name);

struct tb_event;
bool editor_waitkey(struct editor *editor, struct tb_event *ev);
char editor_getchar(struct editor *editor);
void editor_handle_key_press(struct editor *editor, struct tb_event *ev);

ATTR_PRINTFLIKE(2, 3)
void editor_status_msg(struct editor *editor, const char *format, ...);
ATTR_PRINTFLIKE(2, 3)
void editor_status_err(struct editor *editor, const char *format, ...);
void editor_status_clear(struct editor *editor);

void editor_push_event(struct editor *editor, struct tb_event *ev);
void editor_send_keys(struct editor *editor, const char *keys);

void editor_undo(struct editor *editor);
void editor_redo(struct editor *editor);

void editor_source(struct editor *editor, char *path);
bool editor_try_modify(struct editor *editor);

char *editor_find_in_path(struct editor *editor, char *file);

struct editor_command {
  const char *name;
  const char *shortname;
  enum completion_kind completion_kind;
  void (*action)(struct editor*, char*, bool);

  TAILQ_ENTRY(editor_command) pointers;
};
void register_editor_command(struct editor_command *command);
struct editor_command *command_parse(char *command, char **arg, bool *force);
char **commands_get_sorted(int *len);

void editor_execute_command(struct editor *editor, char *command);

#define EDITOR_COMMAND_WITH_COMPLETION(name, shortname, completion) \
  static void editor_command_##name(struct editor*, char*, bool); \
  static struct editor_command command_##name = { \
      #name, #shortname, completion, editor_command_##name, {0}}; \
  __attribute__((constructor)) \
  static void _constructor_##name(void) { \
    register_editor_command(&command_##name); \
  } \
  static void editor_command_##name( \
      struct editor *editor ATTR_UNUSED, \
      char *arg ATTR_UNUSED, \
      bool force ATTR_UNUSED)

#define EDITOR_COMMAND(name, shortname) \
  EDITOR_COMMAND_WITH_COMPLETION(name, shortname, COMPLETION_NONE)
