#include "save_state.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    char value[GK_SAVE_VALUE_SIZE];
    KhanSaveRecord original = { .leader = "ASK", .reform = { 30, "gk_council_reform" } };
    KhanSaveRecord restored = {0};
    assert(encode_khan_state(&original, value));
    assert(!strcmp(value, "1|great_khan|great_khan_main|ASK|30|gk_council_reform"));
    assert(decode_khan_state(value, &restored) == GK_LOAD_VALID);
    assert(!strcmp(restored.leader, original.leader));
    assert(restored.reform.authority == original.reform.authority);
    assert(!strcmp(restored.reform.passed_id, original.reform.passed_id));

    original = (KhanSaveRecord){ .reform.authority = 100 };
    assert(encode_khan_state(&original, value));
    assert(!strcmp(value, "1|great_khan|great_khan_main|none|100|-"));
    assert(decode_khan_state(value, &restored) == GK_LOAD_VALID);
    assert(!restored.leader[0] && restored.reform.authority == 100 && !restored.reform.passed_id[0]);
    assert(decode_khan_state("2|great_khan|great_khan_main|ASK|30|-", &restored) == GK_LOAD_UNSUPPORTED);
    assert(decode_khan_state("1|great_khan|second|ASK|30|-", &restored) == GK_LOAD_UNSUPPORTED);
    assert(decode_khan_state("1|celestial_empire|great_khan_main|ASK|30|-", &restored) == GK_LOAD_UNSUPPORTED);
    static const char *const invalid[] = {
        "", "a|great_khan|great_khan_main|ASK|30|-",
        "1|great_khan|great_khan_main|ask|30|-", "1|great_khan|great_khan_main|AS|30|-",
        "1|great_khan|great_khan_main|ASK|101|-", "1|great_khan|great_khan_main|ASK|+1|-",
        "1|great_khan|great_khan_main|ASK|30|", "1|great_khan|great_khan_main|ASK|30|-|extra",
    };
    for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i)
        assert(decode_khan_state(invalid[i], &restored) == GK_LOAD_INVALID);
    assert(!restored.leader[0] && restored.reform.authority == 100 && !restored.reform.passed_id[0]);

    strcpy(original.leader, "D01");
    original.reform.authority = 0;
    memset(original.reform.passed_id, '_', sizeof(original.reform.passed_id) - 1);
    assert(encode_khan_state(&original, value));
    assert(decode_khan_state(value, &restored) == GK_LOAD_VALID);
    assert(!strcmp(restored.leader, original.leader) && !strcmp(restored.reform.passed_id, original.reform.passed_id));
    original.reform.passed_id[GK_SCRIPT_WORD_SIZE - 1] = 'a';
    assert(!encode_khan_state(&original, value));
    NativeSaveField field = {0};
    const PoliticalSaveRecord khanate = {
        .type = "gkhanate", .instance = "gkhanate_main", .data.leader = "KHA", .history_date = 14441111,
    };
    PoliticalSaveRecord political = {0};
    assert(encode_political_state(&khanate, field.value));
    assert(!strcmp(field.value, "3|gkhanate|gkhanate_main|14441111|KHA|0|-"));
    assert(decode_political_state(&field, &political) == GK_LOAD_VALID);
    assert(political.history_date == 14441111 && !strcmp(political.data.leader, "KHA"));
    strcpy(field.value, "3|gkhanate|gkhanate_main|14440229|KHA|0|-");
    assert(decode_political_state(&field, &political) == GK_LOAD_INVALID);
    strcpy(field.value, "2|great_khan|great_khan_second|UES|72|-");
    assert(decode_political_state(&field, &political) == GK_LOAD_VALID && !political.history_date);
    const PoliticalInstanceSpec spec = { .id = "third", .type = "league", .title = "sample_league" };
    const PoliticalLifecycle life = { .mode = POLITICAL_SUSPENDED, .founder = 0x415246, .created = 14441111,
        .dynamic = true, .automatic = true };
    PoliticalLifecycleRecord lifecycle;
    assert(encode_lifecycle(&spec, &life, field.value) && decode_lifecycle(field.value, &lifecycle));
    assert(lifecycle.lifecycle.mode == POLITICAL_SUSPENDED && lifecycle.lifecycle.automatic &&
        lifecycle.lifecycle.founder == life.founder && !strcmp(lifecycle.name, spec.title));
    strcpy(field.value, "Y1|third|league|3|1|1|14441111|FRA|sample_league|");
    assert(!decode_lifecycle(field.value, &lifecycle));
    const PoliticalAction invitation = { .kind = ACTION_INVITATION, .id = "invite" };
    PoliticalProcess request = { .round = 1, .issuer = 0x415246, .target = 0x474E45, .opened = 14441111,
        .deadline = 14441113, .status = PROCESS_PENDING, .notified = true };
    ActionKind kind;
    char instance[GK_SCRIPT_WORD_SIZE], action[GK_SCRIPT_WORD_SIZE];
    assert(encode_process(spec.id, &invitation, &request, field.value));
    assert(decode_process(field.value, instance, action, &kind, &request));
    assert(request.notified && request.target == 0x474E45);
    strcpy(field.value, "Q1|third|invite|4|1|FRA|ENG|14441111|14441113|0|1|-|");
    assert(decode_process(field.value, instance, action, &kind, &request) && !request.notified);
    puts("Save state tests passed: round trip, vacant office, version and instance isolation, and input bounds.");
}
