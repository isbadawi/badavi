#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pwd.h>
#include <unistd.h>

#include <termbox.h>

#include "editor.h"
#include "tags.h"
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

  int err = tb_init();
  if (err) {
    fprintf(stderr, "tb_init() failed with error code %d\n", err);
    return 1;
  }
  atexit(tb_shutdown);

  struct editor editor;
  editor_init(&editor, (size_t) tb_width(), (size_t) tb_height());

  if (tag) {
    editor_jump_to_tag(&editor, tag);
  } else if (i < argc) {
    // Open files in reverse order so that the first file listed ends up as the
    // leftmost or topmost split. Note that this is all happening before we
    // source any config, so that the values of 'splitbelow' or 'splitright'
    // don't affect the behavior.
    // FIXME(ibadawi): Have window_split take arguments to control layout
    // FIXME(ibadawi): instead of looking at options directly.
    editor_open(&editor, argv[argc - 1]);

    if (split_type != WINDOW_LEAF) {
      for (int j = argc - 2; i < j + 1; --j) {
        editor.window = window_split(editor.window, split_type);
        editor_open(&editor, argv[j]);
      }
      editor.window = window_first_leaf(window_root(editor.window));
    }

    if (line && (!*line || atoi(line) > 0)) {
      char buf[32];
      snprintf(buf, 32, "%sG", line);
      editor_send_keys(&editor, buf);
    }
  }

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

  editor_draw(&editor);

  struct tb_event ev;
  while (editor_waitkey(&editor, &ev)) {
    editor_handle_key_press(&editor, &ev);
    editor_draw(&editor);
  }

  return 0;
}
