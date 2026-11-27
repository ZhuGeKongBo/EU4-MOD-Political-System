#include "political_script.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_pack_file(const char *pack, const char *dir, const char *id) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s/%s.txt", pack, dir, id);
    FILE *file = fopen(path, "rb");
    assert(file);
    assert(!fseek(file, 0, SEEK_END));
    long size = ftell(file);
    assert(size > 0);
    rewind(file);
    char *data = calloc((size_t)size + 1, 1);
    assert(data && fread(data, 1, (size_t)size, file) == (size_t)size);
    fclose(file);
    return data;
}

static int authority = 50;
static const char *target;
static const char *query(PsOperation op, const PsArguments *args, unsigned actor, bool *result) {
    assert(actor);
    if (strcmp(args->instance, target)) return "unknown instance";
    assert(op == PS_COMPARE_RESOURCE);
    *result = ps_compare(args->comparison, authority, args->amount);
    return NULL;
}

static unsigned tag(const char *text) {
    unsigned value = 0;
    memcpy(&value, text, 3);
    return value;
}

static void check_pack(PoliticalRoster *roster, const char *pack, const char *id, bool copied) {
    ScriptError error;
    static KhanDefinition type;
    KhanTimeline history;
    KhanReform reform;
    size_t before = roster->count;
    char *registry = read_pack_file(pack, PS_ROSTER_DIRECTORY, id);
    assert(append_roster(registry, strlen(registry), roster, &error) && roster->count == before + 1);
    assert(!append_roster(registry, strlen(registry), roster, &error) && roster->count == before + 1);
    free(registry);
    const PoliticalInstanceSpec *spec = &roster->items[before];
    target = spec->id;
    assert(!strcmp(spec->type, id));
    char *text = read_pack_file(pack, "common/political_systems", id);
    assert(read_type_definition(text, strlen(text), id, &type, &error));
    free(text);
    text = read_pack_file(pack, "history/political_systems", spec->history);
    assert(read_timeline(text, strlen(text), &history, &error));
    free(text);
    assert(!strcmp(history_at(&history, 14441111).tag, "ASK"));
    free(history.entries);
    text = read_pack_file(pack, "common/political_systems/reforms", id);
    assert(read_type_reform(text, strlen(text), id, &reform, &error) && reform.cost == 20 && reform.prestige == 5);
    free(text);
    const char *viewers[] = {"ASK", "OIR", "MNG", "HAB"};
    assert(type.entry.defined && type.entry.close_on_escape && type.entry.close_button && type.entry.toggle);
    assert(!strcmp(type.entry.control, "pf_open") && type.entry.region[0] && type.entry.panel[0] && type.entry.tooltip[0]);
    authority = type.starting_authority;
    for (size_t i = 0; i < 4; ++i) {
        bool visible = false, enabled = false;
        assert(!entry_check(&type.entry.visible, spec->id, tag(viewers[i]), query, &visible));
        if (visible) assert(!entry_check(&type.entry.enabled, spec->id, tag(viewers[i]), query, &enabled));
        assert(visible == (copied ? i == 1 || i == 2 : i <= 1));
        assert(enabled == (copied ? i == 1 : i <= 1));
        printf("%s viewer=%s visible=%d enabled=%d\n", spec->id, viewers[i], visible, enabled);
    }
    bool enabled = true;
    if (!copied) {
        authority = 19;
        assert(!entry_check(&type.entry.enabled, spec->id, tag("ASK"), query, &enabled) && !enabled);
        authority = 20;
        assert(!entry_check(&type.entry.enabled, spec->id, tag("ASK"), query, &enabled) && enabled);
    }
    assert(entry_check(&type.entry.visible, spec->id, 0, query, &enabled) && !enabled);
}

int main(int argc, char **argv) {
    assert(argc == 2 || argc == 3);
    PoliticalRoster roster = {0};
    check_pack(&roster, argv[1], "pf_template", false);
    if (argc == 3) check_pack(&roster, argv[2], "pf_copy", true);
    const char *bad[] = {
        "visible = { NOT = { unregistered = yes } }", "enabled = { ps_set_resource = {} }",
        "visible = { ps_compare_resource = { resource = authority } }", "visible = { tag = 1234 }",
        "visible = { }", "visible = { always = yes } visible = { always = no }",
        "toggle = maybe", "visible = { capital_scope = { tag = ASK } }",
    };
    for (size_t i = 0; i < sizeof(bad)/sizeof(*bad); ++i) {
        char definition[1024];
        snprintf(definition, sizeof(definition),
                 "demo = { emperor = crown members = { source = direct_subjects_of_leader subject_type = vassal }"
                 " interface = { entry_button = { region = demo tooltip = TIP %s } } }", bad[i]);
        static KhanDefinition result;
        ScriptError error;
        assert(!read_type_definition(definition, strlen(definition), "demo", &result, &error));
        assert(error.line && error.column && error.reason[0]);
    }
    ScriptError error;
    size_t before = roster.count;
    const char *mixed = "instances = { fresh = { type = x } PF_TEMPLATE_MAIN = { type = x } }";
    assert(!append_roster(mixed, strlen(mixed), &roster, &error) && roster.count == before);
    puts("PASS content files, viewer-country conditions, matching instance, invalid triggers, duplicate file rollback.");
}
