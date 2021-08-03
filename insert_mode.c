#include "mode.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <termbox.h>

#include "buf.h"
#include "buffer.h"
#include "editor.h"
#include "gap.h"
#include "motion.h"
#include "window.h"

static void editor_show_mode(struct editor *editor) {
  if (editor->opt.showmode) {
    if (editor->recording) {
      editor_status_msg(editor, "-- INSERT --recording @%c", editor->recording);
    } else {
      editor_status_msg(editor, "-- INSERT --");
    }
  }
}

void insert_mode_entered(struct editor *editor) {
  struct insert_mode *mode = editor_get_insert_mode(editor);
  mode->completion_prefix = NULL;
  mode->completions = NULL;
  mode->completion = NULL;

  editor_show_mode(editor);
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

  editor_status_clear(editor);

  struct insert_mode *mode = editor_get_insert_mode(editor);
  free(mode->completion_prefix);
  mode->completion_prefix = NULL;
  mode->completion = NULL;
  editor->popup.visible = false;
  if (mode->completions) {
    history_deinit(mode->completions);
    free(mode->completions);
    mode->completions = NULL;
  }
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
    }
    buf_free(line);

    if (shift) {
      buf_appendf(indent, "%*s", buffer->opt.shiftwidth, "");
    }
  }

  buffer_do_insert(buffer, indent, cursor + 1);
}

static void editor_exit_completion(struct editor *editor) {
  struct editor_popup *popup = &editor->popup;
  struct insert_mode *mode = editor_get_insert_mode(editor);
  if (popup->visible) {
    popup->visible = false;
    popup->len = 0;
    popup->selected = 0;
    free(popup->lines);
    popup->lines = NULL;
    mode->completion = NULL;
  }
  editor_show_mode(editor);
}

static void editor_select_completion(struct editor *editor) {
  struct insert_mode *mode = editor_get_insert_mode(editor);
  struct buffer *buffer = editor->window->buffer;
  size_t cursor = window_cursor(editor->window);
  struct editor_popup *popup = &editor->popup;
  buffer_do_delete(buffer, cursor - popup->pos, popup->pos);
  buffer_do_insert(buffer, buf_copy(mode->completion->buf), popup->pos);
  if (popup->len == 1) {
    editor_exit_completion(editor);
    editor_status_msg(editor, "the only match");
    return;
  }
  editor_status_msg(editor, "match %d of %d", popup->selected + 1, popup->len);
}

#define FIRST_COMPLETION() \
  history_first(mode->completions, buf_startswith, mode->completion_prefix)

#define LAST_COMPLETION() \
  history_last(mode->completions, buf_startswith, mode->completion_prefix)

#define PREV_COMPLETION(entry) \
  history_prev(entry, buf_startswith, mode->completion_prefix)

#define NEXT_COMPLETION(entry) \
  history_next(entry, buf_startswith, mode->completion_prefix)

static void editor_refresh_completions(struct editor *editor) {
  struct insert_mode *mode = editor_get_insert_mode(editor);
  struct editor_popup *popup = &editor->popup;
  if (!popup->visible) {
    return;
  }

  if (window_cursor(editor->window) == popup->pos) {
    editor_exit_completion(editor);
    return;
  }

  struct buf *word = motion_word_before_cursor(editor->window);
  mode->completion_prefix = word->buf;
  free(word);
  mode->completion = FIRST_COMPLETION();
  if (!mode->completion) {
    editor_exit_completion(editor);
    return;
  }

  popup->len = 0;
  popup->selected = 0;
  struct history_entry *entry;
  for (entry = mode->completion; entry; entry = NEXT_COMPLETION(entry)) {
    popup->len++;
  }

  free(popup->lines);
  popup->lines = xmalloc(popup->len * sizeof(*popup->lines));
  int i = 0;
  for (entry = mode->completion; entry; entry = NEXT_COMPLETION(entry)) {
    popup->lines[i++] = entry->buf->buf;
  }
}

static void editor_init_completion(struct editor *editor) {
  struct insert_mode *mode = editor_get_insert_mode(editor);
  struct buffer *buffer = editor->window->buffer;
  size_t cursor = window_cursor(editor->window);
  struct editor_popup *popup = &editor->popup;
  if (!mode->completions) {
    mode->completions = xmalloc(sizeof(*mode->completions));
    history_init(mode->completions, NULL);
    editor_load_completions(editor, COMPLETION_TAGS, mode->completions);
  }

  popup->visible = true;
  editor_refresh_completions(editor);
  if (!popup->visible) {
    return;
  }

  popup->pos = cursor;
  for (char *p = mode->completion_prefix; *p; ++p) {
    popup->pos = gb_utf8prev(buffer->text, popup->pos);
  }

}

static void editor_prev_completion(struct editor *editor) {
  struct insert_mode *mode = editor_get_insert_mode(editor);
  struct editor_popup *popup = &editor->popup;
  if (!popup->visible) {
    editor_init_completion(editor);
  }
  if (!popup->visible) {
    return;
  }

  popup->selected--;
  mode->completion = PREV_COMPLETION(mode->completion);
  if (popup->selected < 0) {
    popup->selected = popup->len - 1;
    mode->completion = LAST_COMPLETION();
  }
  editor_select_completion(editor);
}

static void editor_next_completion(struct editor *editor) {
  struct insert_mode *mode = editor_get_insert_mode(editor);
  struct editor_popup *popup = &editor->popup;
  if (!popup->visible) {
    editor_init_completion(editor);
    if (popup->visible) {
      mode->completion = FIRST_COMPLETION();
      editor_select_completion(editor);
    }
    return;
  }
  popup->selected++;
  mode->completion = NEXT_COMPLETION(mode->completion);
  if (popup->selected == popup->len) {
    popup->selected = 0;
    mode->completion = FIRST_COMPLETION();
  }
  editor_select_completion(editor);
}

void insert_mode_key_pressed(struct editor* editor, struct tb_event* ev) {
  struct buffer *buffer = editor->window->buffer;
  size_t cursor = window_cursor(editor->window);
  uint32_t ch;
  switch (ev->key) {
  case TB_KEY_ESC: case TB_KEY_CTRL_C: editor_pop_mode(editor); return;
  case TB_KEY_CTRL_Y: editor_exit_completion(editor); return;
  case TB_KEY_CTRL_P: editor_prev_completion(editor); return;
  case TB_KEY_CTRL_N: editor_next_completion(editor); return;
  case TB_KEY_BACKSPACE:
    if (cursor > 0) {
      size_t prev = gb_utf8prev(buffer->text, cursor);
      buffer_do_delete(buffer, gb_utf8len(buffer->text, prev), prev);
      editor_refresh_completions(editor);
    }
    return;
  case TB_KEY_ENTER: ch = '\n'; break;
  case TB_KEY_SPACE: ch = ' '; break;
  case TB_KEY_TAB: ch = '\t'; break;
  default: ch = ev->ch; break;
  }

  if (isspace(ch)) {
    editor_exit_completion(editor);
  }

  struct buf *insertion;
  if (ch == '\t' && buffer->opt.expandtab) {
    insertion = buf_create(buffer->opt.shiftwidth);
    buf_printf(insertion, "%*s", buffer->opt.shiftwidth, "");
  } else {
    insertion = buf_from_utf8(ch);
  }

  buffer_do_insert(buffer, insertion, cursor);
  if (ch == '\n') {
    // TODO(ibadawi): If we add indent then leave insert mode, remove it
    insert_indent(buffer, cursor);
  }

  editor_refresh_completions(editor);
}
