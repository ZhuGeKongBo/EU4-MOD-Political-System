#pragma once

#include "script_types.h"

typedef struct ScriptTree ScriptTree;
typedef struct ScriptItem ScriptItem;

typedef enum NativeClauseKind {
    NATIVE_CONDITION, NATIVE_EFFECT, NATIVE_COUNTRY_MODIFIER, NATIVE_PROVINCE_MODIFIER, NATIVE_WEIGHT,
} NativeClauseKind;

typedef struct NativeClause {
    const char *text;
    const char *path;
    size_t length;
    unsigned line, column, value_line;
    const void *program;
} NativeClause;

typedef struct NativeScope {
    unsigned root, from;
    unsigned province;
    const char *instance;
} NativeScope;

bool read_native_clause(const ScriptTree *tree, const ScriptItem *item, NativeClause *out, ScriptError *error);
bool native_clause_present(const NativeClause *clause);
unsigned char *native_script_zip(const char *path, const char *text, size_t length, size_t *archive_length);
