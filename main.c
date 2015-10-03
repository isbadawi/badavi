#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <termbox.h>

#include "editor.h"
#include "tags.h"
#include "util.h"

int main(int argc, char *argv[]) {
  debug_init();

  int err = tb_init();
  if (err) {
    fprintf(stderr, "tb_init() failed with error code %d\n", err);
    return 1;
  }
  atexit(tb_shutdown);

  struct editor_t editor;
  editor_init(&editor, (size_t) tb_width(), (size_t) tb_height());

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
  while (editor_waitkey(&editor, &ev)) {
    editor_handle_key_press(&editor, &ev);
    editor_draw(&editor);
  }

  return 0;
}
