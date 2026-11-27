#include "content_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint32_t date_key(const char *text);

#define COUNT(array) (sizeof(array) / sizeof((array)[0]))
#define F(key) script_field(tree, item, key)

struct TypeDocument {
    TypeDocument *next;
    ScriptTree tree;
    char text[];
};

typedef enum TypeStatus { TYPE_UNRESOLVED, TYPE_VISITING, TYPE_RESOLVED, TYPE_INVALID } TypeStatus;

struct TypeDeclaration {
    TypeDeclaration *next;
    const ScriptTree *tree;
    const ScriptItem *item;
    KhanDefinition *effective;
    TypeStatus status;
    ScriptError error;
};

static TypeDeclaration *find_type(const ContentTypes *types, const char *id) {
    for (TypeDeclaration *type = types->items; type; type = type->next)
        if (!strcmp(type->item->key, id)) return type;
    return NULL;
}

bool has_content_type(const ContentTypes *types, const char *id) {
    return find_type(types, id) != NULL;
}

bool collect_content_types(ContentTypes *types, const char *text, size_t size, const char *path, ScriptError *error) {
    TypeDocument *document = malloc(sizeof(*document) + size + 1);
    if (!document) {
        const ScriptTree source = {0};
        script_problem(&source, NULL, error, "out of memory collecting type declarations");
        snprintf(error->path, sizeof(error->path), "%s", path);
        return false;
    }
    memcpy(document->text, text, size);
    document->text[size] = 0;
    ScriptTree *tree = &document->tree;
    if (!script_tree_read(tree, document->text, size, path, error)) {
        free(document);
        return false;
    }
    TypeDeclaration *existing = types->items;
    for (unsigned i = tree->items[0].first; i; i = tree->items[i].next) {
        const ScriptItem *item = &tree->items[i];
        if (!item->block || !ps_identifier(item->key) || find_type(types, item->key)) {
            script_problem(tree, item, error, "expected a unique named political type block");
            goto failed;
        }
        TypeDeclaration *type = calloc(1, sizeof(*type));
        if (!type) {
            script_problem(tree, item, error, "out of memory collecting types");
            goto failed;
        }
        type->item = item;
        type->tree = tree;
        type->next = types->items;
        types->items = type;
    }
    document->next = types->files;
    types->files = document;
    return true;
failed:
    while (types->items != existing) {
        TypeDeclaration *type = types->items;
        types->items = type->next;
        free(type);
    }
    script_tree_free(tree);
    free(document);
    return false;
}

void free_content_types(ContentTypes *types) {
    while (types->items) {
        TypeDeclaration *type = types->items;
        types->items = type->next;
        free(type->effective);
        free(type);
    }
    while (types->files) {
        TypeDocument *file = types->files;
        types->files = file->next;
        script_tree_free(&file->tree);
        free(file);
    }
}

static bool has_word(const ScriptItem *item) {
    return item && !item->block && item->word[0];
}

static bool cleared(const ScriptItem *item) {
    return script_word(item, "clear") || script_word(item, "null");
}

static bool label(const ScriptTree *tree, const ScriptItem *item, char *out, bool id, ScriptError *error) {
    if (!item) return true;
    if (cleared(item)) {
        *out = '\0';
        return true;
    }
    if (!has_word(item) || (id && !ps_identifier(item->word)))
        return script_problem(tree, item, error, id ? "expected an identifier or null" : "expected text or null");
    strcpy(out, item->word);
    return true;
}

static bool switch_value(const ScriptTree *tree, const ScriptItem *item, bool *out, ScriptError *error) {
    if (!item) return true;
    if (!script_word(item, "yes") && !script_word(item, "no"))
        return script_problem(tree, item, error, "expected yes or no");
    *out = script_word(item, "yes");
    return true;
}

static bool condition(const ScriptTree *tree, const ScriptItem *item, EntryCondition *out, ScriptError *error) {
    if (!item) return true;
    *out = (EntryCondition){0};
    if (cleared(item) || (item->block && !item->first)) return true;
    if (!item->block) return script_problem(tree, item, error, "expected a trigger block");
    ScriptError legacy_error = {0};
    Lexer lexer = lexer_for(item->start, (size_t)(item->end - item->start), &legacy_error);
    lexer.line = item->line;
    if (read_entry_group(&lexer, out, ENTRY_AND)) {
        bool needs_scope = false;
        for (unsigned i = 0; i < out->count && !needs_scope; ++i) {
            if (out->nodes[i].test != ENTRY_QUERY) continue;
            const PsArguments *a = &out->nodes[i].args;
            needs_scope = a->country >= PS_COUNTRY_FROM || a->actor >= PS_COUNTRY_FROM ||
                          a->target >= PS_COUNTRY_FROM || a->province >= PS_PROVINCE_THIS;
        }
        if (!needs_scope) return true;
    }
    *out = (EntryCondition){.count = 1, .nodes = {{.test = ENTRY_NATIVE, .end = 1}}};
    return read_native_clause(tree, item, &out->nodes[0].native, error);
}

static bool patch_entry(const ScriptTree *tree, const ScriptItem *item, EntryButton *out, ScriptError *error) {
    static const char *const keys[] = {
        "region", "icon", "order", "visible", "enabled", "tooltip", "disabled_tooltip", "toggle"
    };
    if (!item) return true;
    if (script_word(item, "no") || cleared(item)) {
        out->defined = out->has_order = false;
        out->toggle = true;
        out->order = 0;
        out->region[0] = out->icon[0] = out->tooltip[0] = out->disabled_tooltip[0] = 0;
        out->visible = out->enabled = (EntryCondition){0};
        return true;
    }
    if (!script_word(item, "yes") && !script_keys(tree, item, keys, COUNT(keys), error)) return false;
    out->defined = true;
    const ScriptItem *order = F("order");
    if (order && !script_number(order, 0, 32767, &out->order))
        return script_problem(tree, order, error, "order requires an integer from 0 to 32767");
    out->has_order = true;
    return label(tree, F("region"), out->region, true, error) &&
           label(tree, F("icon"), out->icon, true, error) &&
           label(tree, F("tooltip"), out->tooltip, true, error) &&
           label(tree, F("disabled_tooltip"), out->disabled_tooltip, true, error) &&
           switch_value(tree, F("toggle"), &out->toggle, error) &&
           condition(tree, F("visible"), &out->visible, error) &&
           condition(tree, F("enabled"), &out->enabled, error);
}

static bool patch_interface(const ScriptTree *tree, const ScriptItem *item, EntryButton *out, ScriptError *error) {
    static const char *const keys[] = {
        "layout", "background", "view_trigger", "entry_button", "close_button", "close_on_escape", "title_key"
    };
    if (!item) return true;
    if (script_word(item, "no") || cleared(item)) {
        *out = (EntryButton){ .toggle = true, .close_button = true, .close_on_escape = true };
        return true;
    }
    if (!script_keys(tree, item, keys, COUNT(keys), error)) return false;
    return label(tree, F("layout"), out->layout, true, error) &&
           label(tree, F("background"), out->background, true, error) &&
           label(tree, F("title_key"), out->title_key, true, error) &&
           condition(tree, F("view_trigger"), &out->view, error) &&
           switch_value(tree, F("close_button"), &out->close_button, error) &&
           switch_value(tree, F("close_on_escape"), &out->close_on_escape, error) &&
           patch_entry(tree, F("entry_button"), out, error);
}

static bool country_rule(const ScriptTree *tree, const ScriptItem *item, EntryCondition *out, ScriptError *error) {
    if (!condition(tree, item, out, error)) return false;
    for (unsigned i = 0; item && i < out->count; ++i) {
        if (out->nodes[i].test != ENTRY_QUERY) continue;
        *out = (EntryCondition){.count = 1, .nodes = {{.test = ENTRY_NATIVE, .end = 1}}};
        return read_native_clause(tree, item, &out->nodes[0].native, error);
    }
    return true;
}

static bool patch_members(const ScriptTree *tree, const ScriptItem *item, KhanDefinition *out, ScriptError *error) {
    static const char *const keys[] = {
        "source", "subject_type", "title", "source_trigger", "join_trigger", "retain_trigger", "leave_trigger",
        "max_per_country", "rejoin_cooldown_days"
    };
    if (!item) return true;
    if (script_word(item, "no") || cleared(item)) {
        out->member_source = MEMBERS_UNSET;
        out->subject_type[0] = out->member_title[0] = 0;
        out->max_memberships = 0;
        out->member_query_defined = false;
        out->member_query = out->member_join = out->member_retain = out->member_leave = (EntryCondition){0};
        return true;
    }
    if (script_word(item, "yes")) {
        if (out->member_source == MEMBERS_UNSET) out->member_source = MEMBERS_EXPLICIT;
        return true;
    }
    if (!script_keys(tree, item, keys, COUNT(keys), error)) return false;
    const ScriptItem *source = F("source");
    if (out->member_source == MEMBERS_UNSET || script_word(source, "explicit"))
        out->member_source = MEMBERS_EXPLICIT;
    if (script_word(source, "leader_subjects")) out->member_source = MEMBERS_DIRECT_SUBJECTS;
    else if (script_word(source, "query")) out->member_source = MEMBERS_QUERY;
    else if (script_word(source, "mixed")) out->member_source = MEMBERS_MIXED;
    else if (script_word(source, "capital_territory")) out->member_source = MEMBERS_CAPITAL_TERRITORY;
    else if (source && !script_word(source, "explicit"))
        return script_problem(tree, source, error, "expected explicit, leader_subjects, capital_territory, query or mixed");
    const ScriptItem *query = F("source_trigger");
    if (query) out->member_query_defined = query->block;
    if (!country_rule(tree, query, &out->member_query, error) ||
        !country_rule(tree, F("join_trigger"), &out->member_join, error) ||
        !country_rule(tree, F("retain_trigger"), &out->member_retain, error) ||
        !country_rule(tree, F("leave_trigger"), &out->member_leave, error)) return false;
    const ScriptItem *cooldown = F("rejoin_cooldown_days");
    if (cooldown && !script_word(cooldown, "0"))
        return script_problem(tree, cooldown, error, "rejoin cooldown awaits member actions");
    const ScriptItem *limit = F("max_per_country");
    int number = 0;
    if (limit) {
        if (!cleared(limit) && !script_number(limit, 0, PS_MAX_INSTANCES, &number))
            return script_problem(tree, limit, error, "max_per_country requires a nonnegative instance count or null");
        if (number && out->member_source != MEMBERS_EXPLICIT)
            return script_problem(tree, limit, error, "max_per_country can currently be set for explicit members");
        out->max_memberships = (unsigned)number;
    }
    const ScriptItem *subject = F("subject_type");
    if (subject) {
        out->subject_type_line = subject->line;
        out->subject_type_column = subject->column;
        snprintf(out->subject_type_path, sizeof(out->subject_type_path), "%s", tree->path);
    }
    if (!label(tree, subject, out->subject_type, true, error) ||
        !label(tree, F("title"), out->member_title, false, error)) return false;
    if (!strcmp(out->subject_type, "daimyo")) strcpy(out->subject_type, "daimyo_vassal");
    return true;
}

static bool patch_territory(const ScriptTree *tree, const ScriptItem *item, KhanDefinition *out, ScriptError *error) {
    if (!item) return true;
    if (cleared(item)) {
        out->territory_enabled = false;
        out->territory_history_key[0] = 0;
        return true;
    }
    if (!item->block) {
        if (!switch_value(tree, item, &out->territory_enabled, error)) return false;
        if (!out->territory_enabled) out->territory_history_key[0] = 0;
        return true;
    }
    static const char *const keys[] = { "source", "history_key" };
    if (!script_keys(tree, item, keys, COUNT(keys), error)) return false;
    const ScriptItem *source = F("source"), *key = F("history_key");
    if (source && !script_word(source, "explicit") && !script_word(source, "province_history"))
        return script_problem(tree, source, error, "expected explicit or province_history territory");
    if (script_word(source, "explicit")) {
        if (key) return script_problem(tree, key, error, "history_key requires province_history territory");
        out->territory_history_key[0] = 0;
    } else {
        if (!label(tree, key, out->territory_history_key, true, error)) return false;
        if (script_word(source, "province_history") && !out->territory_history_key[0])
            return script_problem(tree, item, error, "province_history territory requires history_key");
    }
    out->territory_enabled = true;
    return true;
}

static bool fixed(const ScriptTree *tree, const ScriptItem *item, int32_t *out, ScriptError *error);
static bool unique_named(const ScriptTree *tree, const ScriptItem *item, const ScriptItem *list, ScriptError *error);

static bool patch_permissions(const ScriptTree *tree, const ScriptItem *item, PoliticalPermissions *out, ScriptError *error) {
    if (!item) return true;
    if (cleared(item)) { out->count = 0; return true; }
    if (!item->block) return script_problem(tree, item, error, "expected a permission grant list");
    for (unsigned i = item->first; i; i = tree->items[i].next) {
        const ScriptItem *grant = &tree->items[i];
        if (!unique_named(tree, grant, item, error)) return false;
        bool granted = false;
        if (!switch_value(tree, grant, &granted, error)) return false;
        unsigned slot = 0;
        while (slot < out->count && strcmp(out->keys[slot], grant->key)) ++slot;
        if (slot < out->count && !granted) {
            --out->count;
            memmove(out->keys + slot, out->keys + slot + 1, (out->count - slot) * sizeof(*out->keys));
        } else if (slot == out->count && granted) {
            if (out->count == PS_MAX_PERMISSIONS) return script_problem(tree, grant, error, "too many permission grants");
            strcpy(out->keys[out->count++], grant->key);
        }
    }
    return true;
}

static bool selection_tie(const ScriptTree *tree, const ScriptItem *item, bool *incumbent, bool *defined, ScriptError *error) {
    if (!item) return true;
    if (!script_word(item, "stable_id") && !script_word(item, "incumbent_then_stable_id"))
        return script_problem(tree, item, error, "expected stable_id or incumbent_then_stable_id");
    *defined = true;
    *incumbent = script_word(item, "incumbent_then_stable_id");
    return true;
}

static bool patch_contest(const ScriptTree *tree, const ScriptItem *item, PoliticalContestRule *out, ScriptError *error) {
    const ScriptItem *period = F("contest_period_days"), *cost = F("investment");
    if (period) {
        int days;
        if (!script_number(period, 1, 36500, &days)) return script_problem(tree, period, error, "contest_period_days must be 1..36500");
        out->period = (unsigned)days;
    }
    if (!label(tree, F("score"), out->score, true, error) ||
        !selection_tie(tree, F("tie_break"), &out->incumbent_ties, &out->has_tie_rule, error)) return false;
    if (!cost) return true;
    if (cleared(cost)) { out->investment[0] = 0; out->cost = 0; return true; }
    static const char *const keys[] = {"resource", "payer", "amount"};
    if (!script_keys(tree, cost, keys, COUNT(keys), error)) return false;
    const ScriptItem *resource = script_field(tree, cost, "resource"), *amount = script_field(tree, cost, "amount");
    if (!resource || !amount || !script_word(script_field(tree, cost, "payer"), "actor"))
        return script_problem(tree, cost, error, "contest investment requires a resource, payer=actor and amount");
    if (!label(tree, resource, out->investment, true, error) || !fixed(tree, amount, &out->cost, error)) return false;
    return (out->investment[0] && out->cost >= 0) || script_problem(tree, cost, error, "invalid contest investment");
}

static bool patch_roles(const ScriptTree *tree, const ScriptItem *item, KhanDefinition *out, ScriptError *error) {
    if (!item) return true;
    if (cleared(item) || script_word(item, "no")) { out->role_count = 0; return true; }
    if (!item->block) return script_problem(tree, item, error, "expected named special_members definitions");
    for (unsigned i = item->first; i; i = tree->items[i].next) {
        const ScriptItem *role = &tree->items[i];
        if (!ps_identifier(role->key)) return script_problem(tree, role, error, "expected a role identifier");
        for (unsigned earlier = item->first; earlier != i; earlier = tree->items[earlier].next)
            if (!strcmp(tree->items[earlier].key, role->key))
                return script_problem(tree, role, error, "duplicate role identifier");
        unsigned index = 0;
        while (index < out->role_count && strcmp(out->roles[index].id, role->key)) ++index;
        if (cleared(role) || script_word(role, "no")) {
            if (index == out->role_count) return script_problem(tree, role, error, "cannot clear an unknown role");
            --out->role_count;
            memmove(&out->roles[index], &out->roles[index + 1], (out->role_count - index) * sizeof(out->roles[0]));
            continue;
        }
        static const char *const keys[] = { "acquisition", "max_holders", "require_member", "can_hold_with_leader",
            "candidate_trigger", "retain_trigger", "title_key", "tooltip", "permissions", "contest_period_days",
            "investment", "score", "tie_break", "history_key" };
        if (!script_keys(tree, role, keys, COUNT(keys), error)) return false;
        if (index == out->role_count) {
            if (index == PS_MAX_ROLES) return script_problem(tree, role, error, "role definition capacity exceeded");
            out->roles[index] = (PoliticalRole){ .max_holders = 1, .require_member = true, .can_hold_with_leader = true };
            strcpy(out->roles[index].id, role->key);
            ++out->role_count;
        }
        PoliticalRole *definition = &out->roles[index];
        const ScriptItem *acquisition = script_field(tree, role, "acquisition");
        if (acquisition) {
            if (script_word(acquisition, "explicit")) definition->acquisition = OFFICE_EXPLICIT;
            else if (script_word(acquisition, "appointed")) definition->acquisition = OFFICE_APPOINTED;
            else if (script_word(acquisition, "competition")) definition->acquisition = OFFICE_COMPETITION;
            else return script_problem(tree, acquisition, error, "expected explicit, appointed or competition acquisition");
        }
        const ScriptItem *maximum = script_field(tree, role, "max_holders");
        int count;
        if (maximum) {
            if (!script_number(maximum, 1, PS_MAX_ROLE_HOLDERS, &count))
                return script_problem(tree, maximum, error, "max_holders outside the supported range 1..32");
            definition->max_holders = (unsigned)count;
        }
        if (!switch_value(tree, script_field(tree, role, "require_member"), &definition->require_member, error) ||
            !switch_value(tree, script_field(tree, role, "can_hold_with_leader"), &definition->can_hold_with_leader, error) ||
            !country_rule(tree, script_field(tree, role, "candidate_trigger"), &definition->candidate, error) ||
            !country_rule(tree, script_field(tree, role, "retain_trigger"), &definition->retain, error) ||
            !label(tree, script_field(tree, role, "history_key"), definition->history_key, true, error) ||
            !label(tree, script_field(tree, role, "title_key"), definition->title_key, true, error) ||
            !label(tree, script_field(tree, role, "tooltip"), definition->tooltip_key, true, error) ||
            !patch_permissions(tree, script_field(tree, role, "permissions"), &definition->permissions, error) ||
            !patch_contest(tree, role, &definition->contest, error)) return false;
    }
    return true;
}

static bool nomination_condition(const ScriptTree *tree, const ScriptItem *item, NominationRules *rules,
                                 unsigned *root, ScriptError *error) {
    EntryCondition condition_nodes;
    if (!item) return true;
    if (!country_rule(tree, item, &condition_nodes, error)) return false;
    if (!condition_nodes.count) return true;
    const unsigned first = rules->node_count;
    if (first + condition_nodes.count > COUNT(rules->nodes))
        return script_problem(tree, item, error, "nomination triggers exceed 32 nodes in total");
    *root = first + 1;
    for (unsigned i = 0; i < condition_nodes.count; ++i) {
        rules->nodes[first + i] = condition_nodes.nodes[i];
        rules->nodes[first + i].end += first;
    }
    rules->node_count += condition_nodes.count;
    return true;
}

static bool patch_nomination(const ScriptTree *tree, const ScriptItem *item, NominationRules *rules, ScriptError *error) {
    if (!item) return true;
    if (cleared(item)) { *rules = (NominationRules){0}; return true; }
    static const char *const keys[] = {"min", "max", "base", "opinion_weight", "tooltip", "terms"};
    if (!script_keys(tree, item, keys, COUNT(keys), error)) return false;
    if (!rules->defined) *rules = (NominationRules){
        .defined = true, .minimum = -PS_NOMINATION_LIMIT, .maximum = PS_NOMINATION_LIMIT, .opinion_weight = 1,
    };
    const ScriptItem *minimum = F("min"), *maximum = F("max"), *base = F("base"), *weight = F("opinion_weight");
    if ((minimum && !script_number(minimum, -PS_NOMINATION_LIMIT, PS_NOMINATION_LIMIT, &rules->minimum)) ||
        (maximum && !script_number(maximum, -PS_NOMINATION_LIMIT, PS_NOMINATION_LIMIT, &rules->maximum)) ||
        (base && !script_number(base, -PS_NOMINATION_LIMIT, PS_NOMINATION_LIMIT, &rules->base)))
        return script_problem(tree, item, error, "nomination min, max and base must be integers within -2000..2000");
    if (rules->minimum > rules->maximum) return script_problem(tree, item, error, "nomination min exceeds max");
    if (weight && !script_number(weight, 0, 10, &rules->opinion_weight))
        return script_problem(tree, weight, error, "nomination opinion_weight must be 0..10");
    if (!label(tree, F("tooltip"), rules->tooltip, true, error)) return false;
    const ScriptItem *terms = F("terms");
    if (!terms) return true;
    rules->count = rules->node_count = 0;
    if (cleared(terms)) return true;
    if (!terms->block) return script_problem(tree, terms, error, "nomination terms require a block or clear");
    for (unsigned i = terms->first; i; i = tree->items[i].next) {
        const ScriptItem *item = &tree->items[i];
        if (rules->count == COUNT(rules->terms)) return script_problem(tree, item, error, "too many nomination terms");
        static const char *const fields[] = {"add", "tooltip", "elector_trigger", "candidate_trigger"};
        if (!unique_named(tree, item, terms, error) || !script_keys(tree, item, fields, COUNT(fields), error)) return false;
        NominationTerm *term = &rules->terms[rules->count++];
        *term = (NominationTerm){0};
        strcpy(term->id, item->key);
        if (!script_number(F("add"), -PS_NOMINATION_LIMIT, PS_NOMINATION_LIMIT, &term->add))
            return script_problem(tree, item, error, "nomination term requires integer add within -2000..2000");
        if (!label(tree, F("tooltip"), term->tooltip, true, error) ||
            !nomination_condition(tree, F("elector_trigger"), rules, &term->elector_root, error) ||
            !nomination_condition(tree, F("candidate_trigger"), rules, &term->candidate_root, error)) return false;
    }
    return true;
}

static bool patch_succession(const ScriptTree *tree, const ScriptItem *item, KhanDefinition *type, ScriptError *error) {
    if (!item) return true;
    LeaderSelection *out = &type->succession;
    if (cleared(item)) {
        out->defined = false;
        out->mode = LEADER_EXPLICIT;
        out->nomination = (NominationRules){0};
        type->election = (ElectionRules){0};
        return true;
    }
    static const char *const keys[] = {"event", "mode", "value", "electors_role", "vote_cooldown_days", "cooldown_on_turnover",
        "threshold_resource", "threshold_id", "tie_break", "contest_period_days", "investment", "score", "nomination"};
    if (!script_keys(tree, item, keys, COUNT(keys), error)) return false;
    out->defined = true;
    const ScriptItem *mode = F("mode"), *event = F("event"), *policy = F("cooldown_on_turnover"), *days = F("vote_cooldown_days");
    if (mode) {
        static const char *const modes[] = {"explicit", "appointed", "elected", "competition", "highest_value"};
        unsigned i = 0;
        while (i < COUNT(modes) && !script_word(mode, modes[i])) ++i;
        if (i == COUNT(modes)) return script_problem(tree, mode, error, "unknown leader selection mode");
        out->mode = (LeaderMethod)i;
    }
    if (event) {
        static const char *const events[] = {"explicit", "vacancy", "ruler_changed", "threshold"};
        unsigned i = 0;
        while (i < COUNT(events) && !script_word(event, events[i])) ++i;
        if (i == COUNT(events)) return script_problem(tree, event, error, "unknown succession event");
        out->event = (SelectionEvent)i;
    }
    if (policy) {
        if (script_word(policy, "reset")) out->cooldown_policy = COOLDOWN_RESET;
        else if (script_word(policy, "restart")) out->cooldown_policy = COOLDOWN_RESTART;
        else if (script_word(policy, "retain")) out->cooldown_policy = COOLDOWN_RETAIN;
        else return script_problem(tree, policy, error, "expected reset, restart or retain cooldown_on_turnover");
        out->has_cooldown_policy = true;
    }
    if (days && !script_number(days, 0, 3650, &type->election.cooldown_days))
        return script_problem(tree, days, error, "vote_cooldown_days must be 0..3650");
    if (!label(tree, F("value"), out->value, true, error) || !label(tree, F("electors_role"), out->elector_role, true, error) ||
        !label(tree, F("threshold_resource"), out->threshold_resource, true, error) ||
        !label(tree, F("threshold_id"), out->threshold, true, error) ||
        !patch_contest(tree, item, &out->contest, error) ||
        !patch_nomination(tree, F("nomination"), &out->nomination, error)) return false;
    type->election.defined = out->mode == LEADER_ELECTED;
    type->election.incumbent_ties = out->contest.incumbent_ties;
    type->election.weight = 1;
    return true;
}

static bool patch_leader(const ScriptTree *tree, const ScriptItem *item, KhanDefinition *out, ScriptError *error) {
    static const char *const keys[] = { "title", "require_member", "candidate_trigger", "retain_trigger", "permissions", "succession" };
    if (!item) return true;
    if (script_word(item, "no") || cleared(item)) {
        out->leader_enabled = false;
        out->emperor[0] = 0;
        out->succession = (LeaderSelection){0};
        out->election = (ElectionRules){0};
        return true;
    }
    if (!script_word(item, "yes") && !script_keys(tree, item, keys, COUNT(keys), error)) return false;
    out->leader_enabled = true;
    return label(tree, F("title"), out->emperor, false, error) &&
           switch_value(tree, F("require_member"), &out->succession.require_member, error) &&
           country_rule(tree, F("candidate_trigger"), &out->succession.candidate, error) &&
           country_rule(tree, F("retain_trigger"), &out->succession.retain, error) &&
           patch_permissions(tree, F("permissions"), &out->succession.permissions, error) &&
           patch_succession(tree, F("succession"), out, error);
}

static bool patch_existence(const ScriptTree *tree, const ScriptItem *item, KhanDefinition *out, ScriptError *error) {
    static const char *const keys[] = {
        "allow_history", "max_instances", "allow_dynamic_creation", "allow_dissolution",
        "create_trigger", "continue_trigger"
    };
    if (!item) return true;
    if (!script_keys(tree, item, keys, COUNT(keys), error)) return false;
    const ScriptItem *limit = F("max_instances");
    int number;
    if (limit) {
        if (!script_number(limit, 0, PS_MAX_INSTANCES, &number))
            return script_problem(tree, limit, error, "max_instances requires a nonnegative instance count");
        out->max_instances = (unsigned)number;
    }
    return switch_value(tree, F("allow_history"), &out->history_allowed, error) &&
           switch_value(tree, F("allow_dynamic_creation"), &out->dynamic_allowed, error) &&
           switch_value(tree, F("allow_dissolution"), &out->dissolution_allowed, error) &&
           condition(tree, F("create_trigger"), &out->create_trigger, error) &&
           country_rule(tree, F("continue_trigger"), &out->continue_trigger, error);
}

static bool patch_war_rules(const ScriptTree *tree, const ScriptItem *item, KhanDefinition *out, ScriptError *error) {
    static const char *const keys[] = {"internal_war", "protection"};
    if (!item) return true;
    if (script_word(item, "no") || cleared(item)) {
        out->block_direct_war = false;
        return true;
    }
    if (!script_keys(tree, item, keys, COUNT(keys), error)) return false;
    const ScriptItem *internal = F("internal_war"), *protection = F("protection");
    if (protection && !script_word(protection, "no"))
        return script_problem(tree, protection, error, "external protection is not implemented");
    if (internal && !script_word(internal, "block_direct") && !script_word(internal, "vanilla") && !cleared(internal))
        return script_problem(tree, internal, error, "expected internal_war = block_direct or vanilla");
    if (internal) out->block_direct_war = script_word(internal, "block_direct");
    return true;
}

static bool patch_war_obligations(const ScriptTree *tree, const ScriptItem *item, KhanDefinition *out, ScriptError *error) {
    static const char *const keys[] = {"defensive", "offensive", "conflict", "defensive_trigger"};
    if (!item) return true;
    if (script_word(item, "no") || cleared(item)) {
        out->defensive_obligation = false;
        out->defensive_trigger = (EntryCondition){0};
        return true;
    }
    if (!script_keys(tree, item, keys, COUNT(keys), error)) return false;
    const ScriptItem *offensive = F("offensive"), *conflict = F("conflict");
    if (offensive && !script_word(offensive, "no"))
        return script_problem(tree, offensive, error, "offensive obligations are not implemented");
    if (conflict && !script_word(conflict, "vanilla_first"))
        return script_problem(tree, conflict, error, "only vanilla_first conflict handling is implemented");
    return switch_value(tree, F("defensive"), &out->defensive_obligation, error) &&
           condition(tree, F("defensive_trigger"), &out->defensive_trigger, error);
}

static bool fixed(const ScriptTree *tree, const ScriptItem *item, int32_t *out, ScriptError *error) {
    if (!item) return true;
    if (!has_word(item) || !read_fixed_number(item->word, out))
        return script_problem(tree, item, error, "expected an in-range decimal with at most three places");
    return true;
}

static bool unique_named(const ScriptTree *tree, const ScriptItem *item, const ScriptItem *list, ScriptError *error) {
    if (!ps_identifier(item->key)) return script_problem(tree, item, error, "expected a named entry");
    for (unsigned j = list->first; script_item(tree, j) != item; j = tree->items[j].next)
        if (!strcmp(tree->items[j].key, item->key)) return script_problem(tree, item, error, "duplicate named entry");
    return true;
}

static bool compare_mode(const ScriptTree *tree, const ScriptItem *item, PsComparison *out, ScriptError *error) {
    if (!item) return true;
    static const char *const names[] = { "ge", "eq", "le", "gt", "lt", "ne" };
    for (unsigned i = 0; i < COUNT(names); ++i)
        if (script_word(item, names[i])) { *out = (PsComparison)i; return true; }
    return script_problem(tree, item, error, "expected comparison ge, eq, le, gt, lt or ne");
}

static bool read_growth_trigger(const ScriptTree *tree, const ScriptItem *item, ResourceTerm *term,
                                bool monthly, ScriptError *error) {
    if (!item) return true;
    if (!item->block) return script_problem(tree, item, error, "expected a resource trigger block");
    const ScriptItem *trigger = script_item(tree, item->first);
    if (!trigger) return true;
    if (trigger->next) {
        if (!monthly) return script_problem(tree, trigger, error, "expected one legacy condition");
        term->test = ENTRY_NATIVE;
        return read_native_clause(tree, item, &term->native, error);
    }
    if (!strcmp(trigger->key, "political_calculated_value") && !script_field(tree, trigger, "instance") &&
        !script_field(tree, trigger, "type") && !script_field(tree, trigger, "country")) {
        static const char *const keys[] = { "id", "compare", "value" };
        if (!script_keys(tree, trigger, keys, COUNT(keys), error)) return false;
        const ScriptItem *value = script_field(tree, trigger, "value");
        if (!value || !script_field(tree, trigger, "id"))
            return script_problem(tree, trigger, error, "calculated condition requires id and value");
        term->test = ENTRY_QUERY;
        return label(tree, script_field(tree, trigger, "id"), term->calculation, true, error) &&
               compare_mode(tree, script_field(tree, trigger, "compare"), &term->comparison, error) &&
               fixed(tree, value, &term->compare_to, error);
    }
    if (!strcmp(trigger->key, "political_leader")) {
        term->leader_scope = true;
        item = trigger;
        if (!trigger->block) return script_problem(tree, item, error, "expected a political_leader trigger block");
        trigger = script_item(tree, trigger->first);
        if (!trigger) return true;
        if (monthly && trigger->next) {
            term->test = ENTRY_NATIVE;
            return read_native_clause(tree, item, &term->native, error);
        }
        if (trigger->next) return script_problem(tree, item, error, "expected one legacy country condition");
    }
    const PsOperation operation = ps_find_operation(trigger->key);
    if (monthly && (operation == PS_CONTENT_REFORM || operation == PS_HAS_ROLE) &&
        !script_field(tree, trigger, "instance") && !script_field(tree, trigger, "type") &&
        !script_field(tree, trigger, "country")) {
        const char *const keys[] = {operation == PS_CONTENT_REFORM ? "id" : "role", "value"};
        if (!script_keys(tree, trigger, keys, COUNT(keys), error)) return false;
        const ScriptItem *id = script_field(tree, trigger, keys[0]), *value = script_field(tree, trigger, "value");
        if (!id || !label(tree, id, term->operand, true, error))
            return id ? false : script_problem(tree, trigger, error, "political condition requires a named reform or role");
        term->political_test = operation;
        term->expected = true;
        return switch_value(tree, value, &term->expected, error);
    }
    if (!strcmp(trigger->key, "has_country_flag") && has_word(trigger) && ps_identifier(trigger->word))
        term->test = ENTRY_COUNTRY_FLAG;
    else if (!strcmp(trigger->key, "tag") && has_word(trigger) && strlen(trigger->word) == 3 &&
             strspn(trigger->word, "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") == 3)
        term->test = ENTRY_TAG;
    else if (!strcmp(trigger->key, "always") && (script_word(trigger, "yes") || script_word(trigger, "no")))
        term->test = ENTRY_ALWAYS;
    else {
        if (!monthly) return script_problem(tree, trigger, error, "unsupported legacy condition");
        term->test = ENTRY_NATIVE;
        return read_native_clause(tree, item, &term->native, error);
    }
    strcpy(term->operand, trigger->word);
    return true;
}

static bool patch_growth(const ScriptTree *tree, const ScriptItem *item, ResourceDefinition *resource, ScriptError *error) {
    if (!item) return true;
    resource->monthly = 0;
    resource->term_count = 0;
    resource->growth_modifier[0] = 0;
    resource->modifier_source = GROWTH_LEADER;
    if (cleared(item) || script_word(item, "no")) return true;
    if (!item->block) return script_problem(tree, item, error, "expected monthly_growth block");
    for (unsigned i = item->first; i; i = tree->items[i].next) {
        const ScriptItem *field = &tree->items[i];
        if (!unique_named(tree, field, item, error)) return false;
        if (!strcmp(field->key, "value")) {
            if (!fixed(tree, field, &resource->monthly, error)) return false;
            continue;
        }
        if (!strcmp(field->key, "growth_modifier")) {
            static const char *const keys[] = {"key", "source"};
            const ScriptItem *key = script_field(tree, field, "key"), *source = script_field(tree, field, "source");
            if (!script_keys(tree, field, keys, COUNT(keys), error)) return false;
            if (!key || !label(tree, key, resource->growth_modifier, true, error) || !resource->growth_modifier[0])
                return script_problem(tree, key ? key : field, error, "growth_modifier requires a named native modifier key");
            if (!source || script_word(source, "leader")) resource->modifier_source = GROWTH_LEADER;
            else if (script_word(source, "account_country")) resource->modifier_source = GROWTH_ACCOUNT;
            else if (script_word(source, "leader_capital")) resource->modifier_source = GROWTH_LEADER_CAPITAL;
            else if (script_word(source, "account_capital")) resource->modifier_source = GROWTH_ACCOUNT_CAPITAL;
            else return script_problem(tree, source, error, "unknown growth_modifier source");
            continue;
        }
        static const char *const keys[] = { "add", "modifier", "trigger", "tooltip" };
        if (resource->term_count == PS_MAX_RESOURCE_TERMS)
            return script_problem(tree, field, error, "too many monthly resource terms");
        if (!script_keys(tree, field, keys, COUNT(keys), error)) return false;
        const ScriptItem *add = script_field(tree, field, "add"), *modifier = script_field(tree, field, "modifier");
        if ((!add && !modifier) || (add && modifier))
            return script_problem(tree, field, error, "monthly term requires either add or modifier");
        ResourceTerm *term = &resource->terms[resource->term_count++];
        *term = (ResourceTerm){0};
        strcpy(term->id, field->key);
        if (!label(tree, script_field(tree, field, "tooltip"), term->tooltip, false, error)) return false;
        if (add && !fixed(tree, add, &term->add, error)) return false;
        if (modifier && (!fixed(tree, modifier, &term->modifier, error) ||
                         term->modifier < -PS_RESOURCE_SCALE || term->modifier > 10 * PS_RESOURCE_SCALE))
            return script_problem(tree, modifier, error, "monthly modifier must be between -1 and 10");
        if (!read_growth_trigger(tree, script_field(tree, field, "trigger"), term, true, error)) return false;
    }
    return true;
}

static bool patch_thresholds(const ScriptTree *tree, const ScriptItem *item, ResourceDefinition *resource, ScriptError *error) {
    if (!item) return true;
    resource->threshold_count = 0;
    if (cleared(item) || script_word(item, "no")) return true;
    if (!item->block) return script_problem(tree, item, error, "expected thresholds block");
    for (unsigned i = item->first; i; i = tree->items[i].next) {
        const ScriptItem *entry = &tree->items[i];
        static const char *const keys[] = { "at", "direction", "message" };
        if (resource->threshold_count == PS_MAX_THRESHOLDS)
            return script_problem(tree, entry, error, "too many resource thresholds");
        if (!unique_named(tree, entry, item, error) || !script_keys(tree, entry, keys, COUNT(keys), error)) return false;
        ResourceThreshold *threshold = &resource->thresholds[resource->threshold_count++];
        *threshold = (ResourceThreshold){0};
        strcpy(threshold->id, entry->key);
        if (!label(tree, script_field(tree, entry, "message"), threshold->message, true, error)) return false;
        const ScriptItem *at = script_field(tree, entry, "at"), *direction = script_field(tree, entry, "direction");
        if (!at) return script_problem(tree, entry, error, "threshold requires at");
        if (!fixed(tree, at, &threshold->at, error)) return false;
        if (direction && !script_word(direction, "rising") && !script_word(direction, "falling"))
            return script_problem(tree, direction, error, "expected rising or falling");
        threshold->falling = script_word(direction, "falling");
    }
    return true;
}

static bool patch_resource(const ScriptTree *tree, const ScriptItem *item, KhanDefinition *out, ScriptError *error) {
    static const char *const keys[] = { "owner", "account_holders", "on_owner_loss", "min", "max", "initial", "precision", "script_description",
                                      "monthly_growth", "thresholds", "name_key", "tooltip" };
    const unsigned slot = resource_find(out->resources, out->resource_count, item->key);
    if (cleared(item) || script_word(item, "no")) {
        if (slot < out->resource_count) {
            --out->resource_count;
            memmove(out->resources + slot, out->resources + slot + 1,
                    (out->resource_count - slot) * sizeof(*out->resources));
        }
        return true;
    }
    if (!script_word(item, "yes") && !script_keys(tree, item, keys, COUNT(keys), error)) return false;
    if (slot == PS_MAX_RESOURCES) return script_problem(tree, item, error, "too many political resources");
    ResourceDefinition *resource = &out->resources[slot];
    if (slot == out->resource_count) {
        ++out->resource_count;
        *resource = (ResourceDefinition){ .maximum = 100 * PS_RESOURCE_SCALE, .precision = 2 };
        strcpy(resource->id, item->key);
    }
    const ScriptItem *owner = F("owner"), *holders = F("account_holders"), *loss = F("on_owner_loss");
    if (owner) {
        if (script_word(owner, "instance")) resource->owner = RESOURCE_INSTANCE;
        else if (script_word(owner, "country")) {
            if (resource->owner == RESOURCE_INSTANCE) resource->owner = RESOURCE_MEMBER;
        } else return script_problem(tree, owner, error, "expected instance or country owner");
    }
    if (holders && resource->owner != RESOURCE_INSTANCE) {
        if (script_word(holders, "members")) resource->owner = RESOURCE_MEMBER;
        else if (script_word(holders, "leader")) resource->owner = RESOURCE_LEADER;
        else return script_problem(tree, holders, error, "expected members or leader account_holders");
    }
    if (loss && resource->owner != RESOURCE_INSTANCE) {
        if (!script_word(loss, "retain") && !script_word(loss, "remove"))
            return script_problem(tree, loss, error, "expected retain or remove on_owner_loss");
        resource->retain = script_word(loss, "retain");
        resource->loss_defined = true;
    }
    if (resource->owner == RESOURCE_INSTANCE && (holders || loss))
        return script_problem(tree, holders ? holders : loss, error, "only country accounts have holders and an owner loss rule");
    int precision = (int)resource->precision;
    if (F("precision") && !script_number(F("precision"), 0, 3, &precision))
        return script_problem(tree, F("precision"), error, "precision must be 0..3");
    resource->precision = (unsigned)precision;
    if (!patch_growth(tree, F("monthly_growth"), resource, error)) return false;
    if (resource->owner == RESOURCE_INSTANCE && resource->growth_modifier[0] &&
        (resource->modifier_source == GROWTH_ACCOUNT || resource->modifier_source == GROWTH_ACCOUNT_CAPITAL))
        return script_problem(tree, item, error, "an instance resource requires leader or leader_capital growth modifier source");
    return fixed(tree, F("min"), &resource->minimum, error) && fixed(tree, F("max"), &resource->maximum, error) &&
           fixed(tree, F("initial"), &resource->initial, error) &&
           label(tree, F("name_key"), resource->name, true, error) && label(tree, F("tooltip"), resource->tooltip, true, error) &&
           label(tree, F("script_description"), resource->script_description, true, error) &&
           patch_thresholds(tree, F("thresholds"), resource, error);
}

static bool patch_resources(const ScriptTree *tree, const ScriptItem *root, KhanDefinition *out, ScriptError *error) {
    const ScriptItem *simple = script_field(tree, root, "authority");
    const ScriptItem *item = script_field(tree, root, "resources");
    if (simple && !patch_resource(tree, simple, out, error)) return false;
    if (item && (cleared(item) || script_word(item, "no"))) out->resource_count = 0;
    else if (item) {
        if (!item->block) return script_problem(tree, item, error, "expected resource definitions");
        for (unsigned i = item->first; i; i = tree->items[i].next) {
            const ScriptItem *entry = &tree->items[i];
            if (!unique_named(tree, entry, item, error)) return false;
            if (!strcmp(entry->key, "authority") && script_word(simple, "no") && !cleared(entry) && !script_word(entry, "no"))
                return script_problem(tree, entry, error, "authority=no conflicts with an enabled resources.authority");
            if (!patch_resource(tree, entry, out, error)) return false;
        }
    }
    const unsigned authority = resource_find(out->resources, out->resource_count, PS_AUTHORITY);
    out->authority_enabled = authority < out->resource_count;
    out->starting_authority = out->authority_enabled ? out->resources[authority].initial / PS_RESOURCE_SCALE : 0;
    return true;
}

static bool patch_calculations(const ScriptTree *tree, const ScriptItem *item, KhanDefinition *out, ScriptError *error) {
    if (!item) return true;
    if (cleared(item) || script_word(item, "no")) { out->calculation_count = 0; return true; }
    if (!item->block) return script_problem(tree, item, error, "expected calculated_values block");
    for (unsigned i = item->first; i; i = tree->items[i].next) {
        const ScriptItem *entry = &tree->items[i];
        if (!unique_named(tree, entry, item, error)) return false;
        const unsigned slot = calculation_find(out->calculations, out->calculation_count, entry->key);
        if (cleared(entry)) {
            if (slot < out->calculation_count) {
                --out->calculation_count;
                memmove(out->calculations + slot, out->calculations + slot + 1,
                        (out->calculation_count - slot) * sizeof(*out->calculations));
            }
            continue;
        }
        static const char *const keys[] = { "scope", "value" };
        if (slot == PS_MAX_VALUES) return script_problem(tree, entry, error, "too many calculated values");
        if (!script_keys(tree, entry, keys, COUNT(keys), error)) return false;
        CalculationDefinition *value = &out->calculations[slot];
        if (slot == out->calculation_count) ++out->calculation_count;
        *value = (CalculationDefinition){0};
        strcpy(value->id, entry->key);
        const ScriptItem *scope = script_field(tree, entry, "scope"), *expression = script_field(tree, entry, "value");
        if (script_word(scope, "country")) {
            static const char *const expression_keys[] = { "native_value" };
            if (!expression || !script_keys(tree, expression, expression_keys, COUNT(expression_keys), error))
                return expression ? false : script_problem(tree, entry, error, "calculation requires value");
            if (!script_word(script_field(tree, expression, "native_value"), "manpower"))
                return script_problem(tree, expression, error, "the available native country value is manpower");
            continue;
        }
        if (!script_word(scope, "instance")) return script_problem(tree, entry, error, "expected country or instance scope");
        static const char *const expression_keys[] = { "count" }, *const count_keys[] = { "members", "value_id", "compare", "reference" };
        if (!expression) return script_problem(tree, entry, error, "calculation requires value");
        if (!script_keys(tree, expression, expression_keys, COUNT(expression_keys), error)) return false;
        const ScriptItem *count = script_field(tree, expression, "count");
        if (!count) return script_problem(tree, expression, error, "instance calculation requires count");
        if (!script_keys(tree, count, count_keys, COUNT(count_keys), error)) return false;
        if (!script_word(script_field(tree, count, "members"), "yes") ||
            !script_word(script_field(tree, count, "reference"), "leader"))
            return script_problem(tree, count, error, "member count requires members=yes and reference=leader");
        value->kind = VALUE_MEMBER_COUNT;
        value->comparison = PS_GT;
        if (!label(tree, script_field(tree, count, "value_id"), value->member_value, true, error) ||
            !compare_mode(tree, script_field(tree, count, "compare"), &value->comparison, error)) return false;
    }
    return true;
}

static bool check_resources(const ScriptTree *tree, const ScriptItem *item, const KhanDefinition *out, ScriptError *error) {
    for (unsigned i = 0; i < out->resource_count; ++i) {
        const ResourceDefinition *r = &out->resources[i];
        if (r->minimum > r->maximum || r->initial < r->minimum || r->initial > r->maximum ||
            resource_limit(r, r->initial) != r->initial)
            return script_problem(tree, item, error, "resource bounds and initial must agree with precision");
        static const int step[] = {1000, 100, 10, 1};
        if (r->minimum % step[r->precision] || r->maximum % step[r->precision])
            return script_problem(tree, item, error, "resource bounds must agree with precision");
        if ((!strcmp(r->id, PS_AUTHORITY) && r->owner != RESOURCE_INSTANCE) ||
            (r->owner != RESOURCE_INSTANCE && !r->loss_defined) ||
            (r->owner == RESOURCE_MEMBER && out->member_source == MEMBERS_UNSET) ||
            (r->owner == RESOURCE_LEADER && !out->leader_enabled))
            return script_problem(tree, item, error, "resource needs a valid owner and explicit country owner-loss rule");
        for (unsigned j = 0; j < r->threshold_count; ++j)
            if (r->thresholds[j].at < r->minimum || r->thresholds[j].at > r->maximum || r->thresholds[j].at % step[r->precision])
                return script_problem(tree, item, error, "threshold must be in bounds at the resource precision");
        for (unsigned j = 0; j < r->term_count; ++j) {
            const ResourceTerm *term = &r->terms[j];
            const bool needs_country = term->test == ENTRY_TAG || term->test == ENTRY_COUNTRY_FLAG || term->test == ENTRY_NATIVE ||
                                       term->political_test == PS_HAS_ROLE;
            if ((term->leader_scope && !out->leader_enabled) ||
                (!term->leader_scope && r->owner == RESOURCE_INSTANCE && needs_country))
                return script_problem(tree, item, error, "monthly country condition needs its country or political_leader scope");
            if (term->political_test == PS_CONTENT_REFORM) {
                const unsigned slot = political_action_index(out->actions, out->action_count, term->operand);
                if (term->leader_scope || slot == out->action_count || out->actions[slot].kind != ACTION_REFORM)
                    return script_problem(tree, item, error, "monthly reform condition requires a configured reform ID in instance scope");
            }
            if (term->political_test == PS_HAS_ROLE) {
                unsigned role = 0;
                while (role < out->role_count && strcmp(out->roles[role].id, term->operand)) ++role;
                if (role == out->role_count) return script_problem(tree, item, error, "monthly role condition refers to an unknown role");
            }
            if (term->test != ENTRY_QUERY) continue;
            const unsigned slot = calculation_find(out->calculations, out->calculation_count, term->calculation);
            if (slot == out->calculation_count || (r->owner == RESOURCE_INSTANCE && out->calculations[slot].kind != VALUE_MEMBER_COUNT))
                return script_problem(tree, item, error, "monthly term refers to an unknown or wrong-scope calculated value");
        }
    }
    for (unsigned i = 0; i < out->calculation_count; ++i) {
        const CalculationDefinition *value = &out->calculations[i];
        if (value->kind != VALUE_MEMBER_COUNT) continue;
        const unsigned operand = calculation_find(out->calculations, out->calculation_count, value->member_value);
        if (operand == out->calculation_count || out->calculations[operand].kind != VALUE_MANPOWER ||
            !out->leader_enabled || out->member_source == MEMBERS_UNSET)
            return script_problem(tree, item, error, "member calculation requires a country value, members and leader");
    }
    return true;
}

static bool read_action_links(const ScriptTree *tree, const ScriptItem *item, unsigned *used,
                              char links[][GK_SCRIPT_WORD_SIZE], ScriptError *error) {
    if (!item) return true;
    *used = 0;
    if (cleared(item)) return true;
    if (!item->block) return script_problem(tree, item, error, "expected a list of reform IDs");
    for (unsigned i = item->first; i; i = tree->items[i].next) {
        const ScriptItem *id = &tree->items[i];
        if (id->key[0] || !has_word(id) || !ps_identifier(id->word) || *used == PS_ACTION_LINKS)
            return script_problem(tree, id, error, "invalid or too many reform IDs");
        for (unsigned j = 0; j < *used; ++j)
            if (!strcmp(links[j], id->word)) return script_problem(tree, id, error, "duplicate reform ID");
        strcpy(links[(*used)++], id->word);
    }
    return true;
}

static bool action_condition(const ScriptTree *tree, const ScriptItem *item, ResourceTerm *out,
                             NativeClause *native, ScriptError *error) {
    if (!item) return true;
    *out = (ResourceTerm){0};
    *native = (NativeClause){0};
    if (cleared(item)) return true;
    ScriptError legacy_error = {0};
    if (read_growth_trigger(tree, item, out, false, &legacy_error) && out->test != ENTRY_QUERY && !out->leader_scope)
        return true;
    *out = (ResourceTerm){0};
    return read_native_clause(tree, item, native, error);
}

static bool patch_action_cost(const ScriptTree *tree, const ScriptItem *item, PoliticalAction *out, ScriptError *error) {
    if (!item) return true;
    if (cleared(item)) { out->resource[0] = 0; out->cost = 0; return true; }
    static const char *const keys[] = { "resource", "payer", "amount" };
    if (!script_keys(tree, item, keys, COUNT(keys), error)) return false;
    const ScriptItem *resource = F("resource"), *payer = F("payer"), *amount = F("amount");
    if (!resource || !payer || !amount) return script_problem(tree, item, error, "cost requires resource, payer and amount");
    if (script_word(payer, "instance")) out->payer = ACTION_INSTANCE_PAYS;
    else if (script_word(payer, "actor")) out->payer = ACTION_ACTOR_PAYS;
    else if (script_word(payer, "target")) out->payer = ACTION_TARGET_PAYS;
    else return script_problem(tree, payer, error, "expected instance, actor or target payer");
    if (!has_word(resource) || !ps_identifier(resource->word))
        return script_problem(tree, resource, error, "invalid cost resource ID");
    strcpy(out->resource, resource->word);
    if (!fixed(tree, amount, &out->cost, error)) return false;
    return out->cost >= 0 || script_problem(tree, amount, error, "action cost must be nonnegative");
}

static bool patch_action_effect(const ScriptTree *tree, const ScriptItem *effect, int32_t *prestige,
                                NativeClause *native, ScriptError *error) {
    if (!effect) return true;
    *prestige = 0;
    *native = (NativeClause){0};
    if (cleared(effect)) return true;
    if (!effect->block) return script_problem(tree, effect, error, "expected an effect block");
    if (!effect->first) return true;
    const ScriptItem *value = script_field(tree, effect, "add_prestige");
    if (!value || value != &tree->items[effect->first] || value->next)
        return read_native_clause(tree, effect, native, error);
    ScriptError legacy_error = {0};
    int32_t amount;
    if (!fixed(tree, value, &amount, &legacy_error) || amount < -100000 || amount > 100000)
        return read_native_clause(tree, effect, native, error);
    *prestige = amount;
    return true;
}

static bool patch_incident_options(const ScriptTree *tree, const ScriptItem *list, PoliticalAction *out, ScriptError *error) {
    if (!list) return true;
    if (cleared(list)) { out->option_count = 0; return true; }
    if (!list->block) return script_problem(tree, list, error, "expected named incident options");
    for (unsigned i = list->first; i; i = tree->items[i].next) {
        const ScriptItem *item = &tree->items[i];
        if (!unique_named(tree, item, list, error)) return false;
        unsigned slot = incident_option_index(out, item->key);
        if (cleared(item)) {
            if (slot < out->option_count) {
                --out->option_count;
                memmove(out->options + slot, out->options + slot + 1, (out->option_count - slot) * sizeof(*out->options));
            }
            continue;
        }
        if (slot == PS_MAX_INCIDENT_OPTIONS) return script_problem(tree, item, error, "too many incident options");
        if (slot == out->option_count) {
            ++out->option_count;
            out->options[slot] = (IncidentOption){0};
            strcpy(out->options[slot].id, item->key);
        }
        IncidentOption *option = &out->options[slot];
        static const char *const keys[] = { "name", "tooltip", "trigger", "effect" };
        const ScriptItem *effect = F("effect");
        if (!script_keys(tree, item, keys, COUNT(keys), error) ||
            !label(tree, F("name"), option->name, true, error) || !label(tree, F("tooltip"), option->tooltip, true, error) ||
            !read_native_clause(tree, F("trigger"), &option->trigger, error) ||
            !patch_action_effect(tree, effect, &option->prestige, &option->effect, error)) return false;
    }
    return true;
}

static bool patch_process_type(const ScriptTree *tree, const ScriptItem *item, PoliticalAction *out, ScriptError *error) {
    const ScriptItem *response = F("response"), *timeout = F("timeout_days"), *effect = F("effect"), *on_effect = F("on_effect");
    if (on_effect && (effect || out->kind != ACTION_REFORM))
        return script_problem(tree, on_effect, error, "on_effect is the reform alternative to effect");
    const ScriptItem *participants = F("participants"), *resolution = F("resolution"), *options = F("options");
    if (out->kind != ACTION_INVITATION && (response || timeout))
        return script_problem(tree, item, error, "response and timeout_days are diplomatic action fields");
    if (out->kind != ACTION_INCIDENT && (participants || resolution || options))
        return script_problem(tree, item, error, "participants, resolution and options are incident fields");
    if (out->kind == ACTION_INVITATION) {
        if (response && !script_word(response, "request")) return script_problem(tree, response, error, "expected response = request");
        if (response) out->request_response = true;
        int days;
        if (timeout && !script_number(timeout, 1, 36500, &days))
            return script_problem(tree, timeout, error, "timeout_days must be 1..36500");
        if (timeout) out->duration = (unsigned)days;
        if (effect) {
            const ScriptItem *join = script_field(tree, effect, "political_add_member");
            const ScriptItem *jointag = join ? script_field(tree, join, "country") : NULL;
            const bool legacy = join && script_word(jointag, "TARGET");
            if (legacy) {
                static const char *const keys[] = { "political_add_member" }, *const target_keys[] = { "instance", "country" };
                if (!script_keys(tree, effect, keys, COUNT(keys), error) ||
                    !script_keys(tree, join, target_keys, COUNT(target_keys), error)) return false;
                if (!script_word(script_field(tree, join, "instance"), "INSTANCE"))
                    return script_problem(tree, join, error, "legacy invitation shorthand uses instance = INSTANCE");
                out->effect = (NativeClause){0};
            } else if (!read_native_clause(tree, effect, &out->effect, error)) return false;
            out->member_invitation = true;
        }
        return true;
    }
    if (out->kind == ACTION_INCIDENT) {
        if (participants && !script_word(participants, "members"))
            return script_problem(tree, participants, error, "expected participants = members");
        if (resolution && !script_word(resolution, "leader_choice"))
            return script_problem(tree, resolution, error, "expected resolution = leader_choice");
        if (participants) out->member_participants = true;
        if (resolution) out->leader_choice = true;
        return read_native_clause(tree, effect, &out->effect, error) && patch_incident_options(tree, options, out, error);
    }
    if (on_effect) {
        out->prestige = 0;
        return read_native_clause(tree, on_effect, &out->effect, error);
    }
    return patch_action_effect(tree, effect, &out->prestige, &out->effect, error);
}

static bool patch_action(const ScriptTree *tree, const ScriptItem *item, PoliticalAction *out, ScriptError *error) {
    static const char *const keys[] = { "name", "tooltip", "actor_rule", "actor_trigger", "target", "target_trigger",
        "beneficiaries", "cost", "effect", "cooldown_days", "requires", "excludes", "repeatable", "term",
        "duration_days", "exclusive_group", "ui", "actor_permission", "condition_tooltip",
        "response", "timeout_days", "participants", "resolution", "options", "potential", "trigger",
        "on_effect", "off_effect", "removed_effect", "can_stop", "modifier", "emperor", "member", "province", "duration",
        "ai_will_do" };
    if (!script_keys(tree, item, keys, COUNT(keys), error)) return false;
    const ScriptItem *actor = F("actor_rule"), *target = F("target"), *benefit = F("beneficiaries");
    if (actor) {
        if (script_word(actor, "any")) out->actor_rule = ACTION_ANY;
        else if (script_word(actor, "member")) out->actor_rule = ACTION_MEMBER;
        else if (script_word(actor, "leader")) out->actor_rule = ACTION_LEADER;
        else if (script_word(actor, "permission")) out->actor_rule = ACTION_PERMISSION;
        else return script_problem(tree, actor, error, "expected any, leader, member or permission actor_rule");
    }
    if (target && !script_word(target, "none") && !script_word(target, "country"))
        return script_problem(tree, target, error, "expected none or country target");
    if (target) out->country_target = script_word(target, "country");
    if (benefit) {
        if (out->kind >= ACTION_INVITATION) return script_problem(tree, benefit, error, "a process uses its configured result instead of continuous beneficiaries");
        if (script_word(benefit, "actor")) out->beneficiaries = ACTION_BENEFIT_ACTOR;
        else if (script_word(benefit, "leader")) out->beneficiaries = ACTION_BENEFIT_LEADER;
        else if (script_word(benefit, "members")) out->beneficiaries = ACTION_BENEFIT_MEMBERS;
        else if (script_word(benefit, "target")) out->beneficiaries = ACTION_BENEFIT_TARGET;
        else return script_problem(tree, benefit, error, "expected actor, leader, members or target beneficiaries");
    }
    const ScriptItem *duration = F("duration_days"), *cooldown = F("cooldown_days"), *term = F("term");
    if (F("duration")) {
        if (duration || out->kind != ACTION_DECREE)
            return script_problem(tree, F("duration"), error, "duration is the decree alternative to duration_days");
        duration = F("duration");
    }
    int days;
    if (cooldown) {
        if (!script_number(cooldown, 0, 36500, &days)) return script_problem(tree, cooldown, error, "cooldown_days must be 0..36500");
        out->cooldown = (unsigned)days;
    }
    if (duration) {
        if (!script_number(duration, 1, 36500, &days)) return script_problem(tree, duration, error, "duration_days must be 1..36500");
        out->duration = (unsigned)days;
    }
    if (term && !script_word(term, "fixed")) return script_problem(tree, term, error, "only fixed decrees are available here");
    if ((F("requires") || F("excludes")) && out->kind != ACTION_REFORM)
        return script_problem(tree, item, error, "requires and excludes are reform fields");
    if (F("repeatable") && out->kind != ACTION_DECISION)
        return script_problem(tree, item, error, "repeatable is a decision field");
    if ((term || F("exclusive_group")) && out->kind != ACTION_DECREE)
        return script_problem(tree, item, error, "term and exclusive_group are decree fields");
    if (duration && out->kind != ACTION_DECREE && out->kind != ACTION_INCIDENT)
        return script_problem(tree, duration, error, "duration_days is available for a decree or incident");
    if (cooldown && out->kind >= ACTION_INVITATION)
        return script_problem(tree, cooldown, error, "this process has a deadline instead of an action cooldown");
    if (F("off_effect") && out->kind != ACTION_REFORM)
        return script_problem(tree, F("off_effect"), error, "off_effect is a reform field");
    if (F("removed_effect") && out->kind != ACTION_DECREE)
        return script_problem(tree, F("removed_effect"), error, "removed_effect is a decree field");
    if (F("can_stop") && out->kind != ACTION_INCIDENT)
        return script_problem(tree, F("can_stop"), error, "can_stop is an incident field");
    if (F("ai_will_do") && out->kind == ACTION_INVITATION)
        return script_problem(tree, F("ai_will_do"), error, "an invitation uses the AI accept rule");
    static const char *const modifier_keys[] = {"modifier", "emperor", "member", "province"};
    for (unsigned i = 0; i < COUNT(modifier_keys); ++i) {
        const ScriptItem *modifier = F(modifier_keys[i]);
        if (!modifier) continue;
        if (out->kind != ACTION_REFORM && out->kind != ACTION_DECREE)
            return script_problem(tree, modifier, error, "continuous modifiers require a reform or decree");
        if (!read_native_clause(tree, modifier, &out->modifiers[i], error)) return false;
    }
    if (!patch_process_type(tree, item, out, error)) return false;
    const ScriptItem *ui = F("ui");
    if (ui) {
        if (cleared(ui) || script_word(ui, "no")) out->shown = false;
        else {
            static const char *const keys[] = { "region" };
            if (!script_keys(tree, ui, keys, COUNT(keys), error)) return false;
            if (!script_word(script_field(tree, ui, "region"), "actions_region"))
                return script_problem(tree, ui, error, "this build binds actions_region");
            out->shown = true;
        }
    }
    return label(tree, F("name"), out->name, true, error) && label(tree, F("tooltip"), out->tooltip, true, error) &&
           label(tree, F("actor_permission"), out->permission, true, error) &&
           label(tree, F("condition_tooltip"), out->condition_tooltip, true, error) &&
           label(tree, F("exclusive_group"), out->exclusive_group, true, error) &&
           switch_value(tree, F("repeatable"), &out->repeatable, error) &&
           action_condition(tree, F("actor_trigger"), &out->actor_trigger, &out->native_actor, error) &&
           action_condition(tree, F("target_trigger"), &out->target_trigger, &out->native_target, error) &&
           read_native_clause(tree, F("potential"), &out->potential, error) &&
           read_native_clause(tree, F("trigger"), &out->trigger, error) &&
           read_native_clause(tree, F("can_stop"), &out->can_stop, error) &&
           read_native_clause(tree, F("ai_will_do"), &out->ai_rule, error) &&
           read_native_clause(tree, F("off_effect") ? F("off_effect") : F("removed_effect"), &out->off_effect, error) &&
           read_action_links(tree, F("requires"), &out->require_count, out->requires, error) &&
           read_action_links(tree, F("excludes"), &out->exclude_count, out->excludes, error) &&
           patch_action_cost(tree, F("cost"), out, error);
}

static bool patch_actions(const ScriptTree *tree, const ScriptItem *list, ActionKind kind, KhanDefinition *out, ScriptError *error) {
    if (!list) return true;
    if (cleared(list) || script_word(list, "no")) {
        unsigned kept = 0;
        for (unsigned i = 0; i < out->action_count; ++i)
            if (out->actions[i].kind != kind) out->actions[kept++] = out->actions[i];
        out->action_count = kept;
        return true;
    }
    if (!list->block) return script_problem(tree, list, error, "expected named political actions");
    for (unsigned i = list->first; i; i = tree->items[i].next) {
        const ScriptItem *entry = &tree->items[i];
        if (!unique_named(tree, entry, list, error)) return false;
        const unsigned slot = political_action_index(out->actions, out->action_count, entry->key);
        if (slot < out->action_count && out->actions[slot].kind != kind)
            return script_problem(tree, entry, error, "action ID already belongs to another action module");
        if (cleared(entry) || script_word(entry, "no")) {
            if (slot < out->action_count) {
                --out->action_count;
                memmove(out->actions + slot, out->actions + slot + 1, (out->action_count - slot) * sizeof(*out->actions));
            }
            continue;
        }
        if (slot == PS_MAX_ACTIONS) return script_problem(tree, entry, error, "too many political actions");
        PoliticalAction *action = &out->actions[slot];
        if (slot == out->action_count) {
            ++out->action_count;
            *action = (PoliticalAction){ .kind = kind, .repeatable = true };
            strcpy(action->id, entry->key);
        }
        if (!patch_action(tree, entry, action, error)) return false;
    }
    return true;
}

static bool patch_action_ai(const ScriptTree *tree, const ScriptItem *list, KhanDefinition *out, ScriptError *error) {
    if (!list) return true;
    if (cleared(list) || script_word(list, "no")) {
        for (unsigned i = 0; i < out->action_count; ++i) {
            out->actions[i].ai_weight = 0;
            out->actions[i].ai_accept = false;
            out->actions[i].ai_rule = (NativeClause){0};
        }
        return true;
    }
    if (!list->block) return script_problem(tree, list, error, "expected named AI action rules");
    for (unsigned i = list->first; i; i = tree->items[i].next) {
        const ScriptItem *item = &tree->items[i];
        if (!unique_named(tree, item, list, error)) return false;
        const unsigned slot = political_action_index(out->actions, out->action_count, item->key);
        if (slot == out->action_count) return script_problem(tree, item, error, "AI rule refers to an unknown action");
        PoliticalAction *action = &out->actions[slot];
        if (cleared(item) || script_word(item, "no")) {
            action->ai_weight = 0;
            action->ai_rule = (NativeClause){0};
            action->ai_accept = false;
            continue;
        }
        static const char *const keys[] = { "weight", "accept" }, *const weight_keys[] = { "value" };
        if (!script_keys(tree, item, keys, COUNT(keys), error)) return false;
        const ScriptItem *weight = F("weight"), *accept = F("accept");
        if (!weight && !accept) return script_problem(tree, item, error, "AI rule needs weight or accept");
        if (weight) {
            if (!script_keys(tree, weight, weight_keys, COUNT(weight_keys), error) ||
                !fixed(tree, script_field(tree, weight, "value"), &action->ai_weight, error)) return false;
            action->ai_rule = (NativeClause){0};
        }
        if (accept) {
            int decision;
            if (action->kind != ACTION_INVITATION || !script_keys(tree, accept, weight_keys, COUNT(weight_keys), error) ||
                !script_number(script_field(tree, accept, "value"), 0, 1, &decision))
                return script_problem(tree, accept, error, "invitation AI accept needs value = 0 or 1");
            action->ai_accept = decision == 1;
        }
    }
    return true;
}

static bool permission_exists(const KhanDefinition *type, const char *id) {
    if (grants_permission(&type->succession.permissions, id)) return true;
    for (unsigned i = 0; i < type->role_count; ++i)
        if (grants_permission(&type->roles[i].permissions, id)) return true;
    return false;
}

static bool check_actions(const ScriptTree *tree, const ScriptItem *item, const KhanDefinition *type, ScriptError *error) {
    uint32_t requires[PS_MAX_ACTIONS] = {0};
    for (unsigned i = 0; i < type->action_count; ++i) {
        const PoliticalAction *a = &type->actions[i];
        if (a->actor_rule == ACTION_PERMISSION && (!a->permission[0] || !permission_exists(type, a->permission)))
            return script_problem(tree, item, error, "actor_permission has no named leader or office grant");
        if ((a->kind == ACTION_DECREE && !a->duration) || (!a->country_target && (a->target_trigger.test ||
            native_clause_present(&a->native_target) ||
            a->beneficiaries == ACTION_BENEFIT_TARGET || a->payer == ACTION_TARGET_PAYS)) ||
            (a->country_target && a->shown && a->kind != ACTION_INVITATION) ||
            ((a->ai_weight > 0 || native_clause_present(&a->ai_rule)) &&
             (a->actor_rule != ACTION_LEADER || a->country_target)))
            return script_problem(tree, item, error, "action needs a valid duration or target; UI and AI currently use targetless actions");
        if (a->kind == ACTION_INVITATION && (!a->country_target || !a->request_response || !a->duration || !a->member_invitation ||
            (type->member_source != MEMBERS_EXPLICIT && type->member_source != MEMBERS_MIXED)))
            return script_problem(tree, item, error, "invitation requires a country, a timeout, a reply and editable membership");
        if (a->kind == ACTION_INCIDENT && (!a->duration || a->country_target || !a->member_participants || !a->leader_choice ||
            !a->option_count || !type->leader_enabled || type->member_source == MEMBERS_UNSET))
            return script_problem(tree, item, error, "incident requires a duration, members, a leader and selectable options");
        if ((!type->leader_enabled && native_clause_present(&a->modifiers[ACTION_MODIFIER_LEADER])) ||
            (type->member_source == MEMBERS_UNSET && native_clause_present(&a->modifiers[ACTION_MODIFIER_MEMBER])) ||
            (!type->territory_enabled && native_clause_present(&a->modifiers[ACTION_MODIFIER_PROVINCE])))
            return script_problem(tree, item, error, "modifier target needs the leader, members or territory module");
        if (((a->actor_rule == ACTION_LEADER || a->beneficiaries == ACTION_BENEFIT_LEADER) && !type->leader_enabled) ||
            ((a->actor_rule == ACTION_MEMBER || a->beneficiaries == ACTION_BENEFIT_MEMBERS) && type->member_source == MEMBERS_UNSET))
            return script_problem(tree, item, error, "action refers to a disabled leader or member module");
        if (a->resource[0]) {
            const unsigned r = resource_find(type->resources, type->resource_count, a->resource);
            if (r == type->resource_count || (type->resources[r].owner == RESOURCE_INSTANCE) != (a->payer == ACTION_INSTANCE_PAYS))
                return script_problem(tree, item, error, "action cost requires a resource with the matching owner");
            const ResourceDefinition *def = &type->resources[r];
            static const int step[] = {1000, 100, 10, 1};
            if (a->cost % step[def->precision]) return script_problem(tree, item, error, "action cost must match the resource precision");
        }
        for (unsigned j = 0; j < a->require_count + a->exclude_count; ++j) {
            const bool needed = j < a->require_count;
            const char *id = needed ? a->requires[j] : a->excludes[j - a->require_count];
            const unsigned s = political_action_index(type->actions, type->action_count, id);
            if (s == i || s == type->action_count || type->actions[s].kind != ACTION_REFORM)
                return script_problem(tree, item, error, "action links must refer to another enabled reform");
            if (needed) requires[i] |= 1u << s;
            else if (requires[i] & (1u << s)) return script_problem(tree, item, error, "reform both requires and excludes the same action");
        }
    }
    for (unsigned j = 0; j < type->action_count; ++j)
        for (unsigned i = 0; i < type->action_count; ++i)
            if (requires[i] & (1u << j)) requires[i] |= requires[j];
    for (unsigned i = 0; i < type->action_count; ++i)
        if (requires[i] & (1u << i)) return script_problem(tree, item, error, "cyclic reform dependency");
    return true;
}

static bool has_country_calculation(const KhanDefinition *type, const char *id) {
    const unsigned index = calculation_find(type->calculations, type->calculation_count, id);
    return index < type->calculation_count && type->calculations[index].kind == VALUE_MANPOWER;
}

static bool check_contest(const ScriptTree *tree, const ScriptItem *item, const KhanDefinition *type,
                          const PoliticalContestRule *rule, ScriptError *error) {
    if (!rule->period || !rule->has_tie_rule || (strcmp(rule->score, "invested") && !has_country_calculation(type, rule->score)))
        return script_problem(tree, item, error, "competition requires duration, score and tie_break");
    if (!strcmp(rule->score, "invested") && (!rule->investment[0] || !rule->cost))
        return script_problem(tree, item, error, "score=invested requires a positive investment");
    if (rule->investment[0]) {
        const unsigned index = resource_find(type->resources, type->resource_count, rule->investment);
        if (index == type->resource_count || type->resources[index].owner == RESOURCE_INSTANCE)
            return script_problem(tree, item, error, "investment needs a country resource for the candidate");
        static const int step[] = {1000,100,10,1};
        if (rule->cost % step[type->resources[index].precision]) return script_problem(tree, item, error, "investment must match resource precision");
    }
    return true;
}

static bool check_selections(const ScriptTree *tree, const ScriptItem *item, const KhanDefinition *type, ScriptError *error) {
    for (unsigned i = 0; i < type->role_count; ++i) {
        const PoliticalRole *role = &type->roles[i];
        if (role->acquisition == OFFICE_COMPETITION) {
            if (role->max_holders != 1) return script_problem(tree, item, error, "a competition fills one office seat");
            if (!check_contest(tree, item, type, &role->contest, error)) return false;
        }
    }
    const LeaderSelection *s = &type->succession;
    if (s->require_member && type->member_source == MEMBERS_UNSET)
        return script_problem(tree, item, error, "leader requires an enabled membership module");
    if (!s->defined) return true;
    if (s->nomination.defined && s->mode != LEADER_ELECTED)
        return script_problem(tree, item, error, "nomination requires elected succession");
    if (s->event != SELECT_EXPLICIT && s->mode != LEADER_ELECTED && s->mode != LEADER_HIGHEST)
        return script_problem(tree, item, error, "automatic succession requires elected or highest_value");
    if (s->mode == LEADER_COMPETITION) return check_contest(tree, item, type, &s->contest, error);
    if (s->mode == LEADER_HIGHEST && (type->member_source == MEMBERS_UNSET || !s->contest.has_tie_rule || !has_country_calculation(type, s->value)))
        return script_problem(tree, item, error, "highest_value requires members, a country calculation and tie_break");
    if (s->mode == LEADER_ELECTED) {
        bool electors = false;
        for (unsigned i = 0; i < type->role_count; ++i)
            if (!strcmp(type->roles[i].id, s->elector_role) && type->roles[i].require_member) electors = true;
        if (!electors || !s->contest.has_tie_rule || !s->has_cooldown_policy)
            return script_problem(tree, item, error, "elected requires a member elector role, tie_break and cooldown_on_turnover");
    }
    if (s->event == SELECT_THRESHOLD) {
        const unsigned i = resource_find(type->resources, type->resource_count, s->threshold_resource);
        if (i == type->resource_count || type->resources[i].owner != RESOURCE_INSTANCE)
            return script_problem(tree, item, error, "threshold succession requires an instance resource");
        const ResourceDefinition *r = &type->resources[i];
        for (unsigned j = 0; j < r->threshold_count; ++j)
            if (!strcmp(r->thresholds[j].id, s->threshold)) return true;
        return script_problem(tree, item, error, "threshold succession refers to an unknown threshold ID");
    }
    return true;
}

static bool patch_message(const ScriptTree *tree, const ScriptItem *item, PoliticalMessageDefinition *message,
                          ScriptError *error) {
    static const char *const keys[] = {"title", "subtitle", "body", "log", "popup", "options"};
    if (!script_keys(tree, item, keys, COUNT(keys), error)) return false;
    if (!label(tree, F("title"), message->title, true, error) ||
        !label(tree, F("subtitle"), message->subtitle, true, error) ||
        !label(tree, F("log"), message->log, true, error) ||
        !switch_value(tree, F("popup"), &message->popup, error)) return false;
    const ScriptItem *body = F("body");
    if (body) {
        memset(message->body, 0, sizeof(message->body));
        if (body->block) {
            unsigned line = 0;
            for (const ScriptItem *entry = script_item(tree, body->first); entry; entry = script_item(tree, entry->next)) {
                if (line == PS_MESSAGE_BODY_LINES)
                    return script_problem(tree, entry, error, "native message body accepts up to four entries");
                if (entry->key[0] || !ps_identifier(entry->word) || cleared(entry))
                    return script_problem(tree, entry, error, "expected a localisation key in the message body");
                strcpy(message->body[line++], entry->word);
            }
        } else if (!label(tree, body, message->body[0], true, error)) return false;
    }
    const ScriptItem *options = F("options");
    if (cleared(options)) {
        message->close[0] = message->secondary[0] = 0;
    } else if (options) {
        static const char *const option_keys[] = {"close", "secondary"};
        if (!script_keys(tree, options, option_keys, COUNT(option_keys), error) ||
            !label(tree, script_field(tree, options, "close"), message->close, true, error) ||
            !label(tree, script_field(tree, options, "secondary"), message->secondary, true, error)) return false;
    }
    if (!message->title[0] || !message->body[0][0])
        return script_problem(tree, item, error, "message requires a title and at least one body localisation key");
    strcpy(message->path, tree->path);
    message->line = item->line;
    return true;
}

static bool patch_messages(const ScriptTree *tree, const ScriptItem *list, KhanDefinition *type, ScriptError *error) {
    if (!list) return true;
    if (cleared(list) || script_word(list, "no")) {
        type->message_count = 0;
        return true;
    }
    if (!list->block) return script_problem(tree, list, error, "expected a messages block");
    for (const ScriptItem *item = script_item(tree, list->first); item; item = script_item(tree, item->next)) {
        if (!ps_identifier(item->key)) return script_problem(tree, item, error, "expected a message identifier");
        for (const ScriptItem *prev = script_item(tree, list->first); prev != item; prev = script_item(tree, prev->next))
            if (!strcmp(prev->key, item->key)) return script_problem(tree, item, error, "duplicate message");
        unsigned index = 0;
        while (index < type->message_count && strcmp(type->messages[index].id, item->key)) ++index;
        if (cleared(item) || script_word(item, "no")) {
            if (index == type->message_count) continue;
            --type->message_count;
            memmove(&type->messages[index], &type->messages[index + 1],
                    (type->message_count - index) * sizeof(type->messages[0]));
            continue;
        }
        if (index == PS_MAX_MESSAGES) return script_problem(tree, item, error, "too many messages for one political type");
        PoliticalMessageDefinition *message = &type->messages[index];
        if (index == type->message_count) {
            *message = (PoliticalMessageDefinition){.popup = true};
            strcpy(message->id, item->key);
            ++type->message_count;
        }
        if (!patch_message(tree, item, message, error)) return false;
    }
    return true;
}

static bool patch_type(const ScriptTree *tree, const ScriptItem *item, KhanDefinition *out, ScriptError *error) {
    static const char *const keys[] = {
        "copy_from", "abstract", "title", "interface", "members", "leader", "existence", "authority", "resources",
        "territory", "special_members", "calculated_values", "reforms", "decisions", "decrees", "ai",
        "incidents", "diplomatic_actions", "leader_election", "rules", "military_obligations", "modifiers", "on_actions", "messages"
    };
    if (!script_keys(tree, item, keys, COUNT(keys), error)) return false;
    out->abstract = !strcmp(item->key, "default");
    if (!switch_value(tree, F("abstract"), &out->abstract, error) ||
        !label(tree, F("title"), out->title, false, error) ||
        !patch_interface(tree, F("interface"), &out->entry, error) ||
        !patch_members(tree, F("members"), out, error) ||
        !patch_leader(tree, F("leader"), out, error) ||
        !patch_existence(tree, F("existence"), out, error) ||
        !patch_war_rules(tree, F("rules"), out, error) ||
        !patch_war_obligations(tree, F("military_obligations"), out, error) ||
        !patch_resources(tree, item, out, error) ||
        !patch_calculations(tree, F("calculated_values"), out, error) ||
        !patch_territory(tree, F("territory"), out, error) ||
        !patch_roles(tree, F("special_members"), out, error) ||
        !patch_actions(tree, F("reforms"), ACTION_REFORM, out, error) ||
        !patch_actions(tree, F("decisions"), ACTION_DECISION, out, error) ||
        !patch_actions(tree, F("decrees"), ACTION_DECREE, out, error) ||
        !patch_actions(tree, F("diplomatic_actions"), ACTION_INVITATION, out, error) ||
        !patch_actions(tree, F("incidents"), ACTION_INCIDENT, out, error) ||
        !patch_messages(tree, F("messages"), out, error) ||
        !patch_action_ai(tree, F("ai"), out, error)) return false;
    if (!out->abstract && !strcmp(item->key, "default"))
        return script_problem(tree, F("abstract"), error, "default must be abstract");
    static const char *const unsupported[] = {"leader_election", "modifiers", "on_actions"};
    for (size_t i = 0; i < COUNT(unsupported); ++i) {
        const ScriptItem *option = script_field(tree, item, unsupported[i]);
        if (option && !script_word(option, "no") && !cleared(option) && !(option->block && !option->first))
            return script_problem(tree, option, error, "module is not available in this build");
    }
    if (out->abstract) return true;
    if (!check_resources(tree, item, out, error) || !check_selections(tree, item, out, error) || !check_actions(tree, item, out, error)) return false;
    if (out->entry.defined && (!out->entry.layout[0] || !out->entry.region[0] || !out->entry.icon[0]))
        return script_problem(tree, item, error, "enabled entry requires interface.layout, region and icon");
    if ((out->entry.background[0] && !out->entry.layout[0]) ||
        (out->member_source == MEMBERS_DIRECT_SUBJECTS && !out->leader_enabled) ||
        (out->member_source == MEMBERS_CAPITAL_TERRITORY && !out->territory_enabled) ||
        (out->member_source != MEMBERS_DIRECT_SUBJECTS && out->subject_type[0]))
        return script_problem(tree, item, error, "referenced module is disabled or incompatible (layout, leader or members)");
    if ((out->member_source == MEMBERS_QUERY || out->member_source == MEMBERS_MIXED) && !out->member_query_defined)
        return script_problem(tree, F("members"), error, "query and mixed require source_trigger, including an explicit empty block for all countries");
    if (out->max_memberships && out->member_source != MEMBERS_EXPLICIT)
        return script_problem(tree, F("members"), error, "nonzero max_per_country requires explicit members");
    if ((out->block_direct_war || out->defensive_obligation) && out->member_source == MEMBERS_UNSET)
        return script_problem(tree, item, error, "war rules require a member source");
    for (unsigned i = 0; i < out->role_count; ++i)
        if (!out->leader_enabled || (out->roles[i].require_member && out->member_source == MEMBERS_UNSET))
            return script_problem(tree, F("special_members"), error, "appointed roles require a leader and any referenced member module");
    return true;
}

static const KhanDefinition *resolve_type(ContentTypes *types, TypeDeclaration *type, unsigned depth, ScriptError *error) {
    if (type->status == TYPE_RESOLVED) return type->effective;
    if (type->status == TYPE_INVALID) {
        *error = type->error;
        return NULL;
    }
    const ScriptTree *tree = type->tree;
    const ScriptItem *item = type->item;
    const ScriptItem *copy = F("copy_from");
    if (type->status == TYPE_VISITING || depth >= 64) {
        script_problem(tree, copy ? copy : item, error, "copy_from cycle or inheritance depth limit");
        return NULL;
    }
    type->status = TYPE_VISITING;
    if (copy && (script_item(tree, item->first) != copy || !has_word(copy) ||
                 !ps_identifier(copy->word) || !strcmp(item->key, "default"))) {
        script_problem(tree, copy, error, "copy_from must be the first assignment of a derived type");
        goto failed;
    }
    TypeDeclaration *parent = copy ? find_type(types, copy->word) :
        (strcmp(item->key, "default") ? find_type(types, "default") : NULL);
    if (copy && !parent) {
        script_problem(tree, copy, error, "missing copy_from parent");
        goto failed;
    }
    const KhanDefinition *inherited = parent ? resolve_type(types, parent, depth + 1, error) : NULL;
    if (parent && !inherited) goto failed;
    type->effective = calloc(1, sizeof(*type->effective));
    if (!type->effective) {
        script_problem(tree, item, error, "out of memory resolving type");
        goto failed;
    }
    KhanDefinition *out = type->effective;
    if (inherited) {
        *out = *inherited;
    } else {
        out->script_type = out->history_allowed = true;
        out->max_instances = 1;
        out->member_source = MEMBERS_EXPLICIT;
        out->entry.toggle = out->entry.close_button = out->entry.close_on_escape = true;
    }
    if (!patch_type(tree, item, out, error)) goto failed;
    strcpy(out->source_path, tree->path);
    out->source_line = item->line;
    type->status = TYPE_RESOLVED;
    return out;
failed:
    type->status = TYPE_INVALID;
    type->error = *error;
    return NULL;
}

const KhanDefinition *resolve_content_type(ContentTypes *types, const char *id, ScriptError *error) {
    TypeDeclaration *type = find_type(types, id);
    if (!type) return NULL;
    *error = (ScriptError){0};
    return resolve_type(types, type, 0, error);
}

void validate_unused_types(ContentTypes *types, void (*report)(const ScriptError *)) {
    for (TypeDeclaration *type = types->items; type; type = type->next) {
        if (type->status != TYPE_UNRESOLVED) continue;
        ScriptError error = {0};
        if (!resolve_type(types, type, 0, &error)) report(&error);
    }
}

void visit_concrete_types(const ContentTypes *types, void (*visit)(const char *, void *), void *context) {
    for (const TypeDeclaration *type = types->items; type; type = type->next)
        if (type->effective && !type->effective->abstract)
            visit(type->item->key, context);
}

static bool history_tags(const ScriptTree *tree, const ScriptItem *item, MemberTags *tags, ScriptError *error) {
    if (!item) return true;
    if (!item->block) return script_problem(tree, item, error, "members requires a list of country tags");
    for (unsigned i = item->first; i; i = tree->items[i].next) {
        const ScriptItem *tag = &tree->items[i];
        if (tag->key[0] || strlen(tag->word) != 3 || strspn(tag->word, "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") != 3)
            return script_problem(tree, tag, error, "expected a three-character country tag");
        unsigned packed = 0;
        memcpy(&packed, tag->word, 3);
        for (unsigned prior = 0; prior < tags->count; ++prior)
            if (tags->tags[prior] == packed) return script_problem(tree, tag, error, "duplicate member tag");
        if (tags->count == PS_MAX_EXPLICIT_MEMBERS)
            return script_problem(tree, tag, error, "explicit member capacity exceeded");
        tags->tags[tags->count++] = packed;
    }
    return true;
}

static bool read_territory(const ScriptTree *tree, const ScriptItem *list, ProvinceList *out, ScriptError *error) {
    if (!list) return true;
    if (!list->block) return script_problem(tree, list, error, "expected a list of province IDs");
    for (unsigned i = list->first; i; i = tree->items[i].next) {
        const ScriptItem *value = &tree->items[i];
        int id;
        if (value->key[0] || !script_number(value, 1, PS_MAX_PROVINCE_ID, &id))
            return script_problem(tree, value, error, "expected a positive province ID");
        if (out->count == PS_MAX_TERRITORY) return script_problem(tree, value, error, "explicit territory capacity exceeded");
        for (unsigned j = 0; j < out->count; ++j)
            if (out->ids[j] == (unsigned)id) return script_problem(tree, value, error, "duplicate territory province");
        out->ids[out->count++] = (unsigned)id;
    }
    return true;
}

static unsigned history_tag(const char *tag) {
    if (strlen(tag) != 3 || strspn(tag, "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") != 3) return 0;
    return election_tag(tag);
}

static bool history_leader(const ScriptTree *tree, const ScriptItem *item, unsigned *leader, ScriptError *error) {
    *leader = 0;
    if (!item || script_word(item, "none")) return true;
    if (!has_word(item) || !(*leader = history_tag(item->word)))
        return script_problem(tree, item, error, "leader requires a country tag or none");
    return true;
}

static bool history_balances(const ScriptTree *tree, const ScriptItem *list, ResourceInitial *out,
                             unsigned *count, ScriptError *error) {
    if (!list) return true;
    if (!list->block) return script_problem(tree, list, error, "expected historical resource balances");
    for (unsigned i = list->first; i; i = tree->items[i].next) {
        const ScriptItem *money = &tree->items[i];
        if (*count == PS_MAX_RESOURCES) return script_problem(tree, money, error, "too many historical balances");
        ResourceInitial *balance = &out[(*count)++];
        if (!unique_named(tree, money, list, error) || !fixed(tree, money, &balance->balance, error)) return false;
        strcpy(balance->id, money->key);
    }
    return true;
}

typedef struct HistoryWriter {
    PoliticalInstanceSpec *spec;
    size_t capacity;
    uint32_t date;
} HistoryWriter;

static bool write_change(const ScriptTree *tree, const ScriptItem *item, HistoryWriter *writer,
                         PoliticalHistoryChange change, ScriptError *error) {
    PoliticalInstanceSpec *spec = writer->spec;
    if (spec->change_count == GK_MAX_TIMELINE_ENTRIES)
        return script_problem(tree, item, error, "too many historical changes in one instance");
    if (spec->change_count == writer->capacity) {
        const size_t capacity = writer->capacity ? writer->capacity * 2 : 8;
        PoliticalHistoryChange *grown = realloc(spec->changes, capacity * sizeof(*grown));
        if (!grown) return script_problem(tree, item, error, "cannot allocate historical changes");
        spec->changes = grown;
        writer->capacity = capacity;
    }
    change.date = writer->date;
    change.line = item->line;
    change.column = item->column;
    change.order = (unsigned)spec->change_count;
    spec->changes[spec->change_count++] = change;
    return true;
}

static bool write_history_roles(const ScriptTree *tree, const ScriptItem *list, HistoryWriter *writer, ScriptError *error) {
    if (!list) return true;
    if (!list->block) return script_problem(tree, list, error, "roles requires role blocks");
    for (unsigned i = list->first; i; i = tree->items[i].next) {
        const ScriptItem *role = &tree->items[i];
        if (!unique_named(tree, role, list, error)) return false;
        if (!role->block) return script_problem(tree, role, error, "role history requires a TAG = yes/no block");
        for (unsigned j = role->first; j; j = tree->items[j].next) {
            const ScriptItem *country = &tree->items[j];
            PoliticalHistoryChange change = {.kind = HISTORY_ROLE, .key = history_tag(country->key)};
            if (!change.key) return script_problem(tree, country, error, "role history requires a country tag");
            for (unsigned prior = role->first; prior != j; prior = tree->items[prior].next)
                if (!strcmp(tree->items[prior].key, country->key))
                    return script_problem(tree, country, error, "duplicate role holder");
            if (!switch_value(tree, country, &change.enabled, error)) return false;
            strcpy(change.id, role->key);
            if (!write_change(tree, country, writer, change, error)) return false;
        }
    }
    return true;
}

static bool write_history_list(const ScriptTree *tree, const ScriptItem *item, HistoryWriter *writer,
                               bool provinces, bool enabled, bool replace, ScriptError *error) {
    if (!item) return true;
    MemberTags tags = {0};
    ProvinceList land = {0};
    if (provinces ? !read_territory(tree, item, &land, error) : !history_tags(tree, item, &tags, error)) return false;
    if (replace) {
        const PoliticalHistoryKind clear = provinces ? HISTORY_CLEAR_TERRITORY : HISTORY_CLEAR_MEMBERS;
        if (!write_change(tree, item, writer, (PoliticalHistoryChange){.kind = clear}, error)) return false;
    }
    const unsigned count = provinces ? land.count : tags.count;
    const unsigned *ids = provinces ? land.ids : tags.tags;
    for (unsigned i = 0; i < count; ++i) {
        const PoliticalHistoryChange change = {.kind = provinces ? HISTORY_PROVINCE : HISTORY_MEMBER,
                                               .key = ids[i], .enabled = enabled};
        if (!write_change(tree, item, writer, change, error)) return false;
    }
    return true;
}

static bool parse_history_day(const ScriptTree *tree, const ScriptItem *item, HistoryWriter *writer, ScriptError *error) {
    static const char *const keys[] = {"leader", "members", "add_members", "remove_members",
        "territory", "add_provinces", "remove_provinces", "roles", "resources"};
    if (!script_keys(tree, item, keys, COUNT(keys), error)) return false;
    const ScriptItem *leader = F("leader");
    if (leader) {
        PoliticalHistoryChange change = { .kind = HISTORY_LEADER };
        if (!history_leader(tree, leader, &change.key, error) || !write_change(tree, leader, writer, change, error))
            return false;
    }
    if (!write_history_list(tree, F("members"), writer, false, true, true, error) ||
        !write_history_list(tree, F("remove_members"), writer, false, false, false, error) ||
        !write_history_list(tree, F("add_members"), writer, false, true, false, error) ||
        !write_history_list(tree, F("territory"), writer, true, true, true, error) ||
        !write_history_list(tree, F("remove_provinces"), writer, true, false, false, error) ||
        !write_history_list(tree, F("add_provinces"), writer, true, true, false, error) ||
        !write_history_roles(tree, F("roles"), writer, error)) return false;
    ResourceInitial balances[PS_MAX_RESOURCES];
    unsigned count = 0;
    if (!history_balances(tree, F("resources"), balances, &count, error)) return false;
    for (unsigned i = 0; i < count; ++i) {
        PoliticalHistoryChange change = { .kind = HISTORY_BALANCE, .amount = balances[i].balance };
        strcpy(change.id, balances[i].id);
        if (!write_change(tree, F("resources"), writer, change, error)) return false;
    }
    return true;
}

static int history_change_order(const void *left, const void *right) {
    const PoliticalHistoryChange *a = left, *b = right;
    if (a->date != b->date) return a->date < b->date ? -1 : 1;
    return a->order < b->order ? -1 : a->order != b->order;
}

static bool history_instance(const ScriptTree *tree, const ScriptItem *item, PoliticalInstanceSpec *out,
                             ScriptError *error) {
    static const char *const metadata[] = {"type", "founded", "date", "name", "leader", "members", "territory", "resources", "roles"};
    *out = (PoliticalInstanceSpec){ .line = item->line, .starting_authority = -1 };
    if (!ps_identifier(item->key) || !item->block)
        return script_problem(tree, item, error, "expected a named instance block");
    strcpy(out->id, item->key);
    unsigned seen = 0;
    uint32_t first = 0;
    for (unsigned i = item->first; i; i = tree->items[i].next) {
        const ScriptItem *field = &tree->items[i];
        const uint32_t day = date_key(field->key);
        if (day) {
            if (!first || day < first) first = day;
            for (unsigned j = item->first; j != i; j = tree->items[j].next)
                if (date_key(tree->items[j].key) == day)
                    return script_problem(tree, field, error, "duplicate historical date");
            continue;
        }
        unsigned key = 0;
        while (key < COUNT(metadata) && strcmp(metadata[key], field->key)) ++key;
        if (key == COUNT(metadata)) return script_problem(tree, field, error, "unknown history field or invalid date");
        if (seen & (1u << key)) return script_problem(tree, field, error, "duplicate history field");
        seen |= 1u << key;
    }
    if (!label(tree, F("type"), out->type, true, error) || !label(tree, F("name"), out->title, true, error)) return false;
    if (!out->type[0]) return script_problem(tree, item, error, "instance requires type");
    const ScriptItem *founded = F("founded"), *legacy = F("date");
    if (founded && legacy) return script_problem(tree, founded, error, "choose founded or date");
    if (!founded) founded = legacy;
    out->date = founded ? date_key(founded->word) : first;
    if (!out->date) return script_problem(tree, founded ? founded : item, error, "instance requires founded or a valid dated block");

    unsigned leader;
    out->leader_written = F("leader") != NULL;
    out->members_written = F("members") != NULL;
    out->territory_written = F("territory") != NULL;
    if (!history_leader(tree, F("leader"), &leader, error) ||
        !history_tags(tree, F("members"), &out->members, error) ||
        !read_territory(tree, F("territory"), &out->territory, error) ||
        !history_balances(tree, F("resources"), out->balances, &out->balance_count, error)) return false;
    if (leader) election_text(leader, out->leader);
    for (unsigned i = 0; i < out->balance_count; ++i)
        if (!strcmp(out->balances[i].id, PS_AUTHORITY)) out->starting_authority = out->balances[i].balance / PS_RESOURCE_SCALE;
    HistoryWriter writer = {.spec = out};
    if (!write_history_roles(tree, F("roles"), &writer, error)) return false;
    for (unsigned i = item->first; i; i = tree->items[i].next) {
        writer.date = date_key(tree->items[i].key);
        if (writer.date && !parse_history_day(tree, &tree->items[i], &writer, error)) return false;
    }
    if (out->change_count > 1) qsort(out->changes, out->change_count, sizeof(*out->changes), history_change_order);
    return true;
}

bool append_content_history(const char *text, size_t size, const char *path, PoliticalRoster *roster,
                            ScriptError *error) {
    Lexer probe = lexer_for(text, size, error);
    if (next_token(&probe) == TOKEN_END || (probe.kind == TOKEN_WORD && date_key(probe.word))) return true;
    ScriptTree parsed;
    if (!script_tree_read(&parsed, text, size, path, error)) return false;
    const ScriptTree *tree = &parsed;
    const size_t from = roster->count;
    for (unsigned i = tree->items[0].first; i; i = tree->items[i].next) {
        const ScriptItem *item = &tree->items[i];
        if (roster->count == PS_MAX_INSTANCES) {
            script_problem(tree, item, error, "instance capacity exceeded");
            goto failure;
        }
        for (size_t prior = 0; prior < roster->count; ++prior)
            if (!_stricmp(roster->items[prior].id, item->key)) {
                script_problem(tree, item, error, "duplicate instance ID");
                goto failure;
            }
        PoliticalInstanceSpec *instance = &roster->items[roster->count];
        if (!history_instance(tree, item, instance, error)) {
            free(instance->changes);
            instance->changes = NULL;
            goto failure;
        }
        snprintf(instance->path, sizeof(instance->path), "%s", path);
        ++roster->count;
    }
    script_tree_free(&parsed);
    return true;
failure:
    for (size_t i = from; i < roster->count; ++i) {
        free(roster->items[i].changes);
        roster->items[i].changes = NULL;
    }
    roster->count = from;
    script_tree_free(&parsed);
    return false;
}

static bool append_mounted_flag(const ScriptTree *tree, const ScriptItem *item, uint32_t date,
                                KhanFlagTimeline *out, ScriptError *error) {
    bool enabled;
    if (!switch_value(tree, item, &enabled, error)) return false;
    if (!out->entries) {
        out->entries = calloc(tree->count, sizeof(*out->entries));
        if (!out->entries) return script_problem(tree, item, error, "cannot allocate mounted history marks");
    }
    out->entries[out->count++] = (KhanFlagChange){ .date = date, .enabled = enabled };
    return true;
}

bool read_mounted_flag_history(const char *text, size_t size, const char *path, const char *keyword,
                               KhanFlagTimeline *out, ScriptError *error) {
    ScriptTree tree;
    if (!script_tree_read(&tree, text, size, path, error)) return false;
    KhanFlagTimeline timeline = {0};
    bool ok = true;
    for (unsigned i = tree.items[0].first; ok && i; i = tree.items[i].next) {
        const ScriptItem *item = &tree.items[i];
        if (!strcmp(item->key, keyword)) {
            ok = append_mounted_flag(&tree, item, 0, &timeline, error);
            continue;
        }
        const uint32_t date = item->block ? date_key(item->key) : 0;
        if (!date) continue;
        for (unsigned j = item->first; ok && j; j = tree.items[j].next)
            if (!strcmp(tree.items[j].key, keyword))
                ok = append_mounted_flag(&tree, &tree.items[j], date, &timeline, error);
    }
    script_tree_free(&tree);
    if (ok && timeline.count) {
        KhanFlagChange *shrink = realloc(timeline.entries, timeline.count * sizeof(*shrink));
        if (shrink) timeline.entries = shrink;
    }
    if (ok) *out = timeline;
    else free(timeline.entries);
    return ok;
}
