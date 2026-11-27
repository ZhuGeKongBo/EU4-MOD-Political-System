#include "save_state.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    static const char registry[] =
        "instances = { west = { type = crown history = origin title = \"First Crown\" } "
        "east = { type = crown starting_authority = 65 icon = infantry } "
        "delta = { type = river icon = tribute } }";
    ScriptError error;
    PoliticalRoster roster;
    assert(read_roster(registry, sizeof(registry) - 1, &roster, &error));
    assert(roster.count == 3 && !strcmp(roster.items[0].type, roster.items[1].type));
    assert(!strcmp(roster.items[1].history, "east") && !strcmp(roster.items[0].history, "origin"));
    assert(roster.items[0].starting_authority == -1 && roster.items[1].starting_authority == 65);
    assert(!strcmp(roster.items[2].icon, "tribute"));
    static const char duplicates[] = "instances = { delta = { type = river } Delta = { type = coast } }";
    assert(!read_roster(duplicates, sizeof(duplicates) - 1, &roster, &error));
    assert(strstr(error.reason, "duplicate"));
    static const char bad_icon[] = "instances = { delta = { type = river icon = unknown } }";
    assert(!read_roster(bad_icon, sizeof(bad_icon) - 1, &roster, &error) && strstr(error.reason, "icon"));

    KhanDefinition type;
    static const char definition[] =
        "river = { title = \"River League\" emperor = council "
        "members = { title = \"River States\" subject_type = tributary_state source = direct_subjects_of_leader } }";
    assert(read_type_definition(definition, sizeof(definition) - 1, "river", &type, &error));
    assert(!strcmp(type.title, "River League") && !strcmp(type.member_title, "River States"));
    assert(!read_type_definition(definition, sizeof(definition) - 1, "crown", &type, &error));
    KhanTimeline timeline;
    static const char dates[] = "1444.1.1 = { leader = AVA } 1510.1.1 = { leader = TAU }";
    assert(read_timeline(dates, sizeof(dates) - 1, &timeline, &error));
    assert(!strcmp(history_at(&timeline, 14441111).tag, "AVA"));
    assert(!strcmp(history_at(&timeline, 15100101).tag, "TAU"));
    free(timeline.entries);

    PoliticalSaveRecord before[] = {
        {.type = "crown", .instance = "west", .data = {.leader = "ASK", .reform = {30, "council"}}},
        {.type = "crown", .instance = "east", .data = {.leader = "UES", .reform = {72, ""}}},
        {.type = "river", .instance = "delta", .data = {.leader = "AVA", .reform = {45, "court"}}},
    };
    static NativeSaveFields fields = { .count = 3 };
    PoliticalSaveRecord after[3];
    for (size_t i = 0; i < fields.count; ++i)
        assert(encode_political_state(&before[i], fields.entries[i].value));
    assert(decode_saved_instances(&fields, after) == GK_LOAD_VALID);
    for (size_t i = 0; i < fields.count; ++i) {
        assert(!strcmp(before[i].type, after[i].type) && !strcmp(before[i].instance, after[i].instance));
        assert(!strcmp(before[i].data.leader, after[i].data.leader));
        assert(before[i].data.reform.authority == after[i].data.reform.authority);
        assert(!strcmp(before[i].data.reform.passed_id, after[i].data.reform.passed_id));
    }
    fields.entries[2] = fields.entries[0];
    assert(decode_saved_instances(&fields, after) == GK_LOAD_INVALID);
    fields.entries[2].value[0] = '4';
    assert(decode_saved_instances(&fields, after) == GK_LOAD_UNSUPPORTED);
    fields.count = 1;
    fields.entries[0].legacy = true;
    strcpy(fields.entries[0].value, "1|great_khan|great_khan_main|ASK|30|gk_council_reform");
    assert(decode_saved_instances(&fields, after) == GK_LOAD_VALID);
    assert(!strcmp(after[0].instance, PS_LEGACY_INSTANCE) && after[0].data.reform.authority == 30);
    fields.incomplete = true;
    assert(decode_saved_instances(&fields, after) == GK_LOAD_INVALID);
    puts("Instance tests passed: roster, labels, history, isolated saved records, duplicate and legacy migration.");
}
