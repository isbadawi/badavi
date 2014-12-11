#include <stdio.h>
#include <string.h>

#include <termbox.h>

void draw_text(int x, int y, const char* text) {
  int len = strlen(text);
  for (int i = 0; i < len; ++i) {
    tb_change_cell(x + i, y, text[i], TB_WHITE, TB_DEFAULT);
  }
}

void draw_centered(int y_offset, const char* text) {
  int center_x = (tb_width() - strlen(text)) / 2;
  int center_y = tb_height() / 2;
  draw_text(center_x, center_y + y_offset, text);
}

void draw_hello_world(void) {
  tb_clear();
  draw_centered(-1, "hello, world!");
  draw_centered(1, "Press any key to exit.");
  tb_present();
}

int main(int argc, char *argv[]) {
  int err = tb_init();
  if (err) {
    fprintf(stderr, "tb_init() failed with error code %d\n", err);
    return 1;
  }

  draw_hello_world();

  struct tb_event ev;
  while (tb_poll_event(&ev)) {
    switch (ev.type) {
    case TB_EVENT_KEY:
      tb_shutdown();
      return 0;
    case TB_EVENT_RESIZE:
      draw_hello_world();
      break;
    default:
      break;
    }
  }

  tb_shutdown();
  return 0;
}
