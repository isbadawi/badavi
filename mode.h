#pragma once

struct editor_t;
struct tb_event;

struct editing_mode_t {
  // Called when the editor switches into this mode (optional).
  void (*entered)(struct editor_t*);
  // Called when the editor switches out of this mode (optional).
  void (*exited)(struct editor_t*);
  // Called when this mode is active and a key is pressed.
  void (*key_pressed)(struct editor_t*, struct tb_event*);
  // The mode the editor was in when this mode was entered.
  struct editing_mode_t *parent;
};

struct editing_mode_t *normal_mode(void);
struct editing_mode_t *insert_mode(void);
struct editing_mode_t *command_mode(void);
struct editing_mode_t *search_mode(char direction);
struct editing_mode_t *operator_pending_mode(char op);

// These are "utility modes" used to help process input.
struct editing_mode_t *digit_mode(void);
struct editing_mode_t *motion_mode(void);
struct editing_mode_t *quote_mode(void);
