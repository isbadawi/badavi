#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pwd.h>
#include <unistd.h>

#include <termbox.h>

#include "editor.h"
#include "tags.h"
#include "terminal.h"
#include "window.h"

int main(int argc, char *argv[]) {
  char *line = NULL;
  char *tag = NULL;
  enum window_split_type split_type = WINDOW_LEAF;

  char *rc = NULL;
  char default_rc[255];

  int i;
  for (i = 1; i < argc; ++i) {
    char *arg = argv[i];
    if (!strcmp(arg, "-u")) {
      if (i == argc - 1) {
        fprintf(stderr, "argument missing after -u\n");
        return 1;
      }
      rc = argv[++i];
    } else if (!strcmp(arg, "-t")) {
      if (i == argc - 1) {
        fprintf(stderr, "argument missing after -t\n");
        return 1;
      }
      tag = argv[++i];
    } else if (!strcmp(arg, "-o")) {
      split_type = WINDOW_SPLIT_HORIZONTAL;
    } else if (!strcmp(arg, "-O")) {
      split_type = WINDOW_SPLIT_VERTICAL;
    } else if (*arg == '+') {
      line = arg + 1;
    } else {
      break;
    }
  }

  terminal_init();

  struct editor editor;
  editor_init(&editor, (size_t) tb_width(), (size_t) tb_height());

  if (!rc) {
    const char *home = getenv("HOME");
    home = home ? home : getpwuid(getuid())->pw_dir;
    snprintf(default_rc, sizeof(default_rc), "%s/.badavimrc", home);
    if (!access(default_rc, F_OK)) {
      rc = default_rc;
    }
  }

  if (rc && strcmp(rc, "NONE") != 0) {
    editor_source(&editor, rc);
  }

  if (tag) {
    editor_jump_to_tag(&editor, tag);
  } else if (i < argc) {
    editor_open(&editor, argv[i++]);

    if (split_type != WINDOW_LEAF) {
      enum window_split_direction direction;
      if (split_type == WINDOW_SPLIT_HORIZONTAL) {
        direction = WINDOW_SPLIT_BELOW;
      } else {
        assert(split_type == WINDOW_SPLIT_VERTICAL);
        direction = WINDOW_SPLIT_RIGHT;
      }
      for (; i < argc; ++i) {
        editor.window = window_split(editor.window, direction);
        editor_open(&editor, argv[i]);
      }
      editor.window = window_first_leaf(window_root(editor.window));
      window_equalize(editor.window, split_type);
    }

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
