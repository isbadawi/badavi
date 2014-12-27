#ifndef _motion_h_included
#define _motion_h_included

#include "editor.h"
#include "window.h"

typedef int (motion_op_t) (int, window_t*);

struct motion_t {
  char name;
  motion_op_t *op;
};

int motion_apply(editor_t *editor);

#endif
