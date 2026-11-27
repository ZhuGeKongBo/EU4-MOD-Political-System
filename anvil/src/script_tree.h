#pragma once

#include "script_lexer.h"

typedef struct ScriptItem {
    char key[GK_SCRIPT_WORD_SIZE];
    char word[GK_SCRIPT_WORD_SIZE];
    unsigned line, column, value_line, first, next;
    const char *start, *end;
    bool block;
} ScriptItem;

typedef struct ScriptTree {
    ScriptItem *items;
    char *owned_text;
    unsigned count, capacity;
    char path[PS_FILE_PATH_SIZE];
} ScriptTree;

bool script_tree_read(ScriptTree *tree, const char *text, size_t length, const char *path, ScriptError *error);
void script_tree_free(ScriptTree *tree);
const ScriptItem *script_item(const ScriptTree *tree, unsigned index);
const ScriptItem *script_field(const ScriptTree *tree, const ScriptItem *block, const char *key);
bool script_problem(const ScriptTree *tree, const ScriptItem *item, ScriptError *error, const char *reason);
bool script_keys(const ScriptTree *tree, const ScriptItem *block, const char *const *keys, size_t count,
                ScriptError *error);
bool script_word(const ScriptItem *item, const char *word);
bool script_number(const ScriptItem *item, int min, int max, int *number);
