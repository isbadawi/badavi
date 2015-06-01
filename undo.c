#include "undo.h"

#include <stdlib.h>
#include <string.h>

#include "buf.h"
#include "buffer.h"
#include "editor.h"
#include "gap.h"
#include "list.h"
#include "window.h"

void editor_undo(struct editor_t* editor) {
  struct buffer_t *buffer = editor->window->buffer;
  if (list_empty(buffer->undo_stack)) {
    editor_status_msg(editor, "Already at oldest change");
    return;
  }

  struct list_t *group = list_pop(buffer->undo_stack);

  struct gapbuf_t *gb = buffer->text;
  edit_action_t *action;
  LIST_FOREACH(group, action) {
    switch (action->type) {
    case EDIT_ACTION_INSERT:
      gb_del(gb, action->buf->len, action->pos + action->buf->len);
      editor->window->cursor = action->pos;
      break;
    case EDIT_ACTION_DELETE:
      gb_putstring(gb, action->buf->buf, action->buf->len, action->pos);
      editor->window->cursor = action->pos;
      break;
    }
  }

  list_prepend(buffer->redo_stack, group);
}

void editor_redo(struct editor_t* editor) {
  struct buffer_t *buffer = editor->window->buffer;
  if (list_empty(buffer->redo_stack)) {
    editor_status_msg(editor, "Already at newest change");
    return;
  }

  struct list_t *group = list_pop(buffer->redo_stack);

  struct gapbuf_t *gb = buffer->text;
  edit_action_t *action;
  LIST_FOREACH_REVERSE(group, action) {
    switch (action->type) {
    case EDIT_ACTION_INSERT:
      gb_putstring(gb, action->buf->buf, action->buf->len, action->pos);
      editor->window->cursor = action->pos + action->buf->len;
      break;
    case EDIT_ACTION_DELETE:
      gb_del(gb, action->buf->len, action->pos + action->buf->len);
      editor->window->cursor = action->pos;
      break;
    }
  }

  list_prepend(buffer->undo_stack, group);
}

void editor_start_action_group(struct editor_t *editor) {
  struct buffer_t *buffer = editor->window->buffer;
  struct list_t *group;
  LIST_FOREACH(buffer->redo_stack, group) {
    edit_action_t *action;
    LIST_FOREACH(group, action) {
      buf_free(action->buf);
      free(action);
    }
    list_free(group);
  }
  list_clear(buffer->redo_stack);

  list_prepend(buffer->undo_stack, list_create());
}

void editor_add_action(struct editor_t *editor, edit_action_t action) {
  struct buffer_t *buffer = editor->window->buffer;
  edit_action_t *action_copy = malloc(sizeof(edit_action_t));
  memcpy(action_copy, &action, sizeof action);
  struct list_t *group = list_peek(buffer->undo_stack);
  list_prepend(group, action_copy);
}
