#pragma once

#include <stddef.h>
#include <stdint.h>

#include "util.h"

struct editor;
struct tb_event;

#define MODES \
  MODE(normal) \
  MODE(insert) \
  MODE(visual) \
  MODE(cmdline) \
  MODE(operator_pending)

struct editing_mode {
  enum editing_mode_kind {
#define MODE(name) EDITING_MODE_##name,
    MODES
#undef MODE
  } kind;
  // Called when the editor switches into this mode (optional).
  void (*entered)(struct editor*);
  // Called when the editor switches out of this mode (optional).
  void (*exited)(struct editor*);
  // Called when this mode is active and a key is pressed.
  void (*key_pressed)(struct editor*, struct tb_event*);
  // The mode the editor was in when this mode was entered.
  struct editing_mode *parent;
  // Mode-specific argument.
  uint64_t arg;
};

struct normal_mode {
  struct editing_mode mode;
};

struct insert_mode {
  struct editing_mode mode;
};

struct visual_mode {
  struct editing_mode mode;
  enum visual_mode_kind {
    VISUAL_MODE_CHARACTERWISE,
    VISUAL_MODE_LINEWISE,
    // TODO(ibadawi): VISUAL_MODE_BLOCKWISE
  } kind;
  // The position of the cursor when the mode was entered.
  size_t cursor;
  struct region selection;
};
void visual_mode_selection_update(struct editor *editor);

struct cmdline_mode {
  struct editing_mode mode;
  // The position of the cursor when the mode was entered.
  size_t cursor;
  struct history_entry *history_entry;
  struct buf *history_prefix;

  struct history_entry *completion;
  struct history *completions;
  enum completion_kind {
    COMPLETION_NONE,
    COMPLETION_CMDS,
    COMPLETION_OPTIONS,
    COMPLETION_TAGS,
    COMPLETION_PATHS,
  } completion_kind;
  char prompt;
};

typedef void (op_func)(struct editor*, struct region*);
op_func *op_find(char name);
struct operator_pending_mode {
  struct editing_mode mode;
  op_func *op;
};

#define MODE(name) \
  void editor_push_##name##_mode(struct editor *editor, uint64_t arg); \
  struct name##_mode *editor_get_##name##_mode(struct editor *editor); \
  void name##_mode_entered(struct editor *editor); \
  void name##_mode_exited(struct editor *editor); \
  void name##_mode_key_pressed(struct editor* editor, struct tb_event* ev);
MODES
#undef MODE
