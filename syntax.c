#include "syntax.h"

#include <ctype.h>
#include <string.h>

#include "buffer.h"
#include "gap.h"
#include "util.h"

char *c_types[] = {
  "char", "short", "int", "long", "signed", "unsigned", "void",
  "float", "double", "struct", "enum", "union", "typedef",
  "size_t", "ssize_t", "off_t", "ptrdiff_t", "sig_atomic_t",
  "clock_t", "time_t", "va_list", "jmp_buf", "FILE", "DIR",
  "bool", "_Bool", "int8_t", "uint8_t", "int16_t", "uint16_t",
  "int32_t", "uint32_t", "int64_t", "uint64_t", "intptr_t", "uintptr_t",
  // These are not really types but just for the purposes of highlighting
  "auto", "const", "extern", "inline", "register", "restrict",
  "static", "volatile",
};

char *c_preproc[] = {
  "#include", "#define", "#ifdef", "#else", "#error", "#pragma",
  "NULL", "true", "false"
};

char *c_stmts[] = {
  "asm", "break", "case", "continue", "default", "do", "else", "for", "goto",
  "if", "return", "sizeof", "switch", "while",
};

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

static bool gb_startswith_at(struct gapbuf *gb, size_t pos, char *prefix) {
  size_t len = strlen(prefix);
  for (size_t i = 0; i < len; ++i) {
    if (gb_getchar(gb, pos + i) != prefix[i]) {
      return false;
    }
  }
  return true;
}

static bool is_word_char(char c) {
  return isalnum(c) || c == '_';
}

static size_t gb_startswith_any_at(
    struct gapbuf *gb, size_t pos, char **prefixes, size_t num_prefixes) {
  for (size_t i = 0; i < num_prefixes; ++i) {
    size_t len = strlen(prefixes[i]);
    if (gb_startswith_at(gb, pos, prefixes[i]) &&
        !is_word_char(gb_getchar(gb, pos + len))) {
      return len;
    }
  }
  return 0;
}

static size_t gb_findstring(struct gapbuf *gb, size_t from, char *s) {
  size_t i = 0;
  size_t size = gb_size(gb);
  while (from + i < size && !gb_startswith_at(gb, from + i, s)) {
    i++;
  }
  return from + i;
}

static void c_next_token(struct syntax *syntax, struct syntax_token *token) {
  struct gapbuf *gb = syntax->buffer->text;
  size_t size = gb_size(gb);
  char ch = gb_getchar(gb, syntax->pos);
  size_t len;

#define RETURN_TOKEN(type, len_) \
    token->pos = syntax->pos; \
    token->kind = SYNTAX_TOKEN_##type; \
    token->len = len_; \
    syntax->pos += token->len; \
    return;

  if (isspace(ch)) {
    RETURN_TOKEN(IDENTIFIER, 1);
  }

  if (isdigit(ch)) {
    RETURN_TOKEN(LITERAL_NUMBER, 1);
  }

  len = gb_startswith_any_at(gb, syntax->pos, c_types, ARRAY_SIZE(c_types));
  if (len) {
    RETURN_TOKEN(TYPE, len);
  }

  len = gb_startswith_any_at(gb, syntax->pos, c_stmts, ARRAY_SIZE(c_stmts));
  if (len) {
    RETURN_TOKEN(STATEMENT, len);
  }

  len = gb_startswith_any_at(gb, syntax->pos, c_preproc, ARRAY_SIZE(c_preproc));
  if (len) {
    // TODO(ibadawi): State machine to highlight preprocessor differently
    RETURN_TOKEN(PREPROC, len);
  }

  if (gb_startswith_at(gb, syntax->pos, "//")) {
    size_t end = gb_indexof(gb, '\n', syntax->pos);
    RETURN_TOKEN(COMMENT, end - syntax->pos);
  }

  if (gb_startswith_at(gb, syntax->pos, "/*")) {
    size_t end = gb_findstring(gb, syntax->pos, "*/");
    if (end != size) {
      end += 2;
    }
    RETURN_TOKEN(COMMENT, end - syntax->pos);
  }

  // FIXME(ibadawi): Handle escaped backslashes
  if (ch == '"') {
    size_t start = syntax->pos + 1;
    size_t end;
    do {
      end = gb_indexof(gb, '"', start);
      start = end + 1;
    } while (end < size && gb_getchar(gb, end - 1) == '\\');
    RETURN_TOKEN(LITERAL_STRING, end - syntax->pos + 1);
  }

  if (ch == '\'') {
    size_t start = syntax->pos + 1;
    size_t end;
    do {
      end = gb_indexof(gb, '\'', start);
      start = end + 1;
    } while (end < size && gb_getchar(gb, end - 1) == '\\');
    RETURN_TOKEN(LITERAL_CHAR, end - syntax->pos + 1);
  }

  if (ispunct(ch)) {
    RETURN_TOKEN(PUNCTUATION, 1);
  }

  size_t end = syntax->pos + 1;
  while (is_word_char(gb_getchar(gb, end++))) {}
  RETURN_TOKEN(IDENTIFIER, end - syntax->pos - 1);
#undef RETURN_TOKEN
}

static void c_token_at(struct syntax *syntax, struct syntax_token *token, size_t pos) {
  do {
    c_next_token(syntax, token);
  } while (!(token->pos <= pos && pos < token->pos + token->len));
}

static struct filetype {
  char *name;
  char *exts[2];
  tokenizer_func tokenizer;
} supported_filetypes[] = {
  {"c", {"c", "h"}, c_token_at},
};

char *syntax_detect_filetype(char *path) {
  char *dot = strrchr(path, '.');
  if (!dot || dot == path) {
    return "";
  }
  for (size_t i = 0; i < ARRAY_SIZE(supported_filetypes); ++i) {
    struct filetype *filetype = &supported_filetypes[i];
    for (int i = 0; i < 2; ++i) {
      if (!strcmp(dot + 1, filetype->exts[i])) {
        return filetype->name;
      }
    }
  }
  return "";
}

static tokenizer_func tokenizer_for(char *type) {
  for (size_t i = 0; i < ARRAY_SIZE(supported_filetypes); ++i) {
    struct filetype *filetype = &supported_filetypes[i];
    if (!strcmp(type, filetype->name)) {
      return filetype->tokenizer;
    }
  }
  return NULL;
}

bool syntax_init(struct syntax *syntax, struct buffer *buffer) {
  syntax->buffer = buffer;
  syntax->tokenizer = tokenizer_for(buffer->opt.filetype);
  if (!syntax->tokenizer) {
    return false;
  }
  syntax->pos = 0;
  return true;
}

void syntax_token_at(struct syntax *syntax, struct syntax_token *token, size_t pos) {
  syntax->tokenizer(syntax, token, pos);
}
