#include "content_types.h"

static bool visit_nodes(EntryNode *nodes, unsigned count, NativeClauseVisitor visit, ScriptError *error) {
    for (unsigned i = 0; i < count; ++i)
        if (nodes[i].test == ENTRY_NATIVE && !visit(&nodes[i].native, NATIVE_CONDITION, error)) return false;
    return true;
}

bool visit_native_clauses(KhanDefinition *d, NativeClauseVisitor visit, ScriptError *error) {
    EntryCondition *conditions[] = {
        &d->entry.view, &d->entry.visible, &d->entry.enabled, &d->member_query, &d->member_join,
        &d->member_retain, &d->member_leave, &d->succession.candidate, &d->succession.retain,
        &d->create_trigger, &d->continue_trigger, &d->defensive_trigger,
    };
    for (unsigned i = 0; i < sizeof(conditions) / sizeof(*conditions); ++i)
        if (!visit_nodes(conditions[i]->nodes, conditions[i]->count, visit, error)) return false;
    for (unsigned i = 0; i < d->role_count; ++i) {
        PoliticalRole *role = &d->roles[i];
        if (!visit_nodes(role->candidate.nodes, role->candidate.count, visit, error) ||
            !visit_nodes(role->retain.nodes, role->retain.count, visit, error)) return false;
    }
    NominationRules *nomination = &d->succession.nomination;
    if (!visit_nodes(nomination->nodes, nomination->node_count, visit, error)) return false;
    for (unsigned i = 0; i < d->resource_count; ++i) {
        ResourceDefinition *resource = &d->resources[i];
        for (unsigned j = 0; j < resource->term_count; ++j)
            if (!visit(&resource->terms[j].native, NATIVE_CONDITION, error)) return false;
    }
    for (unsigned i = 0; i < d->action_count; ++i) {
        PoliticalAction *a = &d->actions[i];
        if (!visit(&a->potential, NATIVE_CONDITION, error) || !visit(&a->trigger, NATIVE_CONDITION, error) ||
            !visit(&a->native_actor, NATIVE_CONDITION, error) || !visit(&a->native_target, NATIVE_CONDITION, error) ||
            !visit(&a->can_stop, NATIVE_CONDITION, error) || !visit(&a->effect, NATIVE_EFFECT, error) ||
            !visit(&a->off_effect, NATIVE_EFFECT, error) || !visit(&a->ai_rule, NATIVE_WEIGHT, error)) return false;
        for (unsigned j = 0; j < ACTION_MODIFIER_COUNT; ++j)
            if (!visit(&a->modifiers[j], j == ACTION_MODIFIER_PROVINCE ? NATIVE_PROVINCE_MODIFIER : NATIVE_COUNTRY_MODIFIER,
                       error)) return false;
        for (unsigned j = 0; j < a->option_count; ++j) {
            IncidentOption *option = &a->options[j];
            if (!visit(&option->trigger, NATIVE_CONDITION, error) ||
                !visit(&option->effect, NATIVE_EFFECT, error)) return false;
        }
    }
    return true;
}
