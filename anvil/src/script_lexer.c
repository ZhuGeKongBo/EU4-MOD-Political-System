#include "script_lexer.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool error_at(Lexer *lexer, const char *reason) {
    if (!lexer->error->line) {
        lexer->error->line = lexer->line;
        lexer->error->column = lexer->column;
        snprintf(lexer->error->reason, sizeof(lexer->error->reason), "%s (found %.80s)",
                 reason, lexer->word);
    }
    return false;
}

Lexer lexer_for(const char *text, size_t size, ScriptError *error) {
    *error = (ScriptError){0};
    const char *start = size >= 3 && !memcmp(text, "\xef\xbb\xbf", 3) ? text + 3 : text;
    return (Lexer){ .cursor = start, .end = text + size, .line_start = start, .line = 1, .error = error };
}

TokenKind next_token(Lexer *lexer) {
    while (lexer->cursor != lexer->end) {
        const unsigned char ch = (unsigned char)*lexer->cursor;
        if (ch == '#') {
            while (lexer->cursor != lexer->end && *lexer->cursor != '\n') ++lexer->cursor;
        } else if (isspace(ch)) {
            if (*lexer->cursor++ == '\n') {
                ++lexer->line;
                lexer->line_start = lexer->cursor;
            }
        } else {
            break;
        }
    }
    lexer->column = (unsigned)(lexer->cursor - lexer->line_start) + 1;
    if (lexer->cursor == lexer->end) {
        strcpy(lexer->word, "end of file");
        return lexer->kind = TOKEN_END;
    }

    const char ch = *lexer->cursor;
    if (ch && strchr("={}", ch)) {
        lexer->word[0] = *lexer->cursor++;
        lexer->word[1] = '\0';
        return lexer->kind = TOKEN_PUNCTUATION;
    }

    const bool quoted = ch == '"';
    if (quoted) ++lexer->cursor;
    const char *start = lexer->cursor;
    while (lexer->cursor != lexer->end) {
        const unsigned char c = (unsigned char)*lexer->cursor;
        if (quoted ? (c == '"' || c < ' ') : (!c || isspace(c) || strchr("={}#\"", c))) break;
        ++lexer->cursor;
    }
    const size_t length = (size_t)(lexer->cursor - start);
    if ((!quoted && !length) || length >= sizeof(lexer->word) ||
        (quoted && (lexer->cursor == lexer->end || *lexer->cursor != '"'))) {
        strcpy(lexer->word, "invalid token");
        error_at(lexer, "empty, too long or unterminated token");
        return lexer->kind = TOKEN_INVALID;
    }
    memcpy(lexer->word, start, length);
    lexer->word[length] = '\0';
    if (quoted) ++lexer->cursor;
    return lexer->kind = TOKEN_WORD;
}

bool expect(Lexer *lexer, TokenKind kind, const char *value) {
    if (next_token(lexer) == kind && !strcmp(lexer->word, value)) return true;
    char reason[64];
    snprintf(reason, sizeof(reason), "expected '%s'", value);
    return error_at(lexer, reason);
}

bool punctuation(const Lexer *lexer, char ch) {
    return lexer->kind == TOKEN_PUNCTUATION && lexer->word[0] == ch;
}

bool named(const Lexer *lexer, const char *word) {
    return lexer->kind == TOKEN_WORD && !strcmp(lexer->word, word);
}

bool read_id(Lexer *lexer, const char *reason, char out[GK_SCRIPT_WORD_SIZE]) {
    static const char letters[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_";
    static const char identifier[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789";
    if (next_token(lexer) != TOKEN_WORD || !lexer->word[0] || !strchr(letters, lexer->word[0]) ||
        strspn(lexer->word, identifier) != strlen(lexer->word)) return error_at(lexer, reason);
    strcpy(out, lexer->word);
    return true;
}

bool read_integer(Lexer *lexer, int minimum, int maximum, const char *reason, int *out) {
    if (next_token(lexer) != TOKEN_WORD) return error_at(lexer, reason);
    const char *digits = lexer->word + (lexer->word[0] == '-');
    if (!*digits || strspn(digits, "0123456789") != strlen(digits)) return error_at(lexer, reason);
    const long number = strtol(lexer->word, NULL, 10);
    if (number < minimum || number > maximum) return error_at(lexer, reason);
    *out = (int)number;
    return true;
}

