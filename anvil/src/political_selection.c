#include "political_selection.h"

const char *nomination_terms(const NominationRules *rules, const EntryContext *context,
                             bool for_elector, unsigned *mask) {
    *mask = 0;
    for (unsigned i = 0; i < rules->count; ++i) {
        const NominationTerm *term = &rules->terms[i];
        const unsigned index = for_elector ? term->elector_root : term->candidate_root;
        bool applies = true;
        if (index) {
            const char *reason = entry_evaluate_node(rules->nodes, index - 1, context, &applies);
            if (reason) return reason;
        }
        if (applies) *mask |= 1u << i;
    }
    return NULL;
}

int nomination_score(const NominationRules *rules, int opinion, int bonuses, unsigned terms) {
    int score = rules->base + opinion * rules->opinion_weight + bonuses;
    for (unsigned i = 0; i < rules->count; ++i)
        if (terms & (1u << i)) score += rules->terms[i].add;
    if (score < rules->minimum) return rules->minimum;
    if (score > rules->maximum) return rules->maximum;
    return score;
}

#include "election.h"

#include <string.h>

bool grants_permission(const PoliticalPermissions *permissions, const char *id) {
    for (unsigned i = 0; i < permissions->count; ++i)
        if (!strcmp(permissions->keys[i], id)) return true;
    return false;
}

bool better_candidate(unsigned tag, int32_t score, unsigned current, int32_t best,
                      unsigned incumbent, bool incumbent_ties) {
    if (!current || score != best) return !current || score > best;
    if (incumbent_ties && (tag == incumbent || current == incumbent)) return tag == incumbent;
    char candidate[4], previous[4];
    return strcmp(election_text(tag, candidate), election_text(current, previous)) < 0;
}
