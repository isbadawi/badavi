#pragma once

#include <stdbool.h>
#include <stddef.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

struct syntax_token {
  enum syntax_token_kind {
    SYNTAX_TOKEN_NONE,
    SYNTAX_TOKEN_COMMENT,
    SYNTAX_TOKEN_IDENTIFIER,
    SYNTAX_TOKEN_LITERAL_CHAR,
    SYNTAX_TOKEN_LITERAL_NUMBER,
    SYNTAX_TOKEN_LITERAL_STRING,
    SYNTAX_TOKEN_PREPROC,
    SYNTAX_TOKEN_PUNCTUATION,
    SYNTAX_TOKEN_STATEMENT,
    SYNTAX_TOKEN_TYPE,
  } kind;
  size_t pos;
  size_t len;
};

struct syntax;
typedef void (*tokenizer_func)(struct syntax*, struct syntax_token*, size_t);
struct syntax {
  struct buffer *buffer;
  tokenizer_func tokenizer;
  size_t pos;
  enum syntax_state {
    STATE_INIT,
    STATE_PREPROC
  } state;
  pcre2_code *regex;
  pcre2_match_data *groups;
};

char *syntax_detect_filetype(char *path);
bool syntax_init(struct syntax *syntax, struct buffer *buffer);
void syntax_deinit(struct syntax *syntax);
void syntax_token_at(struct syntax *syntax, struct syntax_token *token, size_t pos);
