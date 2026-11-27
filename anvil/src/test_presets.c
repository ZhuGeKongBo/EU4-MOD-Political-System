#include "plugin.c"
#include <assert.h>

static const char *stock;
static char *fixture(const char *root, const char *relative) {
    char path[1024];
    assert(snprintf(path, sizeof(path), "%s/%s", root, relative) < (int)sizeof(path));
    FILE *file = fopen(path, "rb");
    assert(file && !fseek(file, 0, SEEK_END));
    const long length = ftell(file);
    assert(length >= 0 && !fseek(file, 0, SEEK_SET));
    char *bytes = malloc((size_t)length + 1);
    assert(bytes && fread(bytes, 1, (size_t)length, file) == (size_t)length);
    bytes[length] = 0;
    fclose(file);
    return bytes;
}

static bool mounted(const char *relative) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", stock, relative);
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    fclose(file);
    return true;
}

static void check(bool ok, const ScriptError *error) {
    if (!ok) fprintf(stderr, "%s:%u: %s\n", error->path, error->line, error->reason);
    assert(ok);
}

static bool valid_country(const void *tag) { return ((const unsigned char *)tag)[7] != 0; }
static uint64_t *find_country(const void *db, uint64_t *out, const char *tag, bool insensitive) {
    assert(db && insensitive);
    *out = UINT64_C(1) << 56 | UINT64_C(1) << 32;
    if (!strcmp(tag, "FRA") || !strcmp(tag, "SAV") || !strcmp(tag, "CAS") || !strcmp(tag, "POR") ||
        !strcmp(tag, "NAV")) memcpy(out, tag, 3);
    else *out = 0;
    return out;
}

static void check_discovered_capital(void) {
    unsigned char root[SYMVAL_PROVINCE_DISCOVERY_ARRAY_ARRAY + sizeof(void *)] = {0};
    unsigned char provinces[2 * SYMVAL_PROVINCE_DISCOVERY_ARRAY_STRIDE] = {0};
    unsigned char country[SYMVAL_COUNTRY_CAPITAL_SITE_OFFSET + sizeof(int32_t)] = {0};
    unsigned char db[0x120] = {0};
    unsigned char *province = provinces + SYMVAL_PROVINCE_DISCOVERY_ARRAY_STRIDE;
    uint32_t discovery = 0;
    void *table = provinces, *word = &discovery, *world_root = root, *country_db = db;
    void *countries[] = {NULL, country}, *lookup = countries;
    const int32_t capital = 1;
    memcpy(root + SYMVAL_PROVINCE_DISCOVERY_ARRAY_ARRAY, &table, sizeof(table));
    memcpy(province + SYMVAL_PROVINCE_DISCOVERY_FLAGS_WORDS, &word, sizeof(word));
    province[SYMVAL_PROVINCE_DISCOVERY_FLAGS_FLAGS] = SYMVAL_PROVINCE_DISCOVERY_FLAGS_ACTIVE;
    memcpy(country + 0x1344, &capital, sizeof(capital));
    memcpy(country + SYMVAL_COUNTRY_CAPITAL_SITE_OFFSET, &capital, sizeof(capital));
    memcpy(db + 0x118, &lookup, sizeof(lookup));
    plugin.root = root;
    plugin.root_slot = &world_root;
    plugin.countries_db = &country_db;
    plugin.lookup_tag = find_country;
    plugin.valid_tag = valid_country;
    plugin.capital_bound = true;
    plugin.province_discovered = native_province_discovered;
    plugin.province_limit = 2;
    const PoliticalInstanceSpec spec = {.id = "capital_gate"};
    PoliticalType *type = &plugin.types[0];
    type->definition.leader_enabled = true;
    type->definition.entry.defined = true;
    PoliticalInstance *instance = &plugin.instances[0];
    *instance = (PoliticalInstance){ .type = type, .spec = &spec, .enabled = true, .holder = election_tag("FRA") };
    const unsigned viewer = election_tag("SAV");
    EntryAccess access = instance_access(0, viewer);
    assert(!access.view && !access.visible && !access.enabled);
    discovery = 1u << 1;
    access = instance_access(0, viewer);
    assert(access.view && access.visible && access.enabled);
    instance->holder = 0;
    assert(!instance_access(0, viewer).visible);
    *instance = (PoliticalInstance){0};
    type->definition = (KhanDefinition){0};
    plugin.root = NULL;
    plugin.root_slot = NULL;
    plugin.countries_db = NULL;
    plugin.capital_bound = false;
    plugin.province_discovered = NULL;
    puts("PASS the real province discovery bit layout gates view, HUD and click in the shared access path.");
}

static unsigned mock_monarch_number = 100;

static AnvilResult test_monarch(const char *tag, uint32_t kind, AnvilGameMonarch *out) {
    assert(!strcmp(tag, "MNG") && kind == ANVIL_GAME_MONARCH_RULER);
    out->id = mock_monarch_number;
    return ANVIL_OK;
}

static AnvilResult test_monarch_name(const char *tag, uint32_t kind, char *out, uint32_t size, uint32_t *length) {
    assert(!strcmp(tag, "MNG") && kind == ANVIL_GAME_MONARCH_RULER);
    *length = (uint32_t)snprintf(out, size, "%s", mock_monarch_number == 100 ? "Qizhen" : "Jianshen");
    return ANVIL_OK;
}

static AnvilResult test_monarch_dynasty(const char *tag, uint32_t kind, char *out, uint32_t size, uint32_t *length) {
    assert(!strcmp(tag, "MNG") && kind == ANVIL_GAME_MONARCH_RULER);
    *length = (uint32_t)snprintf(out, size, "Zhu");
    return ANVIL_OK;
}

static void check_leader_history(void) {
    const char *files[] = {"history/countries/KHA - MongolKhanate.txt", "history/countries/MNG - Ming.txt"};
    const char *tags[] = {"KHA", "MNG"};
    const char *leaders[] = {"Kublai", "Yuanzhang"};
    const char *dynasties[] = {"Borjigin", "Zhu"};
    const unsigned start[] = {12600505, 13680123}, end[] = {13680123, 14440101};
    PoliticalType *type = &plugin.types[0];
    type->definition.leader_enabled = true;
    PoliticalInstance *instance = &plugin.instances[0];
    *instance = (PoliticalInstance){.type = type, .holder = election_tag("MNG")};
    ScriptError error = {0};
    for (unsigned i = 0; i < 2; ++i) {
        char *source = fixture(stock, files[i]);
        ScriptTree tree;
        check(script_tree_read(&tree, source, strlen(source), files[i], &error), &error);
        LeaderReign reign = {.country = election_tag(tags[i]), .start = start[i], .end = end[i]};
        read_historical_monarch(&tree, &reign);
        assert(!strcmp(reign.name, leaders[i]) && !strcmp(reign.dynasty, dynasties[i]));
        append_leader_reign(&instance->leaders, reign);
        script_tree_free(&tree);
        free(source);
    }
    static const AnvilGameWorldService world = {.monarch = test_monarch, .monarch_name = test_monarch_name,
        .monarch_dynasty = test_monarch_dynasty};
    plugin.world = &world;
    assert(sync_leader_reign(instance, 14441111));
    assert(!strcmp(current_reign(instance)->name, "Qizhen") && !strcmp(current_reign(instance)->dynasty, "Zhu"));
    assert(!sync_leader_reign(instance, 14441112));
    ++mock_monarch_number;
    assert(sync_leader_reign(instance, 14441201));
    assert(instance->leaders.count == 4 && instance->leaders.items[2].end == 14441201);
    assert(!strcmp(current_reign(instance)->name, "Jianshen"));
    char value[GK_SAVE_VALUE_SIZE], id[GK_SCRIPT_WORD_SIZE];
    LeaderReign decoded;
    assert(encode_leader_reign("chinese_style", &instance->leaders.items[2], value));
    assert(decode_leader_reign(value, id, &decoded) && !strcmp(id, "chinese_style"));
    assert(decoded.country == election_tag("MNG") && decoded.ruler == 100 && decoded.end == 14441201 &&
           !strcmp(decoded.name, "Qizhen") && !strcmp(decoded.dynasty, "Zhu"));
    *instance = (PoliticalInstance){0};
    type->definition = (KhanDefinition){0};
    plugin.world = NULL;
    puts("PASS vanilla historical monarch and dynasty, same-country succession, and instance-scoped reign save round trip.");
}

static void compare_rules(const KhanDefinition *inherited, const KhanDefinition *expanded) {
    static KhanDefinition rules[2];
    rules[0] = *inherited;
    rules[1] = *expanded;
    for (size_t i = 0; i < 2; ++i) {
        rules[i].source_line = 0;
        memset(rules[i].source_path, 0, sizeof(rules[i].source_path));
        memset(rules[i].entry.icon, 0, sizeof(rules[i].entry.icon));
        memset(rules[i].entry.tooltip, 0, sizeof(rules[i].entry.tooltip));
        rules[i].entry.order = 0;
    }
    assert(!memcmp(rules, rules + 1, sizeof(*rules)));
}

static unsigned threshold_notices;
static void capture_threshold(AnvilPlugin *self, AnvilLogLevel level, const char *message) {
    (void)self;
    if (level == ANVIL_LOG_INFO && strstr(message, "threshold=leadership_review")) ++threshold_notices;
}

typedef struct MockWidget {
    const void *const *methods;
    UiPoint position;
} MockWidget;

static UiPoint *get_widget_point(void *object, UiPoint *out) {
    *out = ((MockWidget *)object)->position;
    return out;
}

static void set_widget_point(void *object, uint64_t packed) {
    MockWidget *widget = object;
    widget->position.x = (int32_t)packed;
    widget->position.y = (int32_t)(packed >> 32);
}

static void check_placement(const char *mod, const char *map, const ScriptTree *gui, ScriptError *error) {
    static const void *const methods[0x288 / 8 + 1] = {
        [0x98 / 8] = get_widget_point, [0x288 / 8] = set_widget_point
    };
    MockWidget widget = { methods, {433, 539} };
    plugin.shield_position = get_widget_point;
    plugin.shield_move = set_widget_point;
    const PoliticalLayout *hre = &plugin.layouts.items[layout_index(&plugin.layouts, "hre_layout")];
    assert(hre->roles.rows == 4 && hre->roles.columns == 2 && hre->members.wheel.height == 118);
    assert(move_layout_control(&widget, (UiPoint){6, 9}, true));
    assert(widget.position.x == 439 && widget.position.y == 548);
    assert(move_layout_control(&widget, (UiPoint){0}, true));
    assert(widget.position.x == 439 && widget.position.y == 548);

    static ContentLayouts manual;
    char *source = fixture(mod, map);
    for (char *at = source; (at = strstr(at, "auto_place = yes")); ++at) memcpy(at + 13, "no ", 3);
    check(collect_layouts(&manual, source, strlen(source), map, error), error);
    free(source);
    for (size_t i = 0; i < manual.count; ++i) {
        PoliticalLayout *layout = &manual.items[i];
        check(check_layout_gui(layout, gui, error), error);
        assert(!layout->leader_frame[0] && !layout->members.frame[0] && !layout->roles.frame[0]);
        assert(!layout->leader_offset.x && !layout->members.shield_offset[0].y);
    }

    source = fixture(mod, hre->gui_file);
    char *first = strstr(source, "name = \"member_flag_00\"");
    char *size = first ? strstr(first, "size = { x = 32 y = 32 }") : NULL;
    assert(size);
    memcpy(size + 13, "96", 2);
    ScriptTree bad;
    check(script_tree_read(&bad, source, strlen(source), hre->gui_file, error), error);
    PoliticalLayout too_small = *hre;
    assert(!check_layout_gui(&too_small, &bad, error) && error->line &&
           strstr(error->reason, "must fit in one frame cell"));
    script_tree_free(&bad);
    free(source);
    puts("PASS four layouts opt into native frame placement. Manual fallback, shield/label fit, and too-wide shield diagnostic checked.");
}

static const PoliticalInstanceSpec *find_history(const PoliticalRoster *history, const char *id) {
    for (size_t i = 0; i < history->count; ++i)
        if (!strcmp(history->items[i].id, id)) return &history->items[i];
    return NULL;
}

static void settle_pair(const KhanDefinition *inherited, const KhanDefinition *expanded, const PoliticalRoster *history) {
    static const AnvilHost host = { .log = capture_threshold };
    plugin.host = &host;
    unsigned char country[0x1348] = {0}, db[0x120] = {0};
    const int32_t provinces = 1;
    memcpy(country + 0x1344, &provinces, sizeof(provinces));
    void *countries[] = {NULL, country};
    void *array = countries, *db_ref = db;
    memcpy(db + 0x118, &array, sizeof(array));
    plugin.countries_db = &db_ref;
    plugin.lookup_tag = find_country;
    plugin.valid_tag = valid_country;
    const KhanDefinition *const cases[] = {inherited, expanded};
    const char *const ids[] = {"pf075_union_west", "pf075_union_flat"};
    const unsigned actor = election_tag("FRA"), appointee = election_tag("SAV");
    for (size_t i = 0; i < 2; ++i) {
        const PoliticalInstanceSpec *spec = find_history(history, ids[i]);
        assert(spec);
        plugin.types[i].definition = *cases[i];
        PoliticalInstance *instance = &plugin.instances[i];
        *instance = (PoliticalInstance){ .enabled = true, .configured = true, .spec = spec, .type = &plugin.types[i] };
        assert(seed_instance_history(instance, 14441111) && instance->holder == actor && instance->members.count == 2);
        const unsigned slot = political_action_index(cases[i]->actions, cases[i]->action_count, "council");
        bool eligible = false;
        assert(!role_eligible(instance, &cases[i]->roles[0], appointee, true, &eligible) && eligible);
        ActionPlan plan;
        const ActionRequest action = {.slot = slot, .actor = actor};
        assert(!commit_action(instance, &action, 14441111, &plan));
        assert(plan.before == 45000 && plan.after == 35000 && passed_action(instance, "council"));
    }
    assert(threshold_notices == 2);
    puts("PASS inherited and expanded rules match, with identical SAV appointment eligibility, reform settlement from 45 to 35 and threshold crossing.");

    const PoliticalInstanceSpec *south = find_history(history, "pf075_union_south");
    if (south && south->change_count) {
        const unsigned cas = election_tag("CAS"), por = election_tag("POR"), nav = election_tag("NAV");
        PoliticalInstance *instance = &plugin.instances[2];
        *instance = (PoliticalInstance){ .spec = south, .type = &plugin.types[0] };
        assert(seed_instance_history(instance, 14441111) && instance->holder == cas && instance->members.count == 2);
        assert(instance->roles[0].count == 1 && instance->roles[0].countries[0] == por);
        *instance = (PoliticalInstance){ .spec = south, .type = &plugin.types[0] };
        assert(seed_instance_history(instance, 14500101) && instance->holder == cas && instance->members.count == 3);
        assert(has_tag(instance->members.tags, instance->members.count, nav));
        *instance = (PoliticalInstance){ .spec = south, .type = &plugin.types[0] };
        assert(seed_instance_history(instance, 14600101) && instance->holder == por && instance->members.count == 2);
        assert(instance->roles[0].count == 1 && instance->roles[0].countries[0] == cas);
        puts("PASS actual south preset at 1444, 1450 and 1460: CAS/POR, NAV joins and leaves, leader and quartermaster swap.");
    }
}

static void check_script_example(const char *mod) {
    const char *const paths[] = {
        PS_TYPE_DIRECTORY "/pf077_shah_type.txt", PS_HISTORY_DIRECTORY "/pf077_shah_history.txt",
        PS_LAYOUT_DIRECTORY "/pf077_shah_hud.txt", "common/triggered_modifiers/pf077_shah_benefits.txt",
        "events/pf077_shah_probe.txt", "examples/pf077_shah_check.txt"
    };
    ContentTypes catalog = {0};
    ScriptError error = {0};
    char *bytes = fixture(mod, paths[0]);
    check(collect_content_types(&catalog, bytes, strlen(bytes), paths[0], &error), &error);
    const KhanDefinition *type = resolve_content_type(&catalog, "shah_empire", &error);
    check(type != NULL, &error);
    const unsigned authority = resource_find(type->resources, type->resource_count, "authority");
    const unsigned stipend = resource_find(type->resources, type->resource_count, "stipend");
    assert(authority < type->resource_count && stipend < type->resource_count);
    assert(type->resources[authority].terms[0].political_test == PS_CONTENT_REFORM &&
           type->resources[authority].terms[0].modifier == 500);
    assert(type->resources[stipend].terms[0].political_test == PS_HAS_ROLE && type->resources[stipend].terms[0].add == 1000);
    free_content_types(&catalog);
    char *role = strstr(bytes, "role = satrap");
    assert(role);
    memcpy(role + strlen("role = "), "bad_id", strlen("satrap"));
    check(collect_content_types(&catalog, bytes, strlen(bytes), paths[0], &error), &error);
    assert(!resolve_content_type(&catalog, "shah_empire", &error) && strstr(error.reason, "unknown role"));
    free_content_types(&catalog);
    free(bytes);

    PoliticalRoster roster = {0};
    bytes = fixture(mod, paths[1]);
    check(append_content_history(bytes, strlen(bytes), paths[1], &roster, &error), &error);
    assert(roster.count == 1 && !strcmp(roster.items[0].id, "shah_empire_main") &&
           !strcmp(roster.items[0].type, "shah_empire") && !strcmp(roster.items[0].leader, "CAS"));
    free(bytes);
    bytes = fixture(mod, paths[2]);
    check(collect_layouts(&plugin.layouts, bytes, strlen(bytes), paths[2], &error), &error);
    free(bytes);
    HudRegion *hud = &plugin.layouts.hud[plugin.layouts.hud_count - 1];
    const char *const documents[] = {hud->gui_file, hud->gfx_file, paths[3], paths[4], paths[5]};
    for (unsigned i = 0; i < sizeof(documents) / sizeof(*documents); ++i) {
        ScriptTree tree;
        bytes = fixture(mod, documents[i]);
        check(script_tree_read(&tree, bytes, strlen(bytes), documents[i], &error), &error);
        if (i == 0) check(check_hud_gui(hud, &tree, &error), &error);
        if (i == 1) check(check_hud_gfx(hud, &tree, mounted, &error), &error);
        script_tree_free(&tree);
        free(bytes);
    }
    puts("PASS editable Shah type and history, conditional resource rules, missing role diagnostic, stock crown texture and script block structure.");
}

static int32_t native_values[2], read_country_modifier, read_province_modifier;
static unsigned registered_modifiers, flags_used;
static int32_t mock_intern(const GameString *key, bool reserved) {
    assert(!reserved);
    assert(!strcmp(string_text(key), registered_modifiers ? "local_cohesion" : "cohesion"));
    return 40000 + (int32_t)registered_modifiers;
}

static void mock_assign(GameString *string, const char *value, size_t length) {
    string->size = length;
    if (length < sizeof(string->inline_text)) strcpy(string->inline_text, value);
    else {
        string->capacity = length;
        memcpy(string->inline_text, &value, sizeof(value));
    }
}

static void mock_release(GameString *string) { (void)string; }

static void *mock_construct(void *record, int32_t token, uint32_t id, const GameString *label, uint32_t flags, void **value) {
    assert(token == 40000 + (int32_t)registered_modifiers && id == 0 && label->size && !*value);
    assert(flags == (registered_modifiers ? 0x83 : 0x97));
    flags_used |= 1u << registered_modifiers;
    return record;
}

static int32_t mock_register(void *record) {
    assert(record);
    return 1300 + (int32_t)registered_modifiers++;
}

static AnvilResult mock_modifier_lookup(const char *name, int32_t *id) {
    const int32_t index = !strcmp(name, "cohesion") ? 0 : !strcmp(name, "local_cohesion") ? 1 : -1;
    if (index < 0 || (unsigned)index >= registered_modifiers) return ANVIL_ERROR_NOT_FOUND;
    *id = 1300 + index;
    return ANVIL_OK;
}

static AnvilResult mock_country_modifier(const char *tag, int32_t id, int32_t *value) {
    assert(!strcmp(tag, "FRA") && id == 1300);
    *value = native_values[0];
    ++read_country_modifier;
    return ANVIL_OK;
}

static AnvilResult mock_province_modifier(int32_t province, int32_t id, int32_t *value) {
    assert(province == 183 && id == 1301);
    *value = native_values[1];
    ++read_province_modifier;
    return ANVIL_OK;
}

static void check_growth_sources(const char *mod, const KhanDefinition *expanded) {
    static const AnvilHost host = {.log = capture_threshold};
    plugin.host = &host;
    struct { GameString key; unsigned char rest[0x40]; } registry[2] = {
        { .key = { .inline_text = "land_morale", .size = 11, .capacity = 15 } },
        { .key = { .inline_text = "local_cohesion", .size = 14, .capacity = 15 } },
    };
    int32_t native_id;
    assert(startup_modifier_id(registry, registry + 2, "local_cohesion", &native_id) == ANVIL_OK && native_id == 1);
    assert(startup_modifier_id(registry, registry + 2, "cohesion", &native_id) == ANVIL_ERROR_NOT_FOUND);
    assert(startup_modifier_id(NULL, NULL, "cohesion", &native_id) == ANVIL_ERROR_WRONG_PHASE);
    const char *const file = PS_NATIVE_MODIFIER_DIRECTORY "/cohesion.txt";
    char *bytes = fixture(mod, file);
    ScriptError error = {0};
    check(collect_native_modifiers(&plugin.modifiers, bytes, strlen(bytes), file, &error), &error);
    assert(plugin.modifiers.count == 2);
    assert(!collect_native_modifiers(&plugin.modifiers, bytes, strlen(bytes), file, &error) && error.line == 2);
    free(bytes);
    const char *wrong_scope = "other = { scope = ruler }";
    assert(!collect_native_modifiers(&plugin.modifiers, wrong_scope, strlen(wrong_scope), file, &error));
    assert(strstr(error.reason, "country or province") && plugin.modifiers.count == 2);
    const ModifierRegistration registration = {
        mock_intern, mock_construct, mock_register, mock_assign, mock_release, mock_modifier_lookup
    };
    for (unsigned i = 0; i < plugin.modifiers.count; ++i)
        check(register_native_modifier(&registration, &plugin.modifiers.items[i], &error), &error);
    assert(flags_used == 3);
    assert(!register_native_modifier(&registration, &plugin.modifiers.items[0], &error) && registered_modifiers == 2);
    assert(!check_growth_modifiers(expanded));

    static PoliticalType type;
    type.definition = *expanded;
    static PoliticalInstance instance;
    const PoliticalInstanceSpec spec = { .id = "pf075_union_flat" };
    instance.spec = &spec;
    instance.type = &type;
    instance.holder = election_tag("FRA");
    ResourceDefinition *resource = &type.definition.resources[0];
    resource->term_count = 0; // No stronger member in this test.
    assert(!strcmp(resource->id, "cohesion") && resource->monthly == 1000);
    const AnvilGameWorldService world = { .country_modifier = mock_country_modifier, .province_modifier = mock_province_modifier };
    plugin.world = &world;
    assert(store_balance(&instance.accounts[0], 0, 45000));
    const Shogunate no_members = {0};
    native_values[0] = 100;
    assert(settle_resource_month(&instance, &no_members, 144412));
    assert(resource_balance(resource, &instance.accounts[0], 0) == 46100);
    native_values[0] = 200;
    assert(settle_resource_month(&instance, &no_members, 144501));
    assert(resource_balance(resource, &instance.accounts[0], 0) == 47300);
    native_values[0] = 0;
    assert(settle_resource_month(&instance, &no_members, 144502));
    assert(resource_balance(resource, &instance.accounts[0], 0) == 48300);

    strcpy(resource->growth_modifier, "local_cohesion");
    assert(check_growth_modifiers(&type.definition));
    resource->modifier_source = GROWTH_LEADER_CAPITAL;
    assert(!check_growth_modifiers(&type.definition));
    unsigned char country[0x2920] = {0}, db[0x120] = {0};
    const int32_t provinces = 1, capital = 183;
    memcpy(country + 0x1344, &provinces, sizeof(provinces));
    memcpy(country + SYMVAL_COUNTRY_CAPITAL_SITE_OFFSET, &capital, sizeof(capital));
    void *countries[] = {NULL, country};
    void *array = countries, *db_ref = db;
    memcpy(db + 0x118, &array, sizeof(array));
    plugin.countries_db = &db_ref;
    plugin.lookup_tag = find_country;
    plugin.valid_tag = valid_country;
    plugin.capital_bound = true;
    native_values[1] = 100;
    assert(settle_resource_month(&instance, &no_members, 144503));
    assert(resource_balance(resource, &instance.accounts[0], 0) == 49400);
    assert(read_country_modifier == 3 && read_province_modifier == 1);
    plugin.capital_bound = false;
    plugin.countries_db = NULL;
    plugin.world = NULL;

    bytes = fixture(mod, "pf079_modifiers_check.txt");
    EntryCondition triggers;
    const char *native_read = strstr(bytes, "{ political_compare_resource");
    Lexer lexer = lexer_for(native_read, strlen(native_read), &error);
    memset(&triggers, 0, sizeof(triggers));
    assert(read_entry_group(&lexer, &triggers, ENTRY_AND) && triggers.nodes[1].args.fixed_amount == 46100);
    free(bytes);
    puts("PASS mounted modifier declarations and diagnostics, scoped mocked engine registration, inherited growth binding, native sum 0/0.1/0.2 and provincial capital lookup.");
}

static const char *mock_government(unsigned country, const char *key, bool *yes) {
    assert(!strcmp(key, "monarchy"));
    *yes = country != election_tag("ULM");
    return NULL;
}

static const char *mock_has_heir(unsigned country, bool *yes) {
    *yes = country == election_tag("HAB");
    return NULL;
}

static void check_nomination(const KhanDefinition *electoral) {
    const unsigned hab = election_tag("HAB"), ulm = election_tag("ULM");
    const NominationRules *rules = &electoral->succession.nomination;
    assert(rules->defined && rules->count == 2);
    const EntryContext viewer = {.viewer = ulm, .country_government = mock_government, .country_heir = mock_has_heir};
    EntryContext voter = viewer;
    voter.viewer = hab;
    unsigned elector_terms, viewer_terms;
    assert(!nomination_terms(rules, &voter, true, &elector_terms));
    assert(!nomination_terms(rules, &viewer, false, &viewer_terms));
    assert((elector_terms & viewer_terms) == 3);
    assert(nomination_score(rules, 50, 0, elector_terms & viewer_terms) == -1950);
    assert(nomination_score(rules, -50, 0, elector_terms & viewer_terms) == -2000);
    assert(!nomination_terms(rules, &voter, false, &viewer_terms) && !viewer_terms);
    const unsigned fra = election_tag("FRA");
    voter.viewer = fra;
    assert(!nomination_terms(rules, &voter, false, &viewer_terms) && viewer_terms == 2);

    ElectionCountry countries[] = {{.tag = hab, .tumen = true}, {.tag = ulm}, {.tag = fra}};
    ElectionTable table = {.rows = countries, .count = 3};
    const int16_t opinions[] = {0, 50, -10, -5, 0, 0, 20, 0, 0};
    const int16_t nomination[] = {0, -1950, -1010, -5, -2000, -1000, 20, -2000, -1000};
    ElectionState state = {0};
    election_tally(&electoral->election, &state, &table, hab, opinions, nomination);
    assert(countries[0].support == hab && countries[0].weight == 1 && table.winner == hab);
    assert(!election_prefer(&state, hab, ulm, 2));
    election_tally(&electoral->election, &state, &table, hab, opinions, nomination);
    assert(countries[0].support == ulm && countries[0].manual && countries[0].weight == 2);
    assert(countries[1].votes == 2);
    puts("PASS authored nomination conditions, signed range, elector-to-viewer opinion, nominations vs candidate eligibility and vote weight.");
}

int main(int argc, char **argv) {
    assert(argc == 3 || argc == 4);
    const char *mod = argv[1];
    stock = argv[2];
    check_discovered_capital();
    check_leader_history();
    ContentTypes catalog = {0};
    ScriptError error = {0};
    static const char *const files[] = {
        PS_TYPE_DIRECTORY "/pf075_styles.txt", PS_TYPE_DIRECTORY "/pf075_cases.txt", PS_TYPE_DIRECTORY "/pf075_expanded.txt"
    };
    for (size_t i = 0; i < sizeof(files) / sizeof(*files); ++i) {
        char *bytes = fixture(mod, files[i]);
        check(collect_content_types(&catalog, bytes, strlen(bytes), files[i], &error), &error);
        free(bytes);
    }
    const char *map = PS_LAYOUT_DIRECTORY "/pf075_layouts.txt";
    char *bytes = fixture(mod, map);
    check(collect_layouts(&plugin.layouts, bytes, strlen(bytes), map, &error), &error);
    free(bytes);
    assert(plugin.layouts.count == 4 && plugin.layouts.hud_count == 1);
    HudRegion *hud = &plugin.layouts.hud[0];
    ScriptTree tree;
    bytes = fixture(mod, hud->gui_file);
    check(script_tree_read(&tree, bytes, strlen(bytes), hud->gui_file, &error), &error);
    static const char *const parents[] = {"hre_style", "celestial_style", "papacy_style", "federation_style"};
    static const char *const cases[] = {"pf075_electoral_assembly", "pf075_celestial_court", "pf075_curia_council", "demo_union"};
    for (size_t i = 0; i < 4; ++i) {
        const KhanDefinition *parent = resolve_content_type(&catalog, parents[i], &error);
        check(parent && parent->abstract, &error);
        const KhanDefinition *config = resolve_content_type(&catalog, cases[i], &error);
        check(config && !config->abstract && config->entry.defined, &error);
        const int index = layout_index(&plugin.layouts, config->entry.layout);
        assert(index >= 0);
        PoliticalLayout *layout = &plugin.layouts.items[index];
        check(check_layout_gui(layout, &tree, &error), &error);
        assert(!strcmp(layout->background_sprite, config->entry.background));
        assert(layout->area.x >= -640 && layout->area.y >= -360 &&
               layout->area.x + layout->area.width <= 640 && layout->area.y + layout->area.height <= 360);
        assert(layout->members.rows && layout->roles.rows && layout->resources.rows && layout->actions.rows);
        assert(layout->leader_frame[0] && layout->roles.rows == 4 && layout->roles.columns == 2);
        assert(layout->roles.group_heading[0] && layout->roles.group_hover[0] && !layout->roles.label[0]);
        assert(layout->leader_history[0] && layout->resource_summary.value[0] && layout->resource_summary.growth[0]);
        // Authored positions in the bundled panels must match the frame calculation.
        assert(!layout->leader_offset.x && !layout->leader_offset.y);
        for (unsigned n = 0; n < layout->members.rows * layout->members.columns; ++n) {
            UiPoint offset = layout->members.shield_offset[n];
            assert(!offset.x && !offset.y);
        }
        for (unsigned n = 0; n < layout->roles.rows * layout->roles.columns; ++n) {
            UiPoint shield = layout->roles.shield_offset[n];
            UiPoint label = layout->roles.label_offset[n];
            assert(!shield.x && !shield.y && !label.x && !label.y);
        }
        if (!i) assert(layout->ballot[0] && config->succession.mode == LEADER_ELECTED);
    }
    check(check_hud_gui(hud, &tree, &error), &error);
    check_placement(mod, map, &tree, &error);
    script_tree_free(&tree);
    free(bytes);
    bytes = fixture(mod, hud->gfx_file);
    check(script_tree_read(&tree, bytes, strlen(bytes), hud->gfx_file, &error), &error);
    check(check_hud_gfx(hud, &tree, mounted, &error), &error);
    assert(hud->icon_count == 5);
    script_tree_free(&tree);
    free(bytes);
    puts("PASS four content presets, independent native layouts and entrance sprites, ballot binding, and panel bounds at 1280x720.");
    puts("PASS all four shipped panels have zero runtime position offsets for shields, role labels and nomination controls.");
    const KhanDefinition *inherited = resolve_content_type(&catalog, "demo_union", &error);
    const KhanDefinition *expanded = resolve_content_type(&catalog, "demo_union_expanded", &error);
    assert(inherited && expanded);
    compare_rules(inherited, expanded);
    check_nomination(resolve_content_type(&catalog, "pf075_electoral_assembly", &error));
    check_growth_sources(mod, expanded);
    const char *history_file = PS_HISTORY_DIRECTORY "/pf075_cases.txt";
    bytes = fixture(mod, history_file);
    PoliticalRoster history = {0};
    check(append_content_history(bytes, strlen(bytes), history_file, &history, &error), &error);
    free(bytes);
    assert(history.count == 6);
    settle_pair(inherited, expanded, &history);
    if (find_history(&history, "pf075_electoral")->change_count) {
        const KhanDefinition *electoral = resolve_content_type(&catalog, "pf075_electoral_assembly", &error);
        const KhanDefinition *mandate = resolve_content_type(&catalog, "pf075_celestial_court", &error);
        const KhanDefinition *curia = resolve_content_type(&catalog, "pf075_curia_council", &error);
        assert(electoral->member_source == MEMBERS_CAPITAL_TERRITORY && !strcmp(electoral->territory_history_key, "hre"));
        assert(!strcmp(electoral->roles[0].id, "elector") && !strcmp(electoral->roles[0].history_key, "elector"));
        assert(mandate->member_source == MEMBERS_DIRECT_SUBJECTS && !strcmp(mandate->subject_type, "tributary_state"));
        assert(curia->member_source == MEMBERS_QUERY && curia->member_query.nodes[1].test == ENTRY_RELIGION &&
               !strcmp(curia->member_query.nodes[1].args.reform, "catholic"));
        puts("PASS actual four presets select world/territory sources from type rules, with no explicit HRE, Mandate or Curia roster.");
    }
    for (size_t i = 0; i < history.count; ++i) free(history.items[i].changes);
    free_content_types(&catalog);
    if (argc == 4) check_script_example(argv[3]);
}
