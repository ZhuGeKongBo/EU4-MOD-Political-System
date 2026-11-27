#pragma once

#include "political_script.h"
#include "script_tree.h"

#define PS_TYPE_DIRECTORY "common/political_systems/types"
#define PS_LAYOUT_DIRECTORY "common/political_system_layouts"
#define PS_HISTORY_DIRECTORY "history/political_systems"

typedef struct TypeDocument TypeDocument;
typedef struct TypeDeclaration TypeDeclaration;
typedef struct ContentTypes {
    TypeDocument *files;
    TypeDeclaration *items;
} ContentTypes;

bool read_entry_group(Lexer *lexer, EntryCondition *condition, EntryTest kind);
bool collect_content_types(ContentTypes *types, const char *text, size_t size, const char *path, ScriptError *error);
bool has_content_type(const ContentTypes *types, const char *id);
const KhanDefinition *resolve_content_type(ContentTypes *types, const char *id, ScriptError *error);
void validate_unused_types(ContentTypes *types, void (*report)(const ScriptError *));
void visit_concrete_types(const ContentTypes *types, void (*visit)(const char *, void *), void *context);
void free_content_types(ContentTypes *types);
typedef bool (*NativeClauseVisitor)(NativeClause *, NativeClauseKind, ScriptError *);
bool visit_native_clauses(KhanDefinition *definition, NativeClauseVisitor visit, ScriptError *error);
bool append_content_history(const char *text, size_t size, const char *path, PoliticalRoster *roster,
                            ScriptError *error);
