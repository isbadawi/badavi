#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>

#include <termbox.h>

#include "editor.h"
#include "tags.h"
#include "util.h"

// termbox catches ctrl-z as a regular key event. To suspend the process as
// normal, manually raise SIGTSTP.
//
// Not 100% sure why we need to shutdown termbox, but the terminal gets all
// weird if we don't. Mainly copied this approach from here:
// https://github.com/nsf/godit/blob/master/suspend_linux.go
static void suspend(struct editor_t *editor) {
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

  int err = tb_init();
  if (err) {
    fprintf(stderr, "tb_init() failed with error code %d\n", err);
    return 1;
  }
  atexit(tb_shutdown);

  struct editor_t editor;
  editor_init(&editor);

  char *file = NULL;
  char *line = NULL;
  char *tag = NULL;

  for (int i = 1; i < argc; ++i) {
    char *arg = argv[i];
    if (!strcmp(arg, "-t")) {
      tag = argv[++i];
    } else if (*arg == '+') {
      line = arg + 1;
    } else {
      file = arg;
    }
  }

  if (tag) {
    editor_jump_to_tag(&editor, tag);
  } else if (file) {
    editor_open(&editor, file);
    if (line && (!*line || atoi(line) > 0)) {
      char buf[32];
      snprintf(buf, 32, "%sG", line);
      editor_send_keys(&editor, buf);
    }
  }

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
      editor.width = (size_t) ev.w;
      editor.height = (size_t) ev.h;
      editor_equalize_windows(&editor);
      break;
    default:
      break;
    }
    editor_draw(&editor);
  }

  return 0;
}
