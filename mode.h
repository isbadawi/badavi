#pragma once

#include "editor.h"

struct tb_event;

typedef void (mode_key_func_t) (editor_t*, struct tb_event*);
typedef void (mode_enter_func_t) (editor_t *editor);

struct editing_mode_t {
  // Called when the editor switches into this mode.
  mode_enter_func_t *entered;
  // Called when this mode is active and a key is pressed.
  mode_key_func_t* key_pressed;
  // The mode the editor was in when this mode was entered.
  editing_mode_t *parent;
};


editing_mode_t *normal_mode(void);
editing_mode_t *insert_mode(void);
editing_mode_t *command_mode(void);
editing_mode_t *search_mode(char direction);
editing_mode_t *operator_pending_mode(char op);

// These are "utility modes" used to help process input.
editing_mode_t *digit_mode(void);
editing_mode_t *motion_mode(void);
editing_mode_t *quote_mode(void);
