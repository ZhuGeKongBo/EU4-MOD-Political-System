#include "reform.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct EffectProbe {
    ReformState *state;
    const KhanReform *definition;
    int32_t prestige;
    unsigned calls;
} EffectProbe;

static void grant(void *country, int32_t amount, const KhanReform *definition) {
    EffectProbe *probe = country;
    assert(definition == probe->definition && definition->effect_line > 0);
    ++probe->calls;
    assert(pass_reform(probe->state, probe->definition, 1, 1, country, grant) == REFORM_PASSED);
    probe->prestige += amount;
}

int main(void) {
    static const char valid[] =
        "gk_council_reform = { empire = great_khan title = GK_COUNCIL_REFORM "
        "cost = { authority = 20 } on_effect = { leader_scope = { add_prestige = 5 } } }";
    static const char other_empire[] =
        "gk_council_reform = { empire = hre title = GK_COUNCIL_REFORM "
        "cost = { authority = 20 } on_effect = { leader_scope = { add_prestige = 5 } } }";
    static const char unknown_effect[] =
        "gk_council_reform = { empire = great_khan title = GK_COUNCIL_REFORM "
        "cost = { authority = 20 } on_effect = { leader_scope = { add_stability = 5 } } }";
    static const char bad_cost[] =
        "gk_council_reform = { empire = great_khan title = GK_COUNCIL_REFORM "
        "cost = { authority = -20 } on_effect = { leader_scope = { add_prestige = 5 } } }";
    static const char custom_start[] =
        "great_khan = { emperor = shogun starting_authority = 19 "
        "members = { source = direct_subjects_of_leader subject_type = daimyo } }";
    ScriptError error;
    KhanDefinition system;
    KhanReform reform;
    assert(!read_reform(other_empire, sizeof(other_empire) - 1, &reform, &error) && strstr(error.reason, "great_khan"));
    assert(!read_reform(unknown_effect, sizeof(unknown_effect) - 1, &reform, &error) && strstr(error.reason, "add_prestige"));
    assert(!read_reform(bad_cost, sizeof(bad_cost) - 1, &reform, &error) && strstr(error.reason, "cost.authority"));
    assert(read_reform(valid, sizeof(valid) - 1, &reform, &error));
    assert(!strcmp(reform.id, "gk_council_reform") && reform.cost == 20 && reform.prestige == 5);
    assert(read_definition(custom_start, sizeof(custom_start) - 1, &system, &error));

    ReformState state = { .authority = system.starting_authority };
    EffectProbe probe = { .state = &state, .definition = &reform, .prestige = -5700 };
    assert(pass_reform(&state, &reform, 1, 1, &probe, grant) == REFORM_LOW_AUTHORITY);
    assert(!state.passed_id[0] && state.authority == 19 && probe.calls == 0);

    state.authority = 50;
    assert(pass_reform(&state, &reform, 1, 2, &probe, grant) == REFORM_NOT_LEADER);
    assert(pass_reform(&state, &reform, 1, 0, &probe, grant) == REFORM_NO_LEADER);
    assert(pass_reform(&state, &reform, 1, 1, NULL, grant) == REFORM_NO_LEADER);
    assert(!state.passed_id[0] && state.authority == 50 && probe.calls == 0);

    assert(pass_reform(&state, &reform, 1, 1, &probe, grant) == REFORM_ENACTED);
    assert(pass_reform(&state, &reform, 1, 1, &probe, grant) == REFORM_PASSED);
    assert(!strcmp(state.passed_id, reform.id) && state.authority == 30 && probe.calls == 1 && probe.prestige == -700);
    KhanReform replacement = reform;
    strcpy(replacement.id, "replacement");
    assert(pass_reform(&state, &replacement, 1, 1, &probe, grant) == REFORM_DEFINITION_CHANGED);
    assert(!strcmp(state.passed_id, reform.id) && state.authority == 30 && probe.calls == 1);

    state = (ReformState){ .authority = 20 };
    assert(pass_reform(&state, &reform, 1, 1, &probe, grant) == REFORM_ENACTED);
    assert(state.authority == 0 && !strcmp(state.passed_id, reform.id) && probe.calls == 2);
    puts("Reform tests passed: script checks, eligibility, exact payment, and one reward even on reentry.");
}
