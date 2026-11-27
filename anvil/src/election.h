#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { ELECTION_OVERRIDES = 24, ELECTION_MAX_WEIGHT = 99 };
#define ABSTAIN_VOTE UINT32_MAX

typedef struct ElectionRules {
    bool defined, hereditary, tumen_candidates, all_member_voters, incumbent_ties;
    int weight, self_bonus, incumbent_bonus, cooldown_days;
} ElectionRules;

typedef struct ElectionPreference {
    unsigned voter, choice;
    int weight; /* -1 follows the role, zero revokes it. */
} ElectionPreference;

typedef struct ElectionState {
    uint32_t round, last_votes, last_ties;
    uint64_t ruler;
    unsigned last_winner;
    ElectionPreference entries[ELECTION_OVERRIDES];
    unsigned count;
    struct ElectionCooldown { unsigned voter, until; } cooldowns[ELECTION_OVERRIDES];
    unsigned cooldown_count;
} ElectionState;

typedef struct ElectionCountry {
    unsigned tag, support, votes;
    int weight, score;
    bool tumen, candidate, manual, ineligible;
} ElectionCountry;

typedef struct ElectionTable {
    ElectionCountry *rows;
    size_t count;
    unsigned candidates, voters, winner, high, ties;
} ElectionTable;

typedef struct NominationStanding {
    unsigned country;
    int score;
} NominationStanding;

unsigned election_tag(const char text[4]);
const char *election_text(unsigned tag, char text[4]);
ElectionCountry *election_country(const ElectionTable *table, unsigned tag);
const char *election_prefer(ElectionState *state, unsigned voter, unsigned choice, int weight);
int election_base_score(const ElectionRules *rules, unsigned voter, unsigned candidate, unsigned incumbent, int opinion);
void election_tally(const ElectionRules *rules, const ElectionState *state, ElectionTable *table,
                    unsigned incumbent, const int16_t *opinions, const int16_t *nomination);
unsigned election_rank_candidates(const ElectionRules *rules, const ElectionTable *table,
                                  const ElectionCountry *elector, unsigned incumbent,
                                  const int16_t *opinions, const int16_t *nomination,
                                  NominationStanding *standings, unsigned capacity);
unsigned election_after_days(unsigned date, unsigned days);
unsigned election_locked_until(const ElectionState *state, unsigned actor, unsigned today);
unsigned migrate_withdrawn_ballots(ElectionState *state);
bool election_prune(ElectionState *state, const ElectionTable *table);
const char *election_cast(ElectionState *state, const ElectionTable *table, unsigned round, unsigned actor,
                          unsigned target, unsigned today, unsigned days);
const char *election_finish(ElectionState *state, const ElectionTable *table, unsigned round);
