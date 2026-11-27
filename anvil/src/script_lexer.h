#pragma once
#include "script_types.h"

typedef enum TokenKind { TOKEN_END, TOKEN_WORD, TOKEN_PUNCTUATION, TOKEN_INVALID } TokenKind;

typedef struct Lexer {
    const char *cursor, *end, *line_start;
    unsigned line, column;
    TokenKind kind;
    char word[GK_SCRIPT_WORD_SIZE];
    ScriptError *error;
} Lexer;

bool error_at(Lexer *lexer, const char *reason);
Lexer lexer_for(const char *text, size_t size, ScriptError *error);
TokenKind next_token(Lexer *lexer);
bool expect(Lexer *lexer, TokenKind kind, const char *value);
bool punctuation(const Lexer *lexer, char ch);
bool named(const Lexer *lexer, const char *word);
bool read_id(Lexer *lexer, const char *reason, char out[GK_SCRIPT_WORD_SIZE]);
bool read_integer(Lexer *lexer, int minimum, int maximum, const char *reason, int *out);
