#pragma once

#include "script_types.h"

#define PS_AUTHORITY "authority"
#define PS_ALIASES_PATH "common/political_systems/script_aliases.txt"

typedef enum PsField {
    PS_FIELD_NONE,
    PS_INSTANCE_FIELD,
    PS_COUNTRY_FIELD,
    PS_RESOURCE_FIELD,
    PS_AMOUNT_FIELD,
    PS_VALUE_FIELD,
    PS_COMPARISON_FIELD,
    PS_REFORM_FIELD,
    PS_ROUND_FIELD,
    PS_ROLE_FIELD,
    PS_PROVINCE_FIELD,
    PS_CALCULATED_FIELD,
    PS_ACTION_FIELD, PS_ACTOR_FIELD, PS_TARGET_FIELD,
    PS_PERMISSION_FIELD,
    PS_OPTION_FIELD, PS_STATUS_FIELD,
    PS_TYPE_FIELD, PS_NAME_FIELD, PS_MESSAGE_FIELD,
    PS_FIELD_COUNT,
} PsField;

#define PS_FIELD(field) (1u << (field))

typedef enum PsOperation {
    PS_UNKNOWN,
    PS_HAS_INSTANCE,
    PS_IS_LEADER,
    PS_COMPARE_RESOURCE,
    PS_IS_REFORM_PASSED,
    PS_SET_LEADER,
    PS_CLEAR_LEADER,
    PS_SET_RESOURCE,
    PS_CHANGE_RESOURCE,
    PS_EXECUTE_ACTION,
    PS_IS_CANDIDATE, PS_IS_ELECTOR, PS_VOTE_WEIGHT, PS_VOTE_TOTAL, PS_ELECTION_ROUND,
    PS_SET_ELECTOR, PS_CAST_VOTE, PS_ABSTAIN, PS_RESOLVE_ELECTION,
    PS_IS_MEMBER,
    PS_SET_MEMBER,
    PS_HAS_ROLE,
    PS_SET_ROLE,
    PS_HAS_PROVINCE,
    PS_HAS_CAPITAL,
    PS_COMPARE_CALCULATED,
    PS_RUN_ACTION, PS_CONTENT_REFORM, PS_ACTION_ACTIVE,
    PS_START_CONTEST, PS_INVEST_CONTEST, PS_CONTEST_WON, PS_HAS_PERMISSION, PS_RESOLVE_LEADER,
    PS_RESPOND_REQUEST, PS_WITHDRAW_REQUEST, PS_RESOLVE_INCIDENT, PS_PROCESS_STATUS,
    PS_CREATE_INSTANCE, PS_SUSPEND_INSTANCE, PS_RESUME_INSTANCE, PS_DISSOLVE_INSTANCE, PS_INSTANCE_STATE,
    PS_DIRECT_WAR_ALLOWED,
    PS_SET_PROVINCE, PS_REFORM_LEVEL, PS_SET_REFORM,
    PS_MEMBER_COUNT, PS_ROLE_COUNT, PS_PROVINCE_COUNT, PS_SUPPORTS_CANDIDATE,
    PS_SEND_MESSAGE,
    PS_OPERATION_COUNT,
} PsOperation;

typedef enum PoliticalMode { POLITICAL_ACTIVE = 1, POLITICAL_SUSPENDED, POLITICAL_DISSOLVED } PoliticalMode;
const char *political_mode_name(PoliticalMode mode);

typedef enum PsComparison {
    PS_GE, PS_EQ, PS_LE, PS_GT, PS_LT, PS_NE,
} PsComparison;

typedef struct PsDefinition {
    const char *name;
    bool trigger;
    unsigned required, allowed;
    const char *description;
} PsDefinition;

typedef struct PsArguments {
    unsigned fields;
    unsigned country;
    unsigned round;
    unsigned province;
    unsigned actor, target;
    unsigned status;
    int amount;
    int32_t fixed_amount;
    bool value;
    PsComparison comparison;
    char instance[GK_SCRIPT_WORD_SIZE];
    union { char option[GK_SCRIPT_WORD_SIZE]; char name[GK_SCRIPT_WORD_SIZE]; char message[GK_SCRIPT_WORD_SIZE]; };
    union { char reform[GK_SCRIPT_WORD_SIZE]; char role[GK_SCRIPT_WORD_SIZE]; char resource[GK_SCRIPT_WORD_SIZE];
            char action[GK_SCRIPT_WORD_SIZE]; char permission[GK_SCRIPT_WORD_SIZE]; char type[GK_SCRIPT_WORD_SIZE]; };
} PsArguments;

enum { PS_COUNTRY_ROOT = 0x01000000u, PS_COUNTRY_THIS = 0x02000000u,
       PS_COUNTRY_LEADER = 0x03000000u, PS_COUNTRY_FROM = 0x04000000u, PS_COUNTRY_PREV = 0x05000000u,
       PS_PROVINCE_THIS = PS_MAX_PROVINCE_ID + 1, PS_PROVINCE_ROOT, PS_PROVINCE_FROM, PS_PROVINCE_PREV };

typedef struct PsAlias {
    char name[GK_SCRIPT_WORD_SIZE];
    PsOperation operation;
    PsArguments bound;
    PsField argument;
    unsigned line, column;
} PsAlias;

enum { PS_MAX_ALIASES = 64 };
typedef struct PsAliases {
    PsAlias entries[PS_MAX_ALIASES];
    size_t count;
} PsAliases;

extern const PsDefinition ps_definitions[PS_OPERATION_COUNT];
extern const char *const ps_field_names[PS_FIELD_COUNT];
typedef struct PsSpelling { const char *name; PsOperation operation; } PsSpelling;
extern const PsSpelling ps_spellings[];

PsOperation ps_find_operation(const char *name);
PsField ps_find_field(const char *name);
bool ps_identifier(const char *text);
bool ps_reserved(const char *name);
const char *ps_read_argument(PsArguments *args, PsOperation operation, PsField field, const char *text);
const char *ps_missing_field(PsOperation operation, const PsArguments *args);
const char *ps_conflicting_fields(PsOperation operation, const PsArguments *args);
bool ps_accepts_type_selector(PsOperation operation);
bool ps_selects_by_type(PsOperation operation, const PsArguments *args);
bool ps_accepts_leader_reference(PsOperation operation);
bool ps_accepts_country_target(PsOperation operation);
bool ps_compare(PsComparison comparison, int actual, int expected);
bool read_ps_aliases(const char *text, size_t size, PsAliases *aliases, ScriptError *error);
