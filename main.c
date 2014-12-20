#include <stdio.h>
#include <stdlib.h>

#include <termbox.h>

#include "editor.h"
#include "util.h"

int main(int argc, char *argv[]) {
  debug_init();

  editor_t editor;
  cursor_t cursor;
  editor_init(&editor, &cursor, argc > 1 ? argv[1] : NULL);

  int err = tb_init();
  if (err) {
    fprintf(stderr, "tb_init() failed with error code %d\n", err);
    return 1;
  }
  atexit(tb_shutdown);

  editor_draw(&editor);

  struct tb_event ev;
  while (tb_poll_event(&ev)) {
    switch (ev.type) {
    case TB_EVENT_KEY:
      editor_handle_key_press(&editor, &ev);
      break;
    case TB_EVENT_RESIZE:
      editor_draw(&editor);
      break;
    default:
      break;
    }
  }

  return 0;
}
