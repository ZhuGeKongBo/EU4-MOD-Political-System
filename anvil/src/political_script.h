#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "election.h"
#include "entry_config.h"
#include "political_resources.h"
#include "political_actions.h"
#include "political_selection.h"

#define GK_DEFINITION_PATH "common/political_systems/great_khan.txt"
#define GK_HISTORY_PATH "history/political_systems/great_khan.txt"
#define GK_REFORM_PATH "common/political_systems/reforms/great_khan.txt"
#define PS_ROSTER_PATH "common/political_systems/instances.txt"
#define PS_ROSTER_DIRECTORY "common/political_systems/instances"
#define PS_LEGACY_TYPE "great_khan"
#define PS_LEGACY_INSTANCE "great_khan_main"
#define GK_KHANATE_HISTORY "GKhanate"

enum { GK_MAX_TIMELINE_ENTRIES = 8192 };
enum { PS_MAX_INSTANCES = 32, PS_CONTENT_PATH_SIZE = GK_SCRIPT_WORD_SIZE + 48 };

typedef enum MemberSource {
    MEMBERS_UNSET,
    MEMBERS_DIRECT_SUBJECTS,
    MEMBERS_CAPITAL_PROVINCES,
    MEMBERS_EXPLICIT,
    MEMBERS_QUERY,
    MEMBERS_MIXED,
    MEMBERS_CAPITAL_TERRITORY,
} MemberSource;

enum { PS_MAX_ROLES = 8, PS_MAX_ROLE_HOLDERS = 32, PS_MAX_TERRITORY = 1024, PS_TERRITORY_CHUNK = 40 };
enum { PS_MAX_MESSAGES = 32 };

typedef struct PoliticalMessageDefinition {
    char id[GK_SCRIPT_WORD_SIZE], title[GK_SCRIPT_WORD_SIZE], subtitle[GK_SCRIPT_WORD_SIZE];
    char body[PS_MESSAGE_BODY_LINES][GK_SCRIPT_WORD_SIZE], log[GK_SCRIPT_WORD_SIZE];
    char close[GK_SCRIPT_WORD_SIZE], secondary[GK_SCRIPT_WORD_SIZE];
    char path[PS_FILE_PATH_SIZE];
    unsigned line;
    bool popup;
} PoliticalMessageDefinition;

typedef struct PoliticalRole {
    char id[GK_SCRIPT_WORD_SIZE], title_key[GK_SCRIPT_WORD_SIZE], tooltip_key[GK_SCRIPT_WORD_SIZE];
    char history_key[GK_SCRIPT_WORD_SIZE];
    unsigned max_holders;
    bool require_member, can_hold_with_leader;
    EntryCondition candidate, retain;
    OfficeAcquisition acquisition;
    PoliticalPermissions permissions;
    PoliticalContestRule contest;
} PoliticalRole;

typedef struct RoleHolders {
    unsigned count;
    unsigned countries[PS_MAX_ROLE_HOLDERS];
} RoleHolders;

typedef struct ProvinceList {
    unsigned count;
    unsigned ids[PS_MAX_TERRITORY];
} ProvinceList;

typedef struct KhanDefinition {
    char title[GK_SCRIPT_WORD_SIZE];
    char emperor[GK_SCRIPT_WORD_SIZE];
    char subject_type[GK_SCRIPT_WORD_SIZE];
    char member_title[GK_SCRIPT_WORD_SIZE];
    char territory_history_key[GK_SCRIPT_WORD_SIZE];
    unsigned subject_type_line, subject_type_column;
    char subject_type_path[PS_FILE_PATH_SIZE];
    char source_path[PS_FILE_PATH_SIZE];
    unsigned source_line;
    int starting_authority;
    MemberSource member_source;
    EntryCondition member_query, member_join, member_retain, member_leave;
    bool member_query_defined, territory_enabled;
    PoliticalRole roles[PS_MAX_ROLES];
    unsigned role_count;
    ResourceDefinition resources[PS_MAX_RESOURCES];
    CalculationDefinition calculations[PS_MAX_VALUES];
    unsigned resource_count, calculation_count;
    PoliticalAction actions[PS_MAX_ACTIONS];
    unsigned action_count;
    PoliticalMessageDefinition messages[PS_MAX_MESSAGES];
    unsigned message_count;
    LeaderSelection succession;
    ElectionRules election;
    EntryButton entry;
    EntryCondition create_trigger, continue_trigger;
    EntryCondition defensive_trigger;
    unsigned max_instances, max_memberships;
    bool script_type, abstract, history_allowed, leader_enabled, authority_enabled;
    bool dynamic_allowed, dissolution_allowed, block_direct_war, defensive_obligation;
} KhanDefinition;

typedef struct KhanReform {
    char id[GK_SCRIPT_WORD_SIZE];
    char title[GK_SCRIPT_WORD_SIZE];
    int cost;
    int prestige;
    unsigned effect_line;
    char path[PS_CONTENT_PATH_SIZE];
} KhanReform;

typedef struct KhanHistory {
    uint32_t effective_date;
    char tag[4];
} KhanHistory;

typedef struct KhanTimeline {
    KhanHistory *entries;
    size_t count;
} KhanTimeline;

typedef struct KhanFlagChange {
    uint32_t date;
    bool enabled;
} KhanFlagChange;

typedef struct KhanFlagTimeline {
    KhanFlagChange *entries;
    size_t count;
} KhanFlagTimeline;

typedef enum PoliticalHistoryKind {
    HISTORY_LEADER, HISTORY_CLEAR_MEMBERS, HISTORY_MEMBER, HISTORY_CLEAR_TERRITORY,
    HISTORY_PROVINCE, HISTORY_BALANCE, HISTORY_ROLE,
} PoliticalHistoryKind;

typedef struct PoliticalHistoryChange {
    uint32_t date, key;
    unsigned line, column, order;
    int32_t amount;
    PoliticalHistoryKind kind;
    bool enabled;
    char id[GK_SCRIPT_WORD_SIZE];
} PoliticalHistoryChange;

typedef struct PoliticalInstanceSpec {
    char id[GK_SCRIPT_WORD_SIZE];
    char type[GK_SCRIPT_WORD_SIZE];
    char history[GK_SCRIPT_WORD_SIZE];
    char title[GK_SCRIPT_WORD_SIZE];
    char icon[GK_SCRIPT_WORD_SIZE];
    int starting_authority;
    unsigned line;
    unsigned date;
    char leader[4];
    MemberTags members;
    ProvinceList territory;
    ResourceInitial balances[PS_MAX_RESOURCES];
    unsigned balance_count;
    PoliticalHistoryChange *changes;
    size_t change_count;
    bool members_written, leader_written, territory_written;
    char path[PS_FILE_PATH_SIZE];
} PoliticalInstanceSpec;

typedef struct PoliticalRoster {
    PoliticalInstanceSpec items[PS_MAX_INSTANCES];
    size_t count;
} PoliticalRoster;

bool read_roster(const char *text, size_t size, PoliticalRoster *out, ScriptError *error);
bool append_roster(const char *text, size_t size, PoliticalRoster *out, ScriptError *error);
bool read_type_definition(const char *text, size_t size, const char *id, KhanDefinition *out, ScriptError *error);
bool read_type_reform(const char *text, size_t size, const char *id, KhanReform *out, ScriptError *error);
bool read_definition(const char *text, size_t size, KhanDefinition *out, ScriptError *error);
bool read_reform(const char *text, size_t size, KhanReform *out, ScriptError *error);
bool read_timeline(const char *text, size_t size, KhanTimeline *out, ScriptError *error);
bool read_holder_history(const char *text, size_t size, const char *keyword, KhanTimeline *out, ScriptError *error);
KhanHistory history_at(const KhanTimeline *timeline, uint32_t today);
bool read_flag_history(const char *text, size_t size, const char *keyword, KhanFlagTimeline *out, ScriptError *error);
bool read_mounted_flag_history(const char *text, size_t size, const char *path, const char *keyword,
                               KhanFlagTimeline *out, ScriptError *error);
bool flag_at(const KhanFlagTimeline *timeline, uint32_t date);
