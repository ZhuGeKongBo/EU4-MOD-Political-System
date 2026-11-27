#pragma once

#include <anvil/anvil_plugin.h>
#include "native_clause.h"

AnvilResult install_native_eu4(const AnvilHost *host, AnvilPlugin *self);
bool compile_native_eu4(NativeClause *clause, NativeClauseKind kind, ScriptError *error);
void release_native_eu4(NativeClause *clause);
const char *test_native_eu4(const NativeClause *clause, const NativeScope *scope, uint64_t root, uint64_t from, bool *allowed);
bool native_effect_callable(const NativeClause *clause);
void execute_native_eu4(const NativeClause *clause, const NativeScope *scope, uint64_t root, uint64_t from);
int32_t native_eu4_weight(const NativeClause *clause, const NativeScope *scope, uint64_t root, uint64_t from);
const void *native_eu4_modifier(const NativeClause *clause);
