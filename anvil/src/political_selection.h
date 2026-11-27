#pragma once

#include "political_resources.h"

enum { PS_MAX_PERMISSIONS = 8, PS_CONTEST_CHUNK = 13 };
enum { PS_NOMINATION_TERMS = 12, PS_NOMINATION_LIMIT = 2000 };

typedef struct NominationTerm {
    char id[GK_SCRIPT_WORD_SIZE], tooltip[GK_SCRIPT_WORD_SIZE];
    unsigned elector_root, candidate_root; /* One-based indices into the shared node pool. */
    int add;
} NominationTerm;

typedef struct NominationRules {
    bool defined;
    int minimum, maximum, base, opinion_weight;
    char tooltip[GK_SCRIPT_WORD_SIZE];
    unsigned count, node_count;
    NominationTerm terms[PS_NOMINATION_TERMS];
    EntryNode nodes[ENTRY_MAX_NODES];
} NominationRules;

const char *nomination_terms(const NominationRules *rules, const EntryContext *context,
                             bool for_elector, unsigned *mask);
int nomination_score(const NominationRules *rules, int opinion, int bonuses, unsigned terms);

typedef struct PoliticalPermissions {
    unsigned count;
    char keys[PS_MAX_PERMISSIONS][GK_SCRIPT_WORD_SIZE];
} PoliticalPermissions;

typedef enum OfficeAcquisition { OFFICE_APPOINTED, OFFICE_EXPLICIT, OFFICE_COMPETITION } OfficeAcquisition;
typedef enum LeaderMethod { LEADER_EXPLICIT, LEADER_APPOINTED, LEADER_ELECTED, LEADER_COMPETITION, LEADER_HIGHEST } LeaderMethod;
typedef enum SelectionEvent { SELECT_EXPLICIT, SELECT_VACANCY, SELECT_RULER_CHANGE, SELECT_THRESHOLD } SelectionEvent;
typedef enum ElectionCooldownPolicy { COOLDOWN_RESET, COOLDOWN_RESTART, COOLDOWN_RETAIN } ElectionCooldownPolicy;

typedef struct PoliticalContestRule {
    char investment[GK_SCRIPT_WORD_SIZE], score[GK_SCRIPT_WORD_SIZE];
    unsigned period;
    int32_t cost;
    bool incumbent_ties, has_tie_rule;
} PoliticalContestRule;

typedef struct LeaderSelection {
    LeaderMethod mode;
    SelectionEvent event;
    ElectionCooldownPolicy cooldown_policy;
    bool defined, require_member, has_cooldown_policy;
    char value[GK_SCRIPT_WORD_SIZE], elector_role[GK_SCRIPT_WORD_SIZE];
    char threshold_resource[GK_SCRIPT_WORD_SIZE], threshold[GK_SCRIPT_WORD_SIZE];
    EntryCondition candidate, retain;
    PoliticalPermissions permissions;
    PoliticalContestRule contest;
    NominationRules nomination;
} LeaderSelection;

typedef struct PoliticalContest {
    unsigned opened, due, winner;
    bool settled;
    ResourceLedger bids;
} PoliticalContest;

bool grants_permission(const PoliticalPermissions *permissions, const char *id);
bool better_candidate(unsigned tag, int32_t score, unsigned current, int32_t best,
                      unsigned incumbent, bool incumbent_ties);
