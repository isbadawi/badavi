#include "mode.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <termbox.h>

#include "buf.h"
#include "buffer.h"
#include "editor.h"
#include "gap.h"
#include "window.h"

void insert_mode_entered(struct editor *editor) {
  editor_status_msg(editor, "-- INSERT --");
  buffer_start_action_group(editor->window->buffer);
}

void insert_mode_exited(struct editor *editor) {
  // If we exit insert mode without making changes, let's not add a
  // useless undo action.
  // TODO(isbadawi): Maybe this belongs in an "editor_end_action_group"
  struct action_group_list *undo_stack = &editor->window->buffer->undo_stack;
  struct edit_action_group *group = TAILQ_FIRST(undo_stack);
  if (TAILQ_EMPTY(&group->actions)) {
    TAILQ_REMOVE(undo_stack, group, pointers);
  }

  buf_clear(editor->status);
}

static void insert_indent(struct buffer *buffer, size_t cursor) {
  if (!buffer->opt.autoindent) {
    return;
  }

  struct gapbuf *gb = buffer->text;
  ssize_t newline = gb_lastindexof(gb, '\n', cursor - 1);
  ssize_t nonblank = newline;
  char ch;
  do {
    ++nonblank;
    ch = gb_getchar(gb, nonblank);
  } while (isspace(ch) && ch != '\n');

  struct buf *indent = gb_getstring(gb, newline + 1, nonblank - newline - 1);

  if (buffer->opt.smartindent) {
    struct buf *line = gb_getline(gb, cursor);
    buf_strip_whitespace(line);

    bool shift = buf_endswith(line, "{");
    if (!shift) {
      char *words = xstrdup(buffer->opt.cinwords);
      char *word = strtok(words, ",");
      do {
        if (buf_startswith(line, word)) {
          shift = true;
          break;
        }
      } while ((word = strtok(NULL, ",")));

      free(words);
      buf_free(line);
    }

    if (shift) {
      for (int i = 0; i < buffer->opt.shiftwidth; ++i) {
        buf_append(indent, " ");
      }
    }
  }

  buffer_do_insert(buffer, indent, cursor + 1);
}

void insert_mode_key_pressed(struct editor* editor, struct tb_event* ev) {
  struct buffer *buffer = editor->window->buffer;
  size_t cursor = window_cursor(editor->window);
  char ch;
  switch (ev->key) {
  case TB_KEY_ESC: case TB_KEY_CTRL_C:
    editor_pop_mode(editor);
    return;
  case TB_KEY_BACKSPACE2:
    if (cursor > 0) {
      buffer_do_delete(buffer, 1, cursor - 1);
    }
    return;
  case TB_KEY_ENTER: ch = '\n'; break;
  case TB_KEY_SPACE: ch = ' '; break;
  default: ch = (char) ev->ch; break;
  }

  buffer_do_insert(buffer, buf_from_char(ch), cursor);
  if (ch == '\n') {
    // TODO(ibadawi): If we add indent then leave insert mode, remove it
    insert_indent(buffer, cursor);
  }
}
