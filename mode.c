#include "mode.h"

#include <stdlib.h>
#include <string.h>

#include <termbox.h>

#include "buf.h"
#include "editor.h"
#include "file.h"

static void normal_mode_key_pressed(editor_t* editor, struct tb_event* ev) {
  if (ev->key == TB_KEY_ESC) {
    exit(0);
  }
  cursor_t *cursor = editor->cursor;
  switch (ev->ch) {
    case 'i':
      buf_printf(editor->status, "-- INSERT --");
      editor->mode = &insert_mode;
      break;
    case ':':
      buf_printf(editor->status, ":");
      editor->mode = &command_mode;
      break;
    case '0': cursor->offset = 0; break;
    case '$': cursor->offset = cursor->line->buf->len; break;
    case 'h': editor_move_left(editor); break;
    case 'l': editor_move_right(editor); break;
    case 'j': editor_move_down(editor); break;
    case 'k': editor_move_up(editor); break;
    case 'a': editor_send_keys(editor, "li"); break;
    case 'I': editor_send_keys(editor, "0i"); break;
    case 'A': editor_send_keys(editor, "$i"); break;
    case 'o': editor_send_keys(editor, "A<cr>"); break;
    case 'O': editor_send_keys(editor, "0i<cr><esc>ki"); break;
    case 'x': editor_send_keys(editor, "a<bs><esc>"); break;
    case 'J':
      if (cursor->line->next) {
        editor_send_keys(editor, "A <esc>jI<bs><esc>");
      }
      break;
    case 'D':
      buf_delete(
          cursor->line->buf,
          cursor->offset,
          cursor->line->buf->len - cursor->offset);
      break;
  }
}

static void insert_mode_key_pressed(editor_t* editor, struct tb_event* ev) {
  cursor_t *cursor = editor->cursor;
  char ch;
  switch (ev->key) {
    case TB_KEY_ESC:
      buf_printf(editor->status, "");
      editor->mode = &normal_mode;
      return;
    case TB_KEY_BACKSPACE2:
      if (cursor->offset > 0) {
        buf_delete(cursor->line->buf, cursor->offset-- - 1, 1);
      } else if (cursor->line->prev->buf) {
        int prev_len = cursor->line->prev->buf->len;
        buf_insert(
            cursor->line->prev->buf,
            cursor->line->buf->buf,
            prev_len);
        editor_move_up(editor);
        file_remove_line(editor->file, cursor->line->next);
        cursor->offset = prev_len;
      }
      return;
    case TB_KEY_ENTER:
      file_insert_line_after(
          editor->file,
          cursor->line->buf->buf + cursor->offset,
          cursor->line);
      buf_delete(
          cursor->line->buf,
          cursor->offset,
          cursor->line->buf->len - cursor->offset);
      editor_move_down(editor);
      cursor->offset = 0;
      return;
    case TB_KEY_SPACE:
      ch = ' ';
      break;
    default:
      ch = ev->ch;
  }
  char s[2] = {ch, '\0'};
  buf_insert(cursor->line->buf, s, cursor->offset++);
}

static void command_mode_key_pressed(editor_t *editor, struct tb_event *ev) {
  char ch;
  switch (ev->key) {
    case TB_KEY_ESC:
      buf_printf(editor->status, "");
      editor->mode = &normal_mode;
      return;
    case TB_KEY_BACKSPACE2:
      if (editor->status->len > 1) {
        buf_delete(editor->status, editor->status->len - 1, 1);
      }
      return;
    case TB_KEY_ENTER: {
      char *command = strdup(editor->status->buf + 1);
      editor_execute_command(editor, command);
      free(command);
      editor->mode = &normal_mode;
      return;
    }
    case TB_KEY_SPACE:
      ch = ' ';
      break;
    default:
      ch = ev->ch;
  }
  char s[2] = {ch, '\0'};
  buf_insert(editor->status, s, editor->status->len);
}

editing_mode_t normal_mode = {normal_mode_key_pressed};
editing_mode_t insert_mode = {insert_mode_key_pressed};
editing_mode_t command_mode = {command_mode_key_pressed};
