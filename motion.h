#ifndef _motion_h_included
#define _motion_h_included

#include "editor.h"
#include "window.h"

typedef int (motion_op_t) (int, window_t*);

struct motion_t {
  char name;
  motion_op_t *op;
  // Whether the motion is linewise (vs. characterwise).
  // (See :help linewise in vim for details).
  int linewise;
  // Whether the motion is exclusive (vs. inclusive).
  // (See :help exclusive in vim for details).
  // This is only relevant for characterwise motions.
  int exclusive;
};

int motion_apply(editor_t *editor);

#endif
