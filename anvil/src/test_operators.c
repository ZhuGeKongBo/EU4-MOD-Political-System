#include "operators.c"
#include "political_actions.h"
#include "political_script.h"

#include <assert.h>

static unsigned diagnostics;
static ParserToken tokens[48];
static size_t token_count, token_cursor;
static PsQueryState campaign = { .authority = 50 };
static EffectNode *published;
static int executed_amount;

static void report(AnvilLogLevel level, const char *message) {
    if (level == ANVIL_LOG_ERROR) ++diagnostics;
    assert(message[0]);
}

static void assign_mock(GameString *text, const char *data, size_t length) {
    char *heap = malloc(length + 1);
    assert(heap);
    memcpy(heap, data, length);
    heap[length] = 0;
    text->size = length;
    text->capacity = length + 16;
    write_pointer(text, 0, heap);
}

static void free_mock(GameString *text) {
    free(read_pointer(text, 0));
}

static void append_description_mock(GameString *text, const char *value, size_t length) {
    char combined[512];
    const size_t old_size = text->size;
    assert(old_size + length < sizeof(combined));
    memcpy(combined, text_of(text), old_size);
    memcpy(combined + old_size, value, length);
    free_mock(text);
    assign_mock(text, combined, old_size + length);
}

static GameString *localize_description_mock(GameString *text, const char *key, const void *params, unsigned count) {
    assert(!params && !count);
    const char *value = !strcmp(key, "PS_SCRIPT_DESC_ps_is_leader") ? "检查领袖" :
        !strcmp(key, "PS_SCRIPT_NOT") ? "非 " : key;
    assign_mock(text, value, strlen(value));
    return text;
}

static void check_description_localisation(void) {
    GameString label;
    OperatorCall call = {.name = "khan_holder", .operation = PS_IS_LEADER, .valid = true};
    strcpy(call.args.instance, "second");
    describe(&label, &call, NULL, true);
    assert(!strcmp(text_of(&label), "khan_holder [second]: 非 检查领袖"));
    free_mock(&label);
    call.operation = PS_IS_MEMBER;
    describe(&label, &call, NULL, false);
    assert(strstr(text_of(&label), ps_definitions[PS_IS_MEMBER].description));
    free_mock(&label);
    call.valid = false;
    describe(&label, &call, NULL, false);
    assert(!strcmp(text_of(&label), "khan_holder: invalid political script."));
    free_mock(&label);
    puts("PASS operator descriptions use canonical localisation keys with English fallback.");
}

static void record_mock(void *reader, const GameString *text) {
    (void)reader;
    assert(strchr(text_of(text), ':'));
}

static void *construct_mock(void *node) {
    return node;
}

static void append_mock(void *parent, void **node) {
    (void)parent;
    published = *node;
}

static const char *execute_mock(PsOperation op, const PsArguments *args, unsigned actor) {
    (void)actor;
    assert(op == PS_CHANGE_RESOURCE);
    executed_amount = args->amount;
    return NULL;
}

static bool instance_known_mock(const char *id) {
    return !strcmp(id, PS_LEGACY_INSTANCE) || !strcmp(id, "second");
}

static PsQueryResult query_mock(const PsArguments *args, PsQueryState *out) {
    if ((args->fields & PS_FIELD(PS_REFORM_FIELD)) && strcmp(args->reform, "gk_council_reform")) return PS_QUERY_UNKNOWN_REFORM;
    *out = campaign;
    return PS_QUERY_READY;
}

static const char *war_mock(unsigned root, unsigned target, bool *allowed) {
    assert(root == election_tag("ASK") && target == election_tag("MNG"));
    *allowed = true;
    return NULL;
}

static int parser_mock(const void *parser) {
    if (token_cursor == token_count) return 0;
    const ParserToken *token = &tokens[token_cursor++];
    memcpy((unsigned char *)parser + 0x14, token, sizeof(*token));
    return 1;
}

static void add_token(int kind, const char *text) {
    assert(token_count < sizeof(tokens) / sizeof(*tokens));
    ParserToken *token = &tokens[token_count++];
    token->kind = kind;
    strcpy(token->text, text);
}

static void add_field(const char *name, const char *value) {
    add_token(TOKEN_LITERAL, name);
    add_token(TOKEN_EQUAL, "=");
    add_token(TOKEN_LITERAL, value);
}

static OperatorCall compile(const char *name, bool trigger, const char *scalar, bool recognized) {
    unsigned char reader[0x650] = {0}, parser[0x220] = {0};
    void *const methods[] = {NULL, (void *)parser_mock};
    write_pointer(reader, 0x28, parser);
    write_pointer(parser, 0, methods);
    strcpy((char *)reader + 0x34, name);
    const int kind = scalar ? TOKEN_LITERAL : TOKEN_OPEN;
    memcpy(reader + 0x440, &kind, sizeof(kind));
    if (scalar) strcpy((char *)reader + 0x444, scalar);
    else add_token(TOKEN_CLOSE, "}");
    OperatorCall call = {0};
    assert(compile_call(reader, TOKEN_LITERAL, trigger, &call) == recognized);
    if (recognized && !scalar) assert(token_cursor == token_count);
    token_cursor = token_count = 0;
    return call;
}

static const char *resolve_mock(const char *type, char out[GK_SCRIPT_WORD_SIZE]) {
    if (!strcmp(type, "ambiguous")) return "political type has multiple active instances; specify instance";
    assert(!strcmp(type, "shah_empire"));
    strcpy(out, "second");
    return NULL;
}

static unsigned leader_writes;
static const char *set_leader_mock(PsOperation operation, const PsArguments *args, unsigned actor) {
    assert(operation == PS_SET_LEADER && !strcmp(args->instance, "second"));
    campaign.leader = args->target ? args->target : actor;
    ++leader_writes;
    return NULL;
}

static const char *province_mock(PsOperation op, const PsArguments *args, unsigned actor, bool *result) {
    assert(op == PS_HAS_PROVINCE && !strcmp(args->instance, "second"));
    assert(actor == election_tag("FRA"));
    *result = args->province == 183;
    return NULL;
}

static void check_generic_scopes(void) {
    state.resolve_type = resolve_mock;
    state.execute = set_leader_mock;

    unsigned char scope[0x50] = {0}, node[0x70] = {0};
    memcpy(scope + 0x2c, "FRA", 3);
    memcpy(scope + 0x44, "SAV", 3);
    scope[0x33] = scope[0x4b] = 1;
    add_field("type", "shah_empire");
    add_field("target", "THIS");
    EffectNode effect = {.call = compile("political_set_leader", false, NULL, true)};
    assert(effect.call.valid);
    execute_effect(&effect, scope);
    assert(campaign.leader == election_tag("SAV") && leader_writes == 1);
    add_field("type", "shah_empire");
    OperatorCall query = compile("political_is_leader", true, NULL, true);
    write_pointer(node, 0x68, &query);
    assert(query.valid && evaluate_query(node, scope));

    add_field("type", "shah_empire");
    add_field("target", "ROOT");
    effect.call = compile("political_set_leader", false, NULL, true);
    assert(effect.call.valid);
    execute_effect(&effect, scope);
    assert(campaign.leader == election_tag("FRA") && leader_writes == 2);
    assert(!evaluate_query(node, scope));
    add_field("type", "shah_empire");
    add_field("country", "ROOT");
    query = compile("political_is_leader", true, NULL, true);
    assert(query.valid && evaluate_query(node, scope));

    add_field("instance", "second");
    add_field("role", "satrap");
    add_field("target", "LEADER");
    add_field("value", "no");
    const OperatorCall role = compile("political_set_role", false, NULL, true);
    assert(role.valid);
    PsArguments bound;
    assert(!bind_script_arguments(role.operation, &role.args, election_tag("SAV"), election_tag("FRA"), 0, NULL, &bound));
    assert(bound.country == election_tag("FRA"));
    campaign.leader = 0;
    assert(!strcmp(bind_script_arguments(role.operation, &role.args, 0, 0, 0, NULL, &bound), "LEADER is vacant"));
    campaign.leader = election_tag("FRA");

    state.structure_query = province_mock;
    add_field("type", "shah_empire");
    add_field("province", "THIS");
    query = compile("political_has_province", true, NULL, true);
    scope[0x4b] = 0;
    assert(query.valid && !evaluate_query(node, scope));
    const uint32_t paris = 183;
    memcpy(scope + SYMVAL_SCRIPT_PROVINCE_SCOPE_PROVINCE, &paris, sizeof(paris));
    assert(evaluate_query(node, scope));

    add_field("type", "ambiguous");
    add_field("target", "THIS");
    effect.call = compile("political_set_leader", false, NULL, true);
    assert(effect.call.valid);
    execute_effect(&effect, scope);
    assert(campaign.leader == election_tag("FRA") && leader_writes == 2);
    add_field("type", "shah_empire");
    add_field("instance", "second");
    assert(!compile("political_set_leader", false, NULL, true).valid);
    add_field("type", "shah_empire");
    add_field("country", "SAV");
    add_field("target", "ROOT");
    assert(!compile("political_set_leader", false, NULL, true).valid);
    add_field("type", "shah_empire");
    add_field("role", "satrap");
    add_field("value", "yes");
    assert(!compile("political_set_role", false, NULL, true).valid);
    add_field("instance", "second");
    add_field("amount", "1000");
    query = compile("political_member_count", true, NULL, true);
    assert(query.valid && query.args.amount == 1000);
    add_field("instance", "second");
    add_field("country", "SAV");
    add_field("target", "LEADER");
    query = compile("political_supports_candidate", true, NULL, true);
    assert(query.valid && !bind_script_arguments(query.operation, &query.args, 0, 0, 0, NULL, &bound));
    assert(bound.country == election_tag("SAV") && bound.target == election_tag("FRA"));
    unsigned char origin[0x50] = {0}, previous[0x50] = {0};
    memcpy(origin + 0x2c, "ENG", 3);
    origin[0x33] = 1;
    memcpy(previous + SYMVAL_SCRIPT_PROVINCE_SCOPE_PROVINCE, &paris, sizeof(paris));
    write_pointer(scope, LINK_FROM, origin);
    write_pointer(scope, LINK_PREV, previous);
    add_field("instance", "second");
    add_field("country", "FROM");
    query = compile("political_is_leader", true, NULL, true);
    assert(query.valid && !bind_script_arguments(query.operation, &query.args, 0, 0, 0, scope, &bound));
    assert(bound.country == election_tag("ENG"));
    add_field("instance", "second");
    add_field("province", "PREV");
    query = compile("political_has_province", true, NULL, true);
    assert(query.valid && !bind_script_arguments(query.operation, &query.args, 0, 0, 0, scope, &bound));
    assert(bound.province == paris);
    add_field("instance", "second");
    assert(!compile("political_iss_member", true, NULL, true).valid);
    assert(!compile("set_shah_empire", false, "yes", false).valid);
    puts("PASS generic selectors, ROOT/THIS/FROM/PREV, vacant LEADER, province scope, counts and ambiguous types (mock reader).");
}

int main(void) {
    state = (OperatorState){ .log = report, .query = query_mock, .instance_known = instance_known_mock };
    reader_error = record_mock;
    assign_string = assign_mock;
    append_game_string = append_description_mock;
    free_string = free_mock;
    localize = localize_description_mock;
    check_description_localisation();
    static const char config[] =
        "\xef\xbb\xbf" "# native aliases\n"
        "aliases = { grant_council_power = { operation = ps_change_resource instance = great_khan_main "
        "resource = authority argument = amount } "
        "khan_holder = { operation = ps_is_leader instance = second } }";
    PsAliases valid = {0};
    ScriptError error;
    assert(read_ps_aliases(config, sizeof(config) - 1, &valid, &error) && valid.count == 2);
    static const char *const rejected[] = {
        "aliases = { ps_set_leader = { operation = ps_set_leader instance = great_khan_main } }",
        "aliases = { x = { operation = ps_change_resource instance = great_khan_main argument = amount } }",
        "aliases = { x = { operation = ps_set_leader instance = old-game argument = country } }",
        "aliases = { x = { operation = ps_unknown instance = great_khan_main } }",
        "aliases = { x = { operation = ps_is_leader instance = great_khan_main } X = { "
        "operation = ps_is_leader instance = great_khan_main } }",
    };
    for (size_t i = 0; i < sizeof(rejected) / sizeof(*rejected); ++i) {
        PsAliases invalid = {0};
        assert(!read_ps_aliases(rejected[i], strlen(rejected[i]), &invalid, &error));
        assert(error.line == 1 && error.column && error.reason[0]);
    }
    aliases = valid;
    OperatorCall call = compile("grant_council_power", false, "-10", true);
    assert(call.valid && call.operation == PS_CHANGE_RESOURCE && call.args.amount == -10);
    call = compile("set_emperor_of_china", false, "MNG", false);
    assert(!call.valid && diagnostics == 0);

    game_new = malloc;
    effect_ctor = construct_mock;
    append = append_mock;
    state.execute = execute_mock;
    unsigned char effect_parent[0x20] = {0}, effect_reader[0x650] = {0};
    strcpy((char *)effect_reader + 0x34, "grant_council_power");
    strcpy((char *)effect_reader + 0x444, "10");
    read_effect(effect_parent, effect_reader, TOKEN_LITERAL);
    assert(published && published->call.valid);
    // The previous native field is available to the engine during execution.
    memset(published->native + 0x40, 0xbb, 0x20);
    execute_effect(published, NULL);
    assert(executed_amount == 10);
    free(published);

    add_field("value", "yes");
    call = compile("khan_holder", true, NULL, true);
    assert(call.valid && call.operation == PS_IS_LEADER && !strcmp(call.args.instance, "second"));
    unsigned char node[0x70] = {0}, scope[0x50] = {0};
    memcpy(&campaign.leader, "ASK", 3);
    memcpy(scope + 0x2c, "ASK", 3);
    scope[0x33] = 1;
    write_pointer(node, 0x68, &call);
    assert(evaluate_query(node, scope));
    memcpy(scope + 0x44, "MNG", 3);
    scope[0x4b] = 1;
    assert(!evaluate_query(node, scope));
    assert(!evaluate_query(node, NULL));

    add_field("instance", PS_LEGACY_INSTANCE);
    add_field("resource", PS_AUTHORITY);
    add_field("amount", "51");
    call = compile("ps_compare_resource", true, NULL, true);
    assert(call.valid && !evaluate_query(node, scope));
    campaign.authority = 51;
    assert(evaluate_query(node, scope));
    add_field("instance", PS_LEGACY_INSTANCE);
    add_field("amount", "5");
    add_field("amount", "10");
    assert(!compile("ps_set_resource", false, NULL, true).valid);
    add_field("instance", PS_LEGACY_INSTANCE);
    add_token(TOKEN_LITERAL, "country");
    add_token(TOKEN_EQUAL, "=");
    add_token(TOKEN_OPEN, "{");
    add_field("nested", "MNG");
    add_token(TOKEN_CLOSE, "}");
    assert(!compile("ps_set_leader", false, NULL, true).valid);
    add_field("instance", "missing");
    add_field("value", "no");
    assert(!compile("ps_is_leader", true, NULL, true).valid);
    add_field("instance", PS_LEGACY_INSTANCE);
    assert(!compile("ps_is_leadr", true, NULL, true).valid);
    assert(!compile("ps_is_leader", true, "yes", true).valid);
    assert(diagnostics == 5);
    add_field("instance", "second");
    add_field("action", "invite");
    add_field("round", "2");
    add_field("value", "yes");
    call = compile("political_respond_request", false, NULL, true);
    assert(call.valid && call.operation == PS_RESPOND_REQUEST && call.args.round == 2 && call.args.value);
    add_field("instance", "second");
    add_field("action", "diet");
    add_field("status", "settled");
    add_field("option", "approve");
    call = compile("political_process_status", true, NULL, true);
    assert(call.valid && call.operation == PS_PROCESS_STATUS && call.args.status == PROCESS_SETTLED && !strcmp(call.args.option, "approve"));
    add_field("instance", "second");
    add_field("action", "diet");
    add_field("status", "done");
    assert(!compile("political_process_status", true, NULL, true).valid && diagnostics == 6);
    add_field("instance", "future_instance");
    add_field("type", "second");
    add_field("name", "shared_title");
    call = compile("political_create_instance", false, NULL, true);
    assert(call.valid && !strcmp(call.args.type, "second") && !strcmp(call.args.name, "shared_title"));
    call = compile("political_direct_war_allowed", true, NULL, true);
    state.war_query = war_mock;
    assert(call.valid && evaluate_query(node, scope));
    check_generic_scopes();
    add_field("instance", "second");
    add_field("message", "court_dispatch");
    add_field("actor", "ROOT");
    add_field("target", "PREV");
    call = compile("political_send_message", false, NULL, true);
    assert(call.valid && call.operation == PS_SEND_MESSAGE && !strcmp(call.args.message, "court_dispatch"));
    const unsigned char *previous = scope;
    unsigned char nested[0x50] = {0};
    memcpy(nested + 0x2c, "CAS", 3);
    nested[0x33] = 1;
    write_pointer(nested, LINK_PREV, previous);
    PsArguments bound;
    assert(!bind_script_arguments(call.operation, &call.args, election_tag("CAS"), election_tag("FRA"), 0, nested, &bound));
    assert(bound.actor == election_tag("FRA") && bound.target == election_tag("ASK"));
    add_field("instance", "second");
    assert(!compile("political_send_message", false, NULL, true).valid);
    add_field("instance", "second");
    add_field("message", "court_dispatch");
    assert(!compile("political_send_message", true, NULL, true).valid);
    puts("PASS native political_send_message arguments, ROOT and PREV resolution, missing message and trigger rejection.");
    puts("Native script mock passed: alias schema, scalar and block forms, scope, default comparison, bad values and vanilla delegation.");
}
