#include "script_tree.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_SCRIPT_NODES = 24000, MAX_SCRIPT_DEPTH = 48 };

static unsigned add_item(ScriptTree *tree) {
    if (tree->count == MAX_SCRIPT_NODES) return 0;
    if (tree->count == tree->capacity) {
        unsigned capacity = tree->capacity * 2;
        if (capacity > MAX_SCRIPT_NODES) capacity = MAX_SCRIPT_NODES;
        ScriptItem *items = realloc(tree->items, capacity * sizeof(*items));
        if (!items) return 0;
        tree->items = items;
        tree->capacity = capacity;
    }
    const unsigned index = tree->count++;
    tree->items[index] = (ScriptItem){0};
    return index;
}

static bool read_items(ScriptTree *tree, unsigned parent, Lexer *lexer, unsigned depth) {
    if (depth == MAX_SCRIPT_DEPTH) return error_at(lexer, "script nesting limit exceeded");
    unsigned previous = 0;
    while (next_token(lexer) == TOKEN_WORD) {
        const unsigned current = add_item(tree);
        if (!current) return error_at(lexer, "script is too large to parse");
        ScriptItem *item = &tree->items[current];
        item->line = lexer->line;
        item->column = lexer->column;
        if (previous) tree->items[previous].next = current;
        else tree->items[parent].first = current;
        previous = current;
        Lexer lookahead = *lexer;
        next_token(&lookahead);
        if (!punctuation(&lookahead, '=')) {
            strcpy(item->word, lexer->word);
            continue;
        }
        strcpy(item->key, lexer->word);
        *lexer = lookahead;
        item->start = lexer->cursor;
        item->value_line = lexer->line;
        if (next_token(lexer) == TOKEN_WORD) {
            strcpy(item->word, lexer->word);
        } else if (punctuation(lexer, '{')) {
            item->block = true;
            if (!read_items(tree, current, lexer, depth + 1)) return false;
        } else {
            return error_at(lexer, "expected a value or block after '='");
        }
        tree->items[current].end = lexer->cursor;
    }
    return (depth ? punctuation(lexer, '}') : lexer->kind == TOKEN_END) ||
           error_at(lexer, depth ? "expected '}'" : "expected an assignment");
}

bool script_tree_read(ScriptTree *tree, const char *text, size_t length, const char *path, ScriptError *error) {
    *tree = (ScriptTree){ .capacity = 64, .count = 1 };
    snprintf(tree->path, sizeof(tree->path), "%s", path);
    Lexer lexer = lexer_for(text, length, error);
    tree->items = calloc(tree->capacity, sizeof(*tree->items));
    if (tree->items && read_items(tree, 0, &lexer, 0)) return true;
    if (!error->line) script_problem(tree, NULL, error, "out of memory reading script");
    snprintf(error->path, sizeof(error->path), "%s", path);
    script_tree_free(tree);
    return false;
}

void script_tree_free(ScriptTree *tree) {
    free(tree->items);
    free(tree->owned_text);
    tree->owned_text = NULL;
    tree->items = NULL;
    tree->count = tree->capacity = 0;
}

const ScriptItem *script_item(const ScriptTree *tree, unsigned index) {
    return index ? &tree->items[index] : NULL;
}

const ScriptItem *script_field(const ScriptTree *tree, const ScriptItem *block, const char *key) {
    for (unsigned i = block->first; i; i = tree->items[i].next)
        if (!strcmp(tree->items[i].key, key)) return &tree->items[i];
    return NULL;
}

bool script_problem(const ScriptTree *tree, const ScriptItem *item, ScriptError *error, const char *reason) {
    *error = (ScriptError){ .line = item ? item->line : 1, .column = item ? item->column : 1 };
    snprintf(error->path, sizeof(error->path), "%s", tree->path);
    snprintf(error->reason, sizeof(error->reason), "%s%s%.74s", reason,
             item && item->key[0] ? ": " : "", item ? item->key : "");
    return false;
}

bool script_keys(const ScriptTree *tree, const ScriptItem *block, const char *const *keys, size_t count,
                 ScriptError *error) {
    if (!block->block) return script_problem(tree, block, error, "expected a block");
    uint64_t seen = 0;
    for (unsigned i = block->first; i; i = tree->items[i].next) {
        const ScriptItem *item = &tree->items[i];
        size_t k = 0;
        while (k < count && strcmp(item->key, keys[k])) ++k;
        if (k == count) return script_problem(tree, item, error, "unknown field");
        const uint64_t mask = UINT64_C(1) << k;
        if (seen & mask) return script_problem(tree, item, error, "duplicate field");
        seen |= mask;
    }
    return true;
}

bool script_word(const ScriptItem *item, const char *word) {
    return item && !item->block && !strcmp(item->word, word);
}

bool script_number(const ScriptItem *item, int min, int max, int *number) {
    if (!item || item->block) return false;
    const char *digits = item->word + (item->word[0] == '-');
    if (!*digits || strspn(digits, "0123456789") != strlen(digits) || strlen(digits) > 9) return false;
    const long value = strtol(item->word, NULL, 10);
    if (value < min || value > max) return false;
    *number = (int)value;
    return true;
}
