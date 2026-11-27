#include "reform.h"

#include <string.h>

ReformStatus evaluate_reform(const ReformState *state, const KhanReform *definition,
                            unsigned actor, unsigned leader) {
    if (state->passed_id[0])
        return strcmp(state->passed_id, definition->id) ? REFORM_DEFINITION_CHANGED : REFORM_PASSED;
    if (!leader) return REFORM_NO_LEADER;
    if (actor != leader) return REFORM_NOT_LEADER;
    if (state->authority < definition->cost) return REFORM_LOW_AUTHORITY;
    return REFORM_AVAILABLE;
}

ReformStatus pass_reform(ReformState *state, const KhanReform *definition,
                        unsigned actor, unsigned leader, void *country, ReformEffect effect) {
    const ReformStatus status = evaluate_reform(state, definition, actor, leader);
    if (status != REFORM_AVAILABLE) return status;
    if (!country) return REFORM_NO_LEADER;
    // Consume the action before calling the native effect, even if the effect calls back.
    state->authority -= definition->cost;
    strcpy(state->passed_id, definition->id);
    effect(country, definition->prestige * INT32_C(1000), definition);
    return REFORM_ENACTED;
}
