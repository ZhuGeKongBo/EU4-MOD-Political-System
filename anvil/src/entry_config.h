#pragma once
#include "ps_script.h"
#include "native_clause.h"

enum { ENTRY_MAX_NODES = 32 };
typedef enum EntryTest {
    ENTRY_AND, ENTRY_OR, ENTRY_NOT, ENTRY_ALWAYS, ENTRY_TAG, ENTRY_COUNTRY_FLAG, ENTRY_QUERY,
    ENTRY_RELIGION, ENTRY_GOVERNMENT, ENTRY_HEIR, ENTRY_NATIVE
} EntryTest;
typedef struct EntryNode {
    EntryTest test;
    unsigned end;  /* One past the entire subtree in preorder. */
    PsOperation operation;
    union {
        PsArguments args;
        NativeClause native;
    };
} EntryNode;
typedef struct EntryCondition {
    unsigned count;
    EntryNode nodes[ENTRY_MAX_NODES];
} EntryCondition;
typedef struct EntryButton {
    char region[GK_SCRIPT_WORD_SIZE], control[GK_SCRIPT_WORD_SIZE], tooltip[GK_SCRIPT_WORD_SIZE];
    char title_key[GK_SCRIPT_WORD_SIZE], panel[GK_SCRIPT_WORD_SIZE];
    char layout[GK_SCRIPT_WORD_SIZE], background[GK_SCRIPT_WORD_SIZE];
    char icon[GK_SCRIPT_WORD_SIZE], disabled_tooltip[GK_SCRIPT_WORD_SIZE];
    EntryCondition view, visible, enabled;
    bool defined, toggle, close_button, close_on_escape, has_order;
    int order;
} EntryButton;
typedef const char *(*EntryQuery)(PsOperation, const PsArguments *, unsigned, bool *);
typedef const char *(*CountryFlagQuery)(unsigned country, const char *flag, bool *result);
typedef const char *(*CountryReligionQuery)(unsigned country, const char *religion, bool *result);
typedef const char *(*CountryHeirQuery)(unsigned country, bool *result);
typedef struct EntryContext {
    const char *instance;
    unsigned viewer;
    EntryQuery query;
    CountryFlagQuery country_flag;
    CountryReligionQuery country_religion;
    CountryReligionQuery country_government;
    CountryHeirQuery country_heir;
    unsigned from;
    const char *(*native_condition)(const NativeClause *, const NativeScope *, bool *);
} EntryContext;

typedef struct EntryAccess {
    bool view, visible, enabled;
} EntryAccess;

const char *entry_evaluate(const EntryCondition *condition, const EntryContext *context, bool *allowed);
const char *entry_evaluate_node(const EntryNode *nodes, unsigned root, const EntryContext *context, bool *allowed);
const char *entry_permissions(const EntryButton *entry, const EntryContext *context, EntryAccess *access);
const char *entry_check(const EntryCondition *condition, const char *instance, unsigned actor,
                        EntryQuery query, bool *allowed);
unsigned government_category(const char *key);
