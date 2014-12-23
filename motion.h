#ifndef _motion_h_included
#define _motion_h_included

#include "window.h"

typedef pos_t (motion_op_t) (pos_t, window_t*);

typedef struct {
  char *name;
  motion_op_t *op;
  char last;
  int count;
} motion_t;

struct tb_event;
// Returns -1 if the key is not a motion key, 0 for a partial motion, or 1
// if the motion is ready to apply.
int motion_key(motion_t *motion, struct tb_event *ev);
pos_t motion_apply(motion_t *motion, window_t *window);

#endif
