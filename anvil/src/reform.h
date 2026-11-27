#pragma once

#include "political_script.h"

typedef enum ReformStatus {
    REFORM_AVAILABLE,
    REFORM_PASSED,
    REFORM_NO_LEADER,
    REFORM_NOT_LEADER,
    REFORM_LOW_AUTHORITY,
    REFORM_DEFINITION_CHANGED,
    REFORM_ENACTED,
} ReformStatus;

typedef struct ReformState {
    int authority;
    char passed_id[GK_SCRIPT_WORD_SIZE];
} ReformState;

typedef void (*ReformEffect)(void *country, int32_t prestige, const KhanReform *definition);

ReformStatus evaluate_reform(const ReformState *state, const KhanReform *definition,
                            unsigned actor, unsigned leader);
ReformStatus pass_reform(ReformState *state, const KhanReform *definition,
                        unsigned actor, unsigned leader, void *country, ReformEffect effect);
