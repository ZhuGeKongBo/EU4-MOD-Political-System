#pragma once

#include "entry_config.h"
#include "ps_script.h"

enum { PS_RESOURCE_SCALE = 1000, PS_MAX_RESOURCES = 8, PS_MAX_RESOURCE_TERMS = 8,
       PS_MAX_THRESHOLDS = 8, PS_MAX_VALUES = 8, PS_RESOURCE_CHUNK = 16 };

typedef enum ResourceOwner { RESOURCE_INSTANCE, RESOURCE_MEMBER, RESOURCE_LEADER } ResourceOwner;
typedef enum GrowthModifierSource { GROWTH_LEADER, GROWTH_ACCOUNT, GROWTH_LEADER_CAPITAL, GROWTH_ACCOUNT_CAPITAL } GrowthModifierSource;

typedef struct ResourceThreshold {
    char id[GK_SCRIPT_WORD_SIZE], message[GK_SCRIPT_WORD_SIZE];
    int32_t at;
    bool falling;
} ResourceThreshold;

typedef struct ResourceTerm {
    char id[GK_SCRIPT_WORD_SIZE], calculation[GK_SCRIPT_WORD_SIZE];
    char operand[GK_SCRIPT_WORD_SIZE], tooltip[GK_SCRIPT_WORD_SIZE];
    int32_t add, modifier, compare_to;
    PsComparison comparison;
    EntryTest test;
    PsOperation political_test;
    bool leader_scope, expected;
    NativeClause native;
} ResourceTerm;

typedef struct ResourceDefinition {
    char id[GK_SCRIPT_WORD_SIZE], name[GK_SCRIPT_WORD_SIZE], tooltip[GK_SCRIPT_WORD_SIZE];
    char script_description[GK_SCRIPT_WORD_SIZE];
    int32_t minimum, maximum, initial, monthly;
    ResourceOwner owner;
    unsigned precision, term_count, threshold_count;
    bool retain, loss_defined;
    char growth_modifier[GK_SCRIPT_WORD_SIZE];
    GrowthModifierSource modifier_source;
    ResourceTerm terms[PS_MAX_RESOURCE_TERMS];
    ResourceThreshold thresholds[PS_MAX_THRESHOLDS];
} ResourceDefinition;

typedef struct ResourceAccount {
    unsigned country;
    int32_t balance;
} ResourceAccount;

typedef struct ResourceLedger {
    unsigned count;
    ResourceAccount accounts[PS_MAX_EXPLICIT_MEMBERS];
} ResourceLedger;

typedef enum CalculationKind { VALUE_MANPOWER, VALUE_MEMBER_COUNT } CalculationKind;

typedef struct CalculationDefinition {
    char id[GK_SCRIPT_WORD_SIZE], member_value[GK_SCRIPT_WORD_SIZE];
    CalculationKind kind;
    PsComparison comparison;
} CalculationDefinition;

typedef struct ResourceInitial {
    char id[GK_SCRIPT_WORD_SIZE];
    int32_t balance;
} ResourceInitial;

bool read_fixed_number(const char *text, int32_t *out);
void format_fixed_number(int32_t value, unsigned precision, char text[24]);
int32_t resource_limit(const ResourceDefinition *definition, int64_t value);
unsigned resource_find(const ResourceDefinition *definitions, unsigned count, const char *id);
unsigned calculation_find(const CalculationDefinition *definitions, unsigned count, const char *id);
int32_t resource_balance(const ResourceDefinition *definition, const ResourceLedger *ledger, unsigned country);
bool store_balance(ResourceLedger *ledger, unsigned country, int32_t balance);
unsigned crossed_thresholds(const ResourceDefinition *definition, int32_t before, int32_t after);
bool growth_modifier_is_local(const ResourceDefinition *definition);
