#ifndef _mode_h_included
#define _mode_h_included

#include "editor.h"

struct tb_event;

// The editing mode for now is just a function that handles key events.
typedef void (mode_keypress_handler_t) (editor_t*, struct tb_event*);

struct editing_mode_t {
  mode_keypress_handler_t* key_pressed;
};

editing_mode_t *normal_mode(void);
editing_mode_t *insert_mode(void);
editing_mode_t *command_mode(void);
editing_mode_t *operator_pending_mode(char op);

#endif
