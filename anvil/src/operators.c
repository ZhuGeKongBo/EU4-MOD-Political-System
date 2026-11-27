#include "operators.h"
#include "symbols.gen.h"

#include <stdio.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

enum { EFFECT_SLOTS = 19, TRIGGER_SLOTS = 16, TRIGGER_READER_SLOTS = 8 };
enum { TOKEN_EQUAL = 1, TOKEN_OPEN = 3, TOKEN_CLOSE = 4, TOKEN_LITERAL = 0xf, TOKEN_QUOTED = 0x17 };

typedef struct ParserToken {
    int kind;
    char text[513];
} ParserToken;

typedef struct OperatorCall {
    PsOperation operation;
    PsArguments args;
    unsigned line;
    bool valid;
    atomic_bool warning_logged;
    char name[GK_SCRIPT_WORD_SIZE], path[260];
} OperatorCall;

typedef struct EffectNode {
    unsigned char native[0x60];
    OperatorCall call;
} EffectNode;

typedef struct TriggerParse {
    bool invalid;
} TriggerParse;

static OperatorState state;
static PsAliases aliases;
static _Thread_local TriggerParse *current_trigger_parse;
static _Thread_local const char *active_instance;
static _Thread_local OperatorOrigin script_origin;
static void *(*original_trigger)(void *, void **, unsigned char *, int);
static void (*original_effect)(void *, unsigned char *, int);
static void *(*game_new)(size_t);
static void *(*effect_ctor)(void *);
static void **(*make_bool)(void **);
static void (*append)(void *, void **);
static void (*assign_string)(GameString *, const char *, size_t);
static void (*append_game_string)(GameString *, const char *, size_t);
static void (*free_string)(GameString *);
static GameString *(*localize)(GameString *, const char *, const void *, unsigned);
static void (*reader_error)(void *, const GameString *);
static const void *keyword_registry;
static const int *(*find_native_keyword)(const void *, const char *);
static void (*original_init)(void);
static void *(*original_trigger_delete)(void *, unsigned);
static void *effect_table[EFFECT_SLOTS], *trigger_table[TRIGGER_SLOTS], *trigger_reader_table[TRIGGER_READER_SLOTS];

static void *read_pointer(const void *object, size_t offset) {
    void *ptr;
    memcpy(&ptr, (const unsigned char *)object + offset, sizeof(ptr));
    return ptr;
}

static void write_pointer(void *object, size_t offset, const void *ptr) {
    memcpy((unsigned char *)object + offset, &ptr, sizeof(ptr));
}

static const char *text_of(const GameString *value) {
    return value->capacity < 16 ? value->inline_text : read_pointer(value, 0);
}

static unsigned scoped_tag(const unsigned char *tag) {
    unsigned result = 0;
    if (tag && tag[7]) memcpy(&result, tag, 3);
    return result;
}

static unsigned scope_country(const unsigned char *scope) {
    return scope ? scoped_tag(scope + (scope[0x4b] ? 0x44 : 0x2c)) : 0;
}

/* These links are used by vanilla targets at 5ED8A2, 5ED8E9 and 5ED904. */
enum { LINK_ROOT = 0x10, LINK_FROM = 0x18, LINK_PREV = 0x20 };

static const unsigned char *linked_scope(const void *scope, unsigned offset) {
    return scope ? read_pointer(scope, offset) : NULL;
}

static unsigned scope_root(const unsigned char *scope) {
    const unsigned char *root = linked_scope(scope, LINK_ROOT);
    return root ? scoped_tag(root + 0x2c) : scope ? scoped_tag(scope + 0x2c) : 0;
}

static unsigned scope_province(const unsigned char *scope) {
    uint32_t province = 0;
    if (scope) memcpy(&province, scope + SYMVAL_SCRIPT_PROVINCE_SCOPE_PROVINCE, sizeof(province));
    return province <= PS_MAX_PROVINCE_ID ? province : 0;
}

const char *operators_instance(const char *instance) {
    const char *previous = active_instance;
    active_instance = instance;
    return previous;
}

static const char *bind_script_arguments(PsOperation operation, const PsArguments *source,
                                        unsigned current, unsigned root, unsigned province,
                                        const unsigned char *scope, PsArguments *out) {
    *out = *source;
    if (!ps_selects_by_type(operation, source) && !strcmp(source->instance, "INSTANCE")) {
        if (!active_instance) return "INSTANCE requires the context of a political system";
        strcpy(out->instance, active_instance);
    }
    if (ps_selects_by_type(operation, source)) {
        if (!state.resolve_type) return "political type selection unavailable";
        const char *reason = state.resolve_type(source->instance, out->instance);
        if (reason) return reason;
    }
    if (out->province >= PS_PROVINCE_THIS) {
        const unsigned reference = out->province;
        if (reference == PS_PROVINCE_THIS) out->province = province;
        else out->province = scope_province(linked_scope(scope, reference == PS_PROVINCE_ROOT ? LINK_ROOT :
                                            reference == PS_PROVINCE_FROM ? LINK_FROM : LINK_PREV));
        if (!out->province) return "referenced scope has no province";
    }
    unsigned leader = 0;
    unsigned *const slots[] = {&out->country, &out->actor, &out->target};
    for (size_t i = 0; i < sizeof(slots) / sizeof(*slots); ++i) {
        if (*slots[i] == PS_COUNTRY_ROOT) {
            if (!root) return "ROOT has no country in this script context";
            *slots[i] = root;
        } else if (*slots[i] == PS_COUNTRY_THIS) {
            if (!current) return "THIS has no country in this script context";
            *slots[i] = current;
        } else if (*slots[i] == PS_COUNTRY_FROM || *slots[i] == PS_COUNTRY_PREV) {
            const unsigned char *other = linked_scope(scope, *slots[i] == PS_COUNTRY_FROM ? LINK_FROM : LINK_PREV);
            const unsigned tag = other ? scoped_tag(other + 0x2c) : 0;
            if (!tag) return "referenced scope has no country";
            *slots[i] = tag;
        } else if (*slots[i] == PS_COUNTRY_LEADER) {
            if (!leader) {
                PsQueryState selected = {0};
                const PsQueryResult status = state.query(out, &selected);
                if (status == PS_QUERY_UNKNOWN_INSTANCE) return "unknown instance";
                if (status != PS_QUERY_READY) return "inactive campaign instance";
                if (!selected.leader) return "LEADER is vacant";
                leader = selected.leader;
            }
            *slots[i] = leader;
        }
    }
    if (ps_accepts_country_target(operation) && out->target) out->country = out->target;
    if (operation == PS_DIRECT_WAR_ALLOWED) {
        if (!out->actor) out->actor = root;
        if (!out->target) out->target = current;
    }
    return NULL;
}

static void log_call(const OperatorCall *call, AnvilLogLevel level, const char *field, const char *result) {
    char message[640];
    snprintf(message, sizeof(message), "political script %s:%u name=%s field=%s %s",
             call->path, call->line, call->name, field, result);
    state.log(level, message);
}

OperatorOrigin operators_origin(OperatorOrigin origin) {
    OperatorOrigin previous = script_origin;
    script_origin = origin;
    return previous;
}

unsigned native_reader_line(const void *reader) {
    const void *parser = read_pointer(reader, 0x28);
    const unsigned char *file = parser ? read_pointer(parser, 8) : NULL;
    unsigned line = 0;
    if (file) memcpy(&line, file + 8, sizeof(line));
    return line;
}

static void capture_location(const unsigned char *reader, OperatorCall *call) {
    call->line = native_reader_line(reader);
    const void *parser = read_pointer(reader, 0x28);
    const unsigned char *file = parser ? read_pointer(parser, 8) : NULL;
    if (script_origin.path) {
        snprintf(call->path, sizeof(call->path), "%s", script_origin.path);
        call->line = script_origin.line + (call->line ? call->line - 1 : 0);
    } else if (file) {
        const GameString *name = (const void *)(file + 0x18);
        snprintf(call->path, sizeof(call->path), "%.259s", text_of(name));
    }
    if (!call->path[0]) strcpy(call->path, "<script>");
    snprintf(call->name, sizeof(call->name), "%.127s", (const char *)reader + 0x34);
}

static bool next_token(unsigned char *reader, ParserToken *token) {
    const void *parser = read_pointer(reader, 0x28);
    int (*read)(const void *) = read_pointer(read_pointer(parser, 0), 8);
    if (!read(parser)) return false;
    memcpy(&token->kind, (const unsigned char *)parser + 0x14, sizeof(token->kind));
    memcpy(token->text, (const unsigned char *)parser + 0x18, sizeof(token->text));
    token->text[sizeof(token->text) - 1] = '\0';
    return true;
}

static bool read_block(unsigned char *reader, OperatorCall *call, const char **field, const char **reason) {
    unsigned depth = 1;
    unsigned part = 0;
    PsField key = PS_FIELD_NONE;
    ParserToken token;
    while (next_token(reader, &token)) {
        if (token.kind == TOKEN_CLOSE) {
            if (--depth == 0) {
                if (!*reason && part) *reason = "incomplete field assignment";
                return !*reason;
            }
            continue;
        }
        if (token.kind == TOKEN_OPEN) {
            ++depth;
            if (!*reason) *reason = "expected a literal field value";
            continue;
        }
        if (*reason || depth != 1) continue;
        if (part == 0) {
            key = ps_find_field(token.text);
            *field = key ? ps_field_names[key] : "block";
            if (!key) *reason = "unknown field";
        } else if (part == 1) {
            if (token.kind != TOKEN_EQUAL) *reason = "expected '=' after field name";
        } else {
            *reason = ps_read_argument(&call->args, call->operation, key, token.text);
        }
        part = (part + 1) % 3;
    }
    if (!*reason) *reason = "unterminated argument block";
    return false;
}

static const PsAlias *find_alias(const char *name) {
    for (size_t i = 0; i < aliases.count; ++i)
        if (!_stricmp(name, aliases.entries[i].name)) return &aliases.entries[i];
    return NULL;
}

static bool compile_call(unsigned char *reader, int token, bool trigger, OperatorCall *call) {
    if (token != TOKEN_LITERAL && token != TOKEN_QUOTED) return false;
    const char *name = (const char *)reader + 0x34;
    const PsAlias *alias = NULL;
    if (ps_reserved(name)) call->operation = ps_find_operation(name);
    else {
        alias = find_alias(name);
        if (!alias) return false;
        call->operation = alias->operation;
        call->args = alias->bound;
    }
    capture_location(reader, call);

    const PsDefinition *definition = &ps_definitions[call->operation];
    const char *field = "operator";
    const char *reason = NULL;
    if (!call->operation) reason = "unknown political operation";
    else if (definition->trigger != trigger) reason = trigger ? "effect used as a trigger" : "trigger used as an effect";
    int value_kind;
    memcpy(&value_kind, reader + 0x440, sizeof(value_kind));
    const bool block = value_kind == TOKEN_OPEN;
    const bool scalar_alias = alias && alias->argument;
    if (!reason && block == scalar_alias) {
        field = "argument";
        reason = block ? "alias requires a scalar" : "operation requires an argument block";
    }
    if (block) {
        read_block(reader, call, &field, &reason);
    } else if (!reason) {
        field = ps_field_names[alias->argument];
        reason = ps_read_argument(&call->args, call->operation, alias->argument, (const char *)reader + 0x444);
    }
    if (!reason) {
        if (script_origin.path && ps_accepts_type_selector(call->operation) &&
            !(call->args.fields & (PS_FIELD(PS_INSTANCE_FIELD) | PS_FIELD(PS_TYPE_FIELD))))
            ps_read_argument(&call->args, call->operation, PS_INSTANCE_FIELD, "INSTANCE");
        const char *missing = ps_missing_field(call->operation, &call->args);
        if (missing) {
            field = missing;
            reason = "required field is missing";
        } else if ((reason = ps_conflicting_fields(call->operation, &call->args))) {
            field = "arguments";
        } else if (!alias && !ps_selects_by_type(call->operation, &call->args) &&
                   call->operation < PS_IS_MEMBER && _strnicmp(call->name, "political_", 10) &&
                   strcmp(call->args.instance, "INSTANCE") && !state.instance_known(call->args.instance)) {
            field = "instance";
            reason = "unknown instance";
        }
    }
    if (reason) {
        char detail[335];
        snprintf(detail, sizeof(detail), "%s.%s: %s", call->name, field, reason);
        GameString error = { .capacity = 15 };
        assign_string(&error, detail, strlen(detail));
        reader_error(reader, &error);
        free_string(&error);
        log_call(call, ANVIL_LOG_ERROR, field, reason);
    } else {
        call->valid = true;
    }
    return true;
}

static void append_description_label(GameString *out, const char *key, const char *fallback) {
    GameString text = {.capacity = 15};
    void *heap_text = NULL;
    localize(&text, key, NULL, 0);
    if (text.capacity > 15) memcpy(&heap_text, text.inline_text, sizeof(heap_text));
    const char *translated = text.capacity > 15 ? heap_text : text.inline_text;
    if (translated && text.size && strcmp(translated, key) != 0) append_game_string(out, translated, text.size);
    else append_game_string(out, fallback, strlen(fallback));
    free_string(&text);
}

static GameString *describe(GameString *out, const OperatorCall *call, const unsigned char *scope, bool invert) {
    char prefix[2 * GK_SCRIPT_WORD_SIZE + 8];
    if (call->valid) snprintf(prefix, sizeof(prefix), "%s [%s]: ", call->name, call->args.instance);
    else snprintf(prefix, sizeof(prefix), "%s: ", call->name);
    *out = (GameString){ .capacity = 15 };
    assign_string(out, prefix, strlen(prefix));
    if (call->valid) {
        const PsDefinition *definition = &ps_definitions[call->operation];
        char key[GK_SCRIPT_WORD_SIZE + sizeof("PS_SCRIPT_DESC_")];
        snprintf(key, sizeof(key), "PS_SCRIPT_DESC_%s", definition->name);
        if (invert) append_description_label(out, "PS_SCRIPT_NOT", "NOT ");
        PsArguments bound = call->args;
        const unsigned country = scope_country(scope);
        if (state.describe) {
            PsArguments resolved;
            if (!bind_script_arguments(call->operation, &call->args, country, scope_root(scope), scope_province(scope), scope, &resolved)) {
                if (ps_selects_by_type(call->operation, &call->args))
                    resolved.fields = (resolved.fields & ~PS_FIELD(PS_TYPE_FIELD)) | PS_FIELD(PS_INSTANCE_FIELD);
                bound = resolved;
            }
            if (state.describe(call->operation, &bound, country, key, out)) return out;
        }
        append_description_label(out, key, definition->description);
    } else append_description_label(out, "PS_SCRIPT_INVALID", "invalid political script.");
    return out;
}

static void execute_effect(const EffectNode *node, const unsigned char *scope) {
    const OperatorCall *call = &node->call;
    const unsigned country = scope_country(scope);
    PsArguments arguments;
    const char *error = bind_script_arguments(call->operation, &call->args, country, scope_root(scope), scope_province(scope),
                                              scope, &arguments);
    if (!error) error = state.execute(call->operation, &arguments, country);
    log_call(call, error ? ANVIL_LOG_WARNING : ANVIL_LOG_INFO, "execute", error ? error :
             (call->operation == PS_RUN_ACTION || call->operation == PS_START_CONTEST ||
              call->operation == PS_INVEST_CONTEST || call->operation == PS_RESOLVE_LEADER ||
              call->operation == PS_RESPOND_REQUEST || call->operation == PS_WITHDRAW_REQUEST ||
              call->operation == PS_RESOLVE_INCIDENT) ? "queued" : "applied");
}

static GameString *describe_effect(const EffectNode *node, GameString *out) {
    return describe(out, &node->call, NULL, false);
}

static void query_error(OperatorCall *call, const char *field, const char *reason) {
    if (!atomic_exchange_explicit(&call->warning_logged, true, memory_order_relaxed))
        log_call(call, ANVIL_LOG_WARNING, field, reason);
}

static const char *test_bound_condition(PsOperation op, const PsArguments *args, unsigned actor, bool *result) {
    if (op == PS_INSTANCE_STATE)
        return state.lifecycle_query ? state.lifecycle_query(args, result) : "instance lifecycle query unavailable";
    if (op == PS_DIRECT_WAR_ALLOWED)
        return state.war_query ? state.war_query(args->actor ? args->actor : actor, args->target, result)
                               : "political war query unavailable";
    PsQueryState current = {0};
    const PsQueryResult status = state.query(args, &current);
    if (status == PS_QUERY_UNKNOWN_INSTANCE) return "unknown instance";
    if (op == PS_HAS_INSTANCE) {
        *result = (status == PS_QUERY_READY) == args->value;
        return NULL;
    }
    if (status == PS_QUERY_UNKNOWN_REFORM) return "unknown reform";
    if (status == PS_QUERY_UNKNOWN_RESOURCE) return "resource not enabled for this instance";
    if (status != PS_QUERY_READY) return "inactive campaign instance";
    if ((op >= PS_IS_CANDIDATE && op <= PS_ELECTION_ROUND) || op == PS_SUPPORTS_CANDIDATE)
        return state.election_query ? state.election_query(op, args, actor, result) : "election query unavailable";
    if (op == PS_IS_MEMBER)
        return state.member_query ? state.member_query(args, actor, result) : "member query unavailable";
    if (op == PS_HAS_ROLE || op == PS_HAS_PROVINCE || op == PS_HAS_CAPITAL ||
        op == PS_MEMBER_COUNT || op == PS_ROLE_COUNT || op == PS_PROVINCE_COUNT)
        return state.structure_query ? state.structure_query(op, args, actor, result) : "political structure query unavailable";
    if (state.resource_query && (op == PS_COMPARE_RESOURCE || op == PS_COMPARE_CALCULATED))
        return state.resource_query(op, args, actor, result);
    if (op == PS_CONTENT_REFORM || op == PS_ACTION_ACTIVE || op == PS_CONTEST_WON || op == PS_HAS_PERMISSION ||
        op == PS_PROCESS_STATUS || op == PS_REFORM_LEVEL)
        return state.action_query ? state.action_query(op, args, actor, result) : "political action query unavailable";
    switch (op) {
    case PS_IS_LEADER: {
        const unsigned country = args->country ? args->country : actor;
        if (!country) return "expected a country scope";
        *result = (country == current.leader) == (!(args->fields & PS_FIELD(PS_VALUE_FIELD)) || args->value);
        break;
    }
    case PS_COMPARE_RESOURCE: *result = ps_compare(args->comparison, current.authority, args->amount); break;
    case PS_IS_REFORM_PASSED: *result = current.passed == args->value; break;
    default: return "unsupported political condition";
    }
    return NULL;
}

static const char *test_in_scope(PsOperation op, const PsArguments *source,
                                 unsigned current, unsigned root, unsigned province, const unsigned char *scope, bool *result) {
    *result = false;
    PsArguments bound;
    const char *reason = bind_script_arguments(op, source, current, root, province, scope, &bound);
    return reason ? reason : test_bound_condition(op, &bound, current, result);
}

const char *ps_test_condition(PsOperation op, const PsArguments *args, unsigned actor, bool *result) {
    return test_in_scope(op, args, actor, actor, 0, NULL, result);
}

static bool evaluate_query(const unsigned char *node, const unsigned char *scope) {
    OperatorCall *call = read_pointer(node, 0x68);
    if (!call || !call->valid) return false;
    bool result = false;
    const char *reason = test_in_scope(call->operation, &call->args, scope_country(scope), scope_root(scope),
                                      scope_province(scope), scope, &result);
    if (reason) query_error(call, "condition", reason);
    return !reason && result;
}

static GameString *describe_trigger(const unsigned char *node, GameString *out, const void *scope, bool invert) {
    return describe(out, read_pointer(node, 0x68), scope, invert);
}

static void *delete_trigger(void *node, unsigned how) {
    free(read_pointer(node, 0x68));
    write_pointer(node, 0x68, NULL);
    return original_trigger_delete(node, how);
}

static void *delete_trigger_reader(unsigned char *reader, unsigned how) {
    return delete_trigger(reader - 0x38, how);
}

static void read_effect(void *parent, unsigned char *reader, int token) {
    OperatorCall call = {0};
    if (!compile_call(reader, token, false, &call)) {
        original_effect(parent, reader, token);
        return;
    }
    if (!call.valid) return;
    EffectNode *node = game_new(sizeof(*node));
    if (!node) return;
    memset(node, 0, sizeof(*node));
    effect_ctor(node);
    write_pointer(node, 0, effect_table);
    // The game's effect fields belong to EU4. The compiled arguments are stored after them.
    node->call = call;
    void *child = node;
    append((unsigned char *)parent + 0x10, &child);
}

static void make_query_node(void **output, const OperatorCall *call) {
    OperatorCall *owned = malloc(sizeof(*owned));
    if (!owned) {
        log_call(call, ANVIL_LOG_ERROR, "operator", "could not allocate a query");
        *output = NULL;
        return;
    }
    *owned = *call;
    make_bool(output);
    if (*output) {
        write_pointer(*output, 0, trigger_table);
        write_pointer(*output, 0x38, trigger_reader_table);
        write_pointer(*output, 0x68, owned);
    } else {
        free(owned);
    }
}

static void *read_trigger(void *parent, void **output, unsigned char *reader, int token) {
    OperatorCall call = {0};
    if (compile_call(reader, token, true, &call)) {
        if (!call.valid && current_trigger_parse) current_trigger_parse->invalid = true;
        make_query_node(output, &call);
        return output;
    }

    // A malformed custom condition vetoes its enclosing native trigger, including a negated group.
    TriggerParse frame = {0};
    TriggerParse *previous = current_trigger_parse;
    current_trigger_parse = &frame;
    void *result = original_trigger(parent, output, reader, token);
    current_trigger_parse = previous;
    if (frame.invalid) {
        if (previous) previous->invalid = true;
        if (*output) {
            void *(*destroy)(void *, unsigned) = read_pointer(read_pointer(*output, 0), 0);
            destroy(*output, 1);
        }
        strcpy(call.name, "Political script group");
        make_query_node(output, &call);
    }
    return result;
}

static void load_script_aliases(void) {
    original_init();
    PsAliases loaded = {0};
    state.load_aliases(&loaded);
    aliases.count = 0;
    for (size_t i = 0; i < loaded.count; ++i) {
        const PsAlias *alias = &loaded.entries[i];
        char message[335];
        if (find_native_keyword(keyword_registry, alias->name)) {
            snprintf(message, sizeof(message), "%s:%u:%u alias=%s conflicts with a native keyword",
                     PS_ALIASES_PATH, alias->line, alias->column, alias->name);
            state.log(ANVIL_LOG_ERROR, message);
            continue;
        }
        if (!state.instance_known(alias->bound.instance)) {
            snprintf(message, sizeof(message), "%s:%u:%u alias=%s has unknown instance %.127s",
                     PS_ALIASES_PATH, alias->line, alias->column, alias->name, alias->bound.instance);
            state.log(ANVIL_LOG_ERROR, message);
            continue;
        }
        aliases.entries[aliases.count++] = *alias;
        snprintf(message, sizeof(message), "political script alias=%s operation=%s instance=%s",
                 alias->name, ps_definitions[alias->operation].name, alias->bound.instance);
        state.log(ANVIL_LOG_INFO, message);
    }
}

static bool copy_table(const AnvilHost *host, AnvilPlugin *self, AnvilSymbol symbol, void **table, size_t count) {
    uint64_t rva;
    if (host->symbol_value(self, symbol, "table", &rva) != ANVIL_OK ||
        rva >= host->game->image_size || count * sizeof(*table) > host->game->image_size - rva) return false;
    memcpy(table, (const void *)(host->game->image_base + rva), count * sizeof(*table));
    return true;
}

AnvilResult register_operators(const AnvilHost *host, AnvilPlugin *self, OperatorState callbacks) {
    state = callbacks;
    game_new = host->address_of(self, SYM_GAME_NEW);
    effect_ctor = host->address_of(self, SYM_SET_CHINA_CTOR);
    make_bool = host->address_of(self, SYM_IS_CHINA_FACTORY);
    append = host->address_of(self, SYM_EFFECT_APPEND);
    assign_string = host->address_of(self, SYM_ASSIGN_STRING);
    append_game_string = host->address_of(self, SYM_APPEND_STRING);
    free_string = host->address_of(self, SYM_FREE_STRING);
    localize = host->address_of(self, SYM_LOCALIZE);
    reader_error = host->address_of(self, SYM_SCRIPT_READER_ERROR);
    find_native_keyword = host->address_of(self, SYM_NATIVE_KEYWORD_LOOKUP);
    uint64_t registry_rva;
    if (!game_new || !effect_ctor || !make_bool || !append || !assign_string || !append_game_string || !free_string ||
        !localize || !reader_error ||
        !find_native_keyword || !host->address_of(self, SYM_SCRIPT_PROVINCE_SCOPE) ||
        host->symbol_value(self, SYM_NATIVE_KEYWORD_REGISTRY, "table", &registry_rva) != ANVIL_OK ||
        registry_rva >= host->game->image_size ||
        !copy_table(host, self, SYM_SET_CHINA_VTABLE, effect_table, EFFECT_SLOTS) ||
        !copy_table(host, self, SYM_IS_CHINA_VTABLE, trigger_table, TRIGGER_SLOTS) ||
        !copy_table(host, self, SYM_IS_CHINA_READER_VTABLE, trigger_reader_table, TRIGGER_READER_SLOTS))
        return ANVIL_ERROR_UNSUPPORTED;
    keyword_registry = (const void *)(host->game->image_base + registry_rva);

    effect_table[0x78 / 8] = (void *)execute_effect;
    effect_table[0x68 / 8] = (void *)describe_effect;
    original_trigger_delete = trigger_table[0];
    trigger_table[0] = (void *)delete_trigger;
    trigger_reader_table[0] = (void *)delete_trigger_reader;
    trigger_table[0x30 / 8] = (void *)evaluate_query;
    trigger_table[0x48 / 8] = (void *)describe_trigger;
    AnvilChange *effect_change;
    AnvilResult result = host->hook_function(self, SYM_EFFECT_READ_MEMBER, (void *)read_effect,
                                            (void **)&original_effect, &effect_change);
    if (result != ANVIL_OK) return result;
    AnvilChange *trigger_change;
    result = host->hook_function(self, SYM_TRIGGER_READ_MEMBER, (void *)read_trigger,
                                 (void **)&original_trigger, &trigger_change);
    if (result != ANVIL_OK) {
        if (host->remove(self, effect_change) != ANVIL_OK) return ANVIL_ERROR_INTERNAL;
        return result;
    }
    result = host->redirect_call(self, SYM_SCRIPT_INIT_CALL, (void *)load_script_aliases, (void **)&original_init, NULL);
    if (result != ANVIL_OK) {
        if (host->remove(self, trigger_change) != ANVIL_OK || host->remove(self, effect_change) != ANVIL_OK)
            return ANVIL_ERROR_INTERNAL;
        return result;
    }
    state.log(ANVIL_LOG_INFO, "political script native readers registered");
    return ANVIL_OK;
}
