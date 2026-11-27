#include "political_script.h"
#include "ps_script.h"
#include "script_lexer.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool read_ps_alias(Lexer *lexer, PsAlias *alias) {
    char fields[PS_FIELD_COUNT][GK_SCRIPT_WORD_SIZE] = {{0}};
    if (!expect(lexer, TOKEN_PUNCTUATION, "=") || !expect(lexer, TOKEN_PUNCTUATION, "{"))
        return false;
    while (next_token(lexer) == TOKEN_WORD) {
        if (named(lexer, "operation") && alias->operation == PS_UNKNOWN) {
            if (!expect(lexer, TOKEN_PUNCTUATION, "=") || next_token(lexer) != TOKEN_WORD)
                return error_at(lexer, "alias operation requires a name");
            alias->operation = ps_find_operation(lexer->word);
            if (alias->operation == PS_UNKNOWN) return error_at(lexer, "unknown alias operation");
        } else if (named(lexer, "argument") && alias->argument == PS_FIELD_NONE) {
            if (!expect(lexer, TOKEN_PUNCTUATION, "=") || next_token(lexer) != TOKEN_WORD)
                return error_at(lexer, "alias argument requires a field name");
            alias->argument = ps_find_field(lexer->word);
            if (alias->argument == PS_FIELD_NONE) return error_at(lexer, "unknown alias argument");
        } else {
            const PsField field = ps_find_field(lexer->word);
            if (!field || fields[field][0]) return error_at(lexer, "unknown or duplicate alias field");
            if (!expect(lexer, TOKEN_PUNCTUATION, "=") || next_token(lexer) != TOKEN_WORD)
                return error_at(lexer, "expected an alias field value");
            strcpy(fields[field], lexer->word);
        }
    }
    if (!punctuation(lexer, '}') || !alias->operation || !fields[PS_INSTANCE_FIELD][0])
        return error_at(lexer, "alias requires a closing brace, operation and instance");
    for (PsField i = PS_FIELD_NONE + 1; i < PS_FIELD_COUNT; ++i) {
        if (!fields[i][0]) continue;
        const char *reason = ps_read_argument(&alias->bound, alias->operation, i, fields[i]);
        if (reason) return error_at(lexer, reason);
    }
    if (alias->argument) {
        const unsigned bit = PS_FIELD(alias->argument);
        const PsDefinition *op = &ps_definitions[alias->operation];
        PsArguments expanded = alias->bound;
        expanded.fields |= bit;
        if (!(op->allowed & bit) || (alias->bound.fields & bit) ||
            ps_missing_field(alias->operation, &expanded) || ps_conflicting_fields(alias->operation, &expanded))
            return error_at(lexer, "scalar alias must supply every required field");
    }
    return true;
}

bool read_ps_aliases(const char *text, size_t size, PsAliases *aliases, ScriptError *error) {
    Lexer lexer = lexer_for(text, size, error);
    aliases->count = 0;
    if (!expect(&lexer, TOKEN_WORD, "aliases") || !expect(&lexer, TOKEN_PUNCTUATION, "=") ||
        !expect(&lexer, TOKEN_PUNCTUATION, "{")) return false;
    while (next_token(&lexer) == TOKEN_WORD) {
        if (!ps_identifier(lexer.word) || ps_reserved(lexer.word) || aliases->count == PS_MAX_ALIASES)
            return error_at(&lexer, "invalid or reserved alias name, or too many aliases");
        for (size_t i = 0; i < aliases->count; ++i)
            if (!_stricmp(aliases->entries[i].name, lexer.word)) return error_at(&lexer, "duplicate alias name");
        PsAlias *alias = &aliases->entries[aliases->count];
        *alias = (PsAlias){ .line = lexer.line, .column = lexer.column };
        strcpy(alias->name, lexer.word);
        if (!read_ps_alias(&lexer, alias)) return false;
        ++aliases->count;
    }
    if (!punctuation(&lexer, '}') || next_token(&lexer) != TOKEN_END)
        return error_at(&lexer, "expected the end of the aliases block");
    return true;
}

static bool read_label(Lexer *lexer, char out[GK_SCRIPT_WORD_SIZE]) {
    if (!expect(lexer, TOKEN_PUNCTUATION, "=") || next_token(lexer) != TOKEN_WORD || !lexer->word[0])
        return error_at(lexer, "expected a label");
    strcpy(out, lexer->word);
    return true;
}

static bool read_instance(Lexer *lexer, PoliticalInstanceSpec *instance) {
    if (!expect(lexer, TOKEN_PUNCTUATION, "=") || !expect(lexer, TOKEN_PUNCTUATION, "{")) return false;
    while (next_token(lexer) == TOKEN_WORD) {
        if (named(lexer, "type") && !instance->type[0]) {
            if (!expect(lexer, TOKEN_PUNCTUATION, "=") ||
                !read_id(lexer, "instance type requires an identifier", instance->type)) return false;
        } else if (named(lexer, "history") && !instance->history[0]) {
            if (!expect(lexer, TOKEN_PUNCTUATION, "=") ||
                !read_id(lexer, "history requires a file name without extension", instance->history)) return false;
        } else if (named(lexer, "title") && !instance->title[0]) {
            if (!read_label(lexer, instance->title)) return false;
        } else if (named(lexer, "icon") && !instance->icon[0]) {
            if (!expect(lexer, TOKEN_PUNCTUATION, "=") ||
                !read_id(lexer, "icon requires a supported icon name", instance->icon)) return false;
            if (strcmp(instance->icon, "cavalry") && strcmp(instance->icon, "infantry") &&
                strcmp(instance->icon, "tribute") && strcmp(instance->icon, "khanate"))
                return error_at(lexer, "icon must be cavalry, infantry, tribute or khanate");
        } else if (named(lexer, "starting_authority") && instance->starting_authority < 0) {
            if (!expect(lexer, TOKEN_PUNCTUATION, "=") ||
                !read_integer(lexer, 0, GK_MAX_AUTHORITY, "starting_authority requires 0 to 100",
                              &instance->starting_authority)) return false;
        } else {
            return error_at(lexer, "unknown or duplicate instance field");
        }
    }
    if (!punctuation(lexer, '}') || !instance->type[0]) return error_at(lexer, "instance requires a type and closing brace");
    if (!instance->history[0]) strcpy(instance->history, instance->id);
    return true;
}

bool read_roster(const char *text, size_t size, PoliticalRoster *out, ScriptError *error) {
    Lexer lexer = lexer_for(text, size, error);
    out->count = 0;
    if (!expect(&lexer, TOKEN_WORD, "instances") || !expect(&lexer, TOKEN_PUNCTUATION, "=") ||
        !expect(&lexer, TOKEN_PUNCTUATION, "{")) return false;
    while (next_token(&lexer) == TOKEN_WORD) {
        if (!ps_identifier(lexer.word) || out->count == PS_MAX_INSTANCES)
            return error_at(&lexer, "invalid instance identifier or more than 32 instances");
        for (size_t i = 0; i < out->count; ++i)
            if (!_stricmp(out->items[i].id, lexer.word)) return error_at(&lexer, "duplicate instance identifier");
        PoliticalInstanceSpec *instance = &out->items[out->count];
        *instance = (PoliticalInstanceSpec){ .starting_authority = -1, .line = lexer.line };
        strcpy(instance->id, lexer.word);
        if (!read_instance(&lexer, instance)) return false;
        ++out->count;
    }
    if (!punctuation(&lexer, '}') || next_token(&lexer) != TOKEN_END)
        return error_at(&lexer, "expected the end of the instances block");
    return true;
}

bool append_roster(const char *text, size_t size, PoliticalRoster *out, ScriptError *error) {
    PoliticalRoster additions;
    if (!read_roster(text, size, &additions, error)) return false;
    for (size_t i = 0; i < additions.count; ++i) {
        const PoliticalInstanceSpec *item = &additions.items[i];
        const char *reason = NULL;
        if (out->count + i >= PS_MAX_INSTANCES) reason = "instance capacity exceeded";
        for (size_t j = 0; j < out->count && !reason; ++j)
            if (!_stricmp(item->id, out->items[j].id)) reason = "duplicate instance identifier";
        if (reason) {
            error->line = item->line;
            error->column = 1;
            snprintf(error->reason, sizeof(error->reason), "%s: %s", reason, item->id);
            return false;
        }
    }
    memcpy(out->items + out->count, additions.items, additions.count * sizeof(additions.items[0]));
    out->count += additions.count;
    return true;
}

static bool read_bool(Lexer *lexer, bool *value) {
    if (next_token(lexer) != TOKEN_WORD || (strcmp(lexer->word, "yes") && strcmp(lexer->word, "no")))
        return error_at(lexer, "expected yes or no");
    *value = named(lexer, "yes");
    return true;
}

static bool read_entry_query(Lexer *lexer, EntryNode *node) {
    if (!expect(lexer, TOKEN_PUNCTUATION, "{")) return false;
    while (next_token(lexer) == TOKEN_WORD) {
        PsField field = ps_find_field(lexer->word);
        if (!field) return error_at(lexer, "unknown entry query field");
        if (!expect(lexer, TOKEN_PUNCTUATION, "=") || next_token(lexer) != TOKEN_WORD)
            return error_at(lexer, "expected query argument");
        const char *reason = ps_read_argument(&node->args, node->operation, field, lexer->word);
        if (reason) return error_at(lexer, reason);
    }
    if (!punctuation(lexer, '}')) return error_at(lexer, "expected end of entry query");
    /* Omitted instance is bound to this button's instance at evaluation. */
    if (!(node->args.fields & PS_FIELD(PS_TYPE_FIELD))) node->args.fields |= PS_FIELD(PS_INSTANCE_FIELD);
    const char *conflict = ps_conflicting_fields(node->operation, &node->args);
    if (conflict) return error_at(lexer, conflict);
    const char *missing = ps_missing_field(node->operation, &node->args);
    if (missing) {
        char message[64];
        snprintf(message, sizeof(message), "entry query requires '%s'", missing);
        return error_at(lexer, message);
    }
    return true;
}

bool read_entry_group(Lexer *lexer, EntryCondition *condition, EntryTest kind) {
    if (condition->count == ENTRY_MAX_NODES) return error_at(lexer, "entry condition exceeds 32 nodes");
    EntryNode *group = &condition->nodes[condition->count++];
    group->test = kind;
    if (!expect(lexer, TOKEN_PUNCTUATION, "{")) return false;
    unsigned terms = 0;
    while (next_token(lexer) == TOKEN_WORD) {
        ++terms;
        const bool and = named(lexer, "AND"), or = named(lexer, "OR"), negate = named(lexer, "NOT");
        if (and || or || negate) {
            if (!expect(lexer, TOKEN_PUNCTUATION, "=") ||
                !read_entry_group(lexer, condition, and ? ENTRY_AND : or ? ENTRY_OR : ENTRY_NOT)) return false;
            continue;
        }
        if (condition->count == ENTRY_MAX_NODES) return error_at(lexer, "entry condition exceeds 32 nodes");
        EntryNode *node = &condition->nodes[condition->count++];
        node->end = condition->count;
        if (named(lexer, "always")) {
            node->test = ENTRY_ALWAYS;
            if (!expect(lexer, TOKEN_PUNCTUATION, "=") || !read_bool(lexer, &node->args.value)) return false;
        } else if (named(lexer, "tag")) {
            node->test = ENTRY_TAG;
            if (!expect(lexer, TOKEN_PUNCTUATION, "=") || next_token(lexer) != TOKEN_WORD ||
                strlen(lexer->word) != 3 || strspn(lexer->word, "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") != 3)
                return error_at(lexer, "tag requires a country tag");
            memcpy(&node->args.country, lexer->word, 3);
        } else if (named(lexer, "has_heir")) {
            node->test = ENTRY_HEIR;
            if (!expect(lexer, TOKEN_PUNCTUATION, "=") || !read_bool(lexer, &node->args.value)) return false;
        } else if (named(lexer, "has_country_flag") || named(lexer, "religion") || named(lexer, "government")) {
            node->test = named(lexer, "religion") ? ENTRY_RELIGION :
                         named(lexer, "government") ? ENTRY_GOVERNMENT : ENTRY_COUNTRY_FLAG;
            if (!expect(lexer, TOKEN_PUNCTUATION, "=") ||
                !read_id(lexer, "expected a country condition identifier", node->args.reform)) return false;
            if (node->test == ENTRY_GOVERNMENT && !government_category(node->args.reform))
                return error_at(lexer, "unknown government category");
        } else {
            node->test = ENTRY_QUERY;
            node->operation = ps_find_operation(lexer->word);
            if (!node->operation || !ps_definitions[node->operation].trigger)
                return error_at(lexer, "unsupported entry country trigger or effect used as a condition");
            if (!expect(lexer, TOKEN_PUNCTUATION, "=") || !read_entry_query(lexer, node)) return false;
        }
    }
    if (!punctuation(lexer, '}') || !terms || (kind == ENTRY_NOT && terms != 1))
        return error_at(lexer, "entry groups require terms; NOT takes one term");
    group->end = condition->count;
    return true;
}

static bool read_entry_button(Lexer *lexer, EntryButton *entry) {
    static const char *const options[] = {"region", "control", "tooltip", "order", "visible", "enabled",
                                          "toggle", "close_button", "close_on_escape"};
    unsigned seen = 0;
    entry->defined = entry->toggle = entry->close_button = entry->close_on_escape = true;
    if (!expect(lexer, TOKEN_PUNCTUATION, "{")) return false;
    while (next_token(lexer) == TOKEN_WORD) {
        size_t opt = 0;
        while (opt < sizeof(options)/sizeof(*options) && strcmp(lexer->word, options[opt])) ++opt;
        if (opt == sizeof(options)/sizeof(*options) || (seen & (1u << opt)))
            return error_at(lexer, "unknown or duplicate entry_button field");
        seen |= 1u << opt;
        if (!expect(lexer, TOKEN_PUNCTUATION, "=")) return false;
        if (opt <= 2) {
            char *target = opt == 0 ? entry->region : opt == 1 ? entry->control : entry->tooltip;
            if (!read_id(lexer, "expected an entry GUI or localisation identifier", target)) return false;
        } else if (opt == 3) {
            entry->has_order = true;
            if (!read_integer(lexer, 0, 32767, "order requires 0..32767", &entry->order)) return false;
        } else if (opt <= 5) {
            if (!read_entry_group(lexer, opt == 4 ? &entry->visible : &entry->enabled, ENTRY_AND)) return false;
        } else {
            bool *value = opt == 6 ? &entry->toggle : opt == 7 ? &entry->close_button : &entry->close_on_escape;
            if (!read_bool(lexer, value)) return false;
        }
    }
    if (!punctuation(lexer, '}') || !entry->region[0] || !entry->tooltip[0])
        return error_at(lexer, "entry_button requires region and tooltip");
    if (!entry->control[0]) strcpy(entry->control, "pf_open");
    return true;
}

static bool read_interface(Lexer *lexer, EntryButton *entry) {
    if (!expect(lexer, TOKEN_PUNCTUATION, "=") || !expect(lexer, TOKEN_PUNCTUATION, "{")) return false;
    while (next_token(lexer) == TOKEN_WORD) {
        if (named(lexer, "entry_button") && !entry->defined) {
            if (!expect(lexer, TOKEN_PUNCTUATION, "=") || !read_entry_button(lexer, entry)) return false;
        } else if (named(lexer, "panel") && !entry->panel[0]) {
            if (!expect(lexer, TOKEN_PUNCTUATION, "=") || !read_id(lexer, "expected a panel window name", entry->panel)) return false;
        } else if (named(lexer, "title_key") && !entry->title_key[0]) {
            if (!expect(lexer, TOKEN_PUNCTUATION, "=") || !read_id(lexer, "expected a title key", entry->title_key)) return false;
        } else return error_at(lexer, "unknown or duplicate interface field");
    }
    return (punctuation(lexer, '}') && entry->defined) || error_at(lexer, "interface requires entry_button");
}

static bool read_members(Lexer *lexer, KhanDefinition *definition) {
    if (!expect(lexer, TOKEN_PUNCTUATION, "{")) return false;
    while (next_token(lexer) == TOKEN_WORD) {
        if (named(lexer, "source") && definition->member_source == MEMBERS_UNSET) {
            if (!expect(lexer, TOKEN_PUNCTUATION, "=")) return false;
            next_token(lexer);
            if (named(lexer, "direct_subjects_of_leader")) definition->member_source = MEMBERS_DIRECT_SUBJECTS;
            else if (named(lexer, "capital_provinces")) definition->member_source = MEMBERS_CAPITAL_PROVINCES;
            else return error_at(lexer, "unknown members.source");
        } else if (named(lexer, "subject_type") && !definition->subject_type[0]) {
            if (!expect(lexer, TOKEN_PUNCTUATION, "=") ||
                !read_id(lexer, "members.subject_type requires an EU4 subject type identifier", definition->subject_type))
                return false;
            definition->subject_type_line = lexer->line;
            definition->subject_type_column = lexer->column;
            if (!strcmp(definition->subject_type, "daimyo")) strcpy(definition->subject_type, "daimyo_vassal");
        } else if (named(lexer, "title") && !definition->member_title[0]) {
            if (!read_label(lexer, definition->member_title)) return false;
        } else {
            return error_at(lexer, "unknown or duplicate field in members");
        }
    }
    if (!punctuation(lexer, '}')) return error_at(lexer, "expected the end of members");
    if (definition->member_source == MEMBERS_CAPITAL_PROVINCES)
        return !definition->subject_type[0] || error_at(lexer, "capital_provinces takes no subject_type");
    return (definition->member_source == MEMBERS_DIRECT_SUBJECTS && definition->subject_type[0]) ||
        error_at(lexer, "direct subject members require source and subject_type");
}

static bool read_election(Lexer *lexer, ElectionRules *rules) {
    static const char *const keys[] = {"candidates", "voters", "vote_weight", "tie_break", "succession",
                                        "self_bonus", "incumbent_bonus", "cooldown_days"};
    *rules = (ElectionRules){ .defined = true, .weight = 1, .incumbent_ties = true, .self_bonus = 50, .incumbent_bonus = 20, .cooldown_days = 60 };
    unsigned seen = 0;
    if (!expect(lexer, TOKEN_PUNCTUATION, "{")) return false;
    while (next_token(lexer) == TOKEN_WORD) {
        unsigned key = 0;
        while (key < 8 && !named(lexer, keys[key])) ++key;
        if (key == 8 || (seen & (1u << key))) return error_at(lexer, "unknown or duplicate election setting");
        seen |= 1u << key;
        if (!expect(lexer, TOKEN_PUNCTUATION, "=")) return false;
        if (key == 2) {
            if (!read_integer(lexer, 1, ELECTION_MAX_WEIGHT, "vote_weight must be 1..99", &rules->weight)) return false;
        } else if (key == 7) {
            if (!read_integer(lexer, 0, 3650, "cooldown_days must be 0..3650", &rules->cooldown_days)) return false;
        } else if (key >= 5) {
            if (!read_integer(lexer, -200, 200, "AI bonus must be -200..200",
                              key == 5 ? &rules->self_bonus : &rules->incumbent_bonus)) return false;
        } else {
            if (next_token(lexer) != TOKEN_WORD) return error_at(lexer, "expected an election setting value");
            if (key == 0 || key == 1) {
                if (!named(lexer, "members") && !named(lexer, "tumen")) return error_at(lexer, "expected members or tumen");
                if (key == 0) rules->tumen_candidates = named(lexer, "tumen");
                else rules->all_member_voters = named(lexer, "members");
            } else if (key == 3) {
                if (!named(lexer, "incumbent_then_tag") && !named(lexer, "tag_order")) return error_at(lexer, "unknown tie break");
                rules->incumbent_ties = named(lexer, "incumbent_then_tag");
            } else {
                if (!named(lexer, "ruler_change") && !named(lexer, "hereditary")) return error_at(lexer, "unknown succession rule");
                rules->hereditary = named(lexer, "hereditary");
            }
        }
    }
    return punctuation(lexer, '}') || error_at(lexer, "expected the end of election");
}

bool read_type_definition(const char *text, size_t size, const char *id, KhanDefinition *out, ScriptError *error) {
    Lexer lexer = lexer_for(text, size, error);
    KhanDefinition definition = { .starting_authority = GK_DEFAULT_AUTHORITY,
                                  .leader_enabled = true, .authority_enabled = true };
    bool has_authority = false;
    if (!expect(&lexer, TOKEN_WORD, id) || !expect(&lexer, TOKEN_PUNCTUATION, "=") ||
        !expect(&lexer, TOKEN_PUNCTUATION, "{")) return false;
    while (next_token(&lexer) == TOKEN_WORD) {
        if (named(&lexer, "emperor") && !definition.emperor[0]) {
            if (!expect(&lexer, TOKEN_PUNCTUATION, "=") ||
                !read_id(&lexer, "emperor requires a title identifier", definition.emperor)) return false;
        } else if (named(&lexer, "title") && !definition.title[0]) {
            if (!read_label(&lexer, definition.title)) return false;
        } else if (named(&lexer, "members") && definition.member_source == MEMBERS_UNSET) {
            if (!expect(&lexer, TOKEN_PUNCTUATION, "=") || !read_members(&lexer, &definition)) return false;
        } else if (named(&lexer, "interface") && !definition.entry.defined) {
            if (!read_interface(&lexer, &definition.entry)) return false;
        } else if (named(&lexer, "election") && !definition.election.defined) {
            if (!expect(&lexer, TOKEN_PUNCTUATION, "=") || !read_election(&lexer, &definition.election)) return false;
        } else if (named(&lexer, "starting_authority") && !has_authority) {
            if (!expect(&lexer, TOKEN_PUNCTUATION, "=") ||
                !read_integer(&lexer, 0, GK_MAX_AUTHORITY, "starting_authority requires an integer from 0 to 100",
                              &definition.starting_authority)) return false;
            has_authority = true;
        } else {
            return error_at(&lexer, "unknown or duplicate field in political type");
        }
    }
    if (!punctuation(&lexer, '}')) return error_at(&lexer, "expected the end of the type");
    if (!definition.emperor[0] || definition.member_source == MEMBERS_UNSET)
        return error_at(&lexer, "type requires emperor and members");
    if (next_token(&lexer) != TOKEN_END) return error_at(&lexer, "expected end of file after the type");
    *out = definition;
    return true;
}

bool read_definition(const char *text, size_t size, KhanDefinition *out, ScriptError *error) {
    return read_type_definition(text, size, PS_LEGACY_TYPE, out, error);
}

bool read_type_reform(const char *text, size_t size, const char *id, KhanReform *out, ScriptError *error) {
    Lexer lexer = lexer_for(text, size, error);
    KhanReform reform = {0};
    bool has_empire = false, has_cost = false;
    if (!read_id(&lexer, "expected a reform identifier", reform.id) ||
        !expect(&lexer, TOKEN_PUNCTUATION, "=") || !expect(&lexer, TOKEN_PUNCTUATION, "{")) return false;
    while (next_token(&lexer) == TOKEN_WORD) {
        if (named(&lexer, "empire") && !has_empire) {
            if (!expect(&lexer, TOKEN_PUNCTUATION, "=") || !expect(&lexer, TOKEN_WORD, id))
                return false;
            has_empire = true;
        } else if (named(&lexer, "title") && !reform.title[0]) {
            if (!expect(&lexer, TOKEN_PUNCTUATION, "=") ||
                !read_id(&lexer, "title requires a localization key", reform.title)) return false;
        } else if (named(&lexer, "cost") && !has_cost) {
            if (!expect(&lexer, TOKEN_PUNCTUATION, "=") || !expect(&lexer, TOKEN_PUNCTUATION, "{") ||
                !expect(&lexer, TOKEN_WORD, "authority") || !expect(&lexer, TOKEN_PUNCTUATION, "=") ||
                !read_integer(&lexer, 0, GK_MAX_AUTHORITY, "cost.authority requires an integer from 0 to 100",
                              &reform.cost) || !expect(&lexer, TOKEN_PUNCTUATION, "}")) return false;
            has_cost = true;
        } else if (named(&lexer, "on_effect") && !reform.effect_line) {
            if (!expect(&lexer, TOKEN_PUNCTUATION, "=") || !expect(&lexer, TOKEN_PUNCTUATION, "{") ||
                !expect(&lexer, TOKEN_WORD, "leader_scope") || !expect(&lexer, TOKEN_PUNCTUATION, "=") ||
                !expect(&lexer, TOKEN_PUNCTUATION, "{") ||
                !expect(&lexer, TOKEN_WORD, "add_prestige") || !expect(&lexer, TOKEN_PUNCTUATION, "=") ||
                !read_integer(&lexer, -100, 100, "add_prestige requires an integer from -100 to 100",
                              &reform.prestige)) return false;
            reform.effect_line = lexer.line;
            if (!expect(&lexer, TOKEN_PUNCTUATION, "}") || !expect(&lexer, TOKEN_PUNCTUATION, "}")) return false;
        } else {
            return error_at(&lexer, "unknown or duplicate reform field");
        }
    }
    if (!punctuation(&lexer, '}')) return error_at(&lexer, "expected the end of the reform");
    if (!has_empire || !reform.title[0] || !has_cost || !reform.effect_line)
        return error_at(&lexer, "reform requires empire, title, cost and on_effect");
    if (next_token(&lexer) != TOKEN_END) return error_at(&lexer, "expected end of file after the reform");
    *out = reform;
    return true;
}

bool read_reform(const char *text, size_t size, KhanReform *out, ScriptError *error) {
    return read_type_reform(text, size, PS_LEGACY_TYPE, out, error);
}

uint32_t date_key(const char *text) {
    static const int month_days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    char *end;
    if (!isdigit((unsigned char)*text)) return 0;
    long year = strtol(text, &end, 10);
    if (year < 1 || year > 9999 || *end != '.' || !isdigit((unsigned char)end[1])) return 0;
    long month = strtol(end + 1, &end, 10);
    if (month < 1 || month > 12 || *end != '.' || !isdigit((unsigned char)end[1])) return 0;
    long day = strtol(end + 1, &end, 10);
    if (day < 1 || day > month_days[month - 1] || *end) return 0;
    return (uint32_t)(year * 10000 + month * 100 + day);
}

static bool read_history_entry(Lexer *lexer, const char *keyword, KhanHistory *entry) {
    entry->effective_date = date_key(lexer->word);
    if (!entry->effective_date) return error_at(lexer, "invalid date, expected year.month.day");
    if (!expect(lexer, TOKEN_PUNCTUATION, "=") || !expect(lexer, TOKEN_PUNCTUATION, "{")) return false;
    next_token(lexer);
    if (keyword ? !named(lexer, keyword) : (!named(lexer, "leader") && !named(lexer, "great_khan_emperor")))
        return error_at(lexer, keyword ? "expected khan_emperor" : "expected leader");
    if (!expect(lexer, TOKEN_PUNCTUATION, "=")) return false;
    if (next_token(lexer) != TOKEN_WORD) return error_at(lexer, "leader requires a country tag or none");
    if (strcmp(lexer->word, "none")) {
        if (strlen(lexer->word) != 3 || strspn(lexer->word, "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") != 3)
            return error_at(lexer, "leader requires a three-character country tag or none");
        memcpy(entry->tag, lexer->word, sizeof(entry->tag));
    }
    return expect(lexer, TOKEN_PUNCTUATION, "}");
}

bool read_holder_history(const char *text, size_t size, const char *keyword, KhanTimeline *out, ScriptError *error) {
    Lexer lexer = lexer_for(text, size, error);
    KhanTimeline timeline = {0};
    size_t capacity = 0;
    while (next_token(&lexer) == TOKEN_WORD) {
        KhanHistory entry = {0};
        if (!read_history_entry(&lexer, keyword, &entry)) goto failed;
        if (timeline.count == GK_MAX_TIMELINE_ENTRIES) {
            error_at(&lexer, "too many entries in the Great Khan timeline");
            goto failed;
        }
        if (timeline.count == capacity) {
            capacity = capacity ? capacity * 2 : 8;
            KhanHistory *entries = realloc(timeline.entries, capacity * sizeof(*entries));
            if (!entries) {
                error_at(&lexer, "out of memory reading the Great Khan timeline");
                goto failed;
            }
            timeline.entries = entries;
        }
        timeline.entries[timeline.count++] = entry;
    }
    if (lexer.kind != TOKEN_END) {
        error_at(&lexer, "expected a date or end of file");
        goto failed;
    }
    *out = timeline;
    return true;
failed:
    free(timeline.entries);
    return false;
}

bool read_timeline(const char *text, size_t size, KhanTimeline *out, ScriptError *error) {
    return read_holder_history(text, size, NULL, out, error);
}

KhanHistory history_at(const KhanTimeline *timeline, uint32_t today) {
    KhanHistory selected = {0};
    for (size_t i = 0; i < timeline->count; ++i) {
        const KhanHistory *entry = &timeline->entries[i];
        if (entry->effective_date <= today && entry->effective_date >= selected.effective_date) selected = *entry;
    }
    return selected;
}

bool read_flag_history(const char *text, size_t size, const char *keyword, KhanFlagTimeline *out, ScriptError *error) {
    Lexer lexer = lexer_for(text, size, error);
    KhanFlagTimeline timeline = {0};
    size_t capacity = 0;
    while (next_token(&lexer) == TOKEN_WORD) {
        const bool dated = !named(&lexer, keyword);
        KhanFlagChange change = {0};
        if (dated) {
            change.date = date_key(lexer.word);
            if (!change.date) {
                error_at(&lexer, "expected the history key or a date");
                goto failed;
            }
            if (!expect(&lexer, TOKEN_PUNCTUATION, "=") || !expect(&lexer, TOKEN_PUNCTUATION, "{") ||
                !expect(&lexer, TOKEN_WORD, keyword)) goto failed;
        }
        if (!expect(&lexer, TOKEN_PUNCTUATION, "=")) goto failed;
        next_token(&lexer);
        if (!named(&lexer, "yes") && !named(&lexer, "no")) {
            error_at(&lexer, "history flag requires yes or no");
            goto failed;
        }
        change.enabled = named(&lexer, "yes");
        if (dated && !expect(&lexer, TOKEN_PUNCTUATION, "}")) goto failed;
        if (timeline.count == GK_MAX_TIMELINE_ENTRIES) {
            error_at(&lexer, "too many dated flag entries");
            goto failed;
        }
        if (timeline.count == capacity) {
            capacity = capacity ? capacity * 2 : 4;
            KhanFlagChange *entries = realloc(timeline.entries, capacity * sizeof(*entries));
            if (!entries) {
                error_at(&lexer, "out of memory reading flag history");
                goto failed;
            }
            timeline.entries = entries;
        }
        timeline.entries[timeline.count++] = change;
    }
    if (!timeline.count || lexer.kind != TOKEN_END) {
        error_at(&lexer, "expected a flag record or the end of the file");
        goto failed;
    }
    *out = timeline;
    return true;

failed:
    free(timeline.entries);
    return false;
}

bool flag_at(const KhanFlagTimeline *timeline, uint32_t date) {
    KhanFlagChange selected = {0};
    for (size_t i = 0; i < timeline->count; ++i) {
        const KhanFlagChange *entry = &timeline->entries[i];
        if (entry->date <= date && entry->date >= selected.date) selected = *entry;
    }
    return selected.enabled;
}
