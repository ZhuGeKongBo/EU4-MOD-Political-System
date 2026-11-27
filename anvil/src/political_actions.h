#pragma once

#include "political_resources.h"

enum { PS_MAX_ACTIONS = 16, PS_MAX_ACTION_RUNS = 64, PS_ACTION_LINKS = 8,
       PS_MAX_INCIDENT_OPTIONS = 6, PS_MAX_PROCESSES = PS_MAX_EXPLICIT_MEMBERS };

typedef enum ActionKind { ACTION_REFORM = 1, ACTION_DECISION, ACTION_DECREE, ACTION_INVITATION, ACTION_INCIDENT } ActionKind;
typedef enum ActionActor { ACTION_ANY, ACTION_LEADER, ACTION_MEMBER, ACTION_PERMISSION } ActionActor;
typedef enum ActionPayer { ACTION_INSTANCE_PAYS, ACTION_ACTOR_PAYS, ACTION_TARGET_PAYS } ActionPayer;
typedef enum ActionBeneficiaries { ACTION_BENEFIT_ACTOR, ACTION_BENEFIT_LEADER, ACTION_BENEFIT_MEMBERS, ACTION_BENEFIT_TARGET } ActionBeneficiaries;
typedef enum ActionModifierRecipient {
    ACTION_MODIFIER_COUNTRY, ACTION_MODIFIER_LEADER, ACTION_MODIFIER_MEMBER,
    ACTION_MODIFIER_PROVINCE, ACTION_MODIFIER_COUNT,
} ActionModifierRecipient;

typedef struct IncidentOption {
    char id[GK_SCRIPT_WORD_SIZE], name[GK_SCRIPT_WORD_SIZE], tooltip[GK_SCRIPT_WORD_SIZE];
    int32_t prestige;
    NativeClause trigger, effect;
} IncidentOption;

typedef struct PoliticalAction {
    char id[GK_SCRIPT_WORD_SIZE], name[GK_SCRIPT_WORD_SIZE], tooltip[GK_SCRIPT_WORD_SIZE];
    char resource[GK_SCRIPT_WORD_SIZE], exclusive_group[GK_SCRIPT_WORD_SIZE];
    char permission[GK_SCRIPT_WORD_SIZE], condition_tooltip[GK_SCRIPT_WORD_SIZE];
    char requires[PS_ACTION_LINKS][GK_SCRIPT_WORD_SIZE], excludes[PS_ACTION_LINKS][GK_SCRIPT_WORD_SIZE];
    unsigned require_count, exclude_count;
    unsigned duration, cooldown;
    unsigned option_count;
    IncidentOption options[PS_MAX_INCIDENT_OPTIONS];
    int32_t cost, prestige, ai_weight;
    ResourceTerm actor_trigger, target_trigger;
    NativeClause potential, trigger, effect, off_effect, can_stop, native_actor, native_target, ai_rule;
    NativeClause modifiers[ACTION_MODIFIER_COUNT];
    ActionKind kind;
    ActionActor actor_rule;
    ActionPayer payer;
    ActionBeneficiaries beneficiaries;
    bool country_target, repeatable, shown;
    bool member_invitation, request_response, member_participants, leader_choice, ai_accept;
} PoliticalAction;

typedef struct PoliticalActionRun {
    unsigned slot, actor, target;
    unsigned performed, expires, cooldown_until;
} PoliticalActionRun;

typedef struct PoliticalActionState {
    unsigned count;
    PoliticalActionRun runs[PS_MAX_ACTION_RUNS];
} PoliticalActionState;

typedef enum ProcessStatus {
    PROCESS_PENDING = 1, PROCESS_ACCEPTED, PROCESS_REJECTED, PROCESS_EXPIRED,
    PROCESS_INVALID, PROCESS_WITHDRAWN, PROCESS_SETTLED,
} ProcessStatus;

typedef struct PoliticalProcess {
    unsigned slot, round, issuer, target, opened, deadline, resolved;
    ProcessStatus status;
    bool notified;
    char option[GK_SCRIPT_WORD_SIZE];
} PoliticalProcess;

typedef struct PoliticalProcesses {
    unsigned count;
    PoliticalProcess items[PS_MAX_PROCESSES];
} PoliticalProcesses;

unsigned political_action_index(const PoliticalAction *actions, unsigned count, const char *id);
const PoliticalActionRun *find_action_run(const PoliticalActionState *state, unsigned slot, unsigned actor);
bool political_action_running(const PoliticalAction *action, const PoliticalActionRun *run, unsigned today);
unsigned political_process_index(const PoliticalProcesses *state, unsigned slot, unsigned target);
const char *process_status_name(ProcessStatus status);
unsigned incident_option_index(const PoliticalAction *action, const char *id);
