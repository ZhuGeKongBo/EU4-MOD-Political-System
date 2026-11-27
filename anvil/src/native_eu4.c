#include "native_eu4.h"
#include "operators.h"
#include "symbols.gen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The sizes, members and calls come from the stock decree reader and CEventScope
   constructors on Steam 1.37.5. The objects are never inserted into the decree DB. */
enum {
    LEXER_SIZE = 0x220, READER_SIZE = 0x6a8, DECREE_SIZE = 0x350, SCOPE_SIZE = 0xd0,
    DECREE_MODIFIER = 0x40, DECREE_WEIGHT = 0x1b8, DECREE_EFFECT = 0x1f0, DECREE_CONDITION = 0x260,
    CONDITION_READER = 0x38, LEXER_FILE = 8, READER_ERROR = 0x20, READER_KEY = 0x34,
    READER_VALUE_KIND = 0x440, READER_BLOCK_TOKEN = 3, SCOPE_FROM = 0x18,
    MAX_NATIVE_DEPTH = 64,
};

static const char clause_keyword[] = "pf_clause";

typedef struct Parser {
    void *(*allocate)(size_t);
    void (*assign)(GameString *, const char *, size_t);
    void (*drop_string)(GameString *);
    void *(*lexer)(void *, GameString *);
    void *(*reader)(void *, void *, const GameString *, bool);
    void (*drop_reader)(void *);
    void (*read_key)(void *);
    void (*read_block)(void *, void *);
    void *(*decree)(void *, GameString *, unsigned);
    void *(*scope)(void *, uint64_t, unsigned, unsigned);
    void (*drop_scope)(void *);
    bool (*test)(const void *, const void *);
    void (*effect)(const void *, void *, unsigned);
    int32_t *(*weight)(const void *, int32_t *, const void *, bool);
    int (*mount)(const void *, uint64_t, void (*)(void *), const char *, const char *, int);
    int (*unmount)(const char *);
    const char *(*source)(const char *);
    void (*report)(void *, const GameString *);
    bool available;
} Parser;

static Parser api;
static unsigned sequence;
static _Thread_local ScriptError *active_error;
static _Thread_local unsigned active_line;
static _Thread_local const void *trigger_stack[MAX_NATIVE_DEPTH];
static _Thread_local const void *effect_stack[MAX_NATIVE_DEPTH];
static _Thread_local unsigned trigger_depth, effect_depth;
static _Thread_local bool recursive_trigger;

static void *pointer(const void *object, size_t offset) {
    void *value;
    memcpy(&value, (const unsigned char *)object + offset, sizeof(value));
    return value;
}

static void destroy(void *object) {
    void (*drop)(void *, unsigned) = pointer(pointer(object, 0), 0);
    drop(object, 1);
}

static void report_parse_error(void *reader, const GameString *message) {
    if (active_error && !active_error->reason[0]) {
        const unsigned line = native_reader_line(reader);
        if (line && active_line) active_error->line = active_line + line - 1;
        const char *text = message->capacity < 16 ? message->inline_text : pointer(message, 0);
        snprintf(active_error->reason, sizeof(active_error->reason), "EU4 rejected script: %.155s", text ? text : "");
    }
    api.report(reader, message);
}

AnvilResult install_native_eu4(const AnvilHost *host, AnvilPlugin *self) {
    api = (Parser){
        .allocate = host->address_of(self, SYM_GAME_NEW),
        .assign = host->address_of(self, SYM_ASSIGN_STRING),
        .drop_string = host->address_of(self, SYM_FREE_STRING),
        .lexer = host->address_of(self, SYM_NATIVE_CONSTRUCT_TEXT_FILE_LEXER),
        .reader = host->address_of(self, SYM_NATIVE_SCRIPT_READER),
        .drop_reader = host->address_of(self, SYM_NATIVE_DROP_READER),
        .read_key = host->address_of(self, SYM_NATIVE_READER_ASSIGNMENT),
        .read_block = host->address_of(self, SYM_NATIVE_READ_BLOCK),
        .decree = host->address_of(self, SYM_NATIVE_DECREE_STORAGE),
        .scope = host->address_of(self, SYM_NATIVE_EVENT_SCOPE),
        .drop_scope = host->address_of(self, SYM_NATIVE_DROP_SCOPE),
        .test = host->address_of(self, SYM_NATIVE_CONDITION_TEST),
        .effect = host->address_of(self, SYM_NATIVE_EFFECT_RUN),
        .weight = host->address_of(self, SYM_NATIVE_SCRIPT_WEIGHT),
        .mount = host->address_of(self, SYM_NATIVE_MOUNT_MEMORY),
        .unmount = host->address_of(self, SYM_NATIVE_UNMOUNT_MEMORY),
        .source = host->address_of(self, SYM_MOD_FILE_SOURCE),
    };
    if (!api.allocate || !api.assign || !api.drop_string || !api.lexer || !api.reader || !api.drop_reader ||
        !api.read_key || !api.read_block || !api.decree || !api.scope || !api.drop_scope || !api.test || !api.effect ||
        !api.weight || !api.mount || !api.unmount || !api.source) return ANVIL_ERROR_UNSUPPORTED;
    AnvilResult result = host->hook_function(self, SYM_SCRIPT_READER_ERROR, (void *)report_parse_error,
                                            (void **)&api.report, NULL);
    api.available = result == ANVIL_OK;
    return result;
}

static bool parse_program(void *object, NativeClauseKind kind, const char *path, unsigned value_line, ScriptError *error) {
    _Alignas(16) unsigned char reader[READER_SIZE] = {0};
    GameString filename = {.capacity = 15};
    api.assign(&filename, path, strlen(path));
    void *lexer = api.allocate(LEXER_SIZE);
    if (!lexer) {
        api.drop_string(&filename);
        return false;
    }
    api.lexer(lexer, &filename);
    api.drop_string(&filename);
    if (!pointer(lexer, LEXER_FILE)) {
        destroy(lexer);
        strcpy(error->reason, "EU4 could not open the temporary script");
        return false;
    }
    api.assign(&filename, error->path, strlen(error->path));
    api.reader(reader, lexer, &filename, true);
    api.drop_string(&filename);

    const size_t offset = kind == NATIVE_CONDITION ? DECREE_CONDITION + CONDITION_READER :
                          kind == NATIVE_EFFECT ? DECREE_EFFECT : kind == NATIVE_WEIGHT ? DECREE_WEIGHT : DECREE_MODIFIER;
    ScriptError *previous = active_error;
    const unsigned previous_line = active_line;
    active_error = error;
    active_line = value_line;
    const OperatorOrigin origin = operators_origin((OperatorOrigin){.path = error->path, .line = value_line});
    api.read_key(reader);
    int value_kind;
    memcpy(&value_kind, reader + READER_VALUE_KIND, sizeof(value_kind));
    if (!reader[READER_ERROR] && !error->reason[0]) {
        if (value_kind == READER_BLOCK_TOKEN && !memcmp(reader + READER_KEY, clause_keyword, sizeof(clause_keyword)))
            api.read_block((unsigned char *)object + offset, reader);
        else
            strcpy(error->reason, "EU4 could not read the temporary script header");
    }
    operators_origin(origin);
    active_error = previous;
    active_line = previous_line;
    const bool failed = reader[READER_ERROR] != 0;
    if (failed && !error->reason[0]) {
        const unsigned line = native_reader_line(reader);
        if (line && value_line) error->line = value_line + line - 1;
        strcpy(error->reason, "EU4 rejected a value in this clause. See the game error log");
    }
    api.drop_reader(reader);
    return !failed && !error->reason[0];
}

bool compile_native_eu4(NativeClause *clause, NativeClauseKind kind, ScriptError *error) {
    if (!clause->text) return true;
    *error = (ScriptError){ .line = clause->line, .column = clause->column };
    snprintf(error->path, sizeof(error->path), "%s", clause->path);
    if (!api.available) {
        strcpy(error->reason, "native EU4 script engine unavailable");
        return false;
    }
    const char assignment[] = "pf_clause = ";
    const size_t length = sizeof(assignment) - 1 + clause->length;
    char *source = malloc(length);
    if (!source) {
        strcpy(error->reason, "could not allocate script compiler input");
        return false;
    }
    memcpy(source, assignment, sizeof(assignment) - 1);
    memcpy(source + sizeof(assignment) - 1, clause->text, clause->length);
    char path[96], archive_name[96];
    const unsigned id = ++sequence;
    snprintf(path, sizeof(path), "political_framework_runtime/clause_%08x.txt", id);
    snprintf(archive_name, sizeof(archive_name), "political_framework_runtime_%08x.zip", id);
    size_t size;
    unsigned char *archive = native_script_zip(path, source, length, &size);
    free(source);
    if (!archive || api.source(path) || !api.mount(archive, size, free, archive_name, NULL, 1)) {
        free(archive);
        strcpy(error->reason, "could not open the native script compiler input");
        return false;
    }

    GameString name = {.capacity = 15};
    api.assign(&name, clause_keyword, sizeof(clause_keyword) - 1);
    void *object = api.allocate(DECREE_SIZE);
    if (object) api.decree(object, &name, 0);
    api.drop_string(&name);
    const bool parsed = object && api.source(path) && parse_program(object, kind, path, clause->value_line, error);
    const bool closed = api.unmount(archive_name) != 0;
    if (!parsed || !closed) {
        if (object) destroy(object);
        if (!error->reason[0]) strcpy(error->reason, "could not compile the complete native script");
        return false;
    }
    clause->program = object;
    clause->text = NULL;
    clause->path = NULL;
    return true;
}

void release_native_eu4(NativeClause *clause) {
    if (clause->program) destroy((void *)clause->program);
    *clause = (NativeClause){0};
}

static void init_scope(void *scope, void *from_scope, const NativeScope *input, uint64_t root, uint64_t from) {
    api.scope(scope, root, input->province, 0);
    if (from) {
        api.scope(from_scope, from, 0, 0);
        memcpy((unsigned char *)scope + SCOPE_FROM, &from_scope, sizeof(from_scope));
    }
}

static bool on_stack(const void *const *stack, unsigned depth, const void *program) {
    for (unsigned i = 0; i < depth; ++i)
        if (stack[i] == program) return true;
    return false;
}

const char *test_native_eu4(const NativeClause *clause, const NativeScope *input, uint64_t root, uint64_t from, bool *allowed) {
    *allowed = !native_clause_present(clause);
    if (*allowed) return NULL;
    if (!clause->program || (!root && !input->province) || (input->from && !from)) return "native trigger scope unavailable";
    if (!trigger_depth) recursive_trigger = false;
    if (trigger_depth == MAX_NATIVE_DEPTH || on_stack(trigger_stack, trigger_depth, clause->program)) {
        recursive_trigger = true;
        return "recursive political trigger";
    }
    trigger_stack[trigger_depth++] = clause->program;
    _Alignas(16) unsigned char scope[SCOPE_SIZE], from_scope[SCOPE_SIZE];
    init_scope(scope, from_scope, input, root, from);
    const char *previous = operators_instance(input->instance);
    *allowed = api.test((const unsigned char *)clause->program + DECREE_CONDITION, scope);
    operators_instance(previous);
    api.drop_scope(scope);
    if (from) api.drop_scope(from_scope);
    --trigger_depth;
    if (!recursive_trigger) return NULL;
    *allowed = false;
    return "recursive political trigger";
}

bool native_effect_callable(const NativeClause *clause) {
    return !native_clause_present(clause) || (clause->program && effect_depth < MAX_NATIVE_DEPTH &&
           !on_stack(effect_stack, effect_depth, clause->program));
}

void execute_native_eu4(const NativeClause *clause, const NativeScope *input, uint64_t root, uint64_t from) {
    if (!clause->program || !native_effect_callable(clause) ||
        (!root && !input->province) || (input->from && !from)) return;
    effect_stack[effect_depth++] = clause->program;
    _Alignas(16) unsigned char scope[SCOPE_SIZE], from_scope[SCOPE_SIZE];
    init_scope(scope, from_scope, input, root, from);
    const char *previous = operators_instance(input->instance);
    api.effect((const unsigned char *)clause->program + DECREE_EFFECT, scope, 0);
    operators_instance(previous);
    api.drop_scope(scope);
    if (from) api.drop_scope(from_scope);
    --effect_depth;
}

const void *native_eu4_modifier(const NativeClause *clause) {
    return clause->program ? (const unsigned char *)clause->program + DECREE_MODIFIER : NULL;
}

int32_t native_eu4_weight(const NativeClause *clause, const NativeScope *input, uint64_t root, uint64_t from) {
    int32_t weight = 0;
    if (!clause->program || (!root && !input->province) || (input->from && !from)) return weight;
    _Alignas(16) unsigned char scope[SCOPE_SIZE], from_scope[SCOPE_SIZE];
    init_scope(scope, from_scope, input, root, from);
    const char *previous = operators_instance(input->instance);
    api.weight((const unsigned char *)clause->program + DECREE_WEIGHT, &weight, scope, true);
    operators_instance(previous);
    api.drop_scope(scope);
    if (from) api.drop_scope(from_scope);
    return weight;
}
