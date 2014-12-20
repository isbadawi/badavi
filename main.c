#include <stdio.h>
#include <stdlib.h>

#include <signal.h>

#include <termbox.h>

#include "editor.h"
#include "util.h"

// termbox catches ctrl-z as a regular key event. To suspend the process as
// normal, manually raise SIGTSTP.
//
// Not 100% sure why we need to shutdown termbox, but the terminal gets all
// weird if we don't. Mainly copied this approach from here:
// https://github.com/nsf/godit/blob/master/suspend_linux.go
static void suspend(editor_t *editor) {
  tb_shutdown();

  raise(SIGTSTP);

  int err = tb_init();
  if (err) {
    fprintf(stderr, "tb_init() failed with error code %d\n", err);
    exit(1);
  }

  editor_draw(editor);
}

int main(int argc, char *argv[]) {
  debug_init();

  editor_t editor;
  editor_init(&editor);

  if (argc > 1) {
    editor_open(&editor, argv[1]);
  } else {
    editor_open_empty(&editor);
  }

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
      if (ev.key == TB_KEY_CTRL_Z) {
        suspend(&editor);
      } else {
        editor_handle_key_press(&editor, &ev);
      }
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
