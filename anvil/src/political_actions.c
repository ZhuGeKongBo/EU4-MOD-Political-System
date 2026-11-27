#include "political_actions.h"

#include <string.h>

unsigned political_action_index(const PoliticalAction *actions, unsigned count, const char *id) {
    unsigned i = 0;
    while (i < count && strcmp(actions[i].id, id)) ++i;
    return i;
}

const PoliticalActionRun *find_action_run(const PoliticalActionState *state, unsigned slot, unsigned actor) {
    for (unsigned i = 0; i < state->count; ++i) {
        const PoliticalActionRun *run = &state->runs[i];
        if (run->slot == slot && (!actor || run->actor == actor)) return run;
    }
    return NULL;
}

bool political_action_running(const PoliticalAction *action, const PoliticalActionRun *run, unsigned today) {
    return run && today >= run->performed && (action->kind == ACTION_REFORM ||
           (action->kind == ACTION_DECREE && today < run->expires));
}

unsigned political_process_index(const PoliticalProcesses *state, unsigned slot, unsigned target) {
    for (unsigned i = 0; i < state->count; ++i) {
        const PoliticalProcess *process = &state->items[i];
        if (process->slot == slot && process->target == target) return i;
    }
    return state->count;
}

const char *process_status_name(ProcessStatus status) {
    static const char *const names[] = { "none", "pending", "accepted", "rejected", "expired", "invalid", "withdrawn", "settled" };
    return status > 0 && status <= PROCESS_SETTLED ? names[status] : names[0];
}

unsigned incident_option_index(const PoliticalAction *action, const char *id) {
    unsigned i = 0;
    while (i < action->option_count && strcmp(action->options[i].id, id)) ++i;
    return i;
}
