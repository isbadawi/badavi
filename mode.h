#ifndef _mode_h_included
#define _mode_h_included

#include "editor.h"

struct tb_event;

typedef void (mode_keypress_handler_t) (editor_t*, struct tb_event*);

struct editing_mode_t {
  // Each mode has its own keyboard handler;
  mode_keypress_handler_t* key_pressed;
  // The mode the editor was in when this mode was entered.
  editing_mode_t *parent;
};

editing_mode_t *normal_mode(void);
editing_mode_t *insert_mode(void);
editing_mode_t *command_mode(void);
editing_mode_t *operator_pending_mode(char op);

#endif
