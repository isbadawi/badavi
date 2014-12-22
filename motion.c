#include "motion.h"

#include <ctype.h>

#include <termbox.h>

#include "util.h"

static pos_t left(pos_t pos, window_t *window) {
  pos.offset = max(pos.offset - 1, 0);
  return pos;
}

static pos_t right(pos_t pos, window_t *window) {
  int len = pos.line->buf->len;
  pos.offset = min(pos.offset + 1, len);
  return pos;
}

static pos_t up(pos_t pos, window_t *window) {
  if (!pos.line->prev->buf) {
    return pos;
  }
  pos.line = pos.line->prev;
  pos.offset = min(pos.offset, pos.line->buf->len);
  return pos;
}

static pos_t down(pos_t pos, window_t *window) {
  if (!pos.line->next) {
    return pos;
  }
  pos.line = pos.line->next;
  pos.offset = min(pos.offset, pos.line->buf->len);
  return pos;
}

static pos_t line_start(pos_t pos, window_t *window) {
  pos.offset = 0;
  return pos;
}

static pos_t line_end(pos_t pos, window_t *window) {
  pos.offset = pos.line->buf->len;
  return pos;
}

static pos_t buffer_top(pos_t pos, window_t *window) {
  pos.line = window->buffer->head->next;
  pos.offset = 0;
  return pos;
}

static pos_t buffer_bottom(pos_t pos, window_t *window) {
  pos.line = buffer_get_line(
      window->buffer, window->buffer->nlines - 1);
  pos.offset = 0;
  return pos;
}

static char char_at(pos_t pos) {
  return pos.line->buf->buf[pos.offset];
}

static pos_t next_char(pos_t pos) {
  int len = pos.line->buf->len;
  if (pos.offset < len - 1 || (pos.offset == len - 1 && !pos.line->next)) {
    pos.offset++;
  } else if (pos.line->next) {
    pos.line = pos.line->next;
    pos.offset = 0;
  }
  return pos;
}

static int is_word_char(char c) {
  return isalnum(c) || c == '_';
}

static int is_word_start(pos_t pos) {
  char c = char_at(pos);
  if (isspace(c)) {
    return 0;
  }
  if (pos.offset == 0) {
    return 1;
  }
  pos.offset--;
  char last = char_at(pos);
  if (isspace(last)) {
    return 1;
  }
  return is_word_char(c) + is_word_char(last) == 1;
}

static int last_char(pos_t pos) {
  return pos.offset == pos.line->buf->len && !pos.line->next;
}

static pos_t next_word_start(pos_t pos, window_t *window) {
  do {
    pos = next_char(pos);
  } while (!is_word_start(pos) && !last_char(pos));
  return pos;
}

int motion_key(motion_t *motion, struct tb_event *ev) {
  if (!isdigit(motion->last)) {
    motion->count = 0;
  }

  switch (ev->ch) {
  case '$': motion->op = line_end; break;
  case '0':
    if (motion->count > 0) {
      motion->count *= 10;
    } else {
      motion->op = line_start;
    }
    break;
  case '1': case '2': case '3': case '4': case '5':
  case '6': case '7': case '8': case '9':
    motion->count *= 10;
    motion->count += ev->ch - '0';
    break;
  case 'h': motion->op = left; break;
  case 'j': motion->op = down; break;
  case 'k': motion->op = up; break;
  case 'l': motion->op = right; break;
  case 'w': motion->op = next_word_start; break;
  // TODO(isbadawi): G should jump to line "count", but ops can't access that.
  case 'G': motion->op = buffer_bottom; break;
  case 'g':
    if (motion->last == 'g') {
      motion->op = buffer_top;
    }
    break;
  default:
    motion->op = NULL;
    motion->count = 0;
    motion->last = 0;
    return -1;
  }
  motion->last = ev->ch;
  return motion->op != NULL;
}

pos_t motion_apply(motion_t *motion, window_t *window) {
  pos_t result = window->cursor;
  int n = motion->count ? motion->count : 1;
  for (int i = 0; i < n; ++i) {
    result = motion->op(result, window);
  }

  motion->op = NULL;
  motion->count = 0;
  motion->last = 0;
  return result;
}
