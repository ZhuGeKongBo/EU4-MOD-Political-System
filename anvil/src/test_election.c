#include "political_script.h"
#include "save_state.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static ElectionTable recount(const ElectionRules *rules, const ElectionState *state,
                             ElectionCountry *rows, unsigned holder, unsigned count) {
    const char *tags[] = {"OIR", "KHA", "CHG", "KRC"};
    for (unsigned i = 0; i < count; ++i)
        rows[i] = (ElectionCountry){ .tag = election_tag(tags[i]), .tumen = i == 0 };
    ElectionTable table = { .rows = rows, .count = count };
    int16_t opinions[16] = {0};
    for (unsigned i=0; i<count; ++i)
        for (unsigned j=0; j<count; ++j)
            opinions[i*count+j] = i == 0 && j == 1 ? 80 : -20;
    election_tally(rules, state, &table, holder, opinions, NULL);
    return table;
}

int main(void) {
    KhanDefinition definition;
    ScriptError err;
    const char type[] = "foo={emperor=khan members={source=capital_provinces} election={"
        "voters=tumen candidates=members vote_weight=1 tie_break=incumbent_then_tag succession=ruler_change self_bonus=50 incumbent_bonus=20}}";
    assert(read_type_definition(type, sizeof(type)-1, "foo", &definition, &err));
    const char invalid[] = "foo={emperor=khan members={source=capital_provinces} election={vote_weight=0}}";
    assert(!read_type_definition(invalid, sizeof(invalid)-1, "foo", &definition, &err));
    ElectionRules rules = definition.election;
    const unsigned KHA = election_tag("KHA"), OIR = election_tag("OIR"), CHG = election_tag("CHG");
    ElectionState state = {.round=1}, other = {.round=1};
    ElectionCountry rows[4];
    ElectionTable table = recount(&rules,&state,rows,KHA,4);
    assert(table.candidates == 4 && table.voters == 1 && table.winner == KHA && rows[0].score == 100);
    assert(election_cast(&state,&table,1,CHG,CHG,14441111,0) && election_cast(&state,&table,1,OIR,election_tag("MNG"),14441111,0));
    assert(!election_cast(&state,&table,1,OIR,OIR,14441111,0));
    assert(!election_prefer(&state,CHG,CHG,2) && !election_prefer(&state,KHA,OIR,1));
    table = recount(&rules,&state,rows,KHA,4);
    assert(table.high == 2 && table.ties == 2 && table.winner == CHG && rows[2].votes == 2 && rows[0].votes == 2);
    table = recount(&rules,&state,rows,OIR,4);
    assert(table.winner == OIR);
    rules.incumbent_ties = false;
    table = recount(&rules,&state,rows,OIR,4);
    assert(table.winner == CHG);
    assert(!election_finish(&state,&table,1) && election_finish(&state,&table,1));
    assert(state.round == 2 && state.last_winner == CHG && state.last_votes == 2 && state.last_ties == 2);
    assert(election_cast(&state,&table,1,OIR,KHA,14441111,0));
    assert(other.round == 1 && other.count == 0);
    state.ruler = UINT64_C(18446744073709551017);
    char save[GK_SAVE_VALUE_SIZE], id[GK_SCRIPT_WORD_SIZE];
    ElectionState restored;
    assert(encode_election_state("second_instance",&state,save));
    assert(decode_election_state(save,id,&restored));
    assert(!strcmp(id,"second_instance") && restored.ruler == state.ruler && restored.round == 2);
    assert(restored.last_ties == 2 && restored.count == 3);
    char again[GK_SAVE_VALUE_SIZE];
    assert(encode_election_state(id,&restored,again) && !strcmp(save,again));
    table = recount(&rules,&restored,rows,OIR,2);
    assert(election_prune(&restored,&table));
    table = recount(&rules,&restored,rows,OIR,4);
    assert(!election_country(&table,CHG)->manual);
    assert(election_cast(&restored,&table,2,OIR,ABSTAIN_VOTE,14441111,0));
    table = recount(&rules,&restored,rows,OIR,1);
    assert(table.candidates == 1 && table.voters == 1 && table.winner==OIR);
    assert(!election_cast(&restored,&table,2,OIR,OIR,14441111,0));
    table = recount(&rules,&restored,rows,OIR,1);
    assert(table.winner == OIR && table.high == 1);
    assert(!election_prefer(&restored,OIR,0,0));
    table = recount(&rules,&restored,rows,OIR,1);
    assert(table.candidates == 1 && !table.voters && !table.winner);
    rules.tumen_candidates = true;
    table = recount(&rules,&restored,rows,OIR,1);
    assert(!table.candidates && !table.winner);
    assert(!election_finish(&restored,&table,2) && !restored.last_winner);
    table = recount(&rules,&other,rows,0,0);
    assert(!table.candidates && !table.voters && !table.winner);
    strcpy(save,"E1|first|1|0|---|0|0|OIR--KHAOIR--KHA");
    assert(!decode_election_state(save,id,&other));
    strcpy(save,"E1|first|1|18446744073709551616|---|0|0|");
    assert(!decode_election_state(save,id,&other));
    ElectionState locked = { .round = 1 };
    rules.tumen_candidates = false;
    table = recount(&rules,&locked,rows,KHA,4);
    assert(!election_cast(&locked,&table,1,OIR,KHA,14441111,60));
    assert(election_after_days(14441111,60) == 14450110);
    assert(!strcmp(election_cast(&locked,&table,1,OIR,CHG,14450109,60),"vote change is on cooldown"));
    assert(encode_election_cooldowns("gkhanate_main",&locked,save));
    assert(decode_election_cooldowns(save,id,&restored));
    assert(election_locked_until(&restored,OIR,14450109) == 14450110);
    assert(!election_cast(&locked,&table,1,OIR,CHG,14450110,60));
    strcpy(save,"E1|gkhanate_main|3|0|CHG|2|2|OIR--___KHA02___CHG01CHG");
    assert(decode_election_state(save,id,&restored));
    strcpy(save,"C1|gkhanate_main|OIR14450110KHA14450110CHG14450110");
    assert(decode_election_cooldowns(save,id,&restored));
    assert(migrate_withdrawn_ballots(&restored)==2 && !migrate_withdrawn_ballots(&restored));
    assert(restored.cooldown_count==1 && election_locked_until(&restored,CHG,14441111)==14450110);
    assert(!election_locked_until(&restored,OIR,14441111));
    table=recount(&rules,&restored,rows,CHG,4);
    unsigned weight=0, votes=0;
    for(size_t i=0;i<table.count;++i){
        if(rows[i].weight){ assert(rows[i].support && election_country(&table,rows[i].support)->candidate); }
        weight+=(unsigned)rows[i].weight; votes+=rows[i].votes;
    }
    assert(weight==votes && rows[1].weight==2);
    assert(encode_election_state(id,&restored,save) && !strstr(save,"___"));
    puts("PASS game calendar, first vote, cooldown refusal, exact unlock date and persisted cooldown.");
    puts("PASS compulsory valid ballots, old abstention migration with preserved weights and other locks, both tie rules and isolated save round trip.");
    return 0;
}
