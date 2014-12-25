#include "motion.h"

#include <ctype.h>
#include <string.h>

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

static pos_t prev_char(pos_t pos) {
  if (pos.offset > 0) {
    pos.offset--;
  } else if (pos.line->prev->buf != NULL) {
    pos.line = pos.line->prev;
    int len = pos.line->buf->len;
    pos.offset = len ? len - 1 : 0;
  }
  return pos;
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

static int is_WORD_start(pos_t pos) {
  char c = char_at(pos);
  if (isspace(c)) {
    return 0;
  }
  if (pos.offset == 0) {
    return 1;
  }
  pos.offset--;
  return isspace(char_at(pos));
}

static int is_word_end(pos_t pos) {
  char c = char_at(pos);
  if (isspace(c)) {
    return 0;
  }
  if (pos.offset == pos.line->buf->len - 1) {
    return 1;
  }
  pos.offset++;
  char next = char_at(pos);
  if (isspace(next)) {
    return 1;
  }
  return is_word_char(c) + is_word_char(next) == 1;
}

static int is_WORD_end(pos_t pos) {
  char c = char_at(pos);
  if (isspace(c)) {
    return 0;
  }
  if (pos.offset == pos.line->buf->len - 1) {
    return 1;
  }
  pos.offset++;
  return isspace(char_at(pos));
}

static int first_char(pos_t pos) {
  return pos.offset == 0 && pos.line->prev->buf == NULL;
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

static pos_t next_WORD_start(pos_t pos, window_t *window) {
  do {
    pos = next_char(pos);
  } while (!is_WORD_start(pos) && !last_char(pos));
  return pos;
}

static pos_t prev_word_start(pos_t pos, window_t *window) {
  do {
    pos = prev_char(pos);
  } while (!is_word_start(pos) && !first_char(pos));
  return pos;
}

static pos_t prev_WORD_start(pos_t pos, window_t *window) {
  do {
    pos = prev_char(pos);
  } while (!is_WORD_start(pos) && !first_char(pos));
  return pos;
}

static pos_t next_word_end(pos_t pos, window_t *window) {
  do {
    pos = next_char(pos);
  } while (!is_word_end(pos) && !last_char(pos));
  return pos;
}

static pos_t next_WORD_end(pos_t pos, window_t *window) {
  do {
    pos = next_char(pos);
  } while (!is_WORD_end(pos) && !last_char(pos));
  return pos;
}

static pos_t prev_word_end(pos_t pos, window_t *window) {
  do {
    pos = prev_char(pos);
  } while (!is_word_end(pos) && !first_char(pos));
  return pos;
}

static pos_t prev_WORD_end(pos_t pos, window_t *window) {
  do {
    pos = prev_char(pos);
  } while (!is_WORD_end(pos) && !first_char(pos));
  return pos;
}

static int is_paragraph_start(line_t *line) {
  return line->prev->buf == NULL ||
    (!line->buf->len && line->next && line->next->buf->len);
}

static int is_paragraph_end(line_t *line) {
  return line->next == NULL ||
    (!line->buf->len && line->prev && line->prev->buf->len);
}

static pos_t paragraph_start(pos_t pos, window_t *window) {
  pos.offset = 0;
  if (pos.line->prev->buf) {
    do {
      pos.line = pos.line->prev;
    } while (!is_paragraph_start(pos.line));
  }
  return pos;
}

static pos_t paragraph_end(pos_t pos, window_t *window) {
  if (!pos.line->next) {
    pos.offset = pos.line->buf->len - 1;
  } else {
    do {
      pos.line = pos.line->next;
    } while (!is_paragraph_end(pos.line));
    pos.offset = 0;
  }
  return pos;
}

static motion_t motion_table[] = {
  {"h", left},
  {"j", down},
  {"k", up},
  {"l", right},
  {"0", line_start},
  {"$", line_end},
  {"{", paragraph_start},
  {"}", paragraph_end},
  {"b", prev_word_start},
  {"B", prev_WORD_start},
  {"w", next_word_start},
  {"W", next_WORD_start},
  {"e", next_word_end},
  {"E", next_WORD_end},
  {"ge", prev_word_end},
  {"gE", prev_WORD_end},
  // TODO(isbadawi): G should jump to line "count", but ops can't access that.
  {"G", buffer_bottom},
  {"gg", buffer_top},
  // TODO(isbadawi): What about {t,f,T,F}{char}?
  {NULL, NULL}
};

static motion_t *motion_find(char *name) {
  for (int i = 0; motion_table[i].name != NULL; ++i) {
    if (!strcmp(motion_table[i].name, name)) {
      return &motion_table[i];
    }
  }
  return NULL;
}

static int motion_exists_with_prefix(char *name) {
  for (int i = 0; motion_table[i].name != NULL; ++i) {
    if (!strncmp(motion_table[i].name, name, strlen(name))) {
      return 1;
    }
  }
  return 0;
}

int motion_key(motion_t *motion, struct tb_event *ev) {
  if (!isdigit(motion->last)) {
    motion->count = 0;
  }

  if (isdigit(ev->ch) && !(motion->count == 0 && ev->ch == '0')) {
    motion->count *= 10;
    motion->count += ev->ch - '0';
    motion->last = ev->ch;
    return 0;
  }

  motion_t *m = NULL;
  char name[3];
  if (motion->last) {
    name[0] = motion->last;
    name[1] = ev->ch;
    name[2] = '\0';
    m = motion_find(name);
  }
  if (!m) {
    name[0] = ev->ch;
    name[1] = '\0';
    m = motion_find(name);
  }
  if (m) {
    motion->name = m->name;
    motion->op = m->op;
    return 1;
  }
  if (motion_exists_with_prefix(name)) {
    motion->last = ev->ch;
    return 0;
  }
  return -1;
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
