#ifndef _motion_h_included
#define _motion_h_included

#include "editor.h"
#include "window.h"

typedef struct {
  int pos;
  window_t *window;
  motion_t *motion;
} motion_context_t;

typedef int (motion_op_t) (motion_context_t);

struct motion_t {
  char name;
  motion_op_t *op;
  // Whether the motion is linewise (vs. characterwise).
  // (See :help linewise in vim for details).
  bool linewise;
  // Whether the motion is exclusive (vs. inclusive).
  // (See :help exclusive in vim for details).
  // This is only relevant for characterwise motions.
  bool exclusive;
};

int motion_apply(editor_t *editor);

#endif
