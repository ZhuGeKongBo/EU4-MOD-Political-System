#include "election.h"
#include <limits.h>
#include <string.h>

unsigned election_tag(const char text[4]) {
    unsigned tag = 0;
    memcpy(&tag, text, 3);
    return tag;
}

const char *election_text(unsigned tag, char text[4]) {
    memcpy(text, &tag, 3);
    text[3] = 0;
    return text;
}

static bool precedes(unsigned left, unsigned right) {
    return !right || memcmp(&left, &right, 3) < 0;
}

ElectionCountry *election_country(const ElectionTable *table, unsigned tag) {
    for (size_t i = 0; i < table->count; ++i)
        if (table->rows[i].tag == tag) return &table->rows[i];
    return NULL;
}

static unsigned preference_index(const ElectionState *state, unsigned voter) {
    for (unsigned i = 0; i < state->count; ++i)
        if (state->entries[i].voter == voter) return i;
    return state->count;
}

const char *election_prefer(ElectionState *state, unsigned voter, unsigned choice, int weight) {
    if (!voter || weight < -1 || weight > ELECTION_MAX_WEIGHT) return "invalid elector or vote weight";
    const unsigned i = preference_index(state, voter);
    if (!choice && weight == -1) {
        if (i < state->count) state->entries[i] = state->entries[--state->count];
        return NULL;
    }
    if (i == state->count) {
        if (state->count == ELECTION_OVERRIDES) return "electoral preference table is full";
        ++state->count;
    }
    state->entries[i] = (ElectionPreference){voter, choice, weight};
    return NULL;
}

int election_base_score(const ElectionRules *rules, unsigned voter, unsigned candidate, unsigned incumbent, int opinion) {
    return (voter == candidate ? rules->self_bonus : opinion) + (candidate == incumbent ? rules->incumbent_bonus : 0);
}

static int candidate_score(const ElectionRules *rules, const ElectionTable *table, size_t voter, size_t candidate,
                           unsigned incumbent, const int16_t *opinions, const int16_t *nomination) {
    const size_t pair = voter * table->count + candidate;
    return nomination ? nomination[pair] :
        election_base_score(rules, table->rows[voter].tag, table->rows[candidate].tag, incumbent,
                            opinions ? opinions[pair] : 0);
}

unsigned election_rank_candidates(const ElectionRules *rules, const ElectionTable *table,
                                  const ElectionCountry *elector, unsigned incumbent,
                                  const int16_t *opinions, const int16_t *nomination,
                                  NominationStanding *standings, unsigned capacity) {
    const size_t voter = (size_t)(elector - table->rows);
    unsigned count = 0;
    for (size_t i = 0; i < table->count; ++i) {
        const ElectionCountry *candidate = &table->rows[i];
        if (!candidate->candidate) continue;
        const int score = candidate_score(rules, table, voter, i, incumbent, opinions, nomination);
        unsigned position = 0;
        while (position < count &&
               (standings[position].score > score ||
                (standings[position].score == score && precedes(standings[position].country, candidate->tag)))) ++position;
        if (position >= capacity) continue;
        if (count < capacity) ++count;
        memmove(standings + position + 1, standings + position,
                (count - position - 1) * sizeof(*standings));
        standings[position] = (NominationStanding){candidate->tag, score};
    }
    return count;
}

void election_tally(const ElectionRules *rules, const ElectionState *state, ElectionTable *table,
                    unsigned incumbent, const int16_t *opinions, const int16_t *nomination) {
    table->voters = table->candidates = table->winner = table->high = table->ties = 0;
    for (size_t i = 0; i < table->count; ++i) {
        ElectionCountry *row = &table->rows[i];
        unsigned pref = preference_index(state, row->tag);
        int weight = pref < state->count ? state->entries[pref].weight : -1;
        if (weight >= 0) row->tumen = weight > 0;
        row->weight = weight >= 0 ? weight : (row->tumen || rules->all_member_voters ? rules->weight : 0);
        row->candidate = !row->ineligible && (!rules->tumen_candidates || row->tumen);
        row->votes = row->support = 0;
        row->score = 0;
        row->manual = false;
        table->voters += row->weight > 0;
        table->candidates += row->candidate;
    }
    for (size_t i = 0; i < table->count; ++i) {
        ElectionCountry *voter = &table->rows[i];
        if (!voter->weight) continue;
        const unsigned p = preference_index(state, voter->tag);
        const unsigned choice = p < state->count ? state->entries[p].choice : 0;
        ElectionCountry *target = choice ? election_country(table, choice) : NULL;
        if (target && target->candidate) {
            voter->manual = true;
        } else {
            target = NULL;
            int best = INT_MIN;
            for (size_t j = 0; j < table->count; ++j) {
                ElectionCountry *candidate = &table->rows[j];
                if (!candidate->candidate) continue;
                const int score = candidate_score(rules, table, i, j, incumbent, opinions, nomination);
                if (!target || score > best || (score == best && precedes(candidate->tag, target->tag))) {
                    target = candidate;
                    best = score;
                }
            }
            if (target) voter->score = best;
        }
        if (target) {
            voter->support = target->tag;
            target->votes += (unsigned)voter->weight;
        }
    }
    for (size_t i = 0; i < table->count; ++i) {
        const ElectionCountry *row = &table->rows[i];
        if (!row->votes || row->votes < table->high) continue;
        if (row->votes > table->high) {
            table->winner = row->tag;
            table->high = row->votes;
            table->ties = 1;
        } else {
            ++table->ties;
            if (rules->incumbent_ties && (row->tag == incumbent || table->winner == incumbent)) {
                if (row->tag == incumbent) table->winner = incumbent;
            } else if (precedes(row->tag, table->winner)) table->winner = row->tag;
        }
    }
}

bool election_prune(ElectionState *state, const ElectionTable *table) {
    bool changed = false;
    for (unsigned n = state->count; n > 0; --n) {
        const ElectionPreference entry = state->entries[n - 1];
        const ElectionCountry *voter = election_country(table, entry.voter);
        const ElectionCountry *target = election_country(table, entry.choice);
        if (entry.choice && (!voter || !voter->weight || !target || !target->candidate)) {
            election_prefer(state, entry.voter, 0, entry.weight);
            changed = true;
        }
    }
    return changed;
}

unsigned election_after_days(unsigned date, unsigned days) {
    static const unsigned lengths[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    unsigned y = date / 10000, m = date / 100 % 100, d = date % 100;
    if (!y || y > 9999 || !m || m > 12 || !d || d > lengths[m-1] || days > 36500) return 0;
    d += days;
    while (d > lengths[m-1]) {
        d -= lengths[m-1];
        if (++m == 13) { m = 1; ++y; }
    }
    return y <= 9999 ? y*10000 + m*100 + d : 99991231;
}

unsigned election_locked_until(const ElectionState *s, unsigned actor, unsigned today) {
    for (unsigned i = 0; i < s->cooldown_count; ++i)
        if (s->cooldowns[i].voter == actor && s->cooldowns[i].until > today) return s->cooldowns[i].until;
    return 0;
}

unsigned migrate_withdrawn_ballots(ElectionState *state) {
    unsigned restored = 0;
    for (unsigned n = state->count; n; --n) {
        const ElectionPreference old = state->entries[n-1];
        if (old.choice != ABSTAIN_VOTE) continue;
        election_prefer(state,old.voter,0,old.weight);
        for (unsigned i=0; i<state->cooldown_count; ++i) {
            if (state->cooldowns[i].voter != old.voter) continue;
            state->cooldowns[i] = state->cooldowns[--state->cooldown_count];
            break;
        }
        ++restored;
    }
    return restored;
}

const char *election_cast(ElectionState *state, const ElectionTable *table, unsigned round, unsigned actor,
                          unsigned target, unsigned today, unsigned days) {
    if (!round || round != state->round) return "closed election round";
    const ElectionCountry *voter = election_country(table, actor);
    if (!voter || !voter->weight) return "country has no vote";
    const ElectionCountry *candidate = election_country(table, target);
    if (!candidate || !candidate->candidate) return "country is not a candidate";
    const unsigned i = preference_index(state, actor);
    if (election_locked_until(state, actor, today)) {
        if (i < state->count && state->entries[i].choice == target) return NULL;
        return "vote change is on cooldown";
    }
    const unsigned until = election_after_days(today, days);
    if (!until) return "invalid voting date";
    for (unsigned j = state->cooldown_count; j > 0; --j)
        if (state->cooldowns[j-1].until <= today) state->cooldowns[j-1] = state->cooldowns[--state->cooldown_count];
    if (days && state->cooldown_count == ELECTION_OVERRIDES) return "electoral cooldown table is full";
    const char *error = election_prefer(state, actor, target, i < state->count ? state->entries[i].weight : -1);
    if (!error && days) state->cooldowns[state->cooldown_count++] = (struct ElectionCooldown){actor,until};
    return error;
}

const char *election_finish(ElectionState *state, const ElectionTable *table, unsigned round) {
    if (!round || round != state->round || round >= INT_MAX) return "closed election round";
    state->last_winner = table->winner;
    state->last_votes = table->high;
    state->last_ties = table->ties;
    ++state->round;
    return NULL;
}
