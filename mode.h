#pragma once

struct editor;
struct region;
struct tb_event;

struct editing_mode {
  // Called when the editor switches into this mode (optional).
  void (*entered)(struct editor*);
  // Called when the editor switches out of this mode (optional).
  void (*exited)(struct editor*);
  // Called when this mode is active and a key is pressed.
  void (*key_pressed)(struct editor*, struct tb_event*);
  // The mode the editor was in when this mode was entered.
  struct editing_mode *parent;
};

enum visual_mode_kind {
  VISUAL_MODE_CHARACTERWISE,
  VISUAL_MODE_LINEWISE,
  // TODO(ibadawi): VISUAL_MODE_BLOCKWISE
};

void visual_mode_selection_update(struct editor *editor);

struct editing_mode *normal_mode(void);
struct editing_mode *insert_mode(void);
struct editing_mode *visual_mode(enum visual_mode_kind kind);
struct editing_mode *command_mode(void);
struct editing_mode *search_mode(char direction);
struct editing_mode *operator_pending_mode(char op);

typedef void (op_func) (struct editor*, struct region*);
op_func *op_find(char name);
