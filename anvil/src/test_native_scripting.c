#include "content_types.h"
#include "native_eu4.h"
#include "native_passives.h"
#include "operators.h"
#include "symbols.gen.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char *archive;
static void (*dispose_archive)(void *);
static unsigned char fake_file[32];
static void *last_object;
static char clause_text[1024];
static unsigned executed, destroyed, refresh_count;
static unsigned lexers_destroyed, keys_read, blocks_read;
static bool file_missing, invalid_header;
static void (*reader_report)(void *, const GameString *);
static void (*country_collector)(const void *), (*province_collector)(const void *);

static void put(void *base, size_t offset, const void *value, size_t size) {
    memcpy((unsigned char *)base + offset, value, size);
}

static void *get_pointer(const void *base, size_t offset) {
    void *value;
    memcpy(&value, (const unsigned char *)base + offset, sizeof(value));
    return value;
}

static void assign(GameString *string, const char *source, size_t length) {
    assert(string->capacity == 15);
    if (length < 16) memcpy(string->inline_text, source, length + 1);
    else {
        char *copy = malloc(length + 1);
        assert(copy);
        memcpy(copy, source, length + 1);
        memcpy(string->inline_text, &copy, sizeof(copy));
        string->capacity = length;
    }
    string->size = length;
}

static void drop_string(GameString *string) {
    if (string->capacity >= 16) free(get_pointer(string, 0));
    *string = (GameString){.capacity = 15};
}

static int mount_file(const void *buffer, uint64_t size, void (*dispose)(void *),
                      const char *archive_path, const char *mountpoint, int append) {
    assert(size > 98 && !memcmp(buffer, "PK\x03\x04", 4) && !mountpoint && append);
    assert(!archive && strstr(archive_path, ".zip"));
    archive = (void *)buffer;
    dispose_archive = dispose;
    uint16_t path_size;
    uint32_t content_size;
    memcpy(&path_size, archive + 26, sizeof(path_size));
    memcpy(&content_size, archive + 18, sizeof(content_size));
    assert(size > 30u + path_size + content_size && content_size < sizeof(clause_text));
    memcpy(clause_text, archive + 30 + path_size, content_size);
    clause_text[content_size] = 0;
    assert(!strncmp(clause_text, "pf_clause = ", 12));
    return 1;
}

static const char *file_source(const char *path) {
    assert(strstr(path, "political_framework_runtime/"));
    return archive ? "test archive" : NULL;
}

static int unmount_file(const char *archive_path) {
    assert(archive && strstr(archive_path, ".zip"));
    dispose_archive(archive);
    archive = NULL;
    return 1;
}

static void drop_lexer(void *object, unsigned flags) {
    assert(flags == 1);
    ++lexers_destroyed;
    free(object);
}

static void *lexer(void *object, GameString *path) {
    static void *const table[] = { (void *)drop_lexer };
    const void *vtable = table;
    unsigned char *file = fake_file;
    assert(path->size && archive);
    put(object, 0, &vtable, sizeof(vtable));
    if (!file_missing) put(object, 8, &file, sizeof(file));
    return object;
}

static void *reader(void *object, void *input, const GameString *path, bool owned) {
    assert(input && path->size && owned);
    put(object, 0x28, &input, sizeof(input));
    return object;
}

static void drop_reader(void *object) { drop_lexer(get_pointer(object, 0x28), 1); }
static void read_key(void *object) {
    assert(get_pointer(object, 0x28));
    ++keys_read;
    int value_kind = invalid_header ? 19 : 3;
    put(object, 0x34, "pf_clause", sizeof("pf_clause"));
    put(object, 0x440, &value_kind, sizeof(value_kind));
}
static void *new_object(size_t size) { return calloc(1, size); }

static void drop_object(void *object, unsigned flags) {
    assert(flags == 1);
    ++destroyed;
    free(object);
}

static void *decree(void *object, GameString *name, unsigned index) {
    static void *const table[] = { (void *)drop_object };
    const void *vtable = table;
    assert(name->size && !index);
    put(object, 0, &vtable, sizeof(vtable));
    last_object = object;
    return object;
}

static void read_block(void *dest, void *input) {
    ++blocks_read;
    const ptrdiff_t offset = (unsigned char *)dest - (unsigned char *)last_object;
    assert(offset == 0x298 || offset == 0x1f0 || offset == 0x40 || offset == 0x1b8);
    const unsigned row = 3;
    put(fake_file, 8, &row, sizeof(row));
    if (strstr(clause_text, "unknown_test_token")) {
        const GameString error = { .inline_text = "unknown token", .capacity = 15, .size = 13 };
        reader_report(input, &error);
    } else if (strstr(clause_text, "invalid_value")) {
        ((unsigned char *)input)[0x20] = 1;
    } else {
        assert(strstr(clause_text, "{") && strstr(clause_text, "}"));
        ((unsigned char *)dest)[0] = 42;
    }
}

static void original_report(void *reader, const GameString *message) {
    assert(message->size);
    ((unsigned char *)reader)[0x20] = 1;
}

static void *scope(void *object, uint64_t tag, unsigned province, unsigned auxiliary) {
    assert(!auxiliary);
    memset(object, 0, 0xd0);
    put(object, 0x18, &object, sizeof(object));
    put(object, 0x28, &province, sizeof(province));
    put(object, 0x2c, &tag, sizeof(tag));
    return object;
}

static void drop_scope(void *object) { assert(object); }

static NativeClause recursive;
static bool recurse;
static void check_scope(const void *object) {
    const unsigned char *from = get_pointer(object, 0x18);
    uint64_t root_tag, from_tag;
    unsigned province;
    memcpy(&root_tag, (const unsigned char *)object + 0x2c, sizeof(root_tag));
    memcpy(&from_tag, from + 0x2c, sizeof(from_tag));
    memcpy(&province, (const unsigned char *)object + 0x28, sizeof(province));
    assert(root_tag == 1 && from_tag == 2 && province == 183);
    const char *active = operators_instance("check");
    assert(!strcmp(active, "alpha"));
    operators_instance(active);
}

static bool test(const void *condition, const void *object) {
    check_scope(object);
    assert(((const unsigned char *)condition)[0x38] == 42);
    if (recurse) {
        bool allowed = true;
        const NativeScope input = {.province = 183, .from = 2, .instance = "alpha"};
        assert(test_native_eu4(&recursive, &input, 1, 2, &allowed) && !allowed);
    }
    return true;
}

static void run(const void *effect, void *object, unsigned flags) {
    assert(!flags && ((const unsigned char *)effect)[0] == 42);
    check_scope(object);
    ++executed;
}

static int32_t *weight(const void *clause, int32_t *out, const void *object, bool multiplicative) {
    assert(multiplicative && ((const unsigned char *)clause)[0] == 42);
    check_scope(object);
    *out = 1250;
    return out;
}

static void original_collector(const void *context) { assert(context); }

static void merge(void *total, const void *modifier) {
    *(int *)total += *(const int *)modifier;
}

static void *address(AnvilPlugin *self, AnvilSymbol symbol) {
    (void)self;
    switch (symbol) {
        case SYM_GAME_NEW: return (void *)new_object;
        case SYM_ASSIGN_STRING: return (void *)assign;
        case SYM_FREE_STRING: return (void *)drop_string;
        case SYM_NATIVE_CONSTRUCT_TEXT_FILE_LEXER: return (void *)lexer;
        case SYM_NATIVE_SCRIPT_READER: return (void *)reader;
        case SYM_NATIVE_DROP_READER: return (void *)drop_reader;
        case SYM_NATIVE_READER_ASSIGNMENT: return (void *)read_key;
        case SYM_NATIVE_READ_BLOCK: return (void *)read_block;
        case SYM_NATIVE_DECREE_STORAGE: return (void *)decree;
        case SYM_NATIVE_EVENT_SCOPE: return (void *)scope;
        case SYM_NATIVE_DROP_SCOPE: return (void *)drop_scope;
        case SYM_NATIVE_CONDITION_TEST: return (void *)test;
        case SYM_NATIVE_EFFECT_RUN: return (void *)run;
        case SYM_NATIVE_SCRIPT_WEIGHT: return (void *)weight;
        case SYM_NATIVE_MOUNT_MEMORY: return (void *)mount_file;
        case SYM_NATIVE_UNMOUNT_MEMORY: return (void *)unmount_file;
        case SYM_MOD_FILE_SOURCE: return (void *)file_source;
        case SYM_NATIVE_MODIFIER_MERGE: return (void *)merge;
        default: return NULL;
    }
}

static AnvilResult hook(AnvilPlugin *self, AnvilSymbol symbol, void *detour, void **next, AnvilChange **change) {
    (void)self;
    (void)change;
    if (symbol == SYM_SCRIPT_READER_ERROR) {
        reader_report = detour;
        *next = (void *)original_report;
    } else {
        *next = (void *)original_collector;
        if (symbol == SYM_NATIVE_COUNTRY_IMPERIAL_MODIFIERS) country_collector = detour;
        else if (symbol == SYM_NATIVE_PROVINCE_IMPERIAL_MODIFIERS) province_collector = detour;
        else return ANVIL_ERROR_UNSUPPORTED;
    }
    return ANVIL_OK;
}

static NativeClause clause(const char *text) {
    return (NativeClause){.text = text, .length = strlen(text), .line = 8, .column = 5,
                          .value_line = 9, .path = "common/political_systems/types/check.txt"};
}

static void verify_compiler(void) {
    ScriptError error = {0};
    NativeClause broken = clause("{\n\n unknown_test_token = yes }");
    assert(!compile_native_eu4(&broken, NATIVE_EFFECT, &error));
    assert(strstr(error.reason, "unknown token") && error.line == 11 && !strcmp(error.path, broken.path));
    assert(!archive && destroyed == 1);
    broken = clause("{\n\n invalid_value = yes }");
    assert(!compile_native_eu4(&broken, NATIVE_CONDITION, &error));
    assert(error.line == 11 && !archive && destroyed == 2);

    NativeClause trigger = clause("{ capital_scope = { is_city = yes } }");
    NativeClause effect = clause("{ if = { limit = { capital_scope = { is_city = yes } } add_treasury = 15 } }");
    NativeClause chance = clause("{ factor = 1 modifier = { factor = 1.25 is_at_war = no } }");
    assert(compile_native_eu4(&trigger, NATIVE_CONDITION, &error));
    assert(compile_native_eu4(&effect, NATIVE_EFFECT, &error));
    assert(compile_native_eu4(&chance, NATIVE_WEIGHT, &error));
    const NativeScope input = {.province = 183, .from = 2, .instance = "alpha"};
    bool allowed = false;
    assert(!test_native_eu4(&trigger, &input, 1, 2, &allowed) && allowed);
    assert(test_native_eu4(&trigger, &input, 1, 0, &allowed) && !allowed);
    recursive = trigger;
    recurse = true;
    assert(test_native_eu4(&trigger, &input, 1, 2, &allowed) && !allowed);
    recurse = false;
    execute_native_eu4(&effect, &input, 1, 2);
    assert(executed == 1 && native_eu4_weight(&chance, &input, 1, 2) == 1250 && !operators_instance(NULL));
    release_native_eu4(&trigger);
    release_native_eu4(&effect);
    release_native_eu4(&chance);
    assert(destroyed == 5);
    puts("PASS original-byte bridge, error source and rollback, real scope layout, effect, weight and recursive condition guard (mock engine).");

    NativeClause unreadable = clause("{ capital_scope = { is_city = yes } }");
    unsigned last_key = keys_read, last_block = blocks_read, last_lexer = lexers_destroyed;
    file_missing = true;
    assert(!compile_native_eu4(&unreadable, NATIVE_CONDITION, &error));
    assert(!archive && keys_read == last_key && blocks_read == last_block && lexers_destroyed == last_lexer + 1);
    assert(error.line == 8 && strstr(error.reason, "open the temporary script"));
    file_missing = false;
    invalid_header = true;
    assert(!compile_native_eu4(&unreadable, NATIVE_CONDITION, &error));
    assert(!archive && keys_read == last_key + 1 && blocks_read == last_block && lexers_destroyed == last_lexer + 2);
    assert(error.line == 8 && strstr(error.reason, "read the temporary script header"));
    invalid_header = false;
    puts("PASS unreadable script and incomplete header are rejected before game block dispatch (mock engine).");
}

static void refresh(bool province, uintptr_t recipient) {
    assert(recipient == (province ? 183u : 10u));
    ++refresh_count;
}

static void verify_collectors(void) {
    const int levy = 3, tax = 5;
    NativePassives snapshot = {0};
    assert(append_native_passive(&snapshot, true, 183, &levy));
    assert(append_native_passive(&snapshot, false, 10, &levy));
    assert(append_native_passive(&snapshot, false, 10, &tax));
    replace_native_passives(snapshot, refresh);
    assert(refresh_count == 2);
    int country_total[4] = {0}, province_total[5] = {0};
    uint64_t country[5] = {0};
    unsigned province[9] = {0};
    country[4] = 10;
    province[8] = 183;
    const void *const country_context[] = {country_total, country};
    const void *const province_context[] = {province_total, country, NULL, NULL, province};
    country_collector(country_context);
    province_collector(province_context);
    assert(country_total[2] == 8 && province_total[4] == 3);

    snapshot = (NativePassives){0};
    assert(append_native_passive(&snapshot, false, 10, &tax));
    assert(append_native_passive(&snapshot, true, 183, &levy));
    assert(append_native_passive(&snapshot, false, 10, &levy));
    replace_native_passives(snapshot, refresh);
    assert(refresh_count == 2);
    replace_native_passives((NativePassives){0}, refresh);
    assert(refresh_count == 4);
    country_collector(country_context);
    province_collector(province_context);
    assert(country_total[2] == 8 && province_total[4] == 3);
    puts("PASS member/province totals, same-key stacking, stable snapshot reorder and exact refresh after removal (mock engine).");
}

static void verify_message_schema(void) {
    static const char types[] =
        "base = { abstract = yes messages = { dispatch = { title = title_a body = { line_a line_b } "
        "options = { close = dismiss secondary = keep } } } }\n"
        "custom = { copy_from = base messages = { dispatch = { popup = no subtitle = sub_a } } }\n"
        "bad = { messages = { dispatch = { title = a body = { a b c d e } } } }\n"
        "duplicate = { messages = { dispatch = { title = a body = b } dispatch = { title = c body = d } } }\n"
        "cleared = { copy_from = base messages = { dispatch = clear } }\n";
    ScriptError error = {0};
    ContentTypes catalog = {0};
    assert(collect_content_types(&catalog, types, strlen(types), "fixture/messages.txt", &error));
    const KhanDefinition *type = resolve_content_type(&catalog, "custom", &error);
    assert(type && type->message_count == 1);
    const PoliticalMessageDefinition *message = &type->messages[0];
    assert(!message->popup && !strcmp(message->title, "title_a") && !strcmp(message->subtitle, "sub_a"));
    assert(!strcmp(message->body[0], "line_a") && !strcmp(message->body[1], "line_b"));
    assert(!strcmp(message->close, "dismiss") && !strcmp(message->secondary, "keep"));
    assert(!strcmp(message->path, "fixture/messages.txt") && message->line == 2);
    assert(!resolve_content_type(&catalog, "bad", &error) && error.line == 3 && strstr(error.reason, "up to four"));
    assert(!resolve_content_type(&catalog, "duplicate", &error) && error.line == 4 && strstr(error.reason, "duplicate message"));
    assert(!resolve_content_type(&catalog, "cleared", &error)->message_count);
    free_content_types(&catalog);
    puts("PASS inherited and cleared message definitions, localized fields and line-specific diagnostics.");
}

int main(void) {
    const AnvilHost host = {.address_of = address, .hook_function = hook};
    assert(install_native_eu4(&host, NULL) == ANVIL_OK);
    assert(install_native_passives(&host, NULL) == ANVIL_OK);
    verify_compiler();
    verify_collectors();
    verify_message_schema();
}
