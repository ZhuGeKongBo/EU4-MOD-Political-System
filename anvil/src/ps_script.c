#include "ps_script.h"
#include "election.h"
#include "political_resources.h"
#include "political_actions.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

const char *const ps_field_names[PS_FIELD_COUNT] = {
    [PS_INSTANCE_FIELD] = "instance",
    [PS_COUNTRY_FIELD] = "country",
    [PS_RESOURCE_FIELD] = "resource",
    [PS_AMOUNT_FIELD] = "amount",
    [PS_VALUE_FIELD] = "value",
    [PS_COMPARISON_FIELD] = "comparison",
    [PS_REFORM_FIELD] = "reform",
    [PS_ROUND_FIELD] = "round",
    [PS_ROLE_FIELD] = "role",
    [PS_PROVINCE_FIELD] = "province",
    [PS_CALCULATED_FIELD] = "id",
    [PS_ACTION_FIELD] = "action", [PS_ACTOR_FIELD] = "actor", [PS_TARGET_FIELD] = "target",
    [PS_PERMISSION_FIELD] = "permission",
    [PS_OPTION_FIELD] = "option", [PS_STATUS_FIELD] = "status",
    [PS_TYPE_FIELD] = "type", [PS_NAME_FIELD] = "name", [PS_MESSAGE_FIELD] = "message",
};

#define INSTANCE PS_FIELD(PS_INSTANCE_FIELD)
#define COUNTRY PS_FIELD(PS_COUNTRY_FIELD)
#define RESOURCE PS_FIELD(PS_RESOURCE_FIELD)
#define AMOUNT PS_FIELD(PS_AMOUNT_FIELD)
#define VALUE PS_FIELD(PS_VALUE_FIELD)
#define COMPARE PS_FIELD(PS_COMPARISON_FIELD)
#define REFORM PS_FIELD(PS_REFORM_FIELD)
#define ROUND PS_FIELD(PS_ROUND_FIELD)
#define ROLE PS_FIELD(PS_ROLE_FIELD)
#define PROVINCE PS_FIELD(PS_PROVINCE_FIELD)
#define CALCULATED PS_FIELD(PS_CALCULATED_FIELD)
#define ACTION PS_FIELD(PS_ACTION_FIELD)
#define ACTOR PS_FIELD(PS_ACTOR_FIELD)
#define TARGET PS_FIELD(PS_TARGET_FIELD)
#define PERMISSION PS_FIELD(PS_PERMISSION_FIELD)
#define OPTION PS_FIELD(PS_OPTION_FIELD)
#define STATUS PS_FIELD(PS_STATUS_FIELD)
#define TYPE PS_FIELD(PS_TYPE_FIELD)
#define NAME PS_FIELD(PS_NAME_FIELD)
#define MESSAGE PS_FIELD(PS_MESSAGE_FIELD)
#define SELECT (INSTANCE | TYPE)
#define COUNTRY_TARGET (COUNTRY | TARGET)

const PsDefinition ps_definitions[PS_OPERATION_COUNT] = {
    [PS_HAS_INSTANCE] = {"ps_has_instance", true, INSTANCE | VALUE, INSTANCE | VALUE, "Campaign instance is active"},
    [PS_IS_LEADER] = {"ps_is_leader", true, INSTANCE, SELECT | COUNTRY | VALUE, "Check whether the country holds the leadership"},
    [PS_COMPARE_RESOURCE] = {"ps_compare_resource", true, INSTANCE | RESOURCE | AMOUNT,
                             SELECT | COUNTRY | RESOURCE | AMOUNT | COMPARE, "Compare an instance or country decimal resource"},
    [PS_IS_REFORM_PASSED] = {"ps_is_reform_passed", true, INSTANCE | REFORM | VALUE,
                            SELECT | REFORM | VALUE, "Check whether the reform passed"},
    [PS_SET_LEADER] = {"ps_set_leader", false, INSTANCE, SELECT | COUNTRY_TARGET | VALUE, "Set the instance leader"},
    [PS_CLEAR_LEADER] = {"ps_clear_leader", false, INSTANCE, SELECT | VALUE, "Vacate the office"},
    [PS_SET_RESOURCE] = {"ps_set_resource", false, INSTANCE | RESOURCE | AMOUNT,
                         SELECT | COUNTRY | RESOURCE | AMOUNT, "Set an instance or country decimal resource"},
    [PS_CHANGE_RESOURCE] = {"ps_change_resource", false, INSTANCE | RESOURCE | AMOUNT,
                            SELECT | COUNTRY | RESOURCE | AMOUNT, "Change an instance or country decimal resource"},
    [PS_EXECUTE_ACTION] = {"ps_execute_action", false, INSTANCE | REFORM,
                           SELECT | REFORM, "Pass the reform using the normal cost and eligibility rules"},
    [PS_IS_CANDIDATE] = {"ps_is_candidate", true, INSTANCE | VALUE, SELECT | COUNTRY | VALUE, "Check eligibility to stand for election"},
    [PS_IS_ELECTOR] = {"ps_is_elector", true, INSTANCE | VALUE, SELECT | COUNTRY | VALUE, "Check eligibility to vote"},
    [PS_VOTE_WEIGHT] = {"ps_vote_weight", true, INSTANCE | AMOUNT, SELECT | COUNTRY | AMOUNT | COMPARE, "Compare a country's voting weight"},
    [PS_VOTE_TOTAL] = {"ps_vote_total", true, INSTANCE | AMOUNT, SELECT | COUNTRY | AMOUNT | COMPARE, "Compare votes supporting a country"},
    [PS_ELECTION_ROUND] = {"ps_election_round", true, INSTANCE | ROUND, SELECT | ROUND, "Check the active electoral term"},
    [PS_SET_ELECTOR] = {"ps_set_elector", false, INSTANCE | VALUE, SELECT | VALUE | AMOUNT, "Grant or revoke the scoped country's legacy electoral role"},
    [PS_CAST_VOTE] = {"ps_cast_vote", false, INSTANCE | COUNTRY | ROUND, SELECT | COUNTRY_TARGET | ROUND, "Vote for a candidate this term"},
    [PS_ABSTAIN] = {"ps_abstain", false, INSTANCE | ROUND, INSTANCE | ROUND, "Legacy command rejected: an elector must choose a candidate"},
    [PS_RESOLVE_ELECTION] = {"ps_resolve_election", false, INSTANCE | ROUND, SELECT | ROUND, "Settle the named term and seat its winner"},
    [PS_IS_MEMBER] = {"political_is_member", true, INSTANCE, SELECT | COUNTRY | VALUE,
                      "Test membership of the current or specified country in one instance"},
    [PS_SET_MEMBER] = {"political_set_member", false, INSTANCE | VALUE, SELECT | COUNTRY_TARGET | VALUE,
                      "Add or remove an explicit or mixed member"},
    [PS_HAS_ROLE] = {"political_has_role", true, INSTANCE | ROLE, SELECT | ROLE | COUNTRY | VALUE,
                     "Test an appointment in one instance"},
    [PS_SET_ROLE] = {"political_set_role", false, INSTANCE | ROLE | COUNTRY | VALUE,
                     SELECT | ROLE | COUNTRY_TARGET | VALUE, "Appoint or revoke a role as the instance leader"},
    [PS_HAS_PROVINCE] = {"political_has_province", true, INSTANCE | PROVINCE, SELECT | PROVINCE | VALUE,
                         "Test whether a province belongs to the instance territory"},
    [PS_HAS_CAPITAL] = {"political_has_capital", true, INSTANCE, SELECT | COUNTRY | VALUE,
                        "Test whether a country's current capital is in the instance territory"},
    [PS_COMPARE_CALCULATED] = {"political_calculated_value", true, INSTANCE | CALCULATED | VALUE,
                               SELECT | CALCULATED | VALUE | COUNTRY | COMPARE, "Compare a read-only calculation"},
    [PS_RUN_ACTION] = {"political_execute_action", false, INSTANCE | ACTION, SELECT | ACTION | ACTOR | TARGET,
                       "Submit a configured political action"},
    [PS_CONTENT_REFORM] = {"political_has_reform", true, INSTANCE | CALCULATED, SELECT | CALCULATED | VALUE,
                            "Test whether an instance has enacted the named reform"},
    [PS_ACTION_ACTIVE] = {"political_action_active", true, INSTANCE | ACTION, SELECT | ACTION | COUNTRY | VALUE,
                           "Test whether a country receives this instance's reform or decree"},
    [PS_START_CONTEST] = {"political_start_contest", false, INSTANCE, SELECT | ROLE,
                          "Open competition for the specified office or leader"},
    [PS_INVEST_CONTEST] = {"political_invest_contest", false, INSTANCE, SELECT | ROLE,
                           "Pay the configured investment as the current country"},
    [PS_CONTEST_WON] = {"political_contest_winner", true, INSTANCE, SELECT | ROLE | COUNTRY | VALUE,
                        "Test the last completed result for an office or leader competition"},
    [PS_HAS_PERMISSION] = {"political_has_permission", true, INSTANCE | PERMISSION, SELECT | PERMISSION | COUNTRY | VALUE,
                           "Test whether a current leader or office gives this permission"},
    [PS_RESOLVE_LEADER] = {"political_resolve_leader", false, INSTANCE, SELECT,
                           "Run an explicitly configured highest-value leader selection"},
    [PS_RESPOND_REQUEST] = {"political_respond_request", false, INSTANCE | ACTION | ROUND | VALUE, SELECT | ACTION | ROUND | VALUE,
                            "Accept or decline a specific invitation as its recipient"},
    [PS_WITHDRAW_REQUEST] = {"political_withdraw_request", false, INSTANCE | ACTION | ROUND | TARGET, SELECT | ACTION | ROUND | TARGET,
                              "Withdraw a specific invitation as its issuer"},
    [PS_RESOLVE_INCIDENT] = {"political_resolve_incident", false, INSTANCE | ACTION | ROUND | OPTION, SELECT | ACTION | ROUND | OPTION,
                             "Resolve a named incident with one configured option"},
    [PS_PROCESS_STATUS] = {"political_process_status", true, INSTANCE | ACTION | STATUS, SELECT | ACTION | STATUS | ROUND | TARGET | OPTION,
                           "Match the state of an invitation or incident in one instance"},
    [PS_CREATE_INSTANCE] = {"political_create_instance", false, INSTANCE | TYPE, INSTANCE | TYPE | NAME | COUNTRY,
                            "Create a political instance as the scoped country"},
    [PS_SUSPEND_INSTANCE] = {"political_suspend_instance", false, INSTANCE, INSTANCE,
                            "Suspend an active political instance as its leader"},
    [PS_RESUME_INSTANCE] = {"political_resume_instance", false, INSTANCE, INSTANCE,
                           "Resume a suspended political instance as its leader"},
    [PS_DISSOLVE_INSTANCE] = {"political_dissolve_instance", false, INSTANCE, INSTANCE,
                              "Dissolve a political instance as its leader"},
    [PS_INSTANCE_STATE] = {"political_instance_state", true, INSTANCE | STATUS, INSTANCE | STATUS,
                           "Match an active, suspended or dissolved instance"},
    [PS_DIRECT_WAR_ALLOWED] = {"political_direct_war_allowed", true, 0, ACTOR | TARGET,
                               "Check political restrictions on direct declaration. Defaults to ROOT attacking the scoped country"},
    [PS_SET_PROVINCE] = {"political_set_province", false, INSTANCE | PROVINCE | VALUE, SELECT | PROVINCE | VALUE,
                         "Add or remove a province in the instance's own territory"},
    [PS_REFORM_LEVEL] = {"political_reform_level", true, INSTANCE | AMOUNT, SELECT | AMOUNT | COMPARE,
                         "Compare the number of reforms passed in this instance"},
    [PS_SET_REFORM] = {"political_set_reform", false, INSTANCE | CALCULATED | VALUE, SELECT | CALCULATED | VALUE | TARGET,
                       "Set a reform's enacted state without submitting the action"},
    [PS_MEMBER_COUNT] = {"political_member_count", true, INSTANCE | AMOUNT, SELECT | AMOUNT | COMPARE,
                         "Compare the number of current members in this instance"},
    [PS_ROLE_COUNT] = {"political_role_count", true, INSTANCE | ROLE | AMOUNT, SELECT | ROLE | AMOUNT | COMPARE,
                       "Compare the number of current holders of a named office"},
    [PS_PROVINCE_COUNT] = {"political_province_count", true, INSTANCE | AMOUNT, SELECT | AMOUNT | COMPARE,
                           "Compare the number of provinces in this instance's territory"},
    [PS_SUPPORTS_CANDIDATE] = {"political_supports_candidate", true, INSTANCE | TARGET, SELECT | COUNTRY | TARGET | VALUE,
                               "Check which candidate the country's current vote supports"},
    [PS_SEND_MESSAGE] = {"political_send_message", false, INSTANCE | MESSAGE, SELECT | MESSAGE | ACTOR | TARGET,
                         "Send a configured native message to the scoped or specified country"},
};

const PsSpelling ps_spellings[] = {
    {"political_has_instance", PS_HAS_INSTANCE},
    {"political_is_leader", PS_IS_LEADER},
    {"political_set_leader", PS_SET_LEADER},
    {"political_clear_leader", PS_CLEAR_LEADER},
    {"political_compare_resource", PS_COMPARE_RESOURCE},
    {"political_set_resource", PS_SET_RESOURCE},
    {"political_change_resource", PS_CHANGE_RESOURCE},
    {"political_is_candidate", PS_IS_CANDIDATE},
    {"political_is_elector", PS_IS_ELECTOR},
    {"political_vote_weight", PS_VOTE_WEIGHT},
    {"political_vote_total", PS_VOTE_TOTAL},
    {"political_election_round", PS_ELECTION_ROUND},
    {"political_cast_vote", PS_CAST_VOTE},
    {"political_resolve_election", PS_RESOLVE_ELECTION},
    {NULL, PS_UNKNOWN},
};

const char *political_mode_name(PoliticalMode mode) {
    static const char *const names[] = { "absent", "active", "suspended", "dissolved" };
    return mode > 0 && mode <= POLITICAL_DISSOLVED ? names[mode] : names[0];
}

PsOperation ps_find_operation(const char *name) {
    for (const PsSpelling *spelling = ps_spellings; spelling->name; ++spelling)
        if (!_stricmp(name, spelling->name)) return spelling->operation;
    for (PsOperation i = PS_UNKNOWN + 1; i < PS_OPERATION_COUNT; ++i)
        if (!_stricmp(ps_definitions[i].name, name)) return i;
    return PS_UNKNOWN;
}

PsField ps_find_field(const char *name) {
    if (!strcmp(name, "compare")) return PS_COMPARISON_FIELD;
    for (PsField i = PS_FIELD_NONE + 1; i < PS_FIELD_COUNT; ++i)
        if (!strcmp(ps_field_names[i], name)) return i;
    return PS_FIELD_NONE;
}

bool ps_identifier(const char *text) {
    const size_t size = strlen(text);
    return size && size < GK_SCRIPT_WORD_SIZE &&
           strspn(text, "0123456789") == 0 &&
           strspn(text, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789") == size;
}

bool ps_reserved(const char *name) {
    return !_strnicmp(name, "ps_", 3) || !_strnicmp(name, "political_", 10);
}

static bool resource_operation(PsOperation operation) {
    return operation == PS_COMPARE_RESOURCE || operation == PS_SET_RESOURCE || operation == PS_CHANGE_RESOURCE;
}

static bool read_country(const char *text, bool has_instance, unsigned *out) {
    if (!strcmp(text, "ROOT")) *out = PS_COUNTRY_ROOT;
    else if (!strcmp(text, "THIS")) *out = PS_COUNTRY_THIS;
    else if (!strcmp(text, "FROM")) *out = PS_COUNTRY_FROM;
    else if (!strcmp(text, "PREV")) *out = PS_COUNTRY_PREV;
    else if (has_instance && !strcmp(text, "LEADER")) *out = PS_COUNTRY_LEADER;
    else {
        if (strlen(text) != 3 || strspn(text, "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") != 3) return false;
        *out = 0;
        memcpy(out, text, 3);
    }
    return true;
}

bool ps_accepts_leader_reference(PsOperation operation) {
    return (ps_definitions[operation].required & INSTANCE) && operation != PS_CREATE_INSTANCE;
}

bool ps_accepts_country_target(PsOperation operation) {
    return operation == PS_SET_LEADER || operation == PS_SET_MEMBER || operation == PS_SET_ROLE || operation == PS_CAST_VOTE;
}

const char *ps_read_argument(PsArguments *args, PsOperation operation, PsField field, const char *text) {
    const unsigned bit = PS_FIELD(field);
    if (!field || !(ps_definitions[operation].allowed & bit)) return "unknown field for this operation";
    if (args->fields & bit) return "duplicate or alias-bound field";
    const bool leader_ref = ps_accepts_leader_reference(operation);
    const char *country_error = leader_ref ? "expected a country tag, ROOT, THIS, FROM, PREV or LEADER"
                                           : "expected a country tag, ROOT, THIS, FROM or PREV";

    switch (field) {
    case PS_INSTANCE_FIELD:
        if (!ps_identifier(text)) return "expected a political instance identifier";
        if (ps_selects_by_type(operation, args)) return "choose either instance or type";
        strcpy(args->instance, text);
        break;
    case PS_COUNTRY_FIELD:
        if (!read_country(text, leader_ref, &args->country)) return country_error;
        break;
    case PS_ACTOR_FIELD:
    case PS_TARGET_FIELD:
        if (!read_country(text, leader_ref, field == PS_ACTOR_FIELD ? &args->actor : &args->target))
            return country_error;
        break;
    case PS_ACTION_FIELD:
        if (!ps_identifier(text)) return "expected an action identifier";
        strcpy(args->action, text);
        break;
    case PS_MESSAGE_FIELD:
        if (!ps_identifier(text)) return "expected a message identifier";
        strcpy(args->message, text);
        break;
    case PS_RESOURCE_FIELD:
    case PS_CALCULATED_FIELD:
        if (!ps_identifier(text)) return "expected a political resource or calculated value identifier";
        strcpy(args->resource, text);
        break;
    case PS_AMOUNT_FIELD: {
        if (resource_operation(operation)) {
            if (!read_fixed_number(text, &args->fixed_amount)) return "expected a decimal with at most three places";
            args->amount = args->fixed_amount / PS_RESOURCE_SCALE;
            break;
        }
        const char *digits = text + (*text == '-');
        if (!*digits || strspn(digits, "0123456789") != strlen(digits) || strlen(digits) > 9)
            return "expected a whole number";
        const long value = strtol(text, NULL, 10);
        const int minimum = operation == PS_CHANGE_RESOURCE ? -GK_MAX_AUTHORITY : 0;
        const bool count = operation == PS_MEMBER_COUNT || operation == PS_ROLE_COUNT || operation == PS_PROVINCE_COUNT;
        const long maximum = operation == PS_VOTE_TOTAL || count ? INT_MAX :
            (operation == PS_VOTE_WEIGHT || operation == PS_SET_ELECTOR ? ELECTION_MAX_WEIGHT : GK_MAX_AUTHORITY);
        if (value < minimum || value > maximum) return "amount outside the supported range";
        args->amount = (int)value;
        break;
    }
    case PS_ROUND_FIELD: {
        if (!*text || strspn(text, "0123456789") != strlen(text)) return "expected a positive round";
        errno = 0;
        unsigned long value = strtoul(text, NULL, 10);
        if (errno || !value || value > INT_MAX) return "round outside range";
        args->round = (unsigned)value;
        break;
    }
    case PS_VALUE_FIELD:
        if (operation == PS_COMPARE_CALCULATED) {
            if (!read_fixed_number(text, &args->fixed_amount)) return "expected a decimal with at most three places";
            break;
        }
        if (!_stricmp(text, "yes")) args->value = true;
        else if (!_stricmp(text, "no")) args->value = false;
        else return "expected yes or no";
        break;
    case PS_COMPARISON_FIELD: {
        static const char *const comparisons[] = {"ge", "eq", "le", "gt", "lt", "ne"};
        size_t i = 0;
        while (i < sizeof(comparisons) / sizeof(*comparisons) && strcmp(text, comparisons[i])) ++i;
        if (i == sizeof(comparisons) / sizeof(*comparisons)) return "expected ge, eq, le, gt, lt or ne";
        args->comparison = (PsComparison)i;
        break;
    }
    case PS_REFORM_FIELD:
        if (!ps_identifier(text)) return "expected a reform identifier";
        strcpy(args->reform, text);
        break;
    case PS_ROLE_FIELD:
        if (!ps_identifier(text)) return "expected a role identifier";
        strcpy(args->role, text);
        break;
    case PS_PERMISSION_FIELD:
        if (!ps_identifier(text)) return "expected a permission identifier";
        strcpy(args->permission, text);
        break;
    case PS_OPTION_FIELD:
        if (!ps_identifier(text)) return "expected an incident option identifier";
        strcpy(args->option, text);
        break;
    case PS_TYPE_FIELD:
        if (!ps_identifier(text)) return "expected a political type identifier";
        if (ps_accepts_type_selector(operation)) {
            if (args->fields & INSTANCE) return "choose either instance or type";
            // Only the selected identifier is needed. At execution it resolves to an instance ID.
            strcpy(args->instance, text);
        } else strcpy(args->type, text);
        break;
    case PS_NAME_FIELD:
        if (!ps_identifier(text)) return "expected a localisation key";
        strcpy(args->name, text);
        break;
    case PS_STATUS_FIELD:
        if (operation == PS_INSTANCE_STATE) {
            for (unsigned mode = POLITICAL_ACTIVE; mode <= POLITICAL_DISSOLVED; ++mode)
                if (!strcmp(text, political_mode_name((PoliticalMode)mode))) { args->status = mode; break; }
            if (!args->status) return "expected active, suspended or dissolved";
            break;
        }
        for (unsigned status = PROCESS_PENDING; status <= PROCESS_SETTLED; ++status)
            if (!strcmp(text, process_status_name((ProcessStatus)status))) { args->status = status; break; }
        if (!args->status) return "unknown political process status";
        break;
    case PS_PROVINCE_FIELD: {
        if (!strcmp(text, "THIS") || !strcmp(text, "ROOT") || !strcmp(text, "FROM") || !strcmp(text, "PREV")) {
            args->province = !strcmp(text, "THIS") ? PS_PROVINCE_THIS :
                             !strcmp(text, "ROOT") ? PS_PROVINCE_ROOT :
                             !strcmp(text, "FROM") ? PS_PROVINCE_FROM : PS_PROVINCE_PREV;
            break;
        }
        if (!*text || strlen(text) > 6 || strspn(text, "0123456789") != strlen(text)) return "expected a province ID";
        const unsigned long value = strtoul(text, NULL, 10);
        if (!value || value > PS_MAX_PROVINCE_ID) return "province ID outside the supported range";
        args->province = (unsigned)value;
        break;
    }
    default:
        return "unknown field";
    }
    args->fields |= bit;
    return NULL;
}

const char *ps_missing_field(PsOperation operation, const PsArguments *args) {
    unsigned missing = ps_definitions[operation].required & ~args->fields;
    if (ps_accepts_type_selector(operation) && (args->fields & TYPE)) missing &= ~INSTANCE;
    if (ps_accepts_country_target(operation) && (args->fields & TARGET)) missing &= ~COUNTRY;
    for (PsField i = PS_FIELD_NONE + 1; i < PS_FIELD_COUNT; ++i)
        if (missing & PS_FIELD(i)) return ps_field_names[i];
    return NULL;
}

bool ps_accepts_type_selector(PsOperation operation) {
    return operation != PS_CREATE_INSTANCE && (ps_definitions[operation].allowed & SELECT) == SELECT;
}

bool ps_selects_by_type(PsOperation operation, const PsArguments *args) {
    return ps_accepts_type_selector(operation) && (args->fields & TYPE);
}

const char *ps_conflicting_fields(PsOperation operation, const PsArguments *args) {
    if (ps_accepts_type_selector(operation) && (args->fields & SELECT) == SELECT)
        return "choose either instance or type";
    if (ps_accepts_country_target(operation) && (args->fields & COUNTRY_TARGET) == COUNTRY_TARGET)
        return "choose either country or target";
    return NULL;
}

bool ps_compare(PsComparison comparison, int actual, int expected) {
    switch (comparison) {
    case PS_GE: return actual >= expected;
    case PS_EQ: return actual == expected;
    case PS_LE: return actual <= expected;
    case PS_GT: return actual > expected;
    case PS_LT: return actual < expected;
    case PS_NE: return actual != expected;
    }
    return false;
}
