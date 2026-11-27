#include "entry_config.h"
#include <string.h>

unsigned government_category(const char *key) {
    // The categories returned by EU4's government reform stack.
    static const char *const categories[] = {NULL, "monarchy", "republic", "theocracy", "native", "dictatorship", "tribal"};
    for (unsigned i = 1; i < sizeof(categories) / sizeof(*categories); ++i)
        if (!strcmp(categories[i], key)) return i;
    return 0;
}

const char *entry_evaluate_node(const EntryNode *nodes, unsigned index,
                                const EntryContext *context, bool *allowed) {
    const EntryNode *node = &nodes[index];
    switch (node->test) {
    case ENTRY_NATIVE: {
        const NativeScope scope = {.root = context->viewer, .from = context->from, .instance = context->instance};
        return context->native_condition ? context->native_condition(&node->native, &scope, allowed)
                                         : "native condition requires EU4";
    }
    case ENTRY_ALWAYS: *allowed = node->args.value; return NULL;
    case ENTRY_TAG: *allowed = context->viewer == node->args.country; return NULL;
    case ENTRY_COUNTRY_FLAG:
        return context->country_flag ? context->country_flag(context->viewer, node->args.reform, allowed)
                                     : "native country flag query unavailable";
    case ENTRY_RELIGION:
        return context->country_religion ? context->country_religion(context->viewer, node->args.reform, allowed)
                                         : "native country religion query unavailable";
    case ENTRY_GOVERNMENT:
        return context->country_government ? context->country_government(context->viewer, node->args.reform, allowed)
                                           : "native country government query unavailable";
    case ENTRY_HEIR: {
        if (!context->country_heir) return "native heir query unavailable";
        bool has_heir;
        const char *reason = context->country_heir(context->viewer, &has_heir);
        if (!reason) *allowed = has_heir == node->args.value;
        return reason;
    }
    case ENTRY_QUERY: {
        PsArguments args = node->args;
        if (!ps_selects_by_type(node->operation, &args) && (!args.instance[0] || !strcmp(args.instance, "INSTANCE")))
            strcpy(args.instance, context->instance);
        return context->query ? context->query(node->operation, &args, context->viewer, allowed)
                              : "extension query unavailable";
    }
    case ENTRY_AND:
    case ENTRY_OR:
    case ENTRY_NOT: {
        bool combined = node->test != ENTRY_OR;
        for (unsigned i = index + 1; i < node->end; i = nodes[i].end) {
            bool child = false;
            const char *reason = entry_evaluate_node(nodes, i, context, &child);
            if (reason) return reason;
            if (node->test == ENTRY_OR) combined |= child;
            else combined &= child;
        }
        *allowed = node->test == ENTRY_NOT ? !combined : combined;
        return NULL;
    }
    }
    return "invalid entry predicate";
}

const char *entry_evaluate(const EntryCondition *condition, const EntryContext *context, bool *allowed) {
    *allowed = false;
    if (!context->viewer) return "entry requires a viewing country";
    if (condition->count) return entry_evaluate_node(condition->nodes, 0, context, allowed);
    *allowed = true;
    return NULL;
}

const char *entry_permissions(const EntryButton *entry, const EntryContext *context, EntryAccess *access) {
    *access = (EntryAccess){0};
    const char *reason = entry_evaluate(&entry->view, context, &access->view);
    if (reason || !access->view || !entry->defined) return reason;
    reason = entry_evaluate(&entry->visible, context, &access->visible);
    if (reason || !access->visible) return reason;
    return entry_evaluate(&entry->enabled, context, &access->enabled);
}

const char *entry_check(const EntryCondition *condition, const char *instance, unsigned actor,
                        EntryQuery query, bool *allowed) {
    const EntryContext context = {.instance = instance, .viewer = actor, .query = query};
    return entry_evaluate(condition, &context, allowed);
}
