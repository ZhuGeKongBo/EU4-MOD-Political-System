#include "content_layout.h"
#include "save_state.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *game_directory;

static char *read_fixture(const char *root, const char *relative) {
    char path[1024];
    assert(snprintf(path, sizeof(path), "%s/%s", root, relative) < (int)sizeof(path));
    FILE *file = fopen(path, "rb");
    assert(file && !fseek(file, 0, SEEK_END));
    const long length = ftell(file);
    assert(length >= 0 && !fseek(file, 0, SEEK_SET));
    char *bytes = malloc((size_t)length + 1);
    assert(bytes && fread(bytes, 1, (size_t)length, file) == (size_t)length);
    bytes[length] = 0;
    assert(!fclose(file));
    return bytes;
}

static bool game_file(const char *relative) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", game_directory, relative);
    FILE *file = fopen(path, "rb");
    if (file) fclose(file);
    return file != NULL;
}

static void insist(bool success, const ScriptError *error) {
    if (!success) fprintf(stderr, "%s:%u:%u: %s\n", error->path, error->line, error->column, error->reason);
    assert(success);
}

static ContentTypes types;
static ContentLayouts layouts;
static PoliticalRoster roster;
static unsigned viewer;
static bool show, click, no_view, west_only, query_error;
static const char *flag(unsigned actor, const char *name, bool *result) {
    assert(actor == viewer);
    if (query_error) return "wrong native phase";
    if (!strcmp(name, "demo_union_show")) *result = show;
    else if (!strcmp(name, "demo_union_click")) *result = click;
    else if (!strcmp(name, "demo_union_no_view")) *result = no_view;
    else if (!strcmp(name, "demo_union_west_only")) *result = west_only;
    else assert(false);
    return NULL;
}

static const char *query(PsOperation op, const PsArguments *args, unsigned actor, bool *result) {
    assert(op == PS_IS_MEMBER && args->country == election_tag("POR") && actor == viewer);
    *result = !strcmp(args->instance, "demo_union_south");
    return NULL;
}

static InstanceChoices choices_for(const KhanDefinition *type) {
    InstanceChoices choices = {0};
    for (size_t i = 0; i < 2; ++i) {
        EntryAccess access;
        const EntryContext context = {.instance = roster.items[i].id, .viewer = viewer, .query = query, .country_flag = flag};
        const char *reason = entry_permissions(&type->entry, &context, &access);
        if (query_error) assert(reason && !access.view && !access.enabled);
        else assert(!reason);
        add_instance_choice(&choices, i, access);
    }
    return choices;
}

static void check_conditions(const KhanDefinition *type) {
    viewer = election_tag("FRA");
    InstanceChoices choices = choices_for(type);
    assert(entry_route(&choices) == ENTRY_HIDDEN);
    show = true;
    choices = choices_for(type);
    assert(choices.count == 2 && entry_route(&choices) == ENTRY_DISABLED);
    click = true;
    choices = choices_for(type);
    assert(choices.enabled_count == 2 && entry_route(&choices) == ENTRY_SELECTOR);
    west_only = true;
    choices = choices_for(type);
    assert(choices.enabled_count == 1 && entry_route(&choices) == ENTRY_SELECTOR);
    assert(choice_available(&choices, 0) && !choice_available(&choices, 1));
    no_view = true;
    choices = choices_for(type);
    assert(entry_route(&choices) == ENTRY_HIDDEN);
    no_view = false;
    query_error = true;
    choices = choices_for(type);
    assert(entry_route(&choices) == ENTRY_HIDDEN);
    add_instance_choice(&choices, 0, (EntryAccess){ true, true, true });
    assert(entry_route(&choices) == ENTRY_DIRECT && choice_available(&choices, 0));
    assert(hud_page_count(0, 2) == 0 && hud_page_count(2, 2) == 1 && hud_page_count(3, 2) == 2);
    puts("PASS viewer-scoped flags, NOT error, hidden/disabled/direct/chooser, stale selection and HUD overflow boundaries.");
}

static void check_inheritance(const KhanDefinition *union_type) {
    const char *overrides =
        "flat = {\n"
        " leader = { title = Convener } members = { source = explicit title = Members max_per_country = 1 }\n"
        " existence = { max_instances = 2 } resources = { authority = { initial = 30 } }\n"
        " interface = { layout = demo_union_panel background = GFX_hre_bg title_key = demo_union_name\n"
        " view_trigger = { NOT = { has_country_flag = demo_union_no_view } }\n"
        " entry_button = { region = demo_political_buttons icon = GFX_demo_union_entry order = 10\n"
        " tooltip = demo_union_entry disabled_tooltip = demo_union_entry_disabled visible = { has_country_flag = demo_union_show }\n"
        " enabled = { has_country_flag = demo_union_click NOT = { AND = { has_country_flag = demo_union_west_only\n"
        " political_is_member = { country = POR } } } } } } }\n"
        " without = { copy_from = demo_union authority = no interface = { entry_button = no } }\n"
        " again = { copy_from = without authority = yes }\n"
        " cleared = { copy_from = demo_union resources = { authority = clear } interface = { view_trigger = {} } }\n";
    ScriptError error;
    insist(collect_content_types(&types, overrides, strlen(overrides), "content/flat.txt", &error), &error);
    const KhanDefinition *flat = resolve_content_type(&types, "flat", &error);
    insist(flat != NULL, &error);
    assert(!memcmp(&union_type->entry, &flat->entry, sizeof(flat->entry)));
    assert(!strcmp(flat->emperor, union_type->emperor) && !strcmp(flat->member_title, union_type->member_title) &&
           flat->member_source == union_type->member_source && flat->max_memberships == union_type->max_memberships &&
           flat->max_instances == union_type->max_instances && flat->starting_authority == union_type->starting_authority);
    const KhanDefinition *base = resolve_content_type(&types, "demo_union_base", &error);
    assert(base && base->abstract && base->starting_authority == 20 && !union_type->abstract);
    const KhanDefinition *again = resolve_content_type(&types, "again", &error);
    assert(again && again->authority_enabled && again->starting_authority == 0 && !again->entry.defined);
    const KhanDefinition *clear = resolve_content_type(&types, "cleared", &error);
    assert(clear && !clear->authority_enabled && clear->entry.enabled.count && !clear->entry.view.count);

    const char *bad =
        "bad_field = {\n    ledaer = yes\n}\n"
        "loop_a = { copy_from = loop_b }\nloop_b = { copy_from = loop_a }\n"
        "late_parent = { leader = yes copy_from = default }\n"
        "bad_gate = { interface = { view_trigger = { NOT = { province = { tag = FRA } } } } }\n"
        "bad_dependency = { leader = no members = { source = leader_subjects } }\n"
        "bad_resource = { authority = no resources = { authority = yes } }\n";
    const char *ids[] = { "bad_field", "loop_a", "late_parent", "bad_dependency", "bad_resource" };
    const unsigned lines[] = { 2, 4, 6, 8, 9 };
    insist(collect_content_types(&types, bad, strlen(bad), "content/bad.txt", &error), &error);
    for (size_t i = 0; i < sizeof(ids) / sizeof(*ids); ++i) {
        assert(!resolve_content_type(&types, ids[i], &error));
        assert(!strcmp(error.path, "content/bad.txt") && error.line == lines[i] && error.column > 0);
        printf("PASS diagnostic %s:%u:%u %s\n", error.path, error.line, error.column, error.reason);
    }
    const KhanDefinition *gate = resolve_content_type(&types, "bad_gate", &error);
    assert(gate && gate->entry.view.nodes[0].test == ENTRY_NATIVE &&
           gate->entry.view.nodes[0].native.line == 7);
    puts("PASS forward references, abstract/default handling, flat equivalence, clear, block replacement, disable and re-enable.");
}

static void check_history(void) {
    const PoliticalInstanceSpec *west = &roster.items[0], *south = &roster.items[1];
    assert(roster.count == 3 && west->date == 14441111 && south->date == west->date);
    assert(!strcmp(west->title, south->title) && strcmp(west->id, south->id));
    assert(!strcmp(west->leader, "FRA") && !strcmp(south->leader, "CAS"));
    assert(west->members.count == 2 && west->members.tags[1] == election_tag("SAV"));
    assert(south->members.count == 2 && south->members.tags[1] == election_tag("POR"));
    char record[GK_SAVE_VALUE_SIZE], id[GK_SCRIPT_WORD_SIZE];
    MemberTags members;
    assert(encode_explicit_members(west->id, &west->members, record) &&
           decode_explicit_members(record, id, &members) && !strcmp(id, "demo_union_west") &&
           !memcmp(&members, &west->members, sizeof(members)));
    assert(encode_explicit_members(south->id, &south->members, record) &&
           decode_explicit_members(record, id, &members) && !strcmp(id, "demo_union_south") &&
           !memcmp(&members, &south->members, sizeof(members)));
    strcpy(record, "M1|demo_union_south|PORPOR");
    assert(!decode_explicit_members(record, id, &members));
    strcpy(record, "M1|demo_union_south|");
    assert(decode_explicit_members(record, id, &members) && members.count == 0);
    const char *bad = "new = { type = demo_union date = 1444.1.1 }\n"
                      "demo_union_south = { type = demo_union date = 1444.1.1 }";
    ScriptError error;
    assert(!append_content_history(bad, strlen(bad), "history/bad.txt", &roster, &error) &&
           error.line == 2 && roster.count == 3);
    puts("PASS dated named histories, same display name and isolated IDs and members, save field round trip, invalid and duplicate input.");
}

static void check_window_managed_background(void) {
    const char gui[] =
        "guiTypes = { windowType = { name = fixture_panel backGround = fixture_background\n"
        " guiButtonType = { name = fixture_background quadTextureSprite = GFX_hre_bg } } }";
    ScriptTree tree;
    ScriptError error;
    PoliticalLayout layout = { .window = "fixture_panel", .background = "fixture_background" };
    insist(script_tree_read(&tree, gui, strlen(gui), "interface/fixture.gui", &error), &error);
    insist(check_layout_gui(&layout, &tree, &error), &error);
    assert(!strcmp(layout.background_sprite, "GFX_hre_bg"));
    strcpy(layout.hover, "fixture_background");
    assert(!check_layout_gui(&layout, &tree, &error) && strstr(error.reason, "separate native button"));
    script_tree_free(&tree);
    const char click_backdrop[] =
        "guiTypes = { windowType = { name = fixture_panel\n"
        " guiButtonType = { name = fixture_background quadTextureSprite = GFX_hre_bg } } }";
    insist(script_tree_read(&tree, click_backdrop, strlen(click_backdrop), "interface/fixture.gui", &error), &error);
    assert(!check_layout_gui(&layout, &tree, &error) && strstr(error.reason, "needs an iconType"));
    script_tree_free(&tree);
}

static void check_tooltip_references(void) {
    const char invalid[] = "guiTypes = {\n"
        "windowType = { name = sample_panel\n"
        "guiButtonType = { name = close\n"
        "tooltip = MISSING_TEXT_BOX tooltipText = CLOSE_HINT } } }\n";
    const char valid[] = "guiTypes = {\n"
        "instantTextBoxType = { name = hint_box }\n"
        "windowType = { name = sample_panel\n"
        "guiButtonType = { name = close tooltipText = CLOSE_HINT }\n"
        "guiButtonType = { name = help tooltip = hint_box tooltipText = HELP_HINT } } }\n";
    ScriptTree tree;
    ScriptError error;
    insist(script_tree_read(&tree, invalid, strlen(invalid), "panel-invalid.gui", &error), &error);
    assert(!check_gui_window_tooltips(&tree, "sample_panel", &error) && error.line == 4 &&
           !strcmp(error.path, "panel-invalid.gui") && strstr(error.reason, "MISSING_TEXT_BOX") &&
           strstr(error.reason, "tooltipText"));
    script_tree_free(&tree);
    insist(script_tree_read(&tree, valid, strlen(valid), "panel-valid.gui", &error), &error);
    insist(check_gui_window_tooltips(&tree, "sample_panel", &error), &error);
    script_tree_free(&tree);

    const char hud[] = "guiTypes = {"
        "windowType = { name = strip_00 guiButtonType = {name = open quadTextureSprite = GFX_sample } }\n"
        "windowType = { name = page guiButtonType = { name = ps_page_next tooltip = UNKNOWN_PAGE_BOX } } }";
    HudRegion region = { .container = "strip", .button_template = "open", .page_window = "page", .capacity = 1 };
    insist(script_tree_read(&tree, hud, strlen(hud), "hud-invalid.gui", &error), &error);
    assert(!check_hud_gui(&region, &tree, &error) && error.line == 2 && strstr(error.reason, "UNKNOWN_PAGE_BOX"));
    script_tree_free(&tree);
    puts("PASS tooltip text-box references checked before native panel and HUD pager creation; localisation stays in tooltipText.");
}

static unsigned copy_stage;

static const char *copy_flag(unsigned actor, const char *name, bool *result) {
    assert(actor == election_tag("FRA"));
    if (!strcmp(name, "pf068_copy_show")) *result = copy_stage >= 1;
    else if (!strcmp(name, "pf068_copy_click")) *result = copy_stage >= 2;
    else if (!strcmp(name, "pf068_copy_expand")) *result = copy_stage >= 3;
    else if (!strcmp(name, "pf068_copy_union_no_view")) *result = copy_stage == 4;
    else return "unexpected test flag";
    return NULL;
}

static const char *copy_member(PsOperation op, const PsArguments *args, unsigned actor, bool *result) {
    assert(op == PS_IS_MEMBER && actor == election_tag("FRA") && args->country == election_tag("SAV"));
    *result = !strcmp(args->instance, "pf068_copy_union_west");
    return NULL;
}

static void check_content_copy(const char *root, const KhanDefinition *original) {
    ScriptError error;
    const char *type_file = PS_TYPE_DIRECTORY "/pf068_copy_union.txt";
    char *bytes = read_fixture(root, type_file);
    insist(collect_content_types(&types, bytes, strlen(bytes), type_file, &error), &error);
    free(bytes);
    const KhanDefinition *copy = resolve_content_type(&types, "pf068_copy_union", &error);
    insist(copy != NULL, &error);
    const KhanDefinition *charter = resolve_content_type(&types, "pf068_copy_charter", &error);
    insist(charter != NULL, &error);
    assert(copy->starting_authority == original->starting_authority && copy->member_source == original->member_source &&
           copy->max_instances == original->max_instances && copy->entry.toggle && !charter->entry.toggle);

    const char *history = PS_HISTORY_DIRECTORY "/pf068_copy_union.txt";
    bytes = read_fixture(root, history);
    insist(append_content_history(bytes, strlen(bytes), history, &roster, &error), &error);
    free(bytes);
    assert(roster.count == 6 && !strcmp(roster.items[3].id, "pf068_copy_union_west") &&
           !strcmp(roster.items[4].id, "pf068_copy_union_south") && roster.items[5].date == 14441111);
    for (size_t i = 0; i < 3; ++i) {
        assert(!strcmp(roster.items[i].leader, roster.items[i + 3].leader) &&
               !memcmp(&roster.items[i].members, &roster.items[i + 3].members, sizeof(MemberTags)));
    }

    const char *map = PS_LAYOUT_DIRECTORY "/pf068_copy_union.txt";
    bytes = read_fixture(root, map);
    insist(collect_layouts(&layouts, bytes, strlen(bytes), map, &error), &error);
    free(bytes);
    int layout_id = layout_index(&layouts, copy->entry.layout), region_id = hud_index(&layouts, copy->entry.region);
    assert(layout_id == 1 && region_id == 1);
    PoliticalLayout *layout = &layouts.items[layout_id];
    HudRegion *region = &layouts.hud[region_id];
    ScriptTree tree;
    bytes = read_fixture(root, layout->gui_file);
    insist(script_tree_read(&tree, bytes, strlen(bytes), layout->gui_file, &error), &error);
    insist(check_layout_gui(layout, &tree, &error), &error);
    insist(check_hud_gui(region, &tree, &error), &error);
    script_tree_free(&tree);
    free(bytes);
    bytes = read_fixture(root, region->gfx_file);
    insist(script_tree_read(&tree, bytes, strlen(bytes), region->gfx_file, &error), &error);
    insist(check_hud_gfx(region, &tree, game_file, &error), &error);
    script_tree_free(&tree);
    free(bytes);
    assert(!strcmp(layout->background_sprite, "GFX_hre_bg") && region->capacity == 1 &&
           hud_icon_index(region, copy->entry.icon) == 0 && hud_icon_index(region, charter->entry.icon) == 1);

    const EntryRoute expected[] = {ENTRY_HIDDEN, ENTRY_DISABLED, ENTRY_DIRECT, ENTRY_SELECTOR, ENTRY_HIDDEN};
    for (copy_stage = 0; copy_stage < sizeof(expected) / sizeof(*expected); ++copy_stage) {
        InstanceChoices choices = {0};
        for (size_t i = 3; i <= 4; ++i) {
            EntryAccess access;
            const EntryContext ctx = {.instance = roster.items[i].id, .viewer = election_tag("FRA"), .query = copy_member, .country_flag = copy_flag};
            assert(!entry_permissions(&copy->entry, &ctx, &access));
            add_instance_choice(&choices, i, access);
        }
        assert(entry_route(&choices) == expected[copy_stage]);
        printf("PASS content-only copy stage %u visible=%u enabled=%u\n", copy_stage, choices.count, choices.enabled_count);
    }
    assert(hud_page_count(2, region->capacity) == 2);
    puts("PASS original and copied type catalog, independent history IDs, native panel and paging assets.");
}

static void check_offices_mod(const char *root) {
    ScriptError error;
    const char *type_path = PS_TYPE_DIRECTORY "/pf069_offices_union.txt";
    char *bytes = read_fixture(root, type_path);
    insist(collect_content_types(&types, bytes, strlen(bytes), type_path, &error), &error);
    free(bytes);
    const KhanDefinition *type = resolve_content_type(&types, "pf069_offices_union", &error);
    insist(type != NULL, &error);
    assert(type->member_source == MEMBERS_MIXED && type->member_query_defined && type->territory_enabled &&
           type->role_count == 2 && !strcmp(type->roles[0].id, "quartermaster") && !strcmp(type->roles[1].id, "kanrei"));
    const size_t before = roster.count;
    const char *history = PS_HISTORY_DIRECTORY "/pf069_offices_union.txt";
    bytes = read_fixture(root, history);
    insist(append_content_history(bytes, strlen(bytes), history, &roster, &error), &error);
    free(bytes);
    assert(roster.count == before + 2 && roster.items[before].territory.count == 2 && roster.items[before].territory.ids[0] == 205);
    const char *mapping = PS_LAYOUT_DIRECTORY "/pf069_offices_union.txt";
    bytes = read_fixture(root, mapping);
    insist(collect_layouts(&layouts, bytes, strlen(bytes), mapping, &error), &error);
    free(bytes);
    int panel_id = layout_index(&layouts, type->entry.layout), strip_id = hud_index(&layouts, type->entry.region);
    assert(panel_id >= 0 && strip_id >= 0);
    PoliticalLayout *panel = &layouts.items[panel_id];
    HudRegion *strip = &layouts.hud[strip_id];
    ScriptTree tree;
    bytes = read_fixture(root, panel->gui_file);
    insist(script_tree_read(&tree, bytes, strlen(bytes), panel->gui_file, &error), &error);
    insist(check_layout_gui(panel, &tree, &error), &error);
    insist(check_hud_gui(strip, &tree, &error), &error);
    script_tree_free(&tree);
    free(bytes);
    assert(panel->roles.rows == 1 && !strcmp(panel->roles.scrollbar, "office_scroll") &&
           hud_icon_index(strip, type->entry.icon) == 0);
    puts("PASS actual 0.6.9 sample: named roles, inherited role clear, mixed member rule, territory history and native office scrollbar.");
}

static void check_selection_content(const char *root, const PoliticalLayout *panel) {
    const char *source = PS_TYPE_DIRECTORY "/pf072_selection.txt";
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", root, source);
    FILE *found = fopen(path, "rb");
    if (!found) return;
    fclose(found);
    ScriptError error;
    char *bytes = read_fixture(root, source);
    insist(collect_content_types(&types, bytes, strlen(bytes), source, &error), &error);
    free(bytes);
    const KhanDefinition *contest = resolve_content_type(&types, "pf072_contest", &error);
    insist(contest != NULL, &error);
    const PoliticalRole *office = &contest->roles[0];
    assert(office->acquisition == OFFICE_COMPETITION && office->contest.period == 30 && office->contest.cost == 5000);
    assert(grants_permission(&office->permissions, "execute_privileged_action"));
    const KhanDefinition *rank = resolve_content_type(&types, "pf072_rank", &error);
    insist(rank != NULL, &error);
    assert(rank->succession.mode == LEADER_HIGHEST && rank->succession.event == SELECT_THRESHOLD);
    const KhanDefinition *vote = resolve_content_type(&types, "pf072_ballot", &error);
    insist(vote != NULL, &error);
    assert(vote->election.defined && vote->succession.event == SELECT_EXPLICIT && !strcmp(vote->succession.elector_role, "voter"));
    assert(panel->roles.rows == 1 && panel->action_check[0] && panel->action_seal[0] && panel->action_read_only[0]);
    const char *history = PS_HISTORY_DIRECTORY "/pf072_selection.txt";
    bytes = read_fixture(root, history);
    const size_t before = roster.count;
    insist(append_content_history(bytes, strlen(bytes), history, &roster, &error), &error);
    free(bytes);
    assert(roster.count == before + 3);
    puts("PASS authored 0.7.2 seal and check controls, contest permissions, threshold ranking and named electorate.");
}

static void check_actions_content(const char *root, const PoliticalLayout *panel) {
    const char *source = PS_TYPE_DIRECTORY "/pf071_actions.txt";
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", root, source);
    FILE *found = fopen(path, "rb");
    if (!found) return;
    fclose(found);
    ScriptError error;
    char *bytes = read_fixture(root, source);
    insist(collect_content_types(&types, bytes, strlen(bytes), source, &error), &error);
    free(bytes);
    const KhanDefinition *d = resolve_content_type(&types, "pf071_actions", &error);
    insist(d != NULL, &error);
    assert(d->action_count >= 4 && d->actions[0].kind == ACTION_REFORM && d->actions[0].cost == 20000);
    assert(d->actions[1].require_count == 1 && !strcmp(d->actions[1].requires[0], "charter"));
    assert(d->actions[2].kind == ACTION_DECISION && d->actions[2].payer == ACTION_ACTOR_PAYS && !d->actions[2].repeatable);
    assert(d->actions[3].kind == ACTION_DECREE && d->actions[3].duration == 30 && d->actions[3].cooldown == 45);
    const unsigned diet = political_action_index(d->actions, d->action_count, "diet");
    if (diet < d->action_count) {
        const unsigned invite = political_action_index(d->actions, d->action_count, "invite");
        const unsigned ai = political_action_index(d->actions, d->action_count, "invite_ai");
        assert(invite < d->action_count && d->actions[invite].member_invitation && d->actions[invite].duration == 30);
        assert(ai < d->action_count && d->actions[ai].ai_accept);
        assert(d->actions[diet].kind == ACTION_INCIDENT && d->actions[diet].option_count == 2 && d->actions[diet].options[0].prestige == 3000);
        assert(panel->action_read_only[0] && !panel->action_status[0]);
        puts("PASS 0.7.3 authored invitations, AI response, dated diet options and action rows without status text.");
    }
    assert(panel->actions.rows == 3 && !strcmp(panel->actions.scrollbar, "action_scroll"));
    assert(!strcmp(panel->resources.hover, "ledger_hit") && !strcmp(panel->resources.value, "ledger_value"));
    const char *history = PS_HISTORY_DIRECTORY "/pf071_actions.txt";
    bytes = read_fixture(root, history);
    const size_t before = roster.count;
    insist(append_content_history(bytes, strlen(bytes), history, &roster, &error), &error);
    free(bytes);
    assert(roster.count == before + 2 && !strcmp(roster.items[before].leader, "FRA"));
    static const char invalid[] = "cycle = { reforms = { a = { requires = { b } } b = { requires = { a } } } }";
    insist(collect_content_types(&types, invalid, strlen(invalid), "fixture/invalid_actions.txt", &error), &error);
    assert(!resolve_content_type(&types, "cycle", &error) && error.line && strstr(error.reason, "cyclic reform"));
    puts("PASS 0.7.1 authored number hit controls, native layout and scrollbar, three action kinds, named costs, timings, and cyclic prerequisite diagnostic.");
    check_selection_content(root, panel);
}

static void check_resources_mod(const char *root) {
    ScriptError error;
    const char *type_path = PS_TYPE_DIRECTORY "/pf070_resources.txt";
    char *bytes = read_fixture(root, type_path);
    insist(collect_content_types(&types, bytes, strlen(bytes), type_path, &error), &error);
    free(bytes);
    const KhanDefinition *depth[] = { resolve_content_type(&types, "pf070_depth_one", &error),
                                    resolve_content_type(&types, "pf070_depth_two", &error),
                                    resolve_content_type(&types, "pf070_depth_three", &error) };
    for (unsigned i = 0; i < sizeof(depth) / sizeof(*depth); ++i) {
        insist(depth[i] != NULL, &error);
        assert(!strcmp(depth[i]->resources[0].id, "authority"));
        assert(depth[i]->resources[0].initial == (i ? 10000 : 0));
        assert(depth[i]->resources[0].monthly == (i ? 1000 : 0));
    }
    assert(depth[0]->resource_count == 1 && depth[1]->resource_count == 1 && depth[2]->resource_count == 3);
    assert(depth[2]->resources[0].terms[0].leader_scope && depth[2]->calculation_count == 2);
    const char *history = PS_HISTORY_DIRECTORY "/pf070_resources.txt";
    const size_t before = roster.count;
    bytes = read_fixture(root, history);
    insist(append_content_history(bytes, strlen(bytes), history, &roster, &error), &error);
    free(bytes);
    assert(roster.count == before + 4 && roster.items[before + 3].balances[0].balance == 70000);
    const char *mapping = PS_LAYOUT_DIRECTORY "/pf070_resources.txt";
    bytes = read_fixture(root, mapping);
    insist(collect_layouts(&layouts, bytes, strlen(bytes), mapping, &error), &error);
    free(bytes);
    const int panel_id = layout_index(&layouts, depth[2]->entry.layout), strip_id = hud_index(&layouts, depth[2]->entry.region);
    assert(panel_id >= 0 && strip_id >= 0);
    PoliticalLayout *panel = &layouts.items[panel_id];
    HudRegion *strip = &layouts.hud[strip_id];
    ScriptTree tree;
    bytes = read_fixture(root, panel->gui_file);
    insist(script_tree_read(&tree, bytes, strlen(bytes), panel->gui_file, &error), &error);
    insist(check_layout_gui(panel, &tree, &error), &error);
    insist(check_hud_gui(strip, &tree, &error), &error);
    assert(panel->resources.rows == 2 && panel->resources.wheel.height == 86);
    script_tree_free(&tree);
    free(bytes);
    bytes = read_fixture(root, strip->gfx_file);
    insist(script_tree_read(&tree, bytes, strlen(bytes), strip->gfx_file, &error), &error);
    insist(check_hud_gfx(strip, &tree, game_file, &error), &error);
    script_tree_free(&tree);
    free(bytes);
    puts("PASS actual 0.7.0 sample: shared authority definitions, fractional history, resource rows, vertical rail and HUD assets.");
    check_actions_content(root, panel);
    const char *union_file = PS_TYPE_DIRECTORY "/pf074_union.txt";
    char union_path[1024];
    snprintf(union_path, sizeof(union_path), "%s/%s", root, union_file);
    FILE *probe = fopen(union_path, "rb");
    if (!probe) return;
    fclose(probe);
    bytes = read_fixture(root, union_file);
    insist(collect_content_types(&types, bytes, strlen(bytes), union_file, &error), &error);
    free(bytes);
    const KhanDefinition *unions = resolve_content_type(&types, "pf074_union", &error);
    insist(unions != NULL, &error);
    assert(unions->dynamic_allowed && unions->dissolution_allowed && unions->block_direct_war);
    assert(unions->max_instances == 3 && unions->defensive_obligation && unions->defensive_trigger.count == 2);
    const EntryNode *gate = &unions->defensive_trigger.nodes[1];
    assert(gate->operation == PS_CONTENT_REFORM && !strcmp(gate->args.action, "common_banner"));
    assert(hud_icon_index(strip, unions->entry.icon) >= 0);
    const char *league_history = PS_HISTORY_DIRECTORY "/pf074_union.txt";
    bytes = read_fixture(root, league_history);
    const size_t old_count = roster.count;
    insist(append_content_history(bytes, strlen(bytes), league_history, &roster, &error), &error);
    free(bytes);
    assert(roster.count == old_count + 2 &&
           !strcmp(roster.items[old_count].title, roster.items[old_count + 1].title) &&
           strcmp(roster.items[old_count].id, roster.items[old_count + 1].id));
    puts("PASS actual 0.7.4 pack: two same-name histories, authored lifecycle, banner condition and native artillery binding.");
}

int main(int argc, char **argv) {
    assert(argc >= 4 && argc <= 7);
    game_directory = argv[2];
    const char *const type = PS_TYPE_DIRECTORY "/demo_union.txt", *const history = PS_HISTORY_DIRECTORY "/demo_union.txt";
    const char *const map = PS_LAYOUT_DIRECTORY "/demo_union.txt";
    ScriptError error;
    char *bytes = read_fixture(argv[1], type);
    insist(collect_content_types(&types, bytes, strlen(bytes), type, &error), &error);
    free(bytes);
    const KhanDefinition *union_type = resolve_content_type(&types, "demo_union", &error);
    insist(union_type != NULL, &error);
    check_inheritance(union_type);
    bytes = read_fixture(argv[1], history);
    insist(append_content_history(bytes, strlen(bytes), history, &roster, &error), &error);
    free(bytes);
    check_history();
    check_conditions(union_type);
    bytes = read_fixture(argv[1], map);
    insist(collect_layouts(&layouts, bytes, strlen(bytes), map, &error), &error);
    free(bytes);
    assert(layouts.count == 1 && layouts.hud_count == 1);
    PoliticalLayout *layout = &layouts.items[0];
    HudRegion *hud = &layouts.hud[0];
    ScriptTree tree;
    bytes = read_fixture(argv[1], layout->gui_file);
    insist(script_tree_read(&tree, bytes, strlen(bytes), layout->gui_file, &error), &error);
    insist(check_layout_gui(layout, &tree, &error), &error);
    insist(check_hud_gui(hud, &tree, &error), &error);
    assert(!strcmp(layout->hover, "demo_union_hover") && !strcmp(layout->background_sprite, "GFX_hre_bg") && layout->members.rows == 2 &&
           layout->members.columns == 5 && layout->hint_count == 5 &&
           hud_icon_index(hud, union_type->entry.icon) == 0 && hud_icon_index(hud, "GFX_demo_charter_entry") == 1);
    strcpy(layout->close, layout->hover);
    assert(!check_layout_gui(layout, &tree, &error) && strstr(error.reason, "interactive controls cannot"));
    strcpy(layout->close, "missing_close");
    assert(!check_layout_gui(layout, &tree, &error) && strstr(error.reason, "missing_close"));
    script_tree_free(&tree);
    free(bytes);
    bytes = read_fixture(argv[1], hud->gfx_file);
    insist(script_tree_read(&tree, bytes, strlen(bytes), hud->gfx_file, &error), &error);
    insist(check_hud_gfx(hud, &tree, game_file, &error), &error);
    strcpy(hud->icons[0].sprite, "GFX_missing");
    assert(!check_hud_gfx(hud, &tree, game_file, &error) && strstr(error.reason, "GFX_missing"));
    script_tree_free(&tree);
    free(bytes);
    check_window_managed_background();
    check_tooltip_references();
    bytes = read_fixture(argv[3], PS_SELECTOR_GUI);
    insist(script_tree_read(&tree, bytes, strlen(bytes), PS_SELECTOR_GUI, &error), &error);
    insist(check_gui_window_tooltips(&tree, PS_SELECTOR_WINDOW, &error), &error);
    script_tree_free(&tree);
    free(bytes);
    if (argc >= 5 && strcmp(argv[4], "-")) check_content_copy(argv[4], union_type);
    if (argc >= 6 && strcmp(argv[5], "-")) check_offices_mod(argv[5]);
    if (argc == 7) check_resources_mod(argv[6]);
    free_content_types(&types);
    puts("PASS actual panel, HUD and instance-selector assets, mounted texture paths, editable labels, native vertical rail and missing-asset diagnostics.");
    return 0;
}
