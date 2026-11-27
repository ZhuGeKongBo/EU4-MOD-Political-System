#include "political_script.h"
#include "shogunate.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void expect(const char *text, uint32_t today, bool valid, uint32_t date, const char *tag) {
    ScriptError error;
    KhanTimeline timeline = {0};
    assert(read_timeline(text, strlen(text), &timeline, &error) == valid);
    if (valid) {
        KhanHistory history = history_at(&timeline, today);
        assert(history.effective_date == date && !strcmp(history.tag, tag));
    } else {
        assert(error.line && error.column && error.reason[0] && !timeline.entries);
    }
    free(timeline.entries);
}

static AnvilResult mixed_subjects(const char *tag, AnvilGameTag *out, uint32_t capacity, uint32_t *total) {
    assert(!strcmp(tag, "TIM"));
    static const AnvilGameTag subjects[] = { { "TRS" }, { "AFG" }, { "KHO" } };
    *total = sizeof(subjects) / sizeof(subjects[0]);
    if (out) {
        assert(capacity == *total);
        memcpy(out, subjects, sizeof(subjects));
    }
    return ANVIL_OK;
}

static AnvilResult mixed_types(const char *leader, const char *tag, char *out, uint32_t capacity, uint32_t *length) {
    assert(!strcmp(leader, "TIM"));
    const char *type = !strcmp(tag, "TRS") ? "vassal" : "march";
    *length = (uint32_t)strlen(type);
    assert(capacity > *length);
    strcpy(out, type);
    return ANVIL_OK;
}

int main(int argc, char *argv[]) {
    assert(argc == 1);
    (void)argv;
    expect("1444.11.11 = { great_khan_emperor = ODA }", 14441111, true, 14441111, "ODA");
    expect("1444.11.11 = { great_khan_emperor = \"TIM\" }", 14441111, true, 14441111, "TIM");
    const char *timeline = "\xef\xbb\xbf" "# unordered\n"
        "1500.1.1={great_khan_emperor=none}\n"
        "1467.1.1 = { great_khan_emperor = ODA }\n"
        "1444.11.11 = { # note\n great_khan_emperor = ASK \n}\n";
    expect(timeline, 14441110, true, 0, "");
    expect(timeline, 14441111, true, 14441111, "ASK");
    expect(timeline, 14670101, true, 14670101, "ODA");
    expect(timeline, 15000101, true, 15000101, "");
    expect("1444.2.29 = { great_khan_emperor = ASK }", 14441111, false, 0, "");
    expect("1444.11.11 = { celestial_emperor = MNG }", 14441111, false, 0, "");
    expect("1444.11.11 = { great_khan_emperor = ASKK }", 14441111, false, 0, "");
    expect("1444.11.11 = { great_khan_emperor = ASK", 14441111, false, 0, "");
    expect("1444.11.11 = { great_khan_emperor = ASK }\n1444.11.11 = { great_khan_emperor = TIM }",
           14441111, true, 14441111, "TIM");
    expect("# No historical holder", 14441111, true, 0, "");

    KhanDefinition definition;
    ScriptError error;
    static const char def[] = "\xef\xbb\xbf" "# member rule first\r\n"
        "great_khan={members={subject_type=\"daimyo\" source=direct_subjects_of_leader} emperor=shogun}";
    assert(read_definition(def, sizeof(def) - 1, &definition, &error));
    assert(!strcmp(definition.emperor, "shogun") && !strcmp(definition.subject_type, "daimyo_vassal"));
    static const char changed[] = "great_khan={emperor=mirza members={source=direct_subjects_of_leader subject_type=vassal}}";
    assert(read_definition(changed, sizeof(changed) - 1, &definition, &error));
    assert(!strcmp(definition.emperor, "mirza") && !strcmp(definition.subject_type, "vassal"));
    static const char invalid[] = "great_khan={emperor=shogun\n members={source=direct_subjects_of_leader typo=vassal}}";
    assert(!read_definition(invalid, sizeof(invalid) - 1, &definition, &error) &&
           error.line == 2 && strstr(error.reason, "typo"));
    static const char repeated[] = "great_khan={emperor=shogun emperor=mirza}";
    assert(!read_definition(repeated, sizeof(repeated) - 1, &definition, &error));

    AnvilGameWorldService world = { .country_subjects = mixed_subjects, .subject_type = mixed_types };
    const AnvilGameTag leader = { "TIM" };
    Shogunate state = {0};
    assert(read_subject_members(&world, &leader, definition.subject_type, &state));
    assert(state.count == 1 && state.direct_subjects == 3 && !strcmp(state.daimyo[0].text, "TRS"));
    release_shogunate(&state);
    assert(read_subject_members(&world, &leader, "march", &state) && state.count == 2);
    release_shogunate(&state);
    puts("Script tests passed: aliases, syntax diagnostics, history edits and dates, and exact matching in mixed subjects.");
}
