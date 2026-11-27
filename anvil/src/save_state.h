#pragma once

#include "reform.h"

#define GK_SAVE_KEY "great_khan_state"
#define PS_SAVE_KEY "political_system_state"
enum { GK_SAVE_VALUE_SIZE = 448, PS_PAST_LEADERS = 10, PS_LEADER_REIGNS = PS_PAST_LEADERS + 1,
       PS_RULER_NAME_SIZE = 72, PS_DYNASTY_NAME_SIZE = 56,
       PS_RESOURCE_CHUNKS = (PS_MAX_EXPLICIT_MEMBERS + PS_RESOURCE_CHUNK - 1) / PS_RESOURCE_CHUNK,
       PS_CONTEST_CHUNKS = (PS_MAX_EXPLICIT_MEMBERS + PS_CONTEST_CHUNK - 1) / PS_CONTEST_CHUNK,
       PS_TERRITORY_CHUNKS = (PS_MAX_TERRITORY + PS_TERRITORY_CHUNK - 1) / PS_TERRITORY_CHUNK,
       PS_MAX_SAVE_RECORDS = (7 + PS_LEADER_REIGNS + PS_TERRITORY_CHUNKS + PS_MAX_ROLES + PS_MAX_RESOURCES * PS_RESOURCE_CHUNKS + PS_MAX_ACTION_RUNS + PS_MAX_PROCESSES +
                              (1 + PS_MAX_ROLES) * PS_CONTEST_CHUNKS) * PS_MAX_INSTANCES };

typedef enum KhanLoadStatus {
    GK_LOAD_NEW,
    GK_LOAD_MISSING,
    GK_LOAD_VALID,
    GK_LOAD_UNSUPPORTED,
    GK_LOAD_INVALID,
} KhanLoadStatus;

typedef struct KhanSaveRecord {
    char leader[4];
    ReformState reform;
} KhanSaveRecord;

typedef struct PoliticalSaveRecord {
    char type[GK_SCRIPT_WORD_SIZE];
    char instance[GK_SCRIPT_WORD_SIZE];
    KhanSaveRecord data;
    uint32_t history_date;
} PoliticalSaveRecord;

typedef struct PoliticalLifecycle {
    PoliticalMode mode;
    unsigned founder, created;
    bool dynamic, automatic;
} PoliticalLifecycle;

typedef struct PoliticalLifecycleRecord {
    char instance[GK_SCRIPT_WORD_SIZE], type[GK_SCRIPT_WORD_SIZE], name[GK_SCRIPT_WORD_SIZE];
    PoliticalLifecycle lifecycle;
} PoliticalLifecycleRecord;

typedef struct NativeSaveField {
    char value[GK_SAVE_VALUE_SIZE];
    bool legacy;
} NativeSaveField;

typedef struct NativeSaveFields {
    NativeSaveField entries[PS_MAX_SAVE_RECORDS];
    size_t count;
    bool incomplete;
} NativeSaveFields;

typedef struct LeaderReign {
    uint64_t ruler;
    unsigned country, start, end;
    char name[PS_RULER_NAME_SIZE], dynasty[PS_DYNASTY_NAME_SIZE];
} LeaderReign;

typedef struct LeaderHistory {
    LeaderReign items[PS_LEADER_REIGNS];
    unsigned count;
} LeaderHistory;

void append_leader_reign(LeaderHistory *history, LeaderReign reign);
bool encode_leader_reign(const char *instance, const LeaderReign *reign, char value[GK_SAVE_VALUE_SIZE]);
bool decode_leader_reign(const char *value, char instance[GK_SCRIPT_WORD_SIZE], LeaderReign *reign);

typedef struct TerritorySaveChunk {
    unsigned offset, total, count;
    unsigned ids[PS_TERRITORY_CHUNK];
} TerritorySaveChunk;

bool encode_khan_state(const KhanSaveRecord *record, char value[GK_SAVE_VALUE_SIZE]);
KhanLoadStatus decode_khan_state(const char *value, KhanSaveRecord *record);
bool encode_political_state(const PoliticalSaveRecord *record, char value[GK_SAVE_VALUE_SIZE]);
KhanLoadStatus decode_political_state(const NativeSaveField *field, PoliticalSaveRecord *record);
KhanLoadStatus decode_saved_instances(const NativeSaveFields *fields, PoliticalSaveRecord *out);
bool encode_lifecycle(const PoliticalInstanceSpec *spec, const PoliticalLifecycle *lifecycle, char value[GK_SAVE_VALUE_SIZE]);
bool decode_lifecycle(const char value[GK_SAVE_VALUE_SIZE], PoliticalLifecycleRecord *record);
bool encode_election_state(const char *instance, const ElectionState *state, char value[GK_SAVE_VALUE_SIZE]);
bool decode_election_state(const char value[GK_SAVE_VALUE_SIZE], char instance[GK_SCRIPT_WORD_SIZE], ElectionState *out);
bool encode_election_cooldowns(const char *instance, const ElectionState *state, char value[GK_SAVE_VALUE_SIZE]);
bool decode_election_cooldowns(const char value[GK_SAVE_VALUE_SIZE], char instance[GK_SCRIPT_WORD_SIZE], ElectionState *out);
bool encode_explicit_members(const char *instance, const MemberTags *members, char value[GK_SAVE_VALUE_SIZE]);
bool decode_explicit_members(const char value[GK_SAVE_VALUE_SIZE], char instance[GK_SCRIPT_WORD_SIZE], MemberTags *members);
bool encode_territory(const char *instance, const ProvinceList *territory, char value[GK_SAVE_VALUE_SIZE]);
bool decode_territory(const char value[GK_SAVE_VALUE_SIZE], char instance[GK_SCRIPT_WORD_SIZE], ProvinceList *territory);
bool encode_territory_chunk(const char *instance, const ProvinceList *territory, unsigned offset,
                            char value[GK_SAVE_VALUE_SIZE]);
bool decode_territory_chunk(const char value[GK_SAVE_VALUE_SIZE], char instance[GK_SCRIPT_WORD_SIZE], TerritorySaveChunk *chunk);
bool encode_role(const char *instance, const char *role, const RoleHolders *holders, char value[GK_SAVE_VALUE_SIZE]);
bool decode_role(const char value[GK_SAVE_VALUE_SIZE], char instance[GK_SCRIPT_WORD_SIZE], char role[GK_SCRIPT_WORD_SIZE], RoleHolders *holders);
bool encode_resource_accounts(const char *instance, const char *resource, unsigned month,
                              const ResourceAccount *accounts, unsigned count, char value[GK_SAVE_VALUE_SIZE]);
bool decode_resource_accounts(const char value[GK_SAVE_VALUE_SIZE], char instance[GK_SCRIPT_WORD_SIZE],
                              char resource[GK_SCRIPT_WORD_SIZE], unsigned *month, ResourceLedger *ledger);
bool encode_action_run(const char *instance, const PoliticalAction *action, const PoliticalActionRun *run,
                       char value[GK_SAVE_VALUE_SIZE]);
bool decode_action_run(const char value[GK_SAVE_VALUE_SIZE], char instance[GK_SCRIPT_WORD_SIZE],
                       char action[GK_SCRIPT_WORD_SIZE], ActionKind *kind, PoliticalActionRun *run);
bool encode_process(const char *instance, const PoliticalAction *action, const PoliticalProcess *process,
                    char value[GK_SAVE_VALUE_SIZE]);
bool decode_process(const char value[GK_SAVE_VALUE_SIZE], char instance[GK_SCRIPT_WORD_SIZE],
                    char action[GK_SCRIPT_WORD_SIZE], ActionKind *kind, PoliticalProcess *process);
bool encode_contest(const char *instance, const char *office, const PoliticalContest *contest,
                    unsigned offset, char value[GK_SAVE_VALUE_SIZE]);
bool decode_contest(const char value[GK_SAVE_VALUE_SIZE], char instance[GK_SCRIPT_WORD_SIZE],
                    char office[GK_SCRIPT_WORD_SIZE], PoliticalContest *contest);
