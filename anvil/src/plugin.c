#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <windowsx.h>

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "accession.h"
#include "content_layout.h"
#include "khanate.h"
#include "native_eu4.h"
#include "native_modifiers.h"
#include "native_passives.h"
#include "native_save.h"
#include "native_war.h"
#include "operators.h"
#include "political_script.h"
#include "political_text.h"
#include "reform.h"
#include "shogunate.h"
#include "symbols.gen.h"

enum { DAIMYO_COLUMNS = 8, DAIMYO_ROWS = 4, DAIMYO_SLOTS = DAIMYO_COLUMNS * DAIMYO_ROWS };
enum { KHANATE_MEMBER_ROWS = 3 };
enum { TUMEN_SLOTS = 8, HUD_ICONS = 4 };
enum { SCROLL_SCALE = 1000 };
_Static_assert(sizeof(GameString) == 32, "EU4 x64 string layout");

typedef struct Shield Shield;
typedef struct ButtonTooltip ButtonTooltip;

typedef struct TooltipMethods {
    bool (*describe)(const ButtonTooltip *, const void *, GameString *);
    ButtonTooltip *(*destroy)(ButtonTooltip *, unsigned);
} TooltipMethods;

struct ButtonTooltip {
    const TooltipMethods *methods;
    uint64_t tag;
    void *button;
};

_Static_assert(sizeof(ButtonTooltip) == 0x18 && offsetof(ButtonTooltip, button) == 0x10,
               "EU4 button tooltip layout");

typedef struct ClickListener {
    const void *vtable;
    void *owner;
    void (*on_click)(void *);
    int32_t adjustment;
    unsigned char other_events[0xc4];
} ClickListener;

_Static_assert(sizeof(ClickListener) == 0xe0 && offsetof(ClickListener, on_click) == 0x10,
               "EU4 native click listener layout");

struct Shield {
    ButtonTooltip tooltip;
    ClickListener listener;
    const PoliticalInstanceSpec *instance;
    const char *role_tooltip;
    uint64_t displayed_tag;
    unsigned voter;
    bool ballot, vacant;
};

typedef enum PanelRequest {
    PANEL_REQUEST_NONE,
    PANEL_REQUEST_ICON,
    PANEL_REQUEST_PROVINCE,
    PANEL_REQUEST_CLOSE,
    PANEL_REQUEST_SHORTCUT,
    PANEL_REQUEST_ESCAPE,
    PANEL_REQUEST_ACCESSION,
    PANEL_REQUEST_TYPE_ICON,
    PANEL_REQUEST_SWITCH_INSTANCE,
    PANEL_REQUEST_CHOOSE_INSTANCE,
    PANEL_REQUEST_CANCEL_SELECTOR,
} PanelRequest;

typedef struct HudEntry {
    void *window;
    void *buttons[HUD_ICONS];
    ButtonTooltip tooltip;
    ClickListener listener;
    bool visible, bind_failed, clickable, evaluated, condition_error;
} HudEntry;

typedef struct ProvinceBadge {
    void *card;
    const void *view, *selection;
    ButtonTooltip tooltip;
    ClickListener listener;
    uint64_t memberships;
    int32_t province;
    bool visible, described;
} ProvinceBadge;

typedef enum ReformInput {
    REFORM_INPUT_NONE,
    REFORM_INPUT_REVIEW,
    REFORM_INPUT_CONFIRM,
    REFORM_INPUT_CANCEL,
} ReformInput;

typedef struct ReformView {
    void *authority, *title, *terms, *status_label;
    void *buttons[3];
    ClickListener listeners[3];
    ButtonTooltip tooltip, terms_hover;
    ReformInput pending;
    unsigned confirmed_for;
    const PoliticalInstanceSpec *confirmed_instance;
    ReformStatus status;
    bool bound;
} ReformView;

enum VoteWidget { V_NONE, V_OPEN, V_CLOSE, V_COUNTRY, V_OPINION, V_PICK, V_END };
enum { BALLOT_ROWS = 7 };
typedef struct VoteInput {
    enum VoteWidget action;
    unsigned actor, target, round;
} VoteInput;
typedef struct ListScroll {
    void *bar;
    unsigned top, last;
    bool initialized;
    int wheel;
} ListScroll;

typedef enum NumberKind { NUMBER_MEMBERS, NUMBER_ELECTORS, NUMBER_TERRITORY, NUMBER_AUTHORITY, NUMBER_OPINION } NumberKind;
typedef struct NumberDisplay {
    ButtonTooltip hover;
    const PoliticalInstanceSpec *instance;
    void *control;
    NumberKind kind;
    unsigned country;
    int32_t value;
} NumberDisplay;
typedef struct VoteView {
    void *window, *backdrop, *buttons[V_PICK], *names[BALLOT_ROWS], *opinions[BALLOT_ROWS];
    ClickListener listeners[V_PICK];
    Shield choices[BALLOT_ROWS];
    NumberDisplay relation[BALLOT_ROWS];
    VoteInput input;
    ListScroll list, tumen;
    unsigned displayed_actor, displayed_round;
    bool bound, open, by_opinion, reverse;
} VoteView;

typedef enum ActionStep { ACTION_START, ACTION_ACCEPT, ACTION_DECLINE, ACTION_WITHDRAW, ACTION_RESOLVE, ACTION_VIEW } ActionStep;
typedef struct ActionRowEntry {
    unsigned slot, target, round, option;
    ActionStep step;
} ActionRowEntry;

typedef struct NominationDisplay {
    ButtonTooltip hover;
    void *label;
    const PoliticalInstanceSpec *instance;
    unsigned elector, viewer, terms, support, standing_count;
    int opinion, score, weight;
    NominationStanding standings[10];
    bool candidate, tooltip_logged;
} NominationDisplay;

typedef struct ResourceGrowth {
    unsigned terms, native_country;
    int32_t native_modifier;
    int64_t factor, uncapped;
} ResourceGrowth;

typedef struct PoliticalPanel {
    void *window, *title, *emperor_caption, *emperor_label, *daimyo_label, *scrollbar;
    void *backdrop;
    void *daimyo_labels[DAIMYO_SLOTS];
    Shield holder_shield, daimyo_shields[DAIMYO_SLOTS];
    void *tumen_caption, *tumen_labels[TUMEN_SLOTS];
    void *khanate_authority, *khanate_provinces;
    Shield tumen_shields[TUMEN_SLOTS];
    Shield supports[TUMEN_SLOTS];
    void *support_labels[TUMEN_SLOTS];
    NominationDisplay tumen_scores[TUMEN_SLOTS];
    VoteView vote;
    struct {
        void *caption, *labels[PS_MAX_ROLES];
        struct OfficeGroupHeading {
            void *label;
            ButtonTooltip hover;
            const PoliticalRole *role;
        } groups[PS_MAX_ROLES];
        Shield shields[PS_MAX_ROLES];
        Shield supports[PS_MAX_ROLES];
        NominationDisplay scores[PS_MAX_ROLES];
        ListScroll list;
    } roles;
    struct {
        struct ResourceDisplayRow {
            void *label, *value;
            ButtonTooltip hover;
            const char *ownership, *detail, *name;
            const ResourceDefinition *definition;
            unsigned owner, viewer;
            int32_t balance, change;
            ResourceGrowth growth;
            bool stored, has_growth;
        } rows[PS_MAX_RESOURCES];
        struct ResourceDisplayRow summary;
        void *growth;
        ButtonTooltip growth_hover;
        ListScroll list;
    } resources;
    NumberDisplay members_count, electors_count, territory_count, authority_value;
    struct {
        struct ActionDisplayRow {
            void *label, *seal, *check;
            ButtonTooltip hover, read_only;
            ClickListener listener;
            const PoliticalInstanceSpec *instance;
            const char *reason;
            ActionRowEntry entry;
            unsigned actor;
            bool visible, enacted;
        } rows[PS_MAX_ACTIONS];
        ListScroll list;
    } actions;
    uint32_t member_rows, member_columns;
    int member_wheel;
    void *close_button, *switch_button;
    ButtonTooltip label_hover;
    ButtonTooltip history_hover;
    ClickListener close_listener, switch_listener;
    ReformView reform_view;
    bool bad_template, show_shields;
} PoliticalPanel;

typedef struct ScrollRange {
    int32_t minimum, maximum, step, stops;
} ScrollRange;

typedef struct ModFiles {
    const char *(*source)(const char *);
    void *(*open)(const char *);
    int64_t (*size)(void *);
    int64_t (*read)(void *, void *, uint64_t);
    int (*close)(void *);
    char **(*enumerate)(const char *);
    void (*free_list)(void *);
} ModFiles;

typedef struct PoliticalType {
    char id[GK_SCRIPT_WORD_SIZE];
    KhanDefinition definition;
    KhanReform reform;
    KhanateHistoryFiles territory_history, role_history[PS_MAX_ROLES];
    char emperor_title[GK_SCRIPT_WORD_SIZE];
    const PoliticalLayout *layout;
    int hud, sprite;
    EntryRoute last_route;
    bool usable, entry_logged, native_passives;
} PoliticalType;

static bool have_native_passives, native_passives_dirty;
static unsigned native_passives_day;
static void (*refresh_country_modifiers)(void *, const char *, bool);
static void (*refresh_province_modifiers)(void *);

typedef struct ContentHudSlot ContentHudSlot;
typedef struct TypeHudButton {
    void *control;
    ClickListener listener;
    ContentHudSlot *slot;
} TypeHudButton;

struct ContentHudSlot {
    void *window;
    TypeHudButton buttons[PS_MAX_HUD_ICONS];
    ButtonTooltip tooltip;
    int type_index;
    bool shown, clickable, failed;
};

typedef struct ContentHud {
    ContentHudSlot slots[PS_MAX_HUD_SLOTS];
    void *pager;
    ButtonTooltip tooltip;
    ClickListener listener;
    unsigned page, pages;
    bool shown, failed;
} ContentHud;

enum { SELECTOR_ROWS = 7 };
typedef struct SelectorRow {
    void *label;
    ButtonTooltip tooltip;
    ClickListener listener;
    size_t instance, selector_slot;
    bool enabled;
} SelectorRow;

typedef struct InstanceSelector {
    void *window, *title;
    ClickListener close;
    SelectorRow rows[SELECTOR_ROWS];
    ListScroll list;
    InstanceChoices choices;
    size_t type_index;
    bool open, failed;
} InstanceSelector;

typedef struct ElectionSnapshot {
    ElectionCountry *members;
    uint64_t *rulers;
    int16_t *opinions, *nomination;
    size_t count;
    unsigned holder, date;
    uint64_t holder_ruler;
} ElectionSnapshot;

typedef struct PoliticalInstance {
    const PoliticalInstanceSpec *spec;
    const PoliticalType *type;
    PoliticalLifecycle life;
    KhanTimeline timeline;
    KhanHistory starting_history;
    KhanateHistory khanate_history;
    ElectionState election;
    ElectionSnapshot roll;
    uint32_t election_checked_date;
    ReformState reform;
    MemberTags members;
    MemberTags excluded;
    ProvinceList territory;
    RoleHolders roles[PS_MAX_ROLES];
    ResourceLedger accounts[PS_MAX_RESOURCES];
    PoliticalActionState actions;
    PoliticalProcesses processes;
    LeaderHistory leaders;
    PoliticalContest contests[PS_MAX_ROLES + 1];
    bool threshold_succession_pending;
    unsigned resource_month, restored_month, restored_accounts;
    unsigned computed_values;
    int32_t computed[PS_MAX_VALUES];
    char title[GK_SCRIPT_WORD_SIZE];
    uint32_t history_date;
    unsigned holder, hud_slot;
    bool configured, enabled, has_members, condition_error;
} PoliticalInstance;

typedef struct CustomNotice {
    const PoliticalMessageDefinition *definition;
    unsigned sender, recipient;
    GameString title, subtitle, body[PS_MESSAGE_BODY_LINES], log, close, secondary;
} CustomNotice;

typedef struct CrownNotice {
    struct CrownNotice *next;
    PoliticalInstance *instance;
    const ResourceDefinition *resource;
    const ResourceThreshold *threshold;
    CustomNotice *custom;
    int32_t threshold_value, previous_value;
    unsigned winner;
    uint64_t ruler;
    char name[256];
} CrownNotice;

typedef struct TextField { const char *name; TextValue value; } TextField;
typedef struct PoliticalText {
    const PoliticalInstance *instance;
    const ResourceDefinition *resource;
    const TextField *fields;
    size_t count;
    unsigned country;
    GameString scratch;
    char tag[4];
} PoliticalText;

typedef struct Plugin {
    const AnvilHost *host;
    AnvilPlugin *self;
    const AnvilGameWorldService *world;
    const AnvilGameService *game;
    AccessionMessages messages;
    TextEngine text_engine;
    TextTemplates text_templates;
    const void *const *const *localisation_db;
    const char *(*lookup_localisation_raw)(const void *, uint32_t);
    UiRectangle ballot_area, ballot_opinions[BALLOT_ROWS];
    bool ballot_number_areas;
    NativeDefense defense;
    CrownNotice *first_notice, *last_notice;
    unsigned custom_notice_count;
    void *(*create_window)(void *, const GameString *);
    const void *window_icon_method;
    void *(*find_window_icon)(const char *, int, const void *, const void *);
    void (*set_icon_sprite)(void *, const GameString *);
    const GameString *(*icon_sprite_name)(const void *);
    const void *shield_position, *shield_move, *label_position, *label_move, *set_ui_tooltip;
    void (*retire_window)(void *);
    void (*set_text)(void *, GameString *, bool);
    void (*assign_string)(GameString *, const char *, size_t);
    void (*append_string)(GameString *, const char *, size_t);
    void (*free_string)(GameString *);
    GameString *(*localize)(GameString *, const char *, const void *, unsigned);
    const GameString *(*country_name)(const uint64_t *, bool, bool, bool);
    bool (*describe_daimyo)(const ButtonTooltip *, const void *, GameString *);
    bool (*valid_tag)(const void *);
    uint64_t *(*lookup_tag)(const void *, uint64_t *, const char *, bool);
    const void *(*lookup_subject_type)(const void *, const GameString *);
    void (*add_prestige)(void *, const int32_t *, const char *, int);
    int32_t *(*get_prestige)(const void *, int32_t *);
    void *(*find_button)(void *, const char *);
    void *(*find_scrollbar)(void *, const GameString *);
    void (*open_diplomacy)(void *, uint64_t);
    void (*original_china_init)(void *);
    void (*original_province_card_bind)(void *, void *);
    void (*original_diplomacy_path)(void *, const GameString *);
    void (*native_panel_toggle[3])(void *);
    void *const *root_slot;
    void *const *countries_db;
    void *const *subject_types_db;
    void *root, *gui;
    const void *click_vtable;
    void *shield_class, *shield_signal_class;
    void *button_class, *button_signal_class;
    PoliticalPanel panels[PS_MAX_INSTANCES];
    HudEntry entries[PS_MAX_INSTANCES];
    ContentLayouts layouts;
    ContentHud content_huds[PS_MAX_HUD_REGIONS];
    InstanceSelector selectors[PS_MAX_INSTANCES + 1];
    size_t selector_slot;
    ProvinceBadge province_badge;
    ModFiles files;
    PoliticalRoster roster;
    PoliticalType types[PS_MAX_INSTANCES];
    PoliticalInstance instances[PS_MAX_INSTANCES];
    size_t type_count, selected, enabled_count;
    NativeSaveFields retained;
    Shogunate current;
    int32_t province_limit;
    uint64_t pending_country;
    const PoliticalInstanceSpec *pending_country_instance;
    PanelRequest pending_panel;
    size_t pending_instance;
    uint32_t top_row, max_top_row;
    ULONGLONG next_update, next_entry_check;
    bool active, ready, visible, open_held, escape_held, reset_windows;
    bool religion_bound, capital_bound;
    bool (*province_discovered)(int32_t province, unsigned country);
    const void *(*government_stack)(const void *);
    unsigned (*government_kind)(const void *);
    NativeModifierCatalog modifiers;
    ModifierRegistration modifier_api;
    const void *const *modifier_defs_begin, *const *modifier_defs_end;
    void *(*original_static_modifier_db)(void);
    bool modifiers_loaded;
    bool message_priority;
    bool nomination_hover_hook;
    bool roster_loaded, scripts_loaded, use_shields, selector_gui_valid;
} Plugin;

static Plugin plugin;
static struct {
    const Shield *shield;
    NominationDisplay *score;
} nomination_route;
static struct {
    const void *carrier;
    const NumberDisplay *number;
    const struct ResourceDisplayRow *resource;
    bool monthly;
} number_route;

typedef struct CountryFact {
    unsigned tag, day;
    int32_t capital, manpower;
    uint32_t values;
} CountryFact;

static CountryFact country_facts[INT16_MAX + 1];
static unsigned resource_fact_day;
static ULONGLONG resource_next_check;
static uint64_t last_player_war_block;

static bool prune_resource_accounts(PoliticalInstance *instance);
static bool campaign_date(uint32_t *today);
uint32_t date_key(const char *text);
static void historical_ruler(LeaderReign *reign);
static bool country_fact(unsigned country, CountryFact *out);
static const char *country_religion_matches(unsigned country, const char *key, bool *allowed);
static const char *country_government_matches(unsigned country, const char *key, bool *allowed);
static const char *country_has_heir(unsigned country, bool *allowed);
static void *game_gui_context(void);
static void close_instance_panel(const char *source);
static void localize_key(char output[GK_SCRIPT_WORD_SIZE], const char *key);
static void append_leader_history(const PoliticalInstance *instance, GameString *text);
static bool describe_routed_nomination(const PoliticalInstance *instance, const Shield *shield, GameString *text);
static bool append_political_text(GameString *out, const char *key, PoliticalText context, bool *used_breakdown);
static void append_instance_text(GameString *out, const char *key, const PoliticalInstance *instance, unsigned country);
static const char *text_country(PoliticalText *context, unsigned country);
static void append_election_country(GameString *text, unsigned country);
static bool describe_number(const PoliticalInstance *instance, const NumberDisplay *number, GameString *text);
static void set_number(NumberDisplay *number, const PoliticalInstance *instance, int32_t value, unsigned country);
static bool bind_text_tooltip(void *label, ButtonTooltip *tooltip, const TooltipMethods *methods);
static void bind_number_control(NumberDisplay *number, void *control, NumberKind kind);
static bool elective(const PoliticalInstance *instance);
static bool passed_action(const PoliticalInstance *instance, const char *id);
static void resource_crossing(PoliticalInstance *instance, const ResourceDefinition *def,
                              unsigned country, int32_t before, int32_t after);
static const char *check_member_change(const PoliticalInstance *instance, unsigned country, bool include, bool *changes);
static void apply_member_change(PoliticalInstance *instance, unsigned country, bool include);
static const char *update_pending_process(PoliticalInstance *instance, PoliticalProcess *process, unsigned today);

typedef enum ActionSource { ACTION_SCRIPT, ACTION_UI, ACTION_AI } ActionSource;
typedef struct ActionRequest {
    size_t instance;
    unsigned slot, actor, target;
    ActionSource source;
    ActionStep step;
    unsigned round, option;
} ActionRequest;

static struct {
    unsigned count;
    ActionRequest items[64];
} queued_actions;

typedef struct SelectionRequest { size_t instance; unsigned office, actor, target; PsOperation operation; } SelectionRequest;
static struct { unsigned count; SelectionRequest items[64]; } selection_requests;
static HWND wheel_window;
static float ui_scale = 1.0f;

static InstanceSelector *active_selector(void) {
    return &plugin.selectors[plugin.selector_slot];
}

static void load_ui_scale(void) {
    wchar_t path[32768];
    DWORD len = GetModuleFileNameW(NULL,path,sizeof(path)/sizeof(*path));
    wchar_t *slash = len && len < sizeof(path)/sizeof(*path) ? wcsrchr(path,L'\\') : NULL;
    if (!slash || (size_t)(slash-path)+14 >= sizeof(path)/sizeof(*path)) return;
    wcscpy(slash+1,L"userdir.txt");
    FILE *f = _wfopen(path,L"rb");
    char profile[4096];
    if (!f) return;
    bool ok = fgets(profile,sizeof(profile),f) != NULL;
    fclose(f);
    if (!ok) return;
    profile[strcspn(profile,"\r\n")] = 0;
    int used = MultiByteToWideChar(CP_UTF8,0,profile,-1,path,32750);
    if (!used) return;
    wcscat(path,L"/settings.txt");
    f = _wfopen(path,L"rb");
    if (!f) return;
    char line[256];
    while (fgets(line,sizeof(line),f)) {
        float scale;
        if (sscanf(line," game_ui_scale = %f",&scale) == 1 && scale >= 0.5f && scale <= 4.0f) { ui_scale = scale; break; }
    }
    fclose(f);
}

static bool accepts_shortcuts(void) {
    uint32_t flags = 0;
    return !plugin.game || !ANVIL_GAME_HAS(plugin.game, keyboard) || !plugin.game->keyboard ||
           !plugin.game->keyboard(&flags) || !flags;
}

static bool inside_ui(const RECT *client, POINT p, UiRectangle area) {
    const float x = (p.x - client->right * 0.5f) / ui_scale;
    const float y = (p.y - client->bottom * 0.5f) / ui_scale;
    return x >= area.x && x < area.x + area.width && y >= area.y && y < area.y + area.height;
}

static LRESULT CALLBACK ui_input(HWND window, UINT message, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR data) {
    (void)data;
    if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(window, ui_input, id);
        wheel_window = NULL;
    }
    if ((message == WM_KEYDOWN || message == WM_MOUSEWHEEL) && plugin.ready &&
        (plugin.visible || active_selector()->open) &&
        (plugin.message_priority || (game_gui_context() && native_message_visible(&plugin.messages, plugin.gui))))
        return DefSubclassProc(window, message, w, l);
    if (message == WM_KEYDOWN && w == VK_ESCAPE && plugin.ready &&
        (active_selector()->open || plugin.visible) && accepts_shortcuts()) {
        PoliticalPanel *panel = &plugin.panels[plugin.selected];
        if (!(l & (1L << 30))) {
            if (active_selector()->open) plugin.pending_panel = PANEL_REQUEST_CANCEL_SELECTOR;
            else if (panel->vote.open) panel->vote.input = (VoteInput){ .action = V_CLOSE };
            else if (panel->reform_view.confirmed_for) panel->reform_view.pending = REFORM_INPUT_CANCEL;
            else {
                const KhanDefinition *definition = &plugin.instances[plugin.selected].type->definition;
                if ((definition->script_type || definition->entry.defined) && !definition->entry.close_on_escape)
                    return DefSubclassProc(window, message, w, l);
                plugin.pending_panel = PANEL_REQUEST_ESCAPE;
            }
        }
        return 0;
    }
    if (message == WM_MOUSEWHEEL && plugin.ready && (plugin.visible || active_selector()->open)) {
        RECT client;
        POINT p = { GET_X_LPARAM(l), GET_Y_LPARAM(l) };
        if (GetClientRect(window, &client) && ScreenToClient(window, &p)) {
            PoliticalPanel *panel = &plugin.panels[plugin.selected];
            int *wheel = NULL;
            if (active_selector()->open) {
                if (inside_ui(&client, p, (UiRectangle){-132, -125, 270, 270})) wheel = &active_selector()->list.wheel;
            } else if (panel->vote.open && inside_ui(&client, p, (UiRectangle){-522, -101, 217, 263})) {
                wheel = &panel->vote.list.wheel;
            } else if (plugin.visible) {
                const PoliticalLayout *layout = plugin.instances[plugin.selected].type->layout;
                if (layout) {
                    if (layout->members.rows) {
                        UiRectangle bounds = layout->members.wheel;
                        bounds.x += layout->area.x;
                        bounds.y += layout->area.y;
                        if (inside_ui(&client, p, bounds)) wheel = &panel->member_wheel;
                    }
                    if (!wheel && panel->roles.list.bar) {
                        UiRectangle bounds = layout->roles.wheel;
                        bounds.x += layout->area.x;
                        bounds.y += layout->area.y;
                        if (inside_ui(&client, p, bounds)) wheel = &panel->roles.list.wheel;
                    }
                    if (!wheel && panel->resources.list.bar) {
                        UiRectangle bounds = layout->resources.wheel;
                        bounds.x += layout->area.x;
                        bounds.y += layout->area.y;
                        if (inside_ui(&client, p, bounds)) wheel = &panel->resources.list.wheel;
                    }
                    if (!wheel && panel->actions.list.bar) {
                        UiRectangle bounds = layout->actions.wheel;
                        bounds.x += layout->area.x;
                        bounds.y += layout->area.y;
                        if (inside_ui(&client, p, bounds)) wheel = &panel->actions.list.wheel;
                    }
                }
            }
            if (wheel) {
                if (abs(*wheel) < 100 * WHEEL_DELTA) *wheel += GET_WHEEL_DELTA_WPARAM(w);
                return 0;
            }
        }
    }
    return DefSubclassProc(window,message,w,l);
}

static void enable_ballot_wheel(void) {
    if (wheel_window) return;
    HWND window = GetActiveWindow();
    if (!window || GetWindowThreadProcessId(window,NULL) != GetCurrentThreadId()) return;
    if (SetWindowSubclass(window, ui_input, 1, 0)) {
        wheel_window = window;
        load_ui_scale();
    }
}

static PoliticalPanel *panel_view(void) {
    return &plugin.panels[plugin.selected];
}

static PoliticalInstance *selected_instance(void) {
    return plugin.selected < plugin.roster.count && plugin.instances[plugin.selected].enabled
        ? &plugin.instances[plugin.selected] : NULL;
}

static const char *instance_title(const PoliticalInstance *instance) {
    if (!instance->spec->date && instance->spec->title[0]) return instance->spec->title;
    return instance->title[0] ? instance->title : instance->type->definition.title;
}

static bool uses_capital_members(const PoliticalInstance *instance) {
    return instance->type->definition.member_source == MEMBERS_CAPITAL_PROVINCES;
}

static size_t instance_index(const char *id) {
    for (size_t i = 0; i < plugin.roster.count; ++i)
        if (!strcmp(plugin.roster.items[i].id, id)) return i;
    return plugin.roster.count;
}

static bool instance_known(const char *id) {
    return instance_index(id) < plugin.roster.count;
}

static const PoliticalType *known_type(const char *id) {
    for (size_t i = 0; i < plugin.type_count; ++i)
        if (plugin.types[i].usable && !strcmp(plugin.types[i].id, id)) return &plugin.types[i];
    return NULL;
}

static PoliticalMode instance_mode(const PoliticalInstance *instance) {
    if (instance->enabled) return POLITICAL_ACTIVE;
    return instance->life.mode == POLITICAL_ACTIVE ? 0 : instance->life.mode;
}

static void log_message(AnvilLogLevel level, const char *message) {
    plugin.host->log(plugin.self, level, message);
}

static void log_file_error(const char *path, const char *source, const ScriptError *error);

static void *pointer_at(const void *self, size_t offset) {
    void *result;
    memcpy(&result, (const unsigned char *)self + offset, sizeof(result));
    return result;
}

static void *method(const void *self, size_t offset) {
    return pointer_at(pointer_at(self, 0), offset);
}

static const char *string_text(const GameString *text) {
    return text->capacity < 16 ? text->inline_text : pointer_at(text, 0);
}

static void stock_diplomacy_path(void *paths, const GameString *path) {
    const char *name = string_text(path);
    const size_t suffix_size = sizeof(KHAN_DIPLOMACY_PATH) - 1;
    if (name && path->size >= suffix_size) {
        const size_t start = path->size - suffix_size;
        if ((!start || name[start - 1] == '/') && !_stricmp(name + start, KHAN_DIPLOMACY_PATH)) {
            log_message(ANVIL_LOG_INFO, "khanate delegated history/diplomacy/GKhanate.txt to independent history reader");
            return;
        }
    }
    plugin.original_diplomacy_path(paths, path);
}

static void format_title(char *title, const char *identifier) {
    bool beginning = true;
    for (; *identifier; ++identifier, ++title) {
        const unsigned char ch = (unsigned char)*identifier;
        *title = ch == '_' ? ' ' : (char)(beginning ? toupper(ch) : ch);
        beginning = ch == '_';
    }
    *title = '\0';
}

static void *game_gui_context(void) {
    if (!plugin.gui || !plugin.root || plugin.root != *plugin.root_slot) return NULL;
    void *in_game = pointer_at(plugin.root, 0x1e00);
    return in_game && pointer_at(in_game, 0x58) == plugin.gui ? in_game : NULL;
}

static void capture_gui(void *china_view) {
    plugin.original_china_init(china_view);
    void *context = pointer_at(china_view, 0x2d0);
    void *gui = context ? pointer_at(context, 0x58) : NULL;
    if (!gui || method(gui, 0x48) != (void *)plugin.create_window) return;
    void *root = *plugin.root_slot;
    void *original_shield = pointer_at(china_view, 0x408);
    void *original_close = pointer_at(china_view, 0x3c8);
    void *shield_class = original_shield ? pointer_at(original_shield, 0) : NULL;
    void *signal_class = original_shield ? pointer_at(original_shield, 0xc8) : NULL;
    if (gui == plugin.gui && root == plugin.root) {
        if (shield_class && signal_class) {
            plugin.shield_class = shield_class;
            plugin.shield_signal_class = signal_class;
        }
        if (original_close) {
            plugin.button_class = pointer_at(original_close, 0);
            plugin.button_signal_class = pointer_at(original_close, 0xc8);
        }
        return;
    }
    memset(plugin.panels, 0, sizeof(plugin.panels));
    memset(plugin.entries, 0, sizeof(plugin.entries));
    memset(plugin.content_huds, 0, sizeof(plugin.content_huds));
    memset(plugin.selectors, 0, sizeof(plugin.selectors));
    plugin.selector_slot = 0;
    plugin.visible = false;
    plugin.gui = gui;
    plugin.root = root;
    plugin.shield_class = shield_class;
    plugin.shield_signal_class = signal_class;
    plugin.button_class = original_close ? pointer_at(original_close, 0) : NULL;
    plugin.button_signal_class = original_close ? pointer_at(original_close, 0xc8) : NULL;
    plugin.pending_country = 0;
    plugin.pending_country_instance = NULL;
    plugin.pending_panel = PANEL_REQUEST_NONE;
    plugin.pending_instance = 0;
    log_message(ANVIL_LOG_INFO, "great_khan captured native GUI manager");
}

static void native_text(void *control, const char *value) {
    if (!control) return;
    GameString text = { .capacity = 15 };
    plugin.assign_string(&text, value, strlen(value));
    plugin.set_text(control, &text, false);
    plugin.free_string(&text);
}

static void *create_named_window(const char *window) {
    GameString name = {.capacity = 15};
    plugin.assign_string(&name, window, strlen(window));
    void *result = plugin.create_window(plugin.gui, &name);
    plugin.free_string(&name);
    return result;
}

static void append_label(GameString *text, const char *key) {
    GameString label = {.capacity = 15};
    plugin.localize(&label, key, NULL, 0);
    if (label.size) plugin.append_string(text, string_text(&label), label.size);
    plugin.free_string(&label);
}

static bool lookup_ui_label(GameString *label, const char *key) {
    plugin.localize(label, key, NULL, 0);
    return label->size && strcmp(string_text(label), key) != 0;
}

static const char *raw_localisation(const char *key) {
    if (!plugin.localisation_db || !*plugin.localisation_db || !**plugin.localisation_db) return NULL;
    // Match the name hash at 0x14B34D2–0x14B34FB.
    uint32_t hash = UINT32_C(0x811c9dc5);
    for (const char *p = key; *p; ++p) hash = hash * UINT32_C(0x1000193) ^ (uint32_t)(int8_t)*p;
    return plugin.lookup_localisation_raw(**plugin.localisation_db, hash);
}

static void localisation_problem(const char *path, unsigned line, const char *key, const char *parameter, const char *reason) {
    static _Thread_local char recent[8][640];
    static _Thread_local unsigned next;
    char message[640];
    snprintf(message, sizeof(message), "political localisation %s:%u key=%s parameter=%s %s", path, line, key, parameter, reason);
    for (unsigned i = 0; i < 8; ++i)
        if (!strcmp(message, recent[i])) return;
    strcpy(recent[next++ % 8], message);
    log_message(ANVIL_LOG_WARNING, message);
}

static void append_ui_label(GameString *text, const char *key, const char *fallback) {
    GameString label = {.capacity = 15};
    if (lookup_ui_label(&label, key)) plugin.append_string(text, string_text(&label), label.size);
    else plugin.append_string(text, fallback, strlen(fallback));
    plugin.free_string(&label);
}

static void native_ui_label(void *control, const char *key, const char *fallback) {
    if (!control) return;
    GameString label = {.capacity = 15};
    const bool found = lookup_ui_label(&label, key);
    native_text(control, found ? string_text(&label) : fallback);
    plugin.free_string(&label);
}

static void native_window_visible(void *window, bool show) {
    void *surface = (unsigned char *)window + 0x30;
    void (*fn)(void *) = method(surface, show ? 0x88 : 0x90);
    fn(surface);
}

static void window_visible(bool show) {
    native_window_visible(panel_view()->window, show);
    plugin.visible = show;
}

static void control_visible(void *control, bool show) {
    void (*fn)(void *) = method(control, show ? 0x88 : 0x90);
    fn(control);
}

static void button_enabled(void *button, bool enabled) {
    void (*enable)(void *, bool) = method(button, 0x170);
    enable(button, enabled);
}

static bool signal_has_listener(const void *signal, const void *listener) {
    // The same singly linked listener chain is checked by the original shield binder.
    for (const void *node = pointer_at(signal, 0x08); node; node = pointer_at(node, 0x10)) {
        if (pointer_at(node, 0) == listener) return true;
    }
    return false;
}

static void queue_diplomacy(void *owner) {
    Shield *shield = owner;
    const PoliticalInstance *instance = selected_instance();
    if (instance && shield->instance == instance->spec && plugin.active && plugin.ready && plugin.visible && !active_selector()->open &&
        plugin.root == *plugin.root_slot && !shield->vacant && shield->tooltip.tag) {
        if (shield->ballot) {
            VoteView *view = &panel_view()->vote;
            if (view->open && !view->input.action)
                view->input = (VoteInput){V_PICK, view->displayed_actor, (unsigned)(shield->tooltip.tag & 0xffffff), view->displayed_round};
            return;
        }
        plugin.pending_country = shield->tooltip.tag;
        plugin.pending_country_instance = instance->spec;
    }
}

static bool bind_window_shield(void *window, Shield *shield, const char *name) {
    *shield = (Shield){0};
    void *button = plugin.find_button(window, name);
    if (!button || pointer_at(button, 0) != plugin.shield_class ||
        pointer_at(button, 0xc8) != plugin.shield_signal_class) return false;
    shield->instance = plugin.instances[plugin.selected].spec;
    shield->tooltip.button = button;
    control_visible(button, false);
    return true;
}

static bool bind_shield(Shield *shield, const char *name) {
    return bind_window_shield(panel_view()->window, shield, name);
}

static void remove_default_click(void *signal) {
    void *default_listener = (unsigned char *)plugin.gui + 0x278;
    if (signal_has_listener(signal, default_listener)) {
        void (*unsubscribe)(void *, void *) = method(signal, 0x10);
        unsubscribe(signal, default_listener);
    }
}

static void disconnect_click(void *button, ClickListener *listener) {
    void *signal = (unsigned char *)button + 0xc8;
    if (!signal_has_listener(signal, listener)) return;
    void (*unsubscribe)(void *, void *) = method(signal, 0x10);
    unsubscribe(signal, listener);
}

static void connect_click(void *button, ClickListener *listener, void *owner, void (*callback)(void *)) {
    void *signal = (unsigned char *)button + 0xc8;
    if (signal_has_listener(signal, listener)) return;
    remove_default_click(signal);
    *listener = (ClickListener){ .vtable = plugin.click_vtable, .owner = owner, .on_click = callback };
    void (*subscribe)(void *, void *) = method(signal, 0x08);
    subscribe(signal, listener);
    button_enabled(button, true);
}

static unsigned reform_actor(void);
static bool current_capital(unsigned country, int32_t *out);

static const char *country_has_flag(unsigned actor, const char *name, bool *present) {
    *present = false;
    if (!actor || !ANVIL_GAME_WORLD_HAS(plugin.world, country_flags) ||
        !plugin.world->flag_id || !plugin.world->country_flags) return "native country flag service unavailable";
    const AnvilGameWorldService *world = plugin.world;
    int32_t id;
    AnvilResult result = world->flag_id(name, &id);
    if (result == ANVIL_ERROR_NOT_FOUND) return NULL;
    if (result != ANVIL_OK) return "cannot resolve country flag identifier";
    AnvilGameFlag stack[64];
    AnvilGameFlag *flags = stack;
    char tag[4];
    uint32_t count = 0;
    result = world->country_flags(election_text(actor, tag), flags, 64, &count);
    if (result == ANVIL_OK && count > 64) {
        const uint32_t capacity = count;
        if (capacity > 65536 || !(flags = malloc(capacity * sizeof(*flags)))) return "country flag list exceeds capacity";
        result = world->country_flags(tag, flags, capacity, &count);
        if (count > capacity) result = ANVIL_ERROR_INTERNAL;
    }
    if (result == ANVIL_OK)
        for (uint32_t i = 0; i < count && !*present; ++i) *present = flags[i].id == id;
    if (flags != stack) free(flags);
    return result == ANVIL_OK ? NULL : "cannot read flags in the viewing country's scope";
}

static const char *test_native_condition(const NativeClause *clause, const NativeScope *scope, bool *allowed);

static EntryContext country_context(const char *instance, unsigned country, EntryQuery query) {
    return (EntryContext){
        .instance = instance, .viewer = country, .query = query, .country_flag = country_has_flag,
        .country_religion = country_religion_matches, .country_government = country_government_matches,
        .country_heir = country_has_heir, .native_condition = test_native_condition,
    };
}

static EntryAccess instance_access(size_t index, unsigned actor) {
    PoliticalInstance *instance = &plugin.instances[index];
    EntryAccess access = {0};
    if (!instance->enabled || !actor) return access;
    const KhanDefinition *definition = &instance->type->definition;
    if (plugin.province_discovered && definition->leader_enabled) {
        int32_t capital;
        if (!instance->holder || !current_capital(instance->holder, &capital) ||
            !plugin.province_discovered(capital, actor)) return access;
    }
    EntryContext context = country_context(instance->spec->id, actor, ps_test_condition);
    context.from = instance->holder;
    const char *reason = entry_permissions(&definition->entry, &context, &access);
    if (reason) {
        access.enabled = false;
        if (!instance->condition_error) {
            char message[420];
            snprintf(message, sizeof(message), "political entry condition instance=%s: %s", instance->spec->id, reason);
            log_message(ANVIL_LOG_ERROR, message);
            instance->condition_error = true;
        }
    } else if (!definition->script_type && !definition->entry.defined) {
        access.visible = access.enabled = access.view;
    }
    return access;
}

static bool entry_access(size_t index, unsigned actor, bool *clickable) {
    const EntryAccess access = instance_access(index, actor);
    *clickable = access.enabled;
    return access.visible;
}

static void queue_panel_toggle(void *owner) {
    const HudEntry *entry = owner;
    if (plugin.active && plugin.ready && entry->visible && entry->clickable && game_gui_context()) {
        plugin.pending_instance = (size_t)(entry - plugin.entries);
        plugin.pending_panel = PANEL_REQUEST_ICON;
    }
}

static void queue_panel_close(void *owner) {
    if (owner == panel_view() && plugin.active && plugin.ready && plugin.visible && game_gui_context())
        plugin.pending_panel = PANEL_REQUEST_CLOSE;
}

static void queue_switch_instance(void *owner) {
    if (owner == panel_view() && plugin.active && plugin.ready && plugin.visible && game_gui_context())
        plugin.pending_panel = PANEL_REQUEST_SWITCH_INSTANCE;
}

static void *find_standard_button(void *window, const char *name) {
    if (!plugin.button_class || !plugin.button_signal_class) return NULL;
    void *button = plugin.find_button(window, name);
    if (!button || pointer_at(button, 0) != plugin.button_class ||
        pointer_at(button, 0xc8) != plugin.button_signal_class) return NULL;
    return button;
}

static void capture_province_card(void *controls, void *window) {
    plugin.original_province_card_bind(controls, window);
    ProvinceBadge *badge = &plugin.province_badge;
    if (badge->card == window) return;
    *badge = (ProvinceBadge){ .card = window };
    if (!window) return;
    void *button = plugin.find_button(window, "gk_prov_member");
    const void *hre_button = plugin.find_button(window, "hre_button");
    if (!button || !hre_button || pointer_at(button, 0) != pointer_at(hre_button, 0) ||
        pointer_at(button, 0xc8) != pointer_at(hre_button, 0xc8)) {
        log_message(ANVIL_LOG_WARNING, "khanate province_badge native province_window button unavailable");
        return;
    }
    // The stock call at 0x1345621 passes CProvinceView+0x9b0 to its binder.
    badge->view = (const unsigned char *)controls - 0x9b0;
    badge->tooltip.button = button;
    control_visible(button, false);
    log_message(ANVIL_LOG_INFO, "khanate province_badge parent=province_window action=bind");
}

static void queue_reform_input(ReformView *view, ReformInput input) {
    if (view == &panel_view()->reform_view && view->bound && plugin.active && plugin.ready &&
        plugin.visible && !active_selector()->open && game_gui_context()) view->pending = input;
}

static void queue_reform_review(void *view) { queue_reform_input(view, REFORM_INPUT_REVIEW); }
static void queue_reform_confirm(void *view) { queue_reform_input(view, REFORM_INPUT_CONFIRM); }
static void queue_reform_cancel(void *view) { queue_reform_input(view, REFORM_INPUT_CANCEL); }

static const TooltipMethods reform_tooltip_methods;

static void bind_reform_view(void *(*find_text)(void *, const char *)) {
    ReformView *view = &panel_view()->reform_view;
    view->bound = false;
    view->authority = find_text(panel_view()->window, "gk_authority_value");
    view->title = find_text(panel_view()->window, "gk_reform_title");
    view->terms = find_text(panel_view()->window, "gk_reform_terms");
    bind_number_control(&panel_view()->authority_value, view->authority, NUMBER_AUTHORITY);
    if (plugin.set_ui_tooltip && view->terms) bind_text_tooltip(view->terms, &view->terms_hover, &reform_tooltip_methods);
    view->status_label = find_text(panel_view()->window, "gk_reform_status");
    static const char *const names[] = { "gk_reform_review", "gk_reform_confirm", "gk_reform_cancel" };
    static void (*const callbacks[])(void *) = { queue_reform_review, queue_reform_confirm, queue_reform_cancel };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
        view->buttons[i] = find_standard_button(panel_view()->window, names[i]);
    if (!view->authority || !view->title || !view->terms || !view->status_label ||
        !view->buttons[0] || !view->buttons[1] || !view->buttons[2]) {
        log_message(ANVIL_LOG_WARNING, "great_khan reform controls unavailable");
        return;
    }
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
        connect_click(view->buttons[i], &view->listeners[i], view, callbacks[i]);
    view->tooltip = (ButtonTooltip){ .methods = &reform_tooltip_methods, .button = view->buttons[0] };
    void (*set_tooltip)(void *, const ButtonTooltip *) = method(view->buttons[0], 0x220);
    set_tooltip(view->buttons[0], &view->tooltip);
    view->bound = true;
}

static void queue_vote_input(void *owner) {
    VoteView *view = &panel_view()->vote;
    if (!plugin.ready || !plugin.visible || active_selector()->open || !view->bound || view->input.action) return;
    for (int i = V_OPEN; i < V_PICK; ++i) {
        if (owner != &view->listeners[i] || (!view->open && i != V_OPEN)) continue;
        view->input = (VoteInput){ .action = (enum VoteWidget)i };
        return;
    }
}

static void *named_scrollbar(void *window, const char *name) {
    if (method(window, 0x128) != (void *)plugin.find_scrollbar) return NULL;
    GameString key = { .capacity = 15 };
    plugin.assign_string(&key, name, strlen(name));
    void *bar = plugin.find_scrollbar(window, &key);
    plugin.free_string(&key);
    return bar;
}

static const TooltipMethods label_tooltip;
static const TooltipMethods resource_tooltip;
static const TooltipMethods leader_history_tooltip;
static const TooltipMethods office_group_tooltip;
static const TooltipMethods nomination_tooltip;
static const TooltipMethods action_tooltip;
static const TooltipMethods number_tooltip;
static void attach_tooltip(void *button, ButtonTooltip *tooltip, const TooltipMethods *operations);
static void queue_action_widget(void *owner);

static bool bind_passive_tooltip(void *window, const char *name, ButtonTooltip *tooltip, const TooltipMethods *methods) {
    void *button = find_standard_button(window, name);
    if (!button) return false;
    attach_tooltip(button, tooltip, methods);
    remove_default_click((unsigned char *)button + 0xc8);
    button_enabled(button, false);
    return true;
}

static bool bind_text_tooltip(void *label, ButtonTooltip *tooltip, const TooltipMethods *methods) {
    if (!label) return false;
    const void *setter = method(label, 0x220);
    const void *button_setter = plugin.button_class ? pointer_at(plugin.button_class, 0x220) : NULL;
    // Textboxes and the stock GUI button share the setter. Coordinates are not used here.
    if (!setter || (setter != plugin.set_ui_tooltip && setter != button_setter)) {
        char message[240];
        snprintf(message, sizeof(message), "political text tooltip unavailable; trying panel hover "
                 "textbox_setter=%p declared_setter=%p stock_button_setter=%p",
                 setter, plugin.set_ui_tooltip, button_setter);
        log_message(ANVIL_LOG_WARNING, message);
        return false;
    }
    attach_tooltip(label, tooltip, methods);
    return true;
}

static void bind_number_control(NumberDisplay *number, void *control, NumberKind kind) {
    number->kind = kind;
    number->control = control;
    if (control && plugin.set_ui_tooltip) bind_text_tooltip(control, &number->hover, &number_tooltip);
}

static bool move_layout_control(void *control, UiPoint offset, bool shield) {
    if (!offset.x && !offset.y) return true;
    const void *read = shield ? plugin.shield_position : plugin.label_position;
    const void *move = shield ? plugin.shield_move : plugin.label_move;
    if (!control || !read || !move || method(control, 0x98) != read || method(control, 0x288) != move) {
        log_message(ANVIL_LOG_ERROR, "political auto-placement control did not match its native position methods");
        return false;
    }
    UiPoint point;
    UiPoint *(*get)(void *, UiPoint *) = method(control, 0x98);
    void (*set)(void *, uint64_t) = method(control, 0x288);
    get(control, &point);
    point.x += offset.x;
    point.y += offset.y;
    const uint64_t packed = (uint32_t)point.x | ((uint64_t)(uint32_t)point.y << 32);
    set(control, packed);
    return true;
}

static void bind_nomination_display(NominationDisplay *display, void *label, void *hit, UiPoint offset) {
    if (!label || !move_layout_control(label, offset, false)) return;
    display->label = label;
    if (!hit) return;
    if (hit == label) {
        if (!bind_text_tooltip(label, &display->hover, &nomination_tooltip)) return;
    } else {
        if (!move_layout_control(hit, offset, true)) return;
        attach_tooltip(hit, &display->hover, &nomination_tooltip);
        remove_default_click((unsigned char *)hit + 0xc8);
        button_enabled(hit, true);
    }
    control_visible(hit, false);
}

static bool set_panel_background(void *icon, const char *sprite) {
    if (!icon || !plugin.set_icon_sprite || !plugin.icon_sprite_name ||
        method(icon, 0x2b0) != (const void *)plugin.set_icon_sprite ||
        method(icon, 0x2b8) != (const void *)plugin.icon_sprite_name) return false;
    const GameString *current = plugin.icon_sprite_name(icon);
    if (!current || !current->size) return false;
    if (!strcmp(string_text(current), sprite)) return true;
    GameString name = { .capacity = 15 };
    plugin.assign_string(&name, sprite, strlen(sprite));
    // The native setter leaves the old sprite in place if the GFX lookup fails.
    plugin.set_icon_sprite(icon, &name);
    plugin.free_string(&name);
    current = plugin.icon_sprite_name(icon);
    return current && !strcmp(string_text(current), sprite);
}

static void bind_panel_backdrop(PoliticalPanel *panel, const PoliticalInstance *instance) {
    const PoliticalLayout *layout = instance->type->layout;
    const KhanDefinition *type = &instance->type->definition;
    const char *sprite = type->entry.background;
    const bool override = sprite[0] && strcmp(sprite, layout->background_sprite);
    if (!plugin.nomination_hover_hook && !override) return;

    void *window = panel->window;
    bool applied = false;
    if (layout->background_is_icon) {
        void *(*find_icon)(void *, const char *) = method(window, 0xc8);
        if (override && plugin.find_window_icon && plugin.window_icon_method &&
            find_icon == plugin.window_icon_method) {
            const unsigned char *fixed_window = window;
            // Use the stock lookup before FindIcon's shared placeholder fallback.
            panel->backdrop = plugin.find_window_icon(layout->background, (int)strlen(layout->background),
                fixed_window + SYMVAL_WINDOW_ICON_LOOKUP_NAMES, fixed_window + SYMVAL_WINDOW_ICON_LOOKUP_CONTROLS);
            applied = set_panel_background(panel->backdrop, sprite);
        } else if (plugin.nomination_hover_hook && find_icon) {
            panel->backdrop = find_icon(window, layout->background);
        }
    } else if (plugin.nomination_hover_hook) {
        panel->backdrop = find_standard_button(window, layout->background);
    }
    if (!override) return;

    char message[4 * GK_SCRIPT_WORD_SIZE + PS_FILE_PATH_SIZE + 128];
    snprintf(message, sizeof(message),
             "political panel background instance=%s layout=%s requested=%s default=%s status=%s file=%s:%u",
             instance->spec->id, layout->id, sprite, layout->background_sprite, applied ? "applied" : "fallback",
             type->source_path, type->source_line);
    log_message(applied ? ANVIL_LOG_INFO : ANVIL_LOG_WARNING, message);
}

static bool bind_scripted_panel(const PoliticalInstance *instance) {
    const PoliticalLayout *layout = instance->type->layout;
    if (!layout || !plugin.shield_class || !plugin.shield_signal_class) return false;
    PoliticalPanel *panel = panel_view();
    const KhanDefinition *definition = &instance->type->definition;
    char progress[350];
    snprintf(progress, sizeof(progress), "political panel bind instance=%s window=%s stage=create",
             instance->spec->id, layout->window);
    log_message(ANVIL_LOG_INFO, progress);
    panel->window = create_named_window(layout->window);
    if (!panel->window) return false;
    log_message(ANVIL_LOG_INFO, "political panel bind stage=hide_native_window");
    native_window_visible(panel->window, false);
    void *(*find_text)(void *, const char *) = method(panel->window, 0x98);
    if (!find_text) return false;
    bind_panel_backdrop(panel, instance);
    if (layout->hover[0]) {
        void *hover = find_standard_button(panel->window, layout->hover);
        if (hover) {
            // The invisible area receives hover. The visual backdrop is drawn independently.
            attach_tooltip(hover, &panel->label_hover, &label_tooltip);
            remove_default_click((unsigned char *)hover + 0xc8);
            button_enabled(hover, false);
            log_message(ANVIL_LOG_INFO, "political panel hover_control=read_only");
        } else {
            log_message(ANVIL_LOG_WARNING, "political panel hover_control unavailable");
        }
    }
    panel->title = layout->title[0] ? find_text(panel->window, layout->title) : NULL;
    panel->emperor_caption = layout->leader_caption[0] ? find_text(panel->window, layout->leader_caption) : NULL;
    panel->emperor_label = layout->leader_label[0] ? find_text(panel->window, layout->leader_label) : NULL;
    if (layout->leader_history[0] &&
        !bind_passive_tooltip(panel->window, layout->leader_history, &panel->history_hover, &leader_history_tooltip))
        return false;
    if (layout->leader[0] &&
        (!bind_shield(&panel->holder_shield, layout->leader) ||
         !move_layout_control(panel->holder_shield.tooltip.button, layout->leader_offset, true))) return false;
    log_message(ANVIL_LOG_INFO, "political panel bind stage=member_controls");
    panel->member_rows = layout->members.rows;
    panel->member_columns = layout->members.columns;
    if (panel->member_rows) {
        panel->daimyo_label = find_text(panel->window, layout->members.caption);
        bind_number_control(&panel->members_count, panel->daimyo_label, NUMBER_MEMBERS);
        panel->scrollbar = named_scrollbar(panel->window, layout->members.scrollbar);
        if (!panel->daimyo_label || !panel->scrollbar) return false;
        for (unsigned i = 0; i < panel->member_rows * panel->member_columns; ++i) {
            char name[GK_SCRIPT_WORD_SIZE + 12];
            snprintf(name, sizeof(name), "%s_%02u", layout->members.row, i);
            Shield *shield = &panel->daimyo_shields[i];
            if (!bind_shield(shield, name) ||
                !move_layout_control(shield->tooltip.button, layout->members.shield_offset[i], true)) return false;
        }
    }
    if (layout->roles.rows) {
        panel->roles.caption = find_text(panel->window, layout->roles.caption);
        panel->roles.list.bar = named_scrollbar(panel->window, layout->roles.scrollbar);
        if (!panel->roles.caption || !panel->roles.list.bar) return false;
        if (layout->roles.group_heading[0]) {
            for (unsigned i = 0; i < layout->roles.rows; ++i) {
                struct OfficeGroupHeading *group = &panel->roles.groups[i];
                char name[GK_SCRIPT_WORD_SIZE + 12];
                snprintf(name, sizeof(name), "%s_%02u", layout->roles.group_heading, i);
                group->label = find_text(panel->window, name);
                snprintf(name, sizeof(name), "%s_%02u", layout->roles.group_hover, i);
                if (!group->label || !bind_passive_tooltip(panel->window, name, &group->hover, &office_group_tooltip))
                    return false;
                control_visible(group->hover.button, false);
            }
        }
        for (unsigned i = 0; i < layout->roles.rows * layout->roles.columns; ++i) {
            char name[GK_SCRIPT_WORD_SIZE + 12];
            if (layout->roles.label[0]) {
                snprintf(name, sizeof(name), "%s_%02u", layout->roles.label, i);
                panel->roles.labels[i] = find_text(panel->window, name);
                if (!panel->roles.labels[i] ||
                    !move_layout_control(panel->roles.labels[i], layout->roles.label_offset[i], false)) return false;
            }
            snprintf(name, sizeof(name), "%s_%02u", layout->roles.row, i);
            Shield *shield = &panel->roles.shields[i];
            if (!bind_shield(shield, name) ||
                !move_layout_control(shield->tooltip.button, layout->roles.shield_offset[i], true)) return false;
            if (layout->roles.nomination_score[0]) {
                snprintf(name, sizeof(name), "%s_%02u", layout->roles.nomination_score, i);
                void *label = find_text(panel->window, name);
                void *hit = label;
                if (layout->roles.nomination_hover[0]) {
                    snprintf(name, sizeof(name), "%s_%02u", layout->roles.nomination_hover, i);
                    hit = find_standard_button(panel->window, name);
                }
                bind_nomination_display(&panel->roles.scores[i], label, hit, layout->roles.shield_offset[i]);
            }
            if (layout->roles.support_shield[0]) {
                snprintf(name, sizeof(name), "%s_%02u", layout->roles.support_shield, i);
                Shield *support = &panel->roles.supports[i];
                if (bind_shield(support, name) &&
                    !move_layout_control(support->tooltip.button, layout->roles.shield_offset[i], true))
                    *support = (Shield){0};
            }
        }
    }
    if (layout->authority[0]) {
        panel->khanate_authority = find_text(panel->window, layout->authority);
        if (!panel->khanate_authority) return false;
        bind_number_control(&panel->authority_value, panel->khanate_authority, NUMBER_AUTHORITY);
    }
    const ResourceSummaryLayout *summary = &layout->resource_summary;
    if (summary->value[0]) {
        panel->resources.summary.value = find_text(panel->window, summary->value);
        panel->resources.growth = find_text(panel->window, summary->growth);
        if (summary->hover[0]) {
            bind_passive_tooltip(panel->window, summary->hover, &panel->resources.summary.hover, &resource_tooltip);
        } else {
            bind_text_tooltip(panel->resources.summary.value, &panel->resources.summary.hover, &resource_tooltip);
            bind_text_tooltip(panel->resources.growth, &panel->resources.growth_hover, &resource_tooltip);
        }
    }
    if (layout->resources.rows) {
        panel->resources.list.bar = named_scrollbar(panel->window, layout->resources.scrollbar);
        if (!panel->resources.list.bar) return false;
        for (unsigned i = 0; i < layout->resources.rows; ++i) {
            struct ResourceDisplayRow *row = &panel->resources.rows[i];
            char name[GK_SCRIPT_WORD_SIZE + 12];
            snprintf(name, sizeof(name), "%s_%02u", layout->resources.row, i);
            row->label = find_text(panel->window, name);
            if (!row->label) return false;
            if (!layout->resources.value[0]) continue;
            snprintf(name, sizeof(name), "%s_%02u", layout->resources.value, i);
            row->value = find_text(panel->window, name);
            if (!row->value) continue;
            if (layout->resources.hover[0]) {
                snprintf(name, sizeof(name), "%s_%02u", layout->resources.hover, i);
                bind_passive_tooltip(panel->window, name, &row->hover, &resource_tooltip);
            } else bind_text_tooltip(row->value, &row->hover, &resource_tooltip);
            if (row->hover.button) control_visible(row->hover.button, false);
        }
        if (layout->resources.value[0]) log_message(ANVIL_LOG_INFO, "political resource rows bound to native tooltip controls");
    }
    if (layout->actions.rows) {
        panel->actions.list.bar = named_scrollbar(panel->window, layout->actions.scrollbar);
        if (!panel->actions.list.bar) return false;
        for (unsigned i = 0; i < layout->actions.rows; ++i) {
            struct ActionDisplayRow *row = &panel->actions.rows[i];
            char name[GK_SCRIPT_WORD_SIZE + 12];
            snprintf(name, sizeof(name), "%s_%02u", layout->actions.label, i);
            row->label = find_text(panel->window, name);
            snprintf(name, sizeof(name), "%s_%02u", layout->actions.row, i);
            void *button = find_standard_button(panel->window, name);
            if (!row->label || !button) return false;
            attach_tooltip(button, &row->hover, &action_tooltip);
            remove_default_click((unsigned char *)button + 0xc8);
            control_visible(button, false);
            if (layout->action_status[0]) {
                snprintf(name, sizeof(name), "%s_%02u", layout->action_status, i);
                native_text(find_text(panel->window, name), "");
            }
            if (!layout->action_read_only[0]) continue;
            snprintf(name, sizeof(name), "%s_%02u", layout->action_read_only, i);
            void *passive = find_standard_button(panel->window, name);
            snprintf(name, sizeof(name), "%s_%02u", layout->action_seal, i);
            row->seal = find_standard_button(panel->window, name);
            snprintf(name, sizeof(name), "%s_%02u", layout->action_check, i);
            row->check = find_standard_button(panel->window, name);
            if (!passive || !row->seal || !row->check) return false;
            attach_tooltip(passive, &row->read_only, &action_tooltip);
            remove_default_click((unsigned char *)passive + 0xc8);
            button_enabled(passive, false);
            control_visible(passive, false);
            control_visible(row->seal, false);
            control_visible(row->check, false);
        }
    }
    if (layout->close[0]) {
        void *close = find_standard_button(panel->window, layout->close);
        if (!close) return false;
        panel->close_button = close;
        if (definition->entry.close_button) connect_click(close, &panel->close_listener, panel, queue_panel_close);
        else control_visible(close, false);
    }
    if (layout->instances[0]) {
        panel->switch_button = find_standard_button(panel->window, layout->instances);
        if (!panel->switch_button) return false;
        connect_click(panel->switch_button, &panel->switch_listener, panel, queue_switch_instance);
        control_visible(panel->switch_button, false);
    }
    if (layout->ballot[0]) {
        VoteView *vote = &panel->vote;
        void *button = find_standard_button(panel->window, layout->ballot);
        if (!button) return false;
        if (elective(instance)) {
            vote->buttons[V_OPEN] = button;
            connect_click(button, &vote->listeners[V_OPEN], &vote->listeners[V_OPEN], queue_vote_input);
            vote->bound = true;
        } else control_visible(button, false);
    }
    panel->show_shields = true;
    const char *score_hover = "none", *primary_hover = "none";
    if (layout->roles.nomination_score[0])
        score_hover = !panel->roles.scores[0].hover.button ? "panel" : layout->roles.nomination_hover[0] ? "button" : "text";
    if (summary->value[0])
        primary_hover = !panel->resources.summary.hover.button ? "panel" : summary->hover[0] ? "button" : "text";
    char message[720];
    snprintf(message, sizeof(message), "political native_window=%s layout=%s instance=%s slots=%u shield_placement=%s/%s/%s "
             "nomination_hover=%s support_shield=%s primary_resource_hover=%s",
             layout->window, layout->id, instance->spec->id, panel->member_rows * panel->member_columns,
             layout->leader_frame[0] ? "auto" : "gui", layout->members.frame[0] ? "auto" : "gui",
             layout->roles.frame[0] ? "auto" : "gui",
             score_hover, panel->roles.supports[0].tooltip.button ? layout->roles.support_shield : "none",
             primary_hover);
    log_message(ANVIL_LOG_INFO, message);
    return true;
}

static bool bind_vote_view(void *(*find_text)(void *, const char *), bool shields, bool enabled) {
    VoteView *view = &panel_view()->vote;
    void *window = panel_view()->window;
    view->buttons[V_OPEN] = find_standard_button(window, "gk_vote_open");
    view->tumen.bar = named_scrollbar(window, "gk_tumen_scroll");
    if (!view->buttons[V_OPEN] || !view->tumen.bar) return false;
    control_visible(view->tumen.bar,false);
    connect_click(view->buttons[V_OPEN], &view->listeners[V_OPEN], &view->listeners[V_OPEN], queue_vote_input);
    control_visible(view->buttons[V_OPEN], enabled);
    for (unsigned i = 0; i < TUMEN_SLOTS; ++i) {
        char name[20];
        snprintf(name,sizeof(name),"gk_support_%02u",i);
        if (shields) {
            if (!bind_shield(&panel_view()->supports[i],name)) return false;
            snprintf(name,sizeof(name),"gk_nom_score_%02u",i);
            void *label = find_text(window,name);
            snprintf(name,sizeof(name),"gk_nom_hit_%02u",i);
            if (label) bind_nomination_display(&panel_view()->tumen_scores[i], label,
                    find_standard_button(window,name), (UiPoint){0});
        }
        snprintf(name,sizeof(name),"gk_vote_state_%02u",i);
        panel_view()->support_labels[i] = find_text(window,name);
        if (!panel_view()->support_labels[i]) return false;
    }
    view->bound = true;
    return true;
}

static bool create_ballot(VoteView *view) {
    if (view->window) return true;
    const GameString template = { .inline_text = "gk_ballot_view", .size = 14, .capacity = 15 };
    void *window = plugin.create_window(plugin.gui, &template);
    if (!window) return false;
    native_window_visible(window, false);
    static const char *const names[] = { "", "", "gk_vote_close", "gk_vote_country", "gk_vote_opinion" };
    for (int i = V_CLOSE; i < V_PICK; ++i) {
        view->buttons[i] = find_standard_button(window,names[i]);
        if (!view->buttons[i]) return false;
        connect_click(view->buttons[i], &view->listeners[i], &view->listeners[i], queue_vote_input);
    }
    void *(*find_text)(void *, const char *) = method(window,0x98);
    for (unsigned i=0; i<BALLOT_ROWS; ++i) {
        char name[24];
        snprintf(name,sizeof(name),"gk_pick_%u",i);
        if (!bind_window_shield(window,&view->choices[i],name)) return false;
        view->choices[i].ballot = true;
        snprintf(name,sizeof(name),"gk_name_%u",i);
        view->names[i] = find_text(window,name);
        snprintf(name,sizeof(name),"gk_opinion_%u",i);
        view->opinions[i] = find_text(window,name);
        if (!view->names[i] || !view->opinions[i]) return false;
        bind_number_control(&view->relation[i], view->opinions[i], NUMBER_OPINION);
    }
    view->list.bar = named_scrollbar(window,"gk_vote_scroll");
    if (!view->list.bar) return false;
    view->backdrop = find_standard_button(window, "gk_vote_back");
    view->window = window;
    return true;
}

static bool bind_window_template(bool use_shields) {
    const PoliticalInstance *instance = selected_instance();
    if (!instance) return false;
    const bool capital = uses_capital_members(instance);
    const char *layout = capital ? (use_shields ? "gk_khanate_view" : "gk_khanate_text")
                                 : (use_shields ? "gk_shields_view" : "gk_history_view");
    const EntryButton *entry = &instance->type->definition.entry;
    if (entry->panel[0] && use_shields) layout = entry->panel;
    GameString name = { .capacity = 15 };
    plugin.assign_string(&name, layout, strlen(layout));
    log_message(ANVIL_LOG_INFO, use_shields ? "great_khan creating native shield panel" : "great_khan creating native text panel");
    panel_view()->window = plugin.create_window(plugin.gui, &name);
    plugin.free_string(&name);
    if (!panel_view()->window) return false;
    panel_view()->member_rows = capital ? KHANATE_MEMBER_ROWS : DAIMYO_ROWS;
    panel_view()->member_columns = DAIMYO_COLUMNS;
    const uint32_t member_slots = panel_view()->member_rows * DAIMYO_COLUMNS;
    native_window_visible(panel_view()->window, false);

    void *(*find_text)(void *, const char *) = method(panel_view()->window, 0x98);
    panel_view()->title = find_text(panel_view()->window, "gk_title");
    panel_view()->emperor_caption = find_text(panel_view()->window, "gk_holder_caption");
    panel_view()->emperor_label = find_text(panel_view()->window, "gk_holder_value");
    panel_view()->daimyo_label = find_text(panel_view()->window, "gk_daimyo_count");
    bind_number_control(&panel_view()->members_count, panel_view()->daimyo_label, NUMBER_MEMBERS);
    static const char scrollbar_name[] = "gk_daimyo_scroll";
    GameString scroll_key = { .capacity = 15 };
    plugin.assign_string(&scroll_key, scrollbar_name, sizeof(scrollbar_name) - 1);
    panel_view()->scrollbar = method(panel_view()->window, 0x128) == (void *)plugin.find_scrollbar
        ? plugin.find_scrollbar(panel_view()->window, &scroll_key) : NULL;
    plugin.free_string(&scroll_key);
    if (!panel_view()->emperor_caption || !panel_view()->emperor_label || !panel_view()->daimyo_label || !panel_view()->scrollbar) return false;
    if (use_shields && !bind_shield(&panel_view()->holder_shield, "gk_holder_shield")) return false;
    for (unsigned i = 0; i < member_slots; ++i) {
        char key[16];
        if (use_shields) {
            snprintf(key, sizeof(key), "gk_shield_%02u", i);
            if (!bind_shield(&panel_view()->daimyo_shields[i], key)) return false;
        } else {
            snprintf(key, sizeof(key), "gk_daimyo_%02u", i);
            panel_view()->daimyo_labels[i] = find_text(panel_view()->window, key);
            if (!panel_view()->daimyo_labels[i]) return false;
        }
    }
    if (capital) {
        if (!bind_vote_view(find_text, use_shields, instance->type->definition.election.defined &&
                            !instance->type->definition.election.hereditary)) return false;
        panel_view()->tumen_caption = find_text(panel_view()->window, "gk_tumen_count");
        panel_view()->khanate_authority = find_text(panel_view()->window, "gk_khanate_authority");
        panel_view()->khanate_provinces = find_text(panel_view()->window, "gk_khanate_provinces");
        bind_number_control(&panel_view()->electors_count, panel_view()->tumen_caption, NUMBER_ELECTORS);
        bind_number_control(&panel_view()->authority_value, panel_view()->khanate_authority, NUMBER_AUTHORITY);
        bind_number_control(&panel_view()->territory_count, panel_view()->khanate_provinces, NUMBER_TERRITORY);
        if (!panel_view()->tumen_caption || !panel_view()->khanate_authority || !panel_view()->khanate_provinces)
            return false;
        for (unsigned i = 0; i < TUMEN_SLOTS; ++i) {
            char key[16];
            if (use_shields) {
                snprintf(key, sizeof(key), "gk_tumen_%02u", i);
                if (!bind_shield(&panel_view()->tumen_shields[i], key)) return false;
            } else {
                snprintf(key, sizeof(key), "gk_tumen_txt_%02u", i);
                panel_view()->tumen_labels[i] = find_text(panel_view()->window, key);
                if (!panel_view()->tumen_labels[i]) return false;
            }
        }
    }
    void *close = find_standard_button(panel_view()->window, "gk_close_button");
    if (close && entry->defined && !entry->close_button) control_visible(close, false);
    else if (close) connect_click(close, &panel_view()->close_listener, panel_view(), queue_panel_close);
    if (!close && entry->defined && entry->close_button) return false;
    log_message(close ? ANVIL_LOG_INFO : ANVIL_LOG_WARNING,
                close ? "great_khan panel_close_button=bound" : "great_khan panel_close_button=unavailable");
    if (!capital) bind_reform_view(find_text);
    panel_view()->show_shields = use_shields;
    char message[260];
    snprintf(message, sizeof(message), "political native_window=%s instance=%s window=%p slots=%u",
             layout, plugin.instances[plugin.selected].spec->id, panel_view()->window, member_slots);
    log_message(ANVIL_LOG_INFO, message);
    return true;
}

static bool bind_window(void) {
    if (panel_view()->bad_template) return false;
    if (panel_view()->window) return true;
    if (selected_instance()->type->definition.script_type) {
        if (bind_scripted_panel(selected_instance())) return true;
        panel_view()->bad_template = true;
        const PoliticalLayout *layout = selected_instance()->type->layout;
        if (layout) {
            ScriptError error = { .line = layout->line, .column = 1,
                                  .reason = "mounted native GUI window or control could not be instantiated" };
            log_file_error(layout->path, plugin.files.source(layout->path), &error);
        }
        return false;
    }
    const bool authored = selected_instance()->type->definition.entry.panel[0] != 0;
    if (authored && (!plugin.use_shields || !plugin.shield_class || !plugin.shield_signal_class)) {
        log_message(ANVIL_LOG_ERROR, "political authored panel requires native shield controls");
        return false;
    }
    if (plugin.use_shields && plugin.shield_class && plugin.shield_signal_class) {
        if (bind_window_template(true)) return true;
        if (authored) {
            log_message(ANVIL_LOG_ERROR, "political authored panel missing or incompatible native controls");
            panel_view()->bad_template = true;
            return false;
        }
        log_message(ANVIL_LOG_WARNING, "great_khan shield controls were unavailable; opening the text panel");
    }
    if (bind_window_template(false)) return true;
    panel_view()->bad_template = true;
    log_message(ANVIL_LOG_ERROR, "great_khan could not bind the history window");
    return false;
}

static void *country_from_tag(uint64_t tag) {
    const void *db = *plugin.countries_db;
    if (!db || !tag) return NULL;
    const int index = (int16_t)(tag >> 32);
    void *const *countries = pointer_at(db, 0x118);
    if (index < 0 || !countries || !countries[index]) return NULL;
    unsigned char *country = countries[index];
    int32_t provinces;
    memcpy(&provinces, country + 0x1344, sizeof(provinces));
    return provinces > 0 ? country : NULL;
}

static uint64_t registered_country_tag(const char text[4]) {
    const void *db = *plugin.countries_db;
    if (!text[0] || !db) return 0;
    uint64_t tag = 0;
    plugin.lookup_tag(db, &tag, text, true);
    return plugin.valid_tag(&tag) ? tag : 0;
}

static uint64_t country_tag(const char text[4]) {
    const uint64_t tag = registered_country_tag(text);
    return country_from_tag(tag) ? tag : 0;
}

static const char *country_religion_matches(unsigned country, const char *key, bool *allowed) {
    *allowed = false;
    if (!plugin.religion_bound) return "native country religion is unavailable";
    char tag[4];
    const void *actor = country_from_tag(registered_country_tag(election_text(country, tag)));
    if (!actor) return NULL;
    const unsigned char *religion = pointer_at(actor, SYMVAL_COUNTRY_RELIGION_SITE_OFFSET);
    if (!religion) return NULL;
    const GameString *id = (const void *)(religion + SYMVAL_RELIGION_KEY_SITE_OFFSET);
    const size_t length = strlen(key);
    if (id->size == length && id->capacity >= length) {
        const char *name = string_text(id);
        *allowed = name && memcmp(name, key, length) == 0;
    }
    return NULL;
}

static bool current_capital(unsigned country, int32_t *out) {
    if (plugin.capital_bound) {
        char tag[4];
        const unsigned char *actor = country_from_tag(registered_country_tag(election_text(country, tag)));
        if (!actor) return false;
        memcpy(out, actor + SYMVAL_COUNTRY_CAPITAL_SITE_OFFSET, sizeof(*out));
        return true;
    }
    CountryFact facts;
    if (!country_fact(country, &facts)) return false;
    *out = facts.capital;
    return true;
}

static bool native_province_discovered(int32_t province, unsigned country) {
    if (!plugin.root || plugin.root != *plugin.root_slot || province <= 0 || province >= plugin.province_limit)
        return false;
    char text[4];
    const uint64_t tag = registered_country_tag(election_text(country, text));
    const int index = (int16_t)(tag >> 32);
    if (!tag || index < 0) return false;
    const unsigned char *array = pointer_at(plugin.root, SYMVAL_PROVINCE_DISCOVERY_ARRAY_ARRAY);
    if (!array) return false;
    const unsigned char *site = array + (size_t)province * SYMVAL_PROVINCE_DISCOVERY_ARRAY_STRIDE;
    if (!(site[SYMVAL_PROVINCE_DISCOVERY_FLAGS_FLAGS] & SYMVAL_PROVINCE_DISCOVERY_FLAGS_ACTIVE) ||
        (uint8_t)(tag >> 48) == 7) return true;
    const uint32_t *words = pointer_at(site, SYMVAL_PROVINCE_DISCOVERY_FLAGS_WORDS);
    return words && (words[(unsigned)index / 32] & (UINT32_C(1) << ((unsigned)index % 32)));
}

static const char *country_government_matches(unsigned country, const char *key, bool *allowed) {
    *allowed = false;
    if (!plugin.government_stack || !plugin.government_kind) return "native country government unavailable";
    char tag[4];
    const void *actor = country_from_tag(registered_country_tag(election_text(country, tag)));
    if (!actor) return "no live country for government lookup";
    *allowed = plugin.government_kind(plugin.government_stack(actor)) == government_category(key);
    return NULL;
}

static const char *country_has_heir(unsigned country, bool *allowed) {
    *allowed = false;
    if (!plugin.world || !plugin.world->monarch) return "native heir query unavailable";
    char tag[4];
    AnvilGameMonarch ruler = { .size = sizeof(ruler) };
    const AnvilResult result = plugin.world->monarch(election_text(country, tag), ANVIL_GAME_MONARCH_HEIR, &ruler);
    *allowed = result == ANVIL_OK;
    return result == ANVIL_OK || result == ANVIL_ERROR_NOT_FOUND ? NULL : "native heir query failed";
}

static bool describe_shield(const ButtonTooltip *tooltip, const void *button, GameString *text) {
    const PoliticalInstance *instance = selected_instance();
    const Shield *shield = (const void *)tooltip;
    if (!plugin.active || !plugin.ready || !plugin.visible || plugin.root != *plugin.root_slot ||
        !instance || shield->instance != instance->spec || button != tooltip->button) return false;
    if (number_route.carrier == button && number_route.number) {
        const NumberDisplay *number = number_route.number;
        number_route.carrier = NULL;
        if (describe_number(instance, number, text)) return true;
    }
    if (describe_routed_nomination(instance, shield, text)) return true;
    if (shield->vacant) {
        append_label(text, "PS_OFFICE_VACANT");
        return true;
    }
    if (!tooltip->tag) return false;

    char tag_text[4] = {0};
    memcpy(tag_text, &tooltip->tag, 3);
    const uint64_t tag = country_tag(tag_text);
    if (tag != tooltip->tag) return false;

    const bool is_holder = tooltip == &panel_view()->holder_shield.tooltip;
    const bool capital = uses_capital_members(instance);
    const PoliticalLayout *layout = instance->type->layout;
    if (!is_holder && !capital && !layout && !shield->ballot && !shield->voter)
        return plugin.describe_daimyo(tooltip, button, text);

    const GameString *name = plugin.country_name(&tag, false, false, false);
    const char *label = tag_text;
    size_t length = 3;
    if (name && name->size) {
        label = string_text(name);
        length = name->size;
    }
    plugin.append_string(text, label, length);
    plugin.append_string(text, "\n", 1);
    if (is_holder) {
        append_leader_history(instance, text);
        if (layout && layout->leader_tooltip[0]) {
            plugin.append_string(text, "\n", 1);
            append_instance_text(text, layout->leader_tooltip, instance, instance->holder);
        }
        return true;
    }
    if (shield->role_tooltip) {
        append_instance_text(text, shield->role_tooltip, instance, election_tag(tag_text));
        return true;
    }
    if (layout && !shield->ballot && !shield->voter) {
        const char *key = layout->members.tooltip;
        append_instance_text(text, key[0] ? key : "PS_TOOLTIP_MEMBER", instance, election_tag(tag_text));
        return true;
    }
    if (shield->ballot || shield->voter) {
        const unsigned voter = shield->ballot ? panel_view()->vote.displayed_actor : shield->voter;
        const unsigned until = election_locked_until(&instance->election,voter,instance->roll.date);
        char detail[40], who[4];
        int weight = instance->type->definition.election.weight;
        for (unsigned i=0; i<instance->election.count; ++i)
            if (instance->election.entries[i].voter == voter && instance->election.entries[i].weight >= 0)
                weight = instance->election.entries[i].weight;
        snprintf(detail, sizeof(detail), "%s | ", election_text(voter, who));
        plugin.append_string(text, detail, strlen(detail));
        if (!until) {
            append_ui_label(text, shield->ballot ? "PS_VOTE_CAST" : "PS_VOTE_SUPPORT",
                            shield->ballot ? "Cast vote" : "Supports this country");
            plugin.append_string(text, " | ", 3);
        }
        snprintf(detail, sizeof(detail), "%d ", weight);
        plugin.append_string(text, detail, strlen(detail));
        append_ui_label(text, "PS_VOTE_COUNT", "vote(s)");
        if (until) {
            plugin.append_string(text, "\n", 1);
            append_ui_label(text, "PS_VOTE_NEXT", "Next vote: ");
            snprintf(detail, sizeof(detail), "%u.%u.%u", until / 10000, until / 100 % 100, until % 100);
            plugin.append_string(text, detail, strlen(detail));
        }
        return true;
    }
    if (has_khan_tumen(&instance->khanate_history, instance->history_date, tag_text)) {
        append_ui_label(text, "PS_TUMEN_TITLE", "Tumen");
    } else {
        append_ui_label(text, "PS_MEMBER_TITLE", "Member");
    }
    return true;
}

static ButtonTooltip *release_button_tooltip(ButtonTooltip *tooltip, unsigned flags) {
    (void)flags;
    // Providers are embedded in plugin state, which outlives the controls.
    return tooltip;
}

static const char *office_tooltip_key(const MemberLayout *layout, const PoliticalRole *role) {
    if (role->tooltip_key[0]) return role->tooltip_key;
    return layout->tooltip[0] ? layout->tooltip : "PS_ROLE_MEMBER";
}

static bool describe_office_group(const ButtonTooltip *tooltip, const void *button, GameString *text) {
    if (!plugin.active || !plugin.ready || !plugin.visible || active_selector()->open ||
        !game_gui_context() || button != tooltip->button) return false;
    const PoliticalInstance *instance = selected_instance();
    const PoliticalLayout *layout = instance ? instance->type->layout : NULL;
    if (!layout) return false;
    for (unsigned row = 0; row < layout->roles.rows; ++row) {
        const struct OfficeGroupHeading *group = &panel_view()->roles.groups[row];
        if (tooltip == &group->hover && group->role) {
            append_instance_text(text, office_tooltip_key(&layout->roles, group->role), instance, reform_actor());
            const PoliticalContest *contest = &instance->contests[group->role - instance->type->definition.roles];
            if (contest->opened && !contest->settled) {
                plugin.append_string(text, "\n", 1);
                append_label(text, "PS_ROLE_COMPETITION");
            }
            return true;
        }
    }
    return false;
}

static const TooltipMethods office_group_tooltip = {describe_office_group, release_button_tooltip};

static void append_amount(GameString *text, const char *key, const char *fallback, int64_t amount,
                          unsigned precision, bool sign, const char *suffix) {
    char value[64];
    snprintf(value, sizeof(value), sign ? ": %+.*f%s\n" : ": %.*f%s\n",
             (int)precision, (double)amount / PS_RESOURCE_SCALE, suffix);
    append_ui_label(text, key, fallback);
    plugin.append_string(text, value, strlen(value));
}

static void resource_details(const struct ResourceDisplayRow *row, bool monthly, GameString *text) {
    const ResourceDefinition *def = row->definition;
    if (!def || row->owner == UINT_MAX) return;
    if (!monthly) {
        append_amount(text, row->stored ? "PS_AMOUNT_STORED" : "PS_AMOUNT_INITIAL",
                      row->stored ? "Stored balance" : "Starting balance", row->balance, def->precision, false, "");
        append_amount(text, "PS_AMOUNT_MIN", "Minimum", def->minimum, def->precision, false, "");
        append_amount(text, "PS_AMOUNT_MAX", "Maximum", def->maximum, def->precision, false, "");
        return;
    }
    if (!row->has_growth) {
        append_ui_label(text, "PS_AMOUNT_GROWTH_UNAVAILABLE", "Monthly calculation unavailable");
        return;
    }
    append_amount(text, "PS_AMOUNT_MONTHLY_BASE", "Base per month", def->monthly, def->precision, true, "");
    for (unsigned i = 0; i < def->term_count; ++i) {
        if (!(row->growth.terms & (1u << i))) continue;
        const ResourceTerm *term = &def->terms[i];
        const char *label = term->tooltip[0] ? term->tooltip : term->id;
        if (term->add) append_amount(text, label, term->id, term->add, def->precision, true, "");
        if (term->modifier) append_amount(text, label, term->id, (int64_t)term->modifier * 100, 1, true, "%");
    }
    if (def->growth_modifier[0] && row->growth.native_country) {
        append_election_country(text, row->growth.native_country);
        plugin.append_string(text, " · ", sizeof(" · ") - 1);
        append_amount(text, def->growth_modifier, def->growth_modifier, (int64_t)row->growth.native_modifier * 100, 1, true, "%");
    }
    append_amount(text, "PS_AMOUNT_MONTHLY_FACTOR", "Combined multiplier", row->growth.factor,
                  3, false, "");
    append_amount(text, "PS_AMOUNT_MONTHLY_UNCAPPED", "Before limits", row->growth.uncapped, def->precision, true, "");
    if (row->growth.uncapped != row->change)
        append_amount(text, "PS_AMOUNT_MONTHLY_LIMIT", "Adjustment at the resource limit",
                      (int64_t)row->change - row->growth.uncapped, def->precision, true, "");
    append_amount(text, "PS_AMOUNT_MONTHLY", "Effective monthly change", row->change, def->precision, true, "");
}

static bool describe_resource_row(const struct ResourceDisplayRow *row, const PoliticalInstance *instance,
                                  bool monthly, GameString *text) {
    if (!row->ownership) return false;
    if (row->name) {
        append_label(text, row->name);
        plugin.append_string(text, "\n", 1);
    }
    append_label(text, row->ownership);
    GameString details = {.capacity = 15};
    resource_details(row, monthly, &details);
    bool used_breakdown = false;
    if (row->detail) {
        const TextField fields[] = {
            {"value", row->owner != UINT_MAX && (!monthly || row->has_growth) ?
                       text_fixed(monthly ? row->change : row->balance) : (TextValue){0}},
            {"current", row->owner != UINT_MAX ? text_fixed(row->balance) : (TextValue){0}},
            {"change", row->has_growth ? text_fixed(row->change) : (TextValue){0}},
            {"base", row->definition ? text_fixed(row->definition->monthly) : (TextValue){0}},
            {"breakdown", text_value(string_text(&details))},
        };
        plugin.append_string(text, "\n", 1);
        if (!append_political_text(text, row->detail, (PoliticalText){.instance = instance, .resource = row->definition,
            .country = row->viewer, .fields = fields, .count = sizeof(fields) / sizeof(*fields)}, &used_breakdown) &&
            (!plugin.text_engine.lookup || !plugin.text_engine.lookup(row->detail))) append_label(text, row->detail);
    }
    if (details.size && !used_breakdown) {
        plugin.append_string(text, "\n", 1);
        plugin.append_string(text, string_text(&details), details.size);
    }
    plugin.free_string(&details);
    return true;
}

static bool describe_resource(const ButtonTooltip *tooltip, const void *button, GameString *text) {
    if (!plugin.active || !plugin.ready || !plugin.visible || active_selector()->open ||
        !game_gui_context() || button != tooltip->button) return false;
    const PoliticalInstance *instance = selected_instance();
    const PoliticalLayout *layout = instance ? instance->type->layout : NULL;
    if (!layout) return false;
    if (tooltip == &panel_view()->resources.summary.hover || tooltip == &panel_view()->resources.growth_hover)
        return describe_resource_row(&panel_view()->resources.summary, instance, tooltip == &panel_view()->resources.growth_hover, text);
    for (unsigned i = 0; i < layout->resources.rows; ++i) {
        const struct ResourceDisplayRow *row = &panel_view()->resources.rows[i];
        if (tooltip == &row->hover) return describe_resource_row(row, instance, false, text);
    }
    return false;
}

static const TooltipMethods resource_tooltip = {describe_resource, release_button_tooltip};

static bool describe_number(const PoliticalInstance *instance, const NumberDisplay *number, GameString *out) {
    if (!number->control || number->instance != instance->spec) return false;
    GameString details = {.capacity = 15};
    const char *key = "PS_NUMBER_TOOLTIP";
    const KhanDefinition *d = &instance->type->definition;
    const unsigned viewer = reform_actor();
    TextValue value = text_integer(number->value);
    if (number->kind == NUMBER_MEMBERS) {
        static const char *const keys[] = {"PS_NUMBER_MEMBERS_DISABLED", "PS_NUMBER_SUBJECTS", "PS_NUMBER_CAPITALS",
            "PS_NUMBER_EXPLICIT", "PS_NUMBER_QUERY", "PS_NUMBER_MIXED", "PS_NUMBER_CAPITAL_TERRITORY"};
        static const char *const labels[] = {"Members unavailable", "Direct subjects of the leader", "Capitals marked in history",
            "Countries in the member list", "Countries matching the membership conditions", "Listed or eligible countries",
            "Countries whose capitals are in the territory"};
        append_ui_label(&details, keys[d->member_source], labels[d->member_source]);
        if (d->subject_type[0]) {
            plugin.append_string(&details, " - ", 3);
            plugin.append_string(&details, d->subject_type, strlen(d->subject_type));
        }
        if (instance->type->layout && instance->type->layout->members.tooltip[0]) key = instance->type->layout->members.tooltip;
    } else if (number->kind == NUMBER_OPINION) {
        if (!panel_view()->vote.open || panel_view()->vote.displayed_actor != viewer) return false;
        append_ui_label(&details, "PS_NUMBER_OPINION", "Current diplomatic opinion: ");
        append_election_country(&details, viewer);
        plugin.append_string(&details, " > ", 3);
        append_election_country(&details, number->country);
    } else if (number->kind == NUMBER_ELECTORS) {
        if (elective(instance)) append_ui_label(&details, "PS_NUMBER_ELECTORS", "Eligible electors with a voting weight above zero");
        else append_ui_label(&details, "PS_NUMBER_HISTORICAL_ELECTORS", "Members marked as electors in history");
    } else if (number->kind == NUMBER_TERRITORY) {
        append_ui_label(&details, "PS_NUMBER_TERRITORY", "Provinces recorded as part of this instance");
    } else {
        append_ui_label(&details, "PS_NUMBER_AUTHORITY", "Current authority of this political instance");
        value = text_fixed(number->value);
    }
    char value_label[32];
    if (value.kind == TEXT_FIXED) snprintf(value_label, sizeof(value_label), ": %.3f", (double)number->value / PS_RESOURCE_SCALE);
    else snprintf(value_label, sizeof(value_label), ": %d", number->value);
    plugin.append_string(&details, value_label, strlen(value_label));
    const TextField fields[] = {{"value", value}, {"breakdown", text_value(string_text(&details))}};
    bool included;
    const bool formatted = append_political_text(out, key, (PoliticalText){.instance = instance,
        .country = number->country ? number->country : viewer, .fields = fields, .count = sizeof(fields) / sizeof(*fields)}, &included);
    if (!included) {
        if (formatted && out->size) plugin.append_string(out, "\n", 1);
        plugin.append_string(out, string_text(&details), details.size);
    }
    plugin.free_string(&details);
    return true;
}

static bool describe_number_control(const ButtonTooltip *tooltip, const void *button, GameString *text) {
    const PoliticalInstance *instance = selected_instance();
    return instance && plugin.active && plugin.ready && plugin.visible && !active_selector()->open &&
           game_gui_context() && button == tooltip->button && describe_number(instance, (const NumberDisplay *)tooltip, text);
}

static const TooltipMethods number_tooltip = {describe_number_control, release_button_tooltip};

static bool dynasty_in_name(const LeaderReign *reign) {
    const size_t name = strlen(reign->name), dynasty = strlen(reign->dynasty);
    return !dynasty || (name > dynasty && reign->name[name - dynasty - 1] == ' ' &&
                        !strcmp(reign->name + name - dynasty, reign->dynasty));
}

static void append_ruler_identity(GameString *text, const LeaderReign *reign, bool tag) {
    if (tag || !reign->name[0]) {
        char country[4];
        plugin.append_string(text, election_text(reign->country, country), 3);
        if (reign->name[0]) plugin.append_string(text, " - ", 3);
    }
    if (reign->name[0]) plugin.append_string(text, reign->name, strlen(reign->name));
    if (!dynasty_in_name(reign)) {
        plugin.append_string(text, " ", 1);
        plugin.append_string(text, reign->dynasty, strlen(reign->dynasty));
    }
}

static const LeaderReign *current_reign(const PoliticalInstance *instance) {
    const LeaderHistory *history = &instance->leaders;
    const LeaderReign *last = history->count ? &history->items[history->count - 1] : NULL;
    return last && !last->end && last->country == instance->holder ? last : NULL;
}

static void append_leader_history(const PoliticalInstance *instance, GameString *text) {
    const LeaderReign *current = current_reign(instance);
    if (current) {
        append_label(text, "PS_LEADER_HISTORY_CURRENT");
        append_ruler_identity(text, current, false);
    } else if (instance->holder) {
        append_label(text, "PS_LEADER_HISTORY_CURRENT");
        char source[4];
        plugin.append_string(text, election_text(instance->holder, source), 3);
    } else append_label(text, "PS_LEADER_HISTORY_VACANT");
    const LeaderHistory *history = &instance->leaders;
    const unsigned ended = history->count - (history->count && !history->items[history->count - 1].end);
    const unsigned first = ended > PS_PAST_LEADERS ? ended - PS_PAST_LEADERS : 0;
    bool titled = false;
    for (unsigned i = first; i < ended; ++i) {
        const LeaderReign *reign = &history->items[i];
        if (!reign->end) continue;
        if (!titled) {
            plugin.append_string(text, "\n\n", 2);
            append_label(text, "PS_LEADER_HISTORY_PAST");
            titled = true;
        }
        plugin.append_string(text, "\n", 1);
        append_ruler_identity(text, reign, true);
        char date[96];
        snprintf(date, sizeof(date), ", %u.%02u.%02u - %u.%02u.%02u",
                 reign->start / 10000, reign->start / 100 % 100, reign->start % 100,
                 reign->end / 10000, reign->end / 100 % 100, reign->end % 100);
        plugin.append_string(text, date, strlen(date));
    }
}

static bool describe_leader_history(const ButtonTooltip *tooltip, const void *button, GameString *text) {
    if (!plugin.active || !plugin.ready || !plugin.visible || active_selector()->open ||
        !game_gui_context() || button != tooltip->button) return false;
    const PoliticalInstance *instance = selected_instance();
    if (!instance || !instance->type->definition.leader_enabled) return false;
    append_leader_history(instance, text);
    return true;
}

static const TooltipMethods leader_history_tooltip = {describe_leader_history, release_button_tooltip};

static void append_nomination_reason(GameString *text, const char *label, bool localized, int points) {
    char value[28];
    snprintf(value, sizeof(value), "\n%+d  ", points);
    plugin.append_string(text, value, strlen(value));
    if (localized) append_label(text, label);
    else plugin.append_string(text, label, strlen(label));
}

static void append_election_country(GameString *text, unsigned country) {
    char code[4];
    election_text(country, code);
    const uint64_t tag = country_tag(code);
    const GameString *name = tag && plugin.country_name ? plugin.country_name(&tag, false, false, false) : NULL;
    if (name && name->size) plugin.append_string(text, string_text(name), name->size);
    else plugin.append_string(text, code, 3);
}

static bool describe_nomination_data(const PoliticalInstance *instance, const NominationDisplay *display,
                                      GameString *out) {
    if (display->instance != instance->spec || !display->elector ||
        display->viewer != reform_actor()) return false;
    const NominationRules *rules = &instance->type->definition.succession.nomination;
    GameString details = {.capacity = 15}, *text = &details;
    append_election_country(text, display->elector);
    plugin.append_string(text, " > ", 3);
    append_election_country(text, display->viewer);
    char value[40];
    snprintf(value, sizeof(value), "    %+d\n", display->score);
    plugin.append_string(text, value, strlen(value));
    append_ui_label(text, "PS_NOMINATION_BALLOT_WEIGHT", "Vote weight: ");
    snprintf(value, sizeof(value), "%d\n", display->weight);
    plugin.append_string(text, value, strlen(value));
    append_ui_label(text, "PS_NOMINATION_SUPPORT", "Currently supports: ");
    if (display->support) append_election_country(text, display->support);
    else append_ui_label(text, "PS_NOMINATION_NO_SUPPORT", "No eligible candidate");
    if (rules->defined) {
        if (rules->base) append_nomination_reason(text, "PS_NOMINATION_BASE", true, rules->base);
        if (rules->opinion_weight)
            append_nomination_reason(text, "PS_NOMINATION_OPINION", true, display->opinion * rules->opinion_weight);
        for (unsigned i = 0; i < rules->count; ++i) {
            const NominationTerm *term = &rules->terms[i];
            if (display->terms & (1u << i))
                append_nomination_reason(text, term->tooltip[0] ? term->tooltip : term->id, term->tooltip[0] != 0, term->add);
        }
    } else if (display->elector != display->viewer) {
        append_nomination_reason(text, "PS_NOMINATION_OPINION", true, display->opinion);
    }
    const int bonus = election_base_score(&instance->type->definition.election,
                                          display->elector, display->viewer, instance->holder, 0);
    if (bonus) append_nomination_reason(text, "PS_NOMINATION_BONUS", true, bonus);
    if (!display->candidate) {
        plugin.append_string(text, "\n", 1);
        append_label(text, "PS_NOMINATION_INELIGIBLE");
    }
    if (display->standing_count) {
        plugin.append_string(text, "\n\n", 2);
        append_ui_label(text, "PS_NOMINATION_RANKING", "Eligible candidates (highest scores)");
        for (unsigned i = 0; i < display->standing_count; ++i) {
            const NominationStanding *candidate = &display->standings[i];
            plugin.append_string(text, "\n", 1);
            append_election_country(text, candidate->country);
            snprintf(value, sizeof(value), ": %+d", candidate->score);
            plugin.append_string(text, value, strlen(value));
        }
    }
    PoliticalText voter = {0}, support = {0};
    const TextField fields[] = {
        {"value", text_integer(display->score)}, {"points", text_integer(display->score)},
        {"opinion", text_integer(display->opinion)}, {"weight", text_integer(display->weight)},
        {"elector", text_value(text_country(&voter, display->elector))},
        {"support", text_value(display->support ? text_country(&support, display->support) : "")},
        {"breakdown", text_value(string_text(&details))},
    };
    const char *key = rules->tooltip[0] ? rules->tooltip : "PS_NOMINATION_TIP";
    bool used_breakdown;
    if (!append_political_text(out, key, (PoliticalText){.instance = instance, .country = display->viewer,
        .fields = fields, .count = sizeof(fields) / sizeof(*fields)}, &used_breakdown))
        append_label(out, rules->tooltip[0] && !plugin.text_engine.lookup ? key : "PS_NOMINATION_TIP");
    if (!used_breakdown) {
        plugin.append_string(out, "\n", 1);
        plugin.append_string(out, string_text(&details), details.size);
    }
    plugin.free_string(&details);
    return true;
}

static bool describe_nomination(const ButtonTooltip *tooltip, const void *button, GameString *text) {
    if (!plugin.active || !plugin.ready || !plugin.visible || active_selector()->open ||
        !game_gui_context() || button != tooltip->button) return false;
    const PoliticalInstance *instance = selected_instance();
    return instance && describe_nomination_data(instance, (const NominationDisplay *)tooltip, text);
}

static const TooltipMethods nomination_tooltip = {describe_nomination, release_button_tooltip};

static bool describe_routed_nomination(const PoliticalInstance *instance, const Shield *shield, GameString *text) {
    if (nomination_route.shield != shield) return false;
    NominationDisplay *score = nomination_route.score;
    nomination_route.shield = NULL;
    nomination_route.score = NULL;
    if (!describe_nomination_data(instance, score, text)) return false;
    if (!score->tooltip_logged) {
        char elector[4], viewer[4], message[240];
        snprintf(message, sizeof(message), "political nomination_tooltip instance=%s elector=%s viewer=%s score=%d route=native",
                 instance->spec->id, election_text(score->elector, elector), election_text(score->viewer, viewer), score->score);
        log_message(ANVIL_LOG_INFO, message);
        score->tooltip_logged = true;
    }
    return true;
}

static bool point_in_number(UiRectangle area, float x, float y) {
    return x >= area.x && x < area.x + area.width && y >= area.y && y < area.y + area.height;
}

static void route_panel_number(AnvilCpuContext *cpu, const PoliticalInstance *instance,
                               const PoliticalLayout *layout, PoliticalPanel *panel, float x, float y) {
    const void *hit = (const void *)cpu->r14;
    const void *background = (const unsigned char *)panel->window + 0x30;
    const bool passive_hit = !hit || hit == background || hit == panel->backdrop || hit == panel->label_hover.button;
    const ResourceSummaryLayout *resource = &layout->resource_summary;
    const bool monthly = point_in_number(resource->growth_area, x, y);
    if (monthly || point_in_number(resource->value_area, x, y)) {
        const struct ResourceDisplayRow *row = &panel->resources.summary;
        if (row->ownership && (passive_hit || hit == row->value || hit == row->hover.button ||
                              hit == panel->resources.growth || hit == panel->resources.growth_hover.button)) {
            number_route.resource = row;
            number_route.monthly = monthly;
        }
    } else {
        for (unsigned i = 0; i < layout->resources.rows; ++i) {
            const struct ResourceDisplayRow *row = &panel->resources.rows[i];
            const UiRectangle area = row->value ? layout->resources.values[i] : layout->resources.labels[i];
            if (row->ownership && point_in_number(area, x, y) &&
                (passive_hit || hit == row->value || hit == row->label || hit == row->hover.button)) {
                number_route.resource = row;
                break;
            }
        }
    }
    const NumberDisplay *number = point_in_number(layout->member_count_area, x, y) ? &panel->members_count :
        point_in_number(layout->authority_area, x, y) ? &panel->authority_value : NULL;
    if (number && number->instance == instance->spec && (passive_hit || hit == number->control)) number_route.number = number;
    if ((!number_route.resource && !number_route.number) || native_message_visible(&plugin.messages, plugin.gui)) return;
    number_route.carrier = panel->label_hover.button;
    cpu->r14 = (uintptr_t)number_route.carrier;
}

static void route_nomination_tooltip(AnvilCpuContext *cpu, void *user) {
    (void)user;
    nomination_route.shield = NULL;
    nomination_route.score = NULL;
    number_route.carrier = NULL;
    number_route.resource = NULL;
    number_route.number = NULL;
    number_route.monthly = false;
    if (!plugin.active || !plugin.ready || !plugin.visible || plugin.message_priority || active_selector()->open)
        return;
    const PoliticalInstance *instance = selected_instance();
    const PoliticalLayout *layout = instance ? instance->type->layout : NULL;
    if (!instance || !game_gui_context()) return;
    PoliticalPanel *panel = panel_view();
    if (!panel->window) return;

    // Use the native tooltip dispatcher's own viewport and cursor transform.
    const void *context = pointer_at((const void *)cpu->rsi, 0x20);
    if (!context) return;
    const unsigned char *(*get_viewport)(const void *) = method(context, 0x78);
    const unsigned char *viewport = get_viewport ? get_viewport(context) : NULL;
    if (!viewport) return;
    UiPoint cursor, size;
    float scale[2];
    memcpy(&cursor, (const void *)cpu->r12, sizeof(cursor));
    memcpy(&size, viewport + 0x490, sizeof(size));
    memcpy(scale, viewport + 0x4d8, sizeof(scale));
    if (size.x <= 0 || size.y <= 0 || !(scale[0] > 0 && scale[1] > 0)) return;
    float x = cursor.x * scale[0] - size.x / 2;
    float y = cursor.y * scale[1] - size.y / 2;
    const void *hit = (const void *)cpu->r14;
    VoteView *vote = &panel->vote;
    if (vote->open && plugin.ballot_number_areas && point_in_number(plugin.ballot_area, x, y)) {
        for (unsigned i = 0; i < BALLOT_ROWS; ++i) {
            const NumberDisplay *number = &vote->relation[i];
            const Shield *shield = &vote->choices[i];
            if (number->instance != instance->spec || !shield->tooltip.tag ||
                !point_in_number(plugin.ballot_opinions[i], x - plugin.ballot_area.x, y - plugin.ballot_area.y)) continue;
            if (hit && hit != number->control && hit != vote->backdrop && hit != shield->tooltip.button &&
                hit != (const unsigned char *)vote->window + 0x30) return;
            if (native_message_visible(&plugin.messages, plugin.gui)) return;
            number_route.number = number;
            number_route.carrier = shield->tooltip.button;
            cpu->r14 = (uintptr_t)number_route.carrier;
            return;
        }
        return;
    }
    if (!layout || !layout->area.width) return;
    x -= layout->area.x;
    y -= layout->area.y;
    const void *surface = (const unsigned char *)panel->window + 0x30;
    for (unsigned i = 0; i < layout->roles.rows * layout->roles.columns; ++i) {
        NominationDisplay *score = &panel->roles.scores[i];
        const UiRectangle area = layout->roles.nomination_areas[i];
        if (!score->elector || score->instance != instance->spec ||
            !(x >= area.x && x < area.x + area.width && y >= area.y && y < area.y + area.height)) continue;
        const Shield *shield = &panel->roles.shields[i];
        if (!shield->tooltip.button || (shield->tooltip.tag & 0xffffffu) != score->elector ||
            shield->instance != instance->spec) continue;
        if (hit && hit != score->label && hit != score->hover.button && hit != surface &&
            hit != panel->backdrop && hit != panel->label_hover.button && hit != shield->tooltip.button &&
            hit != panel->roles.supports[i].tooltip.button) continue;
        if (native_message_visible(&plugin.messages, plugin.gui)) return;
        // Carry the score through the native provider call on the occupied row's shield.
        nomination_route.shield = shield;
        nomination_route.score = score;
        cpu->r14 = (uintptr_t)shield->tooltip.button;
        return;
    }
    if (panel->label_hover.button) route_panel_number(cpu, instance, layout, panel, x, y);
}

static bool inside_panel_text(const PoliticalLayout *layout, const RECT *client, POINT pointer, UiRectangle area) {
    area.x += layout->area.x;
    area.y += layout->area.y;
    return inside_ui(client, pointer, area);
}

static bool describe_panel_numbers(const PoliticalInstance *instance, const PoliticalLayout *layout,
                                   const RECT *client, POINT pointer, GameString *text) {
    const PoliticalPanel *panel = panel_view();
    for (unsigned i = 0; i < layout->roles.rows * layout->roles.columns; ++i) {
        const NominationDisplay *score = &panel->roles.scores[i];
        if (!score->hover.button && inside_panel_text(layout, client, pointer, layout->roles.nomination_areas[i]) &&
            describe_nomination_data(instance, score, text)) return true;
    }
    const struct ResourceDisplayRow *summary = &panel->resources.summary;
    const ResourceSummaryLayout *controls = &layout->resource_summary;
    const bool balance = !summary->hover.button && inside_panel_text(layout, client, pointer, controls->value_area);
    const bool growth = !(controls->hover[0] ? summary->hover.button : panel->resources.growth_hover.button) &&
        inside_panel_text(layout, client, pointer, controls->growth_area);
    if (balance || growth) return describe_resource_row(summary, instance, growth, text);
    for (unsigned i = 0; i < layout->resources.rows; ++i) {
        const struct ResourceDisplayRow *row = &panel->resources.rows[i];
        if (!row->hover.button && inside_panel_text(layout, client, pointer, layout->resources.values[i]))
            return describe_resource_row(row, instance, false, text);
    }
    return false;
}

static bool describe_layout_label(const ButtonTooltip *tooltip, const void *button, GameString *text) {
    const PoliticalInstance *instance = selected_instance();
    if (!plugin.active || !plugin.ready || !plugin.visible || active_selector()->open || !instance ||
        tooltip != &panel_view()->label_hover || button != tooltip->button || !game_gui_context()) return false;
    const PoliticalLayout *layout = instance->type->layout;
    if (!layout) return false;
    if (number_route.carrier == button) {
        number_route.carrier = NULL;
        if (number_route.resource) return describe_resource_row(number_route.resource, instance, number_route.monthly, text);
        if (number_route.number) return describe_number(instance, number_route.number, text);
    }
    HWND window = wheel_window ? wheel_window : GetActiveWindow();
    RECT client;
    POINT pointer;
    if (!window || !GetCursorPos(&pointer) || !GetClientRect(window, &client) || !ScreenToClient(window, &pointer))
        return false;
    if (layout->area.width && describe_panel_numbers(instance, layout, &client, pointer, text)) return true;
    for (unsigned i = 0; i < layout->hint_count; ++i) {
        const LayoutHint *hint = &layout->hints[i];
        if (!instance->type->definition.leader_enabled &&
            (!strcmp(hint->control, layout->leader_caption) || !strcmp(hint->control, layout->leader_label))) continue;
        if (!instance->type->definition.authority_enabled && !strcmp(hint->control, layout->authority)) continue;
        if (inside_panel_text(layout, &client, pointer, hint->area)) {
            if (!strcmp(hint->control, layout->title)) {
                const char *title = instance_title(instance);
                plugin.append_string(text, title, strlen(title));
                plugin.append_string(text, "\n", 1);
            }
            append_instance_text(text, hint->key, instance, reform_actor());
            return true;
        }
    }
    for (unsigned i = 0; !layout->resources.hover[0] && i < layout->resources.rows; ++i) {
        if (inside_panel_text(layout, &client, pointer, layout->resources.labels[i]))
            return describe_resource_row(&panel_view()->resources.rows[i], instance, false, text);
    }
    return false;
}

static const TooltipMethods label_tooltip = {describe_layout_label, release_button_tooltip};

static const TooltipMethods shield_tooltip_methods = {
    .describe = describe_shield,
    .destroy = release_button_tooltip,
};

static const char *const reform_status_text[] = {
    [REFORM_AVAILABLE] = "",
    [REFORM_PASSED] = "Passed",
    [REFORM_NO_LEADER] = "No leader",
    [REFORM_NOT_LEADER] = "Leader only",
    [REFORM_LOW_AUTHORITY] = "Insufficient Authority",
    [REFORM_DEFINITION_CHANGED] = "Saved reform unavailable",
};

static const char *const reform_status_key[] = {
    [REFORM_AVAILABLE] = "",
    [REFORM_PASSED] = "PS_REFORM_PASSED",
    [REFORM_NO_LEADER] = "PS_REFORM_NO_LEADER",
    [REFORM_NOT_LEADER] = "PS_REFORM_NOT_LEADER",
    [REFORM_LOW_AUTHORITY] = "PS_REFORM_LOW_AUTHORITY",
    [REFORM_DEFINITION_CHANGED] = "PS_REFORM_DEFINITION_CHANGED",
};

static void append_reform_terms(GameString *text, const KhanReform *reform, bool tooltip) {
    char amount[32];
    if (tooltip) append_ui_label(text, "PS_ACTION_COST", "Cost ");
    snprintf(amount, sizeof(amount), "%d ", reform->cost);
    plugin.append_string(text, amount, strlen(amount));
    append_ui_label(text, "GK_AUTHORITY", "Authority");
    if (tooltip) {
        plugin.append_string(text, "\n", 1);
        append_ui_label(text, "PS_LEADER_TITLE", "Leader");
        plugin.append_string(text, " ", 1);
    } else plugin.append_string(text, " / ", 3);
    snprintf(amount, sizeof(amount), "%+d ", reform->prestige);
    plugin.append_string(text, amount, strlen(amount));
    append_ui_label(text, "PS_REFORM_PRESTIGE", "Prestige");
}

static bool describe_reform(const ButtonTooltip *tooltip, const void *button, GameString *text) {
    const PoliticalInstance *instance = selected_instance();
    if (!plugin.active || !plugin.ready || !plugin.visible || !panel_view()->reform_view.bound ||
        !instance || !instance->type->reform.id[0] ||
        (tooltip != &panel_view()->reform_view.tooltip && tooltip != &panel_view()->reform_view.terms_hover) ||
        tooltip->button != button || !game_gui_context()) return false;
    const KhanReform *reform = &instance->type->reform;
    const ReformStatus status = panel_view()->reform_view.status;
    append_reform_terms(text, reform, true);
    if (status != REFORM_AVAILABLE) {
        plugin.append_string(text, "\n", 1);
        append_ui_label(text, reform_status_key[status], reform_status_text[status]);
    }
    return true;
}

static const TooltipMethods reform_tooltip_methods = {
    .describe = describe_reform,
    .destroy = release_button_tooltip,
};

static bool describe_hud(const ButtonTooltip *tooltip, const void *button, GameString *text) {
    if (!plugin.active || !plugin.ready || !tooltip->tag || tooltip->tag > plugin.roster.count ||
        button != tooltip->button || !game_gui_context()) return false;
    const size_t index = (size_t)tooltip->tag - 1;
    HudEntry *entry = &plugin.entries[index];
    const PoliticalInstance *instance = &plugin.instances[index];
    if (!entry->visible || !instance->enabled || tooltip != &entry->tooltip) return false;
    const bool open = plugin.visible && plugin.selected == index;
    GameString label = {0};
    const EntryButton *spec = &instance->type->definition.entry;
    const char *key = spec->defined ? spec->tooltip : (open ? "GK_HUD_TOOLTIP_CLOSE" : "GK_HUD_TOOLTIP_OPEN");
    plugin.localize(&label, key, NULL, 0);
    plugin.append_string(text, string_text(&label), label.size);
    plugin.free_string(&label);
    if (spec->defined) return true;
    plugin.append_string(text, " ", 1);
    const char *title = instance_title(instance);
    plugin.append_string(text, title, strlen(title));
    return true;
}

static const TooltipMethods hud_tooltip_methods = {
    .describe = describe_hud,
    .destroy = release_button_tooltip,
};

static int32_t viewed_province(void) {
    int32_t province = 0;
    if (ANVIL_GAME_WORLD_HAS(plugin.world, province_view) && plugin.world->province_view &&
        plugin.world->province_view(&province) == ANVIL_OK && province > 0) return province;
    return 0;
}

static bool has_tag(const unsigned *tags, unsigned count, unsigned tag);

static uint64_t province_memberships(int32_t province) {
    uint64_t memberships = 0;
    if (province <= 0) return memberships;
    for (size_t i = 0; i < plugin.roster.count; ++i) {
        const PoliticalInstance *instance = &plugin.instances[i];
        if (!instance->enabled) continue;
        if ((uses_capital_members(instance) && is_khanate_province(&instance->khanate_history, instance->history_date, province)) ||
            (instance->type->definition.territory_enabled &&
             has_tag(instance->territory.ids, instance->territory.count, (unsigned)province) &&
             instance_access(i, reform_actor()).view))
            memberships |= UINT64_C(1) << i;
    }
    return memberships;
}

static bool describe_province_badge(const ButtonTooltip *tooltip, const void *button, GameString *text) {
    ProvinceBadge *badge = &plugin.province_badge;
    if (!plugin.active || !plugin.ready || !badge->visible || tooltip != &badge->tooltip ||
        tooltip->button != button || !game_gui_context() ||
        pointer_at(badge->view, 0x958) != badge->selection) return false;

    char key[24];
    snprintf(key, sizeof(key), "PROV%d", badge->province);
    GameString name = {0}, phrase = {0};
    plugin.localize(&name, key, NULL, 0);
    plugin.localize(&phrase, "GK_PROVINCE_PART_OF", NULL, 0);
    bool first = true;
    for (size_t i = 0; i < plugin.roster.count; ++i) {
        if (!(badge->memberships & (UINT64_C(1) << i))) continue;
        if (!first) plugin.append_string(text, "\n", 1);
        first = false;
        plugin.append_string(text, string_text(&name), name.size);
        plugin.append_string(text, " ", 1);
        plugin.append_string(text, string_text(&phrase), phrase.size);
        plugin.append_string(text, " ", 1);
        const char *title = instance_title(&plugin.instances[i]);
        plugin.append_string(text, title, strlen(title));
    }
    plugin.free_string(&name);
    plugin.free_string(&phrase);
    if (!badge->described) {
        char message[110];
        snprintf(message, sizeof(message), "khanate province_badge action=tooltip province=%d", badge->province);
        log_message(ANVIL_LOG_INFO, message);
        badge->described = true;
    }
    return true;
}

static const TooltipMethods province_badge_tooltip_methods = {
    .describe = describe_province_badge,
    .destroy = release_button_tooltip,
};

static void queue_province_panel(void *owner) {
    const ProvinceBadge *badge = owner;
    if (!plugin.active || !plugin.ready || !badge->visible || !game_gui_context() ||
        pointer_at(badge->view, 0x958) != badge->selection) return;
    for (size_t i = 0; i < plugin.roster.count; ++i) {
        if (!(badge->memberships & (UINT64_C(1) << i))) continue;
        if (plugin.visible && i == plugin.selected) return;
        plugin.pending_instance = i;
        plugin.pending_panel = PANEL_REQUEST_PROVINCE;
        char message[GK_SCRIPT_WORD_SIZE + 70];
        snprintf(message, sizeof(message), "khanate province_badge action=click province=%d instance=%s",
                 badge->province, plugin.instances[i].spec->id);
        log_message(ANVIL_LOG_INFO, message);
        return;
    }
}

static void bind_province_badge(ProvinceBadge *badge) {
    void *button = badge->tooltip.button;
    badge->tooltip.methods = &province_badge_tooltip_methods;
    void (*set_tooltip)(void *, const ButtonTooltip *) = method(button, 0x220);
    set_tooltip(button, &badge->tooltip);
    connect_click(button, &badge->listener, badge, queue_province_panel);
}

static void update_province_badge(int32_t province) {
    ProvinceBadge *badge = &plugin.province_badge;
    if (!badge->tooltip.button) return;
    const uint64_t memberships = province_memberships(province);
    if (province == badge->province && memberships == badge->memberships) return;
    const int32_t previous = badge->province;
    badge->province = province;
    badge->memberships = memberships;
    badge->selection = province ? pointer_at(badge->view, 0x958) : NULL;
    badge->described = false;

    if (!badge->memberships && !badge->visible) return;
    const bool show = badge->memberships != 0;
    if (show && !badge->tooltip.methods) bind_province_badge(badge);
    if (show != badge->visible) control_visible(badge->tooltip.button, show);
    badge->visible = show;
    char message[145];
    snprintf(message, sizeof(message), "khanate province_badge action=%s province=%d previous=%d membership_mask=%llx",
             show ? "show" : "hide", province, previous, (unsigned long long)badge->memberships);
    log_message(ANVIL_LOG_INFO, message);
}

static void hud_visible(HudEntry *entry, bool visible) {
    native_window_visible(entry->window, visible);
    entry->visible = visible;
    if (visible) {
        for (size_t i = 0; i < sizeof(entry->buttons) / sizeof(*entry->buttons); ++i)
            if (entry->buttons[i]) control_visible(entry->buttons[i], entry->buttons[i] == entry->tooltip.button);
    }
}

static void show_hud(size_t index, unsigned actor) {
    static const char *const icons[HUD_ICONS] = {"cavalry", "infantry", "tribute", "khanate"};
    const PoliticalInstance *instance = &plugin.instances[index];
    const EntryButton *spec = &instance->type->definition.entry;
    HudEntry *entry = &plugin.entries[index];
    if (entry->bind_failed || !plugin.button_class || !plugin.button_signal_class) return;
    bool clickable;
    const bool visible = entry_access(index, actor, &clickable);
    if (!entry->evaluated || visible != entry->visible || clickable != entry->clickable) {
        if (spec->defined) {
            char message[260], tag[4] = {0};
            memcpy(tag, &actor, 3);
            snprintf(message, sizeof(message), "political entry instance=%s viewer=%s visible=%u enabled=%u",
                     instance->spec->id, tag, (unsigned)visible, (unsigned)clickable);
            log_message(ANVIL_LOG_INFO, message);
        }
        entry->evaluated = true;
    }
    if (!visible) {
        if (entry->visible) hud_visible(entry, false);
        entry->clickable = false;
        return;
    }
    if (!entry->window) {
        char region[GK_SCRIPT_WORD_SIZE + 8];
        snprintf(region, sizeof(region), "%s_%02u", spec->defined ? spec->region : "gk_hud_slot", instance->hud_slot);
        GameString name = { .capacity = 15 };
        plugin.assign_string(&name, region, strlen(region));
        entry->window = plugin.create_window(plugin.gui, &name);
        plugin.free_string(&name);
        size_t chosen = 0;
        if (entry->window && spec->defined) {
            entry->buttons[0] = find_standard_button(entry->window, spec->control);
        } else if (entry->window) {
            for (size_t i = 0; i < sizeof(icons) / sizeof(*icons); ++i) {
                char key[32];
                snprintf(key, sizeof(key), "gk_icon_%s", icons[i]);
                entry->buttons[i] = find_standard_button(entry->window, key);
                if (!strcmp(instance->spec->icon, icons[i])) chosen = i;
            }
        }
        void *button = entry->buttons[chosen];
        if (!button) {
            if (entry->window) hud_visible(entry, false);
            entry->bind_failed = true;
            char message[520];
            snprintf(message, sizeof(message), "political HUD cannot bind instance=%s region=%s control=%s",
                     instance->spec->id, region, spec->defined ? spec->control : icons[chosen]);
            log_message(ANVIL_LOG_ERROR, message);
            return;
        }
        entry->tooltip = (ButtonTooltip){ .methods = &hud_tooltip_methods, .button = button, .tag = index + 1 };
        void (*set_tooltip)(void *, const ButtonTooltip *) = method(button, 0x220);
        set_tooltip(button, &entry->tooltip);
        connect_click(button, &entry->listener, entry, queue_panel_toggle);
        char message[260];
        snprintf(message, sizeof(message), "political hud=bound instance=%.124s control=%.90s slot=%u",
                 instance->spec->id, spec->defined ? spec->control : icons[chosen], instance->hud_slot);
        log_message(ANVIL_LOG_INFO, message);
        entry->clickable = true; /* connect_click enables the control. */
    }
    if (!entry->visible || entry->clickable != clickable) button_enabled(entry->tooltip.button, clickable);
    entry->clickable = clickable;
    if (!entry->visible) hud_visible(entry, true);
}

static InstanceChoices type_choices(const PoliticalType *type, unsigned actor) {
    InstanceChoices choices = {0};
    for (size_t i = 0; i < plugin.roster.count; ++i)
        if (plugin.instances[i].type == type) add_instance_choice(&choices, i, instance_access(i, actor));
    return choices;
}

static const char *disabled_hint(const EntryButton *entry) {
    return entry->disabled_tooltip[0] ? entry->disabled_tooltip : "PS_TOOLTIP_ENTRY_DISABLED";
}

static bool describe_type_entry(const ButtonTooltip *tooltip, const void *button, GameString *text) {
    const ContentHudSlot *slot = (const void *)((const char *)tooltip - offsetof(ContentHudSlot, tooltip));
    if (!slot->shown || tooltip->button != button || !plugin.active || !plugin.ready ||
        slot->type_index < 0 || (size_t)slot->type_index >= plugin.type_count || !game_gui_context()) return false;
    const PoliticalType *type = &plugin.types[slot->type_index];
    const EntryButton *entry = &type->definition.entry;
    if (!slot->clickable) append_label(text, disabled_hint(entry));
    else if (entry->tooltip[0]) append_label(text, entry->tooltip);
    else {
        append_label(text, "GK_HUD_TOOLTIP_OPEN");
        plugin.append_string(text, " ", 1);
        plugin.append_string(text, type->definition.title, strlen(type->definition.title));
    }
    return true;
}

static const TooltipMethods type_hud_tooltip = {describe_type_entry, release_button_tooltip};

static void queue_type_panel(void *owner) {
    const TypeHudButton *origin = owner;
    const ContentHudSlot *slot = origin->slot;
    if (slot->shown && slot->clickable && slot->type_index >= 0 && origin->control == slot->tooltip.button &&
        plugin.active && plugin.ready && game_gui_context()) {
        plugin.pending_instance = (size_t)slot->type_index;
        plugin.pending_panel = PANEL_REQUEST_TYPE_ICON;
    }
}

static void attach_tooltip(void *button, ButtonTooltip *tooltip, const TooltipMethods *operations) {
    *tooltip = (ButtonTooltip){ .methods = operations, .button = button };
    void (*set)(void *, const ButtonTooltip *) = method(button, 0x220);
    set(button, tooltip);
}

static void paint_type_slot(const HudRegion *region, ContentHudSlot *slot, unsigned position,
                            int type_index, EntryRoute route) {
    if (route == ENTRY_HIDDEN || slot->failed) {
        if (slot->shown) native_window_visible(slot->window, false);
        slot->shown = false;
        return;
    }
    const PoliticalType *type = &plugin.types[type_index];
    if (!slot->window) {
        char name[GK_SCRIPT_WORD_SIZE + 12];
        snprintf(name, sizeof(name), "%s_%02u", region->container, position);
        slot->window = create_named_window(name);
        if (slot->window) native_window_visible(slot->window, false);
        for (unsigned i = 0; slot->window && i < region->icon_count; ++i) {
            slot->buttons[i].control = find_standard_button(slot->window, region->icons[i].control);
            slot->buttons[i].slot = slot;
        }
        for (unsigned i = 0; i < region->icon_count; ++i) {
            if (slot->buttons[i].control) continue;
            slot->failed = true;
            char message[580];
            snprintf(message, sizeof(message), "political HUD native bind failed file=%s window=%s control=%s",
                     region->gui_file, name, region->icons[i].control);
            log_message(ANVIL_LOG_ERROR, message);
            return;
        }
    }
    const bool clickable = route != ENTRY_DISABLED;
    TypeHudButton *binding = &slot->buttons[type->sprite];
    void *button = binding->control;
    if (!slot->shown || type_index != slot->type_index || clickable != slot->clickable) {
        if (slot->tooltip.button != button) {
            if (slot->tooltip.button) {
                void (*clear_tooltip)(void *, const ButtonTooltip *) = method(slot->tooltip.button, 0x220);
                clear_tooltip(slot->tooltip.button, NULL);
            }
            attach_tooltip(button, &slot->tooltip, &type_hud_tooltip);
            connect_click(button, &binding->listener, binding, queue_type_panel);
        }
        slot->type_index = type_index;
        slot->clickable = clickable;
        slot->shown = true;
        native_window_visible(slot->window, true);
        for (unsigned i = 0; i < region->icon_count; ++i)
            control_visible(slot->buttons[i].control, slot->buttons[i].control == button);
        button_enabled(button, clickable);
    }
}

static bool describe_hud_page(const ButtonTooltip *tooltip, const void *button, GameString *text) {
    const ContentHud *hud = (const void *)((const char *)tooltip - offsetof(ContentHud, tooltip));
    if (!hud->shown || !plugin.active || !plugin.ready || tooltip->button != button) return false;
    append_label(text, "PS_HUD_NEXT_PAGE");
    char detail[36];
    snprintf(detail, sizeof(detail), " (%u/%u)", hud->page + 1, hud->pages);
    plugin.append_string(text, detail, strlen(detail));
    return true;
}

static void next_hud_page(void *owner) {
    ContentHud *hud = owner;
    if (plugin.active && plugin.ready && hud->shown && hud->pages > 1 && game_gui_context()) {
        hud->page = (hud->page + 1) % hud->pages;
        plugin.next_entry_check = 0;
    }
}

static const TooltipMethods page_tooltip = {describe_hud_page, release_button_tooltip};

static void show_pager(const HudRegion *region, ContentHud *hud) {
    const bool show = hud->pages > 1;
    if (!hud->pager && show && !hud->failed) {
        hud->pager = create_named_window(region->page_window);
        void *button = hud->pager ? find_standard_button(hud->pager, "ps_page_next") : NULL;
        if (!button) {
            if (hud->pager) native_window_visible(hud->pager, false);
            hud->failed = true;
            log_message(ANVIL_LOG_ERROR, "political HUD cannot bind the configured page control");
        } else {
            attach_tooltip(button, &hud->tooltip, &page_tooltip);
            connect_click(button, &hud->listener, hud, next_hud_page);
        }
    }
    if (hud->pager && !hud->failed && show != hud->shown) native_window_visible(hud->pager, show);
    hud->shown = show && !hud->failed;
}

static int type_order(const void *a, const void *b) {
    const PoliticalType *left = &plugin.types[*(const unsigned *)a], *right = &plugin.types[*(const unsigned *)b];
    const int diff = left->definition.entry.order - right->definition.entry.order;
    return diff ? diff : strcmp(left->id, right->id);
}

static void update_type_huds(unsigned actor) {
    EntryRoute routes[PS_MAX_INSTANCES] = {0};
    for (size_t i = 0; i < plugin.type_count; ++i) {
        PoliticalType *type = &plugin.types[i];
        if (!type->usable || type->hud < 0) continue;
        const InstanceChoices choices = type_choices(type, actor);
        routes[i] = entry_route(&choices);
        if (actor && (!type->entry_logged || routes[i] != type->last_route)) {
            char message[350], tag[4];
            snprintf(message, sizeof(message), "political type_entry type=%s viewer=%s visible_instances=%u enabled_instances=%u",
                     type->id, election_text(actor, tag), choices.count, choices.enabled_count);
            log_message(ANVIL_LOG_INFO, message);
            type->last_route = routes[i];
            type->entry_logged = true;
        }
    }
    for (size_t i = 0; i < plugin.layouts.hud_count; ++i) {
        const HudRegion *region = &plugin.layouts.hud[i];
        if (!region->valid) continue;
        ContentHud *hud = &plugin.content_huds[i];
        unsigned order[PS_MAX_INSTANCES], count = 0;
        for (size_t t = 0; t < plugin.type_count; ++t)
            if (routes[t] != ENTRY_HIDDEN && plugin.types[t].hud == (int)i) order[count++] = (unsigned)t;
        qsort(order, count, sizeof(*order), type_order);
        hud->pages = hud_page_count(count, region->capacity);
        if (hud->page >= hud->pages) hud->page = 0;
        for (unsigned p = 0; p < region->capacity; ++p) {
            const unsigned offset = hud->page * region->capacity + p;
            const int type = offset < count ? (int)order[offset] : -1;
            paint_type_slot(region, &hud->slots[p], p, type, type >= 0 ? routes[type] : ENTRY_HIDDEN);
        }
        show_pager(region, hud);
    }
}

static uint64_t live_holder_tag(unsigned holder) {
    char tag[4] = {0};
    memcpy(tag, &holder, 3);
    return country_tag(tag);
}

static bool has_tag(const unsigned *tags, unsigned count, unsigned tag) {
    for (unsigned i = 0; i < count; ++i)
        if (tags[i] == tag) return true;
    return false;
}

static bool remove_tag(unsigned *tags, unsigned *count, unsigned tag) {
    for (unsigned i = 0; i < *count; ++i) {
        if (tags[i] != tag) continue;
        --*count;
        memmove(&tags[i], &tags[i + 1], (*count - i) * sizeof(tags[0]));
        return true;
    }
    return false;
}

static const char *country_rule_matches(const PoliticalInstance *instance, const EntryCondition *rule,
                                       unsigned country, bool *result) {
    EntryContext context = country_context(instance->spec->id, country, NULL);
    if (live_holder_tag(instance->holder)) context.from = instance->holder;
    return entry_evaluate(rule, &context, result);
}

static const char *test_native_condition(const NativeClause *clause, const NativeScope *scope, bool *allowed) {
    return test_native_eu4(clause, scope, live_holder_tag(scope->root), live_holder_tag(scope->from), allowed);
}

static bool native_country_allows(const PoliticalInstance *instance, const NativeClause *clause,
                                  unsigned root, unsigned from) {
    if (!native_clause_present(clause)) return true;
    const NativeScope scope = {.root = root, .from = from, .instance = instance->spec->id};
    bool allowed = false;
    return !test_native_condition(clause, &scope, &allowed) && allowed;
}

static void run_native_country(const PoliticalInstance *instance, const NativeClause *clause,
                               unsigned root, unsigned from) {
    if (!clause->program) return;
    const NativeScope scope = {.root = root, .from = from, .instance = instance->spec->id};
    execute_native_eu4(clause, &scope, live_holder_tag(root), live_holder_tag(from));
}

static bool records_members(MemberSource source) {
    return source == MEMBERS_EXPLICIT || source == MEMBERS_MIXED;
}

static bool derived_members(MemberSource source) {
    return source == MEMBERS_QUERY || source == MEMBERS_MIXED;
}

static const char *configured_member(const PoliticalInstance *instance, unsigned country, bool *result) {
    const KhanDefinition *definition = &instance->type->definition;
    *result = false;
    if (!live_holder_tag(country) || has_tag(instance->excluded.tags, instance->excluded.count, country)) return NULL;
    bool included = records_members(definition->member_source) && has_tag(instance->members.tags, instance->members.count, country);
    if (definition->member_source == MEMBERS_CAPITAL_TERRITORY) {
        int32_t capital;
        if (!current_capital(country, &capital)) return "country capital unavailable";
        included = capital > 0 && has_tag(instance->territory.ids, instance->territory.count, (unsigned)capital);
    }
    if (!included && derived_members(definition->member_source)) {
        const char *reason = country_rule_matches(instance, &definition->member_query, country, &included);
        if (reason) return reason;
    }
    if (included) return country_rule_matches(instance, &definition->member_retain, country, result);
    return NULL;
}

static int compare_member_tags(const void *a, const void *b) {
    const AnvilGameTag *left = a, *right = b;
    return strcmp(left->text, right->text);
}

static bool instance_members(const PoliticalInstance *instance, const AnvilGameTag *holder, Shogunate *out) {
    const KhanDefinition *definition = &instance->type->definition;
    if (uses_capital_members(instance))
        return read_capital_members(plugin.world, &instance->khanate_history, instance->history_date, holder, out);
    if (definition->member_source == MEMBERS_DIRECT_SUBJECTS) {
        if (!read_subject_members(plugin.world, holder, definition->subject_type, out)) return false;
        if (!definition->member_retain.count) return true;
        uint32_t kept = 0;
        for (uint32_t i = 0; i < out->count; ++i) {
            bool allowed;
            if (country_rule_matches(instance, &definition->member_retain, election_tag(out->daimyo[i].text), &allowed)) {
                release_shogunate(out);
                return false;
            }
            if (allowed) out->daimyo[kept++] = out->daimyo[i];
        }
        out->count = kept;
        return true;
    }
    out->emperor = *holder;
    int32_t limit = 0;
    const bool marked_capitals = definition->member_source == MEMBERS_CAPITAL_TERRITORY;
    if (derived_members(definition->member_source) || marked_capitals) {
        if (!plugin.world->province_limit || !plugin.world->province || (limit = plugin.world->province_limit()) <= 0) return false;
    }
    const uint32_t province_slots = marked_capitals ? instance->territory.count : (limit ? (uint32_t)limit - 1 : 0);
    const uint32_t explicit_count = records_members(definition->member_source) ? instance->members.count : 0;
    const size_t capacity = explicit_count + province_slots;
    if (!capacity) return true;
    out->daimyo = calloc(capacity, sizeof(*out->daimyo));
    if (!out->daimyo) return false;
    uint32_t candidates = 0;
    for (unsigned i = 0; i < explicit_count; ++i)
        election_text(instance->members.tags[i], out->daimyo[candidates++].text);
    for (uint32_t i = 0; i < province_slots; ++i) {
        const unsigned id = marked_capitals ? instance->territory.ids[i] : i + 1;
        if (id >= (unsigned)limit) continue;
        AnvilGameProvince province = { .size = sizeof(province) };
        if (plugin.world->province((int32_t)id, &province) == ANVIL_OK && province.owner.text[0])
            out->daimyo[candidates++] = province.owner;
    }
    if (limit) qsort(out->daimyo, candidates, sizeof(*out->daimyo), compare_member_tags);
    unsigned previous = 0;
    for (uint32_t i = 0; i < candidates; ++i) {
        const unsigned country = election_tag(out->daimyo[i].text);
        if (!country || country == previous) continue;
        previous = country;
        bool member;
        if (configured_member(instance, country, &member)) { release_shogunate(out); return false; }
        if (member) out->daimyo[out->count++] = out->daimyo[i];
    }
    return true;
}

static const char *instance_has_member(const PoliticalInstance *instance, unsigned country, bool *member) {
    if (!uses_capital_members(instance) && instance->type->definition.member_source != MEMBERS_DIRECT_SUBJECTS)
        return configured_member(instance, country, member);
    AnvilGameTag holder = {0};
    election_text(instance->holder, holder.text);
    Shogunate list = {0};
    if (!instance_members(instance, &holder, &list)) return "member list unavailable";
    *member = false;
    for (uint32_t i = 0; i < list.count && !*member; ++i) *member = country == election_tag(list.daimyo[i].text);
    release_shogunate(&list);
    return NULL;
}

static unsigned role_index(const PoliticalInstance *instance, const char *id) {
    const KhanDefinition *definition = &instance->type->definition;
    unsigned i = 0;
    while (i < definition->role_count && strcmp(definition->roles[i].id, id)) ++i;
    return i;
}

static bool holds_role(const PoliticalInstance *instance, unsigned role, unsigned country) {
    const RoleHolders *holders = &instance->roles[role];
    return has_tag(holders->countries, holders->count, country) && live_holder_tag(country);
}

static const char *role_eligible(const PoliticalInstance *instance, const PoliticalRole *role,
                                unsigned country, bool appointing, bool *allowed) {
    *allowed = false;
    if (!live_holder_tag(country) || (!role->can_hold_with_leader && instance->holder == country)) return NULL;
    if (role->require_member) {
        bool member;
        const char *error = instance_has_member(instance, country, &member);
        if (error || !member) return error;
    }
    if (appointing) {
        bool candidate;
        const char *error = country_rule_matches(instance, &role->candidate, country, &candidate);
        if (error || !candidate) return error;
    }
    return country_rule_matches(instance, &role->retain, country, allowed);
}

static const char *leader_eligible(const PoliticalInstance *instance, unsigned country, bool candidate, bool *allowed) {
    const LeaderSelection *rule = &instance->type->definition.succession;
    *allowed = false;
    if (!live_holder_tag(country)) return NULL;
    if (rule->require_member) {
        bool member;
        const char *reason = instance_has_member(instance, country, &member);
        if (reason || !member) return reason;
    }
    if (candidate) {
        bool ok;
        const char *reason = country_rule_matches(instance, &rule->candidate, country, &ok);
        if (reason || !ok) return reason;
    }
    return country_rule_matches(instance, &rule->retain, country, allowed);
}

static bool has_political_permission(const PoliticalInstance *instance, unsigned actor, const char *permission) {
    const KhanDefinition *d = &instance->type->definition;
    if (!live_holder_tag(actor)) return false;
    if (actor == instance->holder && grants_permission(&d->succession.permissions, permission)) return true;
    for (unsigned i = 0; i < d->role_count; ++i)
        if (grants_permission(&d->roles[i].permissions, permission) &&
            has_tag(instance->roles[i].countries, instance->roles[i].count, actor)) return true;
    return false;
}

static bool prune_roles(PoliticalInstance *instance) {
    bool changed = false;
    const KhanDefinition *definition = &instance->type->definition;
    for (unsigned i = 0; i < definition->role_count; ++i) {
        RoleHolders *holders = &instance->roles[i];
        unsigned kept = 0;
        for (unsigned j = 0; j < holders->count; ++j) {
            bool allowed;
            const char *error = role_eligible(instance, &definition->roles[i], holders->countries[j], false, &allowed);
            if ((error || allowed) && kept < definition->roles[i].max_holders) holders->countries[kept++] = holders->countries[j];
        }
        changed |= kept != holders->count;
        holders->count = kept;
    }
    return changed;
}

static bool elective(const PoliticalInstance *instance) {
    const ElectionRules *rules = &instance->type->definition.election;
    return rules->defined && !rules->hereditary;
}

static bool ruler_id(unsigned holder, uint64_t *id) {
    *id = 0;
    if (!holder) return true;
    if (!plugin.world || !plugin.world->monarch) return false;
    char tag[4];
    AnvilGameMonarch ruler = { .size = sizeof(ruler) };
    AnvilResult result = plugin.world->monarch(election_text(holder, tag), ANVIL_GAME_MONARCH_RULER, &ruler);
    if (result == ANVIL_OK) *id = ruler.id;
    return result == ANVIL_OK || result == ANVIL_ERROR_NOT_FOUND;
}

static bool sync_leader_reign(PoliticalInstance *instance, unsigned today) {
    if (!instance->type->definition.leader_enabled) return false;
    LeaderHistory *history = &instance->leaders;
    LeaderReign *last = history->count ? &history->items[history->count - 1] : NULL;
    LeaderReign current = {.country = instance->holder, .start = today};
    if (!ruler_id(current.country, &current.ruler)) return false;
    const bool continuing = last && !last->end && last->country == current.country;
    if (continuing && last->ruler && last->ruler == current.ruler) return false;
    if (current.country) {
        char tag[4];
        uint32_t needed = 0;
        election_text(current.country, tag);
        if (plugin.world->monarch_name &&
            plugin.world->monarch_name(tag, ANVIL_GAME_MONARCH_RULER,
                current.name, sizeof(current.name), &needed) != ANVIL_OK) current.name[0] = 0;
        if (plugin.world->monarch_dynasty &&
            plugin.world->monarch_dynasty(tag, ANVIL_GAME_MONARCH_RULER,
                current.dynasty, sizeof(current.dynasty), &needed) != ANVIL_OK) current.dynasty[0] = 0;
    }
    if (continuing && !last->ruler) {
        current.start = last->start;
        if (!current.name[0]) strcpy(current.name, last->name);
        if (!current.dynasty[0]) strcpy(current.dynasty, last->dynasty);
        const bool changed = memcmp(last, &current, sizeof(current)) != 0;
        *last = current;
        return changed;
    }
    const bool closing = last && !last->end;
    if (closing) {
        if (last->start > today) return false;
        if (last->start == today) --history->count;
        else last->end = today;
    } else if (!current.country) return false;
    if (current.country && current.ruler) append_leader_reign(history, current);
    return closing || (current.country && current.ruler);
}

static void seed_leader_reigns(PoliticalInstance *instance, unsigned today) {
    if (!instance->type->definition.leader_enabled || instance->leaders.count) return;
    const PoliticalInstanceSpec *spec = instance->spec;
    const unsigned opening = instance->history_date ? instance->history_date : today;
    unsigned holder = election_tag(spec->leader);
    unsigned start = spec->date;
    for (size_t i = 0; start && i < spec->change_count && spec->changes[i].date <= opening; ++i) {
        const PoliticalHistoryChange *change = &spec->changes[i];
        if (change->kind != HISTORY_LEADER) continue;
        if (holder && start < change->date) {
            LeaderReign reign = {.country = holder, .start = start, .end = change->date};
            append_leader_reign(&instance->leaders, reign);
        }
        start = change->date;
        holder = change->key;
    }
    if (holder && start && holder == instance->holder) {
        LeaderReign reign = {.country = holder, .start = start};
        append_leader_reign(&instance->leaders, reign);
    }
    for (unsigned i = 0; i < instance->leaders.count; ++i)
        if (instance->leaders.items[i].end) historical_ruler(&instance->leaders.items[i]);
    sync_leader_reign(instance, today);
}

static int electoral_opinion(unsigned voter, unsigned candidate) {
    if (!plugin.world->diplomacy_status) return 0;
    char from[4], to[4];
    AnvilGameDiplomacyStatus relation = { .size = sizeof(relation) };
    return plugin.world->diplomacy_status(election_text(voter, from), election_text(candidate, to), &relation) == ANVIL_OK
        ? relation.opinion : 0;
}

static void release_roll(ElectionSnapshot *roll) {
    free(roll->members); free(roll->rulers); free(roll->opinions); free(roll->nomination);
    *roll = (ElectionSnapshot){0};
}

static int score_nomination(const PoliticalInstance *instance, unsigned elector, unsigned candidate, int opinion, unsigned mask) {
    const KhanDefinition *d = &instance->type->definition;
    if (!d->succession.nomination.defined)
        return election_base_score(&d->election, elector, candidate, instance->holder, opinion);
    const int bonus = election_base_score(&d->election, elector, candidate, instance->holder, 0);
    return nomination_score(&d->succession.nomination, opinion, bonus, mask);
}

static bool read_nomination(const PoliticalInstance *instance, unsigned elector, unsigned viewer,
                            NominationDisplay *display) {
    const NominationRules *rules = &instance->type->definition.succession.nomination;
    unsigned voter_mask, viewer_mask;
    EntryContext voter = country_context(instance->spec->id, elector, NULL);
    EntryContext candidate = country_context(instance->spec->id, viewer, NULL);
    voter.from = viewer;
    candidate.from = elector;
    if (nomination_terms(rules, &voter, true, &voter_mask) ||
        nomination_terms(rules, &candidate, false, &viewer_mask)) return false;
    display->elector = elector;
    display->viewer = viewer;
    display->terms = voter_mask & viewer_mask;
    display->opinion = elector == viewer ? 0 : electoral_opinion(elector, viewer);
    display->score = score_nomination(instance, elector, viewer, display->opinion, display->terms);
    return true;
}

static void render_nomination(const PoliticalInstance *instance, const ElectionTable *table,
                              const ElectionCountry *voter, unsigned viewer, bool candidate, NominationDisplay *display) {
    if (!display->label) return;
    const unsigned old_elector = display->elector, old_viewer = display->viewer;
    const int old_score = display->score;
    const unsigned elector = voter && voter->weight ? voter->tag : 0;
    if (elector != old_elector || viewer != old_viewer) display->tooltip_logged = false;
    const bool visible = elector && viewer && read_nomination(instance, elector, viewer, display);
    if (visible) {
        display->candidate = candidate;
        display->instance = instance->spec;
        display->weight = voter->weight;
        display->support = voter->support;
        display->standing_count = election_rank_candidates(&instance->type->definition.election, table, voter,
            instance->holder, instance->roll.opinions, instance->roll.nomination, display->standings,
            sizeof(display->standings) / sizeof(*display->standings));
        char value[35];
        if (display->score)
            snprintf(value, sizeof(value), "\xA7%s%+d\xA7!", display->score < 0 ? "R" : "G", display->score);
        else strcpy(value, "0");
        native_text(display->label, value);
        if (elector != old_elector || viewer != old_viewer || display->score != old_score) {
            char from[4], to[4], line[260];
            snprintf(line, sizeof(line), "political nomination instance=%s elector=%s viewer=%s score=%d opinion=%d terms=0x%x",
                     instance->spec->id, election_text(elector, from), election_text(viewer, to),
                     display->score, display->opinion, display->terms);
            log_message(ANVIL_LOG_INFO, line);
        }
    } else {
        display->elector = display->viewer = 0;
        display->instance = NULL;
        display->support = display->standing_count = 0;
        display->weight = 0;
        native_text(display->label, "");
    }
    if (display->hover.button) control_visible(display->hover.button, visible);
}

static bool sample_nomination(const PoliticalInstance *instance, ElectionSnapshot *sample) {
    const NominationRules *rules = &instance->type->definition.succession.nomination;
    const size_t n = sample->count;
    if (!rules->defined || !n) return true;
    sample->nomination = malloc(n * n * sizeof(*sample->nomination));
    for (unsigned k = 0; k < rules->node_count; ++k) {
        if (rules->nodes[k].test != ENTRY_NATIVE) continue;
        if (!sample->nomination) return false;
        for (size_t i = 0; i < n; ++i)
            for (size_t j = 0; j < n; ++j) {
                NominationDisplay result = {0};
                if (!read_nomination(instance, sample->members[i].tag, sample->members[j].tag, &result))
                    return false;
                sample->nomination[i*n+j] = (int16_t)result.score;
            }
        return true;
    }
    unsigned *candidate_masks = calloc(n, sizeof(*candidate_masks));
    const char *problem = sample->nomination && candidate_masks ? NULL : "nomination allocation failed";
    for (size_t i = 0; !problem && i < n; ++i) {
        const EntryContext candidate = country_context(instance->spec->id, sample->members[i].tag, NULL);
        problem = nomination_terms(rules, &candidate, false, &candidate_masks[i]);
    }
    for (size_t i = 0; !problem && i < n; ++i) {
        const unsigned elector_tag = sample->members[i].tag;
        const EntryContext elector = country_context(instance->spec->id, elector_tag, NULL);
        unsigned mask;
        problem = nomination_terms(rules, &elector, true, &mask);
        if (problem) break;
        for (size_t j = 0; j < n; ++j) {
            const size_t pair = i*n+j;
            sample->nomination[pair] = (int16_t)score_nomination(instance, elector_tag, sample->members[j].tag,
                sample->opinions[pair], mask & candidate_masks[j]);
        }
    }
    free(candidate_masks);
    if (problem) log_message(ANVIL_LOG_ERROR, problem);
    return !problem;
}

static bool sample_electoral_roll(PoliticalInstance *instance, unsigned today) {
    AnvilGameTag holder = {0};
    election_text(instance->holder, holder.text);
    const KhanDefinition *def = &instance->type->definition;
    Shogunate members = {0};
    const bool ok = def->succession.defined ? instance_members(instance, &holder, &members) : uses_capital_members(instance)
        ? read_capital_members(plugin.world, &instance->khanate_history, instance->history_date, &holder, &members)
        : read_subject_members(plugin.world, &holder, def->subject_type, &members);
    if (!ok) return false;
    const size_t n = members.count;
    ElectionSnapshot sample = { .count = n, .holder = instance->holder, .date = today };
    if (n && n > SIZE_MAX / sizeof(int16_t) / n) { release_shogunate(&members); return false; }
    sample.members = calloc(n, sizeof(*sample.members));
    sample.rulers = calloc(n, sizeof(*sample.rulers));
    sample.opinions = calloc(n*n, sizeof(*sample.opinions));
    if (n && (!sample.members || !sample.rulers || !sample.opinions)) {
        release_roll(&sample); release_shogunate(&members); return false;
    }
    for (uint32_t i = 0; i < members.count; ++i) {
        const char *tag = members.daimyo[i].text;
        sample.members[i].tag = election_tag(tag);
        if (def->succession.defined) {
            const unsigned role = role_index(instance, def->succession.elector_role);
            if (role == def->role_count) { release_roll(&sample); release_shogunate(&members); return false; }
            sample.members[i].tumen = has_tag(instance->roles[role].countries, instance->roles[role].count, sample.members[i].tag);
            bool eligible;
            if (leader_eligible(instance, sample.members[i].tag, true, &eligible)) {
                release_roll(&sample); release_shogunate(&members); return false;
            }
            sample.members[i].ineligible = !eligible;
        } else sample.members[i].tumen = has_khan_tumen(&instance->khanate_history, instance->history_date, tag);
        if (!ruler_id(sample.members[i].tag, &sample.rulers[i])) sample.rulers[i] = UINT64_MAX;
        for (size_t j = 0; j < n; ++j)
            if (j != i) sample.opinions[i*n+j] = (int16_t)electoral_opinion(sample.members[i].tag, election_tag(members.daimyo[j].text));
    }
    release_shogunate(&members);
    if (!sample_nomination(instance, &sample)) {
        release_roll(&sample);
        return false;
    }
    if (!ruler_id(instance->holder, &sample.holder_ruler)) sample.holder_ruler = UINT64_MAX;
    release_roll(&instance->roll);
    instance->roll = sample;
    return true;
}

static bool electoral_table(const PoliticalInstance *instance, ElectionTable *table) {
    const ElectionSnapshot *roll = &instance->roll;
    if (!roll->date) return false;
    table->count = roll->count;
    table->rows = roll->count ? malloc(roll->count*sizeof(*table->rows)) : NULL;
    if (roll->count && !table->rows) return false;
    if (roll->count) memcpy(table->rows, roll->members, roll->count*sizeof(*table->rows));
    election_tally(&instance->type->definition.election, &instance->election, table, instance->holder,
                   roll->opinions, roll->nomination);
    return true;
}

static bool current_ruler(const PoliticalInstance *instance, unsigned holder, uint64_t *id) {
    if (ruler_id(holder, id)) return true;
    const ElectionSnapshot *roll = &instance->roll;
    if (!roll->date) return false;
    if (holder == roll->holder) { *id = roll->holder_ruler; return *id != UINT64_MAX; }
    for (size_t i = 0; i < roll->count; ++i)
        if (roll->members[i].tag == holder) { *id = roll->rulers[i]; return *id != UINT64_MAX; }
    return false;
}

static bool store_structure(NativeSaveFields *fields, const PoliticalInstance *instance) {
    const KhanDefinition *definition = &instance->type->definition;
    const char *id = instance->spec->id;
    unsigned roles = 0;
    for (unsigned i = 0; i < definition->role_count; ++i) roles += instance->roles[i].count != 0;
    const unsigned provinces = instance->territory.count;
    const unsigned territory_records = !definition->territory_enabled ? 0 : provinces > PS_TERRITORY_CHUNK
        ? (provinces + PS_TERRITORY_CHUNK - 1) / PS_TERRITORY_CHUNK : 1;
    if (fields->count + roles + territory_records + (instance->excluded.count != 0) > PS_MAX_SAVE_RECORDS)
        return false;
    if (instance->excluded.count) {
        NativeSaveField *field = &fields->entries[fields->count++];
        field->legacy = false;
        if (!encode_explicit_members(id, &instance->excluded, field->value)) return false;
        field->value[0] = 'X';
    }
    for (unsigned i = 0; i < territory_records; ++i) {
        NativeSaveField *field = &fields->entries[fields->count++];
        field->legacy = false;
        const bool ok = territory_records > 1
            ? encode_territory_chunk(id, &instance->territory, i * PS_TERRITORY_CHUNK, field->value)
            : encode_territory(id, &instance->territory, field->value);
        if (!ok) return false;
    }
    for (unsigned i = 0; i < definition->role_count; ++i) {
        if (!instance->roles[i].count) continue;
        NativeSaveField *field = &fields->entries[fields->count++];
        field->legacy = false;
        if (!encode_role(id, definition->roles[i].id, &instance->roles[i], field->value)) return false;
    }
    return true;
}

static bool store_resources(NativeSaveFields *fields, const PoliticalInstance *instance) {
    const KhanDefinition *d = &instance->type->definition;
    for (unsigned i = 0; i < d->resource_count; ++i) {
        const ResourceLedger *ledger = &instance->accounts[i];
        unsigned offset = 0;
        do {
            if (fields->count == PS_MAX_SAVE_RECORDS) return false;
            NativeSaveField *field = &fields->entries[fields->count++];
            field->legacy = false;
            unsigned count = ledger->count - offset;
            if (count > PS_RESOURCE_CHUNK) count = PS_RESOURCE_CHUNK;
            if (!encode_resource_accounts(instance->spec->id, d->resources[i].id, instance->resource_month,
                                          ledger->accounts + offset, count, field->value)) return false;
            offset += count;
        } while (offset < ledger->count);
    }
    return true;
}

static bool store_actions(NativeSaveFields *fields, const PoliticalInstance *instance) {
    const KhanDefinition *d = &instance->type->definition;
    for (unsigned i = 0; i < instance->actions.count; ++i) {
        if (fields->count == PS_MAX_SAVE_RECORDS) return false;
        NativeSaveField *field = &fields->entries[fields->count++];
        const PoliticalActionRun *run = &instance->actions.runs[i];
        field->legacy = false;
        if (!encode_action_run(instance->spec->id, &d->actions[run->slot], run, field->value)) return false;
    }
    return true;
}

static bool store_processes(NativeSaveFields *fields, const PoliticalInstance *instance) {
    const PoliticalAction *actions = instance->type->definition.actions;
    for (unsigned i = 0; i < instance->processes.count; ++i) {
        if (fields->count == PS_MAX_SAVE_RECORDS) return false;
        NativeSaveField *field = &fields->entries[fields->count++];
        const PoliticalProcess *process = &instance->processes.items[i];
        field->legacy = false;
        if (!encode_process(instance->spec->id, &actions[process->slot], process, field->value)) return false;
    }
    return true;
}

static bool store_selections(NativeSaveFields *fields, const PoliticalInstance *instance) {
    const KhanDefinition *d = &instance->type->definition;
    for (unsigned i = 0; i <= PS_MAX_ROLES; ++i) {
        const PoliticalContest *contest = &instance->contests[i];
        if (!contest->opened) continue;
        unsigned offset = 0;
        do {
            if (fields->count == PS_MAX_SAVE_RECORDS) return false;
            NativeSaveField *field = &fields->entries[fields->count++];
            field->legacy = false;
            const char *name = i == PS_MAX_ROLES ? "" : d->roles[i].id;
            if (!encode_contest(instance->spec->id, name, contest, offset, field->value)) return false;
            offset += PS_CONTEST_CHUNK;
        } while (offset < contest->bids.count);
    }
    if (instance->threshold_succession_pending) {
        if (fields->count == PS_MAX_SAVE_RECORDS) return false;
        NativeSaveField *field = &fields->entries[fields->count++];
        field->legacy = false;
        snprintf(field->value, sizeof(field->value), "L1|%s|", instance->spec->id);
    }
    return true;
}

static void publish_campaign(void) {
    if (!plugin.ready) return;
    native_passives_dirty = true;
    unsigned today = 0;
    campaign_date(&today);
    static NativeSaveFields fields;
    fields.count = plugin.retained.count;
    fields.incomplete = plugin.retained.incomplete;
    memcpy(fields.entries, plugin.retained.entries, fields.count * sizeof(*fields.entries));
    for (size_t i = 0; i < plugin.roster.count; ++i) {
        PoliticalInstance *instance = &plugin.instances[i];
        const PoliticalMode mode = instance_mode(instance);
        if (!mode) continue;
        if (instance->life.dynamic || mode != POLITICAL_ACTIVE) {
            if (fields.count == PS_MAX_SAVE_RECORDS ||
                !encode_lifecycle(instance->spec, &instance->life, fields.entries[fields.count].value)) {
                fields.incomplete = true;
                break;
            }
            fields.entries[fields.count++].legacy = false;
        }
        if (mode == POLITICAL_DISSOLVED) continue;
        if (instance->enabled && today) sync_leader_reign(instance, today);
        if (fields.count + 1u + instance->has_members + (instance->election.round != 0) +
            (instance->election.cooldown_count != 0) > PS_MAX_SAVE_RECORDS) {
            fields.incomplete = true;
            break;
        }
        PoliticalSaveRecord record = { .data.reform = instance->reform, .history_date = instance->history_date };
        if (instance->type->definition.resource_count) record.data.reform.authority = 0;
        strcpy(record.instance, instance->spec->id);
        strcpy(record.type, instance->type->id);
        memcpy(record.data.leader, &instance->holder, 3);
        NativeSaveField *field = &fields.entries[fields.count];
        field->legacy = false;
        if (!encode_political_state(&record, field->value)) {
            fields.incomplete = true;
            break;
        }
        ++fields.count;
        for (unsigned j = 0; j < instance->leaders.count; ++j) {
            if (fields.count == PS_MAX_SAVE_RECORDS ||
                !encode_leader_reign(instance->spec->id, &instance->leaders.items[j],
                                     fields.entries[fields.count].value)) {
                fields.incomplete = true;
                break;
            }
            fields.entries[fields.count++].legacy = false;
        }
        if (fields.incomplete) break;
        if (instance->has_members) {
            field = &fields.entries[fields.count];
            field->legacy = false;
            if (!encode_explicit_members(instance->spec->id, &instance->members, field->value)) {
                fields.incomplete = true;
                break;
            }
            ++fields.count;
        }
        if (instance->election.round) {
            NativeSaveField *extra = &fields.entries[fields.count];
            extra->legacy = false;
            if (!encode_election_state(instance->spec->id, &instance->election, extra->value)) {
                fields.incomplete = true;
                break;
            }
            ++fields.count;
            if (instance->election.cooldown_count) {
                NativeSaveField *lock = &fields.entries[fields.count];
                lock->legacy = false;
                if (!encode_election_cooldowns(instance->spec->id, &instance->election, lock->value)) {
                    fields.incomplete = true;
                    break;
                }
                ++fields.count;
            }
        }
        if (!store_structure(&fields, instance) || !store_resources(&fields, instance) || !store_actions(&fields, instance) ||
            !store_selections(&fields, instance) || !store_processes(&fields, instance)) {
            fields.incomplete = true;
            break;
        }
    }
    publish_native_save(plugin.root, &fields);
}

static void notice_monarch(CrownNotice *notice) {
    char tag[4];
    uint32_t needed = 0;
    if (plugin.world->monarch_name &&
        plugin.world->monarch_name(election_text(notice->winner,tag),ANVIL_GAME_MONARCH_RULER,
                                   notice->name,sizeof(notice->name),&needed) != ANVIL_OK)
        notice->name[0] = 0;
}

static void queue_notice(CrownNotice *notice) {
    if (plugin.last_notice) plugin.last_notice->next = notice;
    else plugin.first_notice = notice;
    plugin.last_notice = notice;
}

static void mark_accession(PoliticalInstance *instance, unsigned winner, uint64_t ruler) {
    const LeaderSelection *rule = &instance->type->definition.succession;
    if (!winner || (!elective(instance) && !rule->defined) ||
        (instance->holder == winner && instance->election.ruler == ruler)) return;
    unsigned unlocked = 0;
    if (!rule->defined || rule->cooldown_policy == COOLDOWN_RESET) {
        unlocked = instance->election.cooldown_count;
        instance->election.cooldown_count = 0;
    } else if (rule->cooldown_policy == COOLDOWN_RESTART) {
        unsigned today;
        if (campaign_date(&today)) {
            unsigned kept = 0;
            const unsigned until = election_after_days(today, (unsigned)instance->type->definition.election.cooldown_days);
            for (unsigned i = 0; i < instance->election.cooldown_count; ++i) {
                struct ElectionCooldown lock = instance->election.cooldowns[i];
                if (lock.until <= today || until <= today) continue;
                lock.until = until;
                instance->election.cooldowns[kept++] = lock;
            }
            unlocked = instance->election.cooldown_count - kept;
            instance->election.cooldown_count = kept;
        }
    }
    char tag[4], line[240];
    snprintf(line,sizeof(line),"%s accession instance=%s new=%s cooldowns_cleared=%u", rule->defined ? "political" : "khan",
             instance->spec->id,election_text(winner,tag),unlocked);
    log_message(ANVIL_LOG_INFO,line);
    CrownNotice *notice = calloc(1,sizeof(*notice));
    if (!notice) { log_message(ANVIL_LOG_ERROR,"khan accession message=out_of_memory"); return; }
    notice->instance = instance;
    notice->winner = winner;
    notice->ruler = ruler;
    notice_monarch(notice);
    queue_notice(notice);
}

static void open_accession(void *receiver) {
    PoliticalInstance *instance = receiver;
    if (!plugin.active || !plugin.ready || !instance->enabled || !game_gui_context()) return;
    if (instance->type->definition.script_type && !instance->type->layout) return;
    plugin.pending_panel = PANEL_REQUEST_ACCESSION;
    plugin.pending_instance = (size_t)(instance-plugin.instances);
    log_message(ANVIL_LOG_INFO,"khan accession message=goto");
}

static void free_custom_notice(CustomNotice *notice) {
    for (unsigned i = 0; i < PS_MESSAGE_BODY_LINES; ++i) plugin.free_string(&notice->body[i]);
    plugin.free_string(&notice->title);
    plugin.free_string(&notice->subtitle);
    plugin.free_string(&notice->log);
    plugin.free_string(&notice->close);
    plugin.free_string(&notice->secondary);
    free(notice);
}

static void discard_first_notice(void) {
    CrownNotice *notice = plugin.first_notice;
    plugin.first_notice = notice->next;
    if (!plugin.first_notice) plugin.last_notice = NULL;
    if (notice->custom) {
        free_custom_notice(notice->custom);
        --plugin.custom_notice_count;
    }
    free(notice);
}

static void deliver_custom_notice(CrownNotice *notice, unsigned viewer) {
    const CustomNotice *message = notice->custom;
    const uint64_t recipient = live_holder_tag(viewer);
    if (message->recipient != viewer || !recipient) {
        discard_first_notice();
        return;
    }
    if (message->definition->popup && native_message_visible(&plugin.messages, plugin.gui)) return;
    uint64_t sender = live_holder_tag(message->sender);
    if (!sender) sender = recipient;
    NativeNoticeText text = {
        .title = string_text(&message->title), .subtitle = string_text(&message->subtitle),
        .log = string_text(&message->log), .close = string_text(&message->close),
        .secondary = string_text(&message->secondary), .popup = message->definition->popup,
    };
    for (unsigned i = 0; i < PS_MESSAGE_BODY_LINES; ++i) text.body[i] = string_text(&message->body[i]);
    const bool posted = post_custom_message(&plugin.messages, recipient, sender, &text);
    char line[720], player[4];
    snprintf(line, sizeof(line), "political message %s:%u instance=%s message=%s target=%s mode=%s result=%s",
             message->definition->path, message->definition->line, notice->instance->spec->id, message->definition->id,
             election_text(viewer, player), text.popup ? "popup" : "log",
             posted ? "submitted" : "missing or incompatible native message type");
    log_message(posted ? ANVIL_LOG_INFO : ANVIL_LOG_WARNING, line);
    discard_first_notice();
}

static const char *notice_line(const char *type, const char *line, const char *marker, const char *custom,
                               const char *suffix, PoliticalText context, GameString *out) {
    if (!plugin.text_engine.lookup) return NULL;
    char key[GK_SCRIPT_WORD_SIZE];
    snprintf(key, sizeof(key), "%s_%s", type, line);
    const char *raw = plugin.text_engine.lookup(key);
    if (!raw || !strstr(raw, marker)) return NULL;
    if (custom && *custom && append_political_text(out, custom, context, NULL)) return string_text(out);
    snprintf(key, sizeof(key), "%s_%s", type, suffix);
    if (append_political_text(out, key, context, NULL)) return string_text(out);
    return instance_title(context.instance);
}

static NoticeText prepare_notice(const char *type, const char *custom, PoliticalText context,
                                 GameString *body, GameString *log) {
    return (NoticeText){notice_line(type, "3", "$PF_BODY$", custom, "BODY", context, body),
                        notice_line(type, "LOG", "$PF_LOG$", NULL, "LOG_TEXT", context, log)};
}

static void deliver_accession(void) {
    CrownNotice *notice = plugin.first_notice;
    if (!notice || !plugin.messages.manager) return;
    AnvilGamePlayer viewer = { .size=sizeof(viewer) };
    if (plugin.world->player_tags(&viewer) != ANVIL_OK) return;
    if (notice->custom) {
        deliver_custom_notice(notice, election_tag(viewer.own.text));
        return;
    }
    const uint64_t player = country_tag(viewer.own.text);
    if (notice->resource) {
        const unsigned actor = election_tag(viewer.own.text);
        const EntryAccess access = instance_access((size_t)(notice->instance - plugin.instances), actor);
        if (player && access.view && (!notice->winner || notice->winner == actor)) {
            const ResourceDefinition *resource = notice->resource;
            GameString name = { .capacity = 15 };
            if (resource->name[0]) plugin.localize(&name, resource->name, NULL, 0);
            char value[24];
            format_fixed_number(notice->threshold_value, resource->precision, value);
            GameString body = {.capacity = 15}, summary = {.capacity = 15}, details = {.capacity = 15};
            const ResourceThreshold *threshold = notice->threshold;
            append_amount(&details, "PS_AMOUNT_PREVIOUS", "Previous balance", notice->previous_value, resource->precision, false, "");
            if (threshold) append_amount(&details, "PS_AMOUNT_THRESHOLD", "Threshold", threshold->at, resource->precision, false, "");
            append_amount(&details, "PS_AMOUNT_STORED", "Stored balance", notice->threshold_value, resource->precision, false, "");
            const TextField fields[] = {
                {"value", text_fixed(notice->threshold_value)}, {"current", text_fixed(notice->threshold_value)},
                {"previous", text_fixed(notice->previous_value)},
                {"threshold", threshold ? text_fixed(threshold->at) : (TextValue){0}},
                {"breakdown", text_value(string_text(&details))},
            };
            const NoticeText rendered = prepare_notice("PS_RES_LIMIT", threshold ? threshold->message : NULL,
                (PoliticalText){.instance = notice->instance, .resource = resource, .country = notice->winner ? notice->winner : actor,
                                .fields = fields, .count = sizeof(fields) / sizeof(*fields)}, &body, &summary);
            const bool posted = post_resource_message(&plugin.messages, player, instance_title(notice->instance),
                name.size ? string_text(&name) : resource->id, value, notice->instance, open_accession, rendered);
            if (posted && rendered.body) {
                char log[500];
                snprintf(log, sizeof(log), "political localisation notice instance=%s resource=%s body_key=%s",
                    notice->instance->spec->id, resource->id, threshold && threshold->message[0] ? threshold->message : "PS_RES_LIMIT_BODY");
                log_message(ANVIL_LOG_INFO, log);
            }
            plugin.free_string(&body);
            plugin.free_string(&summary);
            plugin.free_string(&details);
            plugin.free_string(&name);
            log_message(posted ? ANVIL_LOG_INFO : ANVIL_LOG_WARNING,
                        posted ? "political resource_threshold message=submitted" : "political resource_threshold message=unavailable");
        }
        discard_first_notice();
        return;
    }
    const uint64_t winner = live_holder_tag(notice->winner);
    bool posted = false;
    const bool generic = notice->instance->type->definition.succession.defined;
    if (player && winner) {
        if (!notice->name[0]) {
            uint64_t current;
            if (ruler_id(notice->winner,&current) && current == notice->ruler) notice_monarch(notice);
        }
        const GameString *country = plugin.country_name(&winner,false,false,false);
        char tag[4];
        GameString fallback = { .capacity=15 };
        if (!notice->name[0]) plugin.localize(&fallback,"GK_CROWN_RULER",NULL,0);
        const char *ruler = notice->name[0] ? notice->name : string_text(&fallback);
        GameString body = {.capacity = 15}, summary = {.capacity = 15};
        const TextField fields[] = {{"king", text_value(ruler)}, {"ruler", text_value(ruler)}};
        const NoticeText rendered = prepare_notice(generic ? "PS_NEW_LEADER" : "GK_KHAN_CROWN", NULL,
            (PoliticalText){.instance = notice->instance, .country = notice->winner, .fields = fields,
                            .count = sizeof(fields) / sizeof(*fields)}, &body, &summary);
        bool (*post)(const AccessionMessages *, uint64_t, uint64_t, const char *, const char *, const char *, void *, void (*)(void *), NoticeText) =
            generic ? post_leader_message : post_accession_message;
        posted = post(&plugin.messages,player,winner,instance_title(notice->instance),
                   country && country->size ? string_text(country) : election_text(notice->winner,tag),
                   ruler, notice->instance, open_accession, rendered);
        plugin.free_string(&body);
        plugin.free_string(&summary);
        plugin.free_string(&fallback);
    }
    char log[240], tag[4];
    snprintf(log,sizeof(log),"khan accession message=%s type=%s instance=%s winner=%s ruler_id=%llu",
             posted ? "submitted" : "unavailable", generic ? "PS_NEW_LEADER" : "GK_KHAN_CROWN", notice->instance->spec->id,
             election_text(notice->winner,tag),(unsigned long long)notice->ruler);
    log_message(posted ? ANVIL_LOG_INFO : ANVIL_LOG_WARNING,log);
    discard_first_notice();
}

static void open_invitation(void *receiver) {
    PoliticalInstance *instance = receiver;
    const unsigned actor = reform_actor();
    if (!actor || !instance->enabled) return;
    for (unsigned i = 0; i < instance->processes.count; ++i) {
        const PoliticalProcess *request = &instance->processes.items[i];
        if (request->target != actor || request->status != PROCESS_PENDING) continue;
        if (!plugin.visible || selected_instance() != instance) open_accession(instance);
        log_message(ANVIL_LOG_INFO, "political invitation message=goto");
        return;
    }
}

static void deliver_invitation(void) {
    if (!plugin.messages.manager) return;
    const unsigned target = reform_actor();
    const uint64_t recipient = live_holder_tag(target);
    unsigned today;
    if (!recipient || !campaign_date(&today)) return;
    for (size_t i = 0; i < plugin.roster.count; ++i) {
        PoliticalInstance *instance = &plugin.instances[i];
        if (!instance->enabled) continue;
        for (unsigned j = 0; j < instance->processes.count; ++j) {
            PoliticalProcess *request = &instance->processes.items[j];
            if (request->status != PROCESS_PENDING || request->target != target ||
                request->notified || request->deadline <= today) continue;
            if (update_pending_process(instance, request, today)) continue;
            const uint64_t issuer = live_holder_tag(request->issuer);
            if (!issuer) continue;
            const GameString *name = plugin.country_name(&issuer, false, false, false);
            const unsigned expires = request->deadline;
            char date[16], from[4], to[4], line[380];
            snprintf(date, sizeof(date), "%u.%02u.%02u", expires / 10000, expires / 100 % 100, expires % 100);
            const char *tag = election_text(request->issuer, from);
            const char *issuer_name = name && name->size ? string_text(name) : tag;
            GameString body = {.capacity = 15}, summary = {.capacity = 15};
            const TextField fields[] = {{"issuer", text_value(issuer_name)}, {"deadline", text_value(date)},
                                        {"round", text_integer(request->round)}};
            const NoticeText rendered = prepare_notice("PS_INVITATION", NULL,
                (PoliticalText){.instance = instance, .country = target, .fields = fields,
                               .count = sizeof(fields) / sizeof(*fields)}, &body, &summary);
            const bool posted = post_invitation_message(&plugin.messages, recipient, issuer, instance_title(instance),
                                                        issuer_name, date, instance, open_invitation, rendered);
            plugin.free_string(&body);
            plugin.free_string(&summary);
            if (!posted) return;
            request->notified = true;
            publish_campaign();
            snprintf(line, sizeof(line), "political invitation message=submitted instance=%s issuer=%s recipient=%s round=%u",
                     instance->spec->id, from, election_text(target, to), request->round);
            log_message(ANVIL_LOG_INFO, line);
            return;
        }
    }
}

static bool change_holder(PoliticalInstance *instance, unsigned tag) {
    if (tag && !live_holder_tag(tag)) return false;
    if (instance->election.round) {
        uint64_t ruler = 0;
        if (!current_ruler(instance,tag,&ruler)) return false;
        if (instance->holder != tag && tag && instance->election.round < INT_MAX)
            ++instance->election.round;
        mark_accession(instance,tag,ruler);
        instance->election.ruler = ruler;
    } else if (instance->type->definition.succession.defined && instance->holder != tag) {
        uint64_t ruler = 0;
        ruler_id(tag, &ruler);
        mark_accession(instance, tag, ruler);
    }
    instance->holder = tag;
    prune_roles(instance);
    prune_resource_accounts(instance);
    resource_fact_day = 0;
    publish_campaign();
    return true;
}

static const char *settle_election(PoliticalInstance *instance, const ElectionTable *table,
                                   unsigned round, const char *reason) {
    if (table->winner && !live_holder_tag(table->winner)) return "winning country is no longer on the map";
    uint64_t next_ruler = 0;
    if (table->winner && !current_ruler(instance, table->winner, &next_ruler)) return "ruler information unavailable";
    const char *error = election_finish(&instance->election, table, round);
    if (error) return error;
    char old[4], winner[4], message[390];
    snprintf(message, sizeof(message), "election result instance=%s round=%u cause=%s old=%s winner=%s votes=%u tied=%u candidates=%u electors=%u",
             instance->spec->id, round, reason, instance->holder ? election_text(instance->holder, old) : "none",
             table->winner ? election_text(table->winner, winner) : "none", table->high, table->ties, table->candidates, table->voters);
    mark_accession(instance,table->winner,next_ruler);
    instance->holder = table->winner;
    instance->election.ruler = next_ruler;
    publish_campaign();
    log_message(ANVIL_LOG_INFO, message);
    return NULL;
}

static void render_shield_slot(Shield *shield, const char tag_text[4], bool defined_seat) {
    const uint64_t country = tag_text ? country_tag(tag_text) : 0;
    const bool vacant = defined_seat && !country;
    const uint64_t tag = vacant ? registered_country_tag("REB") : country;
    ButtonTooltip *tooltip = &shield->tooltip;
    void *button = tooltip->button;
    void (*set_tooltip)(void *, const ButtonTooltip *) = method(button, 0x220);
    if (!tag) {
        if (shield->displayed_tag) {
            set_tooltip(button, NULL);
            tooltip->tag = 0;
            disconnect_click(button, &shield->listener);
        }
        shield->displayed_tag = 0;
        shield->vacant = false;
        control_visible(button, false);
        return;
    }

    if (tag != shield->displayed_tag || vacant != shield->vacant) {
        struct { uint64_t tag; uint16_t kind; } scope = { .tag = tag, .kind = 1 };
        void (*set_scope)(void *, const void *) = method(button, 0x218);
        void (*set_frame)(void *, int) = method(button, 0x1b0);
        void (*refresh_sprite)(void *) = method(button, 0x300);
        set_scope(button, &scope);
        set_frame(button, (int16_t)(tag >> 32));
        refresh_sprite(button);
        tooltip->methods = &shield_tooltip_methods;
        tooltip->tag = country;
        shield->displayed_tag = tag;
        shield->vacant = vacant;
        set_tooltip(button, tooltip);
        if (vacant) {
            remove_default_click((unsigned char *)button + 0xc8);
            disconnect_click(button, &shield->listener);
        }
        button_enabled(button, !vacant);
    }
    if (!vacant) connect_click(button, &shield->listener, shield, queue_diplomacy);
    control_visible(button, true);
}

static void render_shield(Shield *shield, const char tag_text[4]) {
    render_shield_slot(shield, tag_text, false);
}

static void resize_scroll_rail(void *bar, unsigned last) {
    void *slider = (unsigned char *)bar + 0xc8;
    void (*set)(void *, const ScrollRange *) = method(slider, 0x28);
    const unsigned limit = last ? last : 1;
    const ScrollRange range = {0, (int32_t)limit * SCROLL_SCALE, SCROLL_SCALE, (int32_t)limit + 1};
    set(slider, &range);
}

static void position_scroll_rail(void *bar, unsigned row) {
    void *slider = (unsigned char *)bar + 0xc8;
    void (*set)(void *, float, int, bool) = method(slider, 0x38);
    // EU4 uses the configured units (0..last). The setter also moves the thumb.
    set(slider, (float)row, 0, false);
}

static unsigned read_scroll_row(void *bar, unsigned last, int *wheel) {
    void *slider = (unsigned char *)bar + 0xc8;
    float (*get)(void *) = method(slider, 0x20);
    const float value = get(slider);
    const unsigned row = !(value > 0.0f) ? 0 : value >= (float)last ? last : (unsigned)(value + 0.5f);
    const int movement = *wheel / WHEEL_DELTA;
    if (!movement) return row;
    *wheel %= WHEEL_DELTA;
    const int desired = (int)row - movement;
    const unsigned target = desired <= 0 ? 0 : (unsigned)desired > last ? last : (unsigned)desired;
    if (target != row) position_scroll_rail(bar, target);
    return target;
}

static void sync_scrollbar(bool reset) {
    if (!panel_view()->scrollbar) return;
    const unsigned columns = panel_view()->member_columns ? panel_view()->member_columns : DAIMYO_COLUMNS;
    const uint32_t count = plugin.current.count;
    const uint32_t rows = count / columns + (count % columns != 0);
    const uint32_t visible_rows = panel_view()->member_rows;
    const uint32_t last_row = rows > visible_rows ? rows - visible_rows : 0;
    if (reset) {
        plugin.top_row = 0;
        panel_view()->member_wheel = 0;
    }
    if (plugin.top_row > last_row) plugin.top_row = last_row;
    if (last_row && (reset || last_row != plugin.max_top_row)) {
        resize_scroll_rail(panel_view()->scrollbar, last_row);
        position_scroll_rail(panel_view()->scrollbar, plugin.top_row);
    }
    control_visible(panel_view()->scrollbar, last_row != 0);
    if (reset || last_row != plugin.max_top_row) {
        char message[103];
        snprintf(message, sizeof(message), "great_khan daimyo scrollbar=%s max_row=%u", last_row ? "shown" : "hidden", last_row);
        log_message(ANVIL_LOG_INFO, message);
    }
    plugin.max_top_row = last_row;
}

static void render_daimyo(void) {
    const Shogunate *state = &plugin.current;
    const unsigned columns = panel_view()->member_columns ? panel_view()->member_columns : DAIMYO_COLUMNS;
    const uint32_t slots = panel_view()->member_rows * columns;
    for (uint32_t i = 0; i < slots; ++i) {
        const uint32_t index = plugin.top_row * columns + i;
        const char *tag = index < state->count ? state->daimyo[index].text : NULL;
        if (panel_view()->show_shields) render_shield(&panel_view()->daimyo_shields[i], tag);
        else native_text(panel_view()->daimyo_labels[i], tag ? tag : "");
    }
}

static void read_scrollbar(void) {
    if (!plugin.max_top_row) {
        panel_view()->member_wheel = 0;
        return;
    }
    const unsigned row = read_scroll_row(panel_view()->scrollbar, plugin.max_top_row, &panel_view()->member_wheel);
    if (row != plugin.top_row) {
        plugin.top_row = row;
        render_daimyo();
    }
}

static bool open_pending_diplomacy(void) {
    const uint64_t requested = plugin.pending_country;
    const PoliticalInstanceSpec *requested_instance = plugin.pending_country_instance;
    plugin.pending_country = 0;
    plugin.pending_country_instance = NULL;
    const PoliticalInstance *instance = selected_instance();
    if (!requested || !instance || requested_instance != instance->spec) return false;
    char tag_text[4] = {0};
    memcpy(tag_text, &requested, 3);
    const uint64_t tag = country_tag(tag_text);
    void *in_game = game_gui_context();
    if (!tag || tag != requested || !in_game) return false;
    void *country_panel = pointer_at(in_game, 0x1110);
    if (!country_panel || !pointer_at(country_panel, 0x20)) return false;
    char message[96];
    snprintf(message, sizeof(message), "great_khan shield clicked tag=%s panel=kept_open", tag_text);
    log_message(ANVIL_LOG_INFO, message);
    plugin.open_diplomacy(in_game, tag);
    return true;
}

static void log_original_empires(void) {
    const unsigned char *root = plugin.root;
    char hre[4] = {0}, china[4] = {0};
    memcpy(hre, root + 0x15e0, 3);
    memcpy(china, root + 0x1b50, 3);
    char message[160];
    snprintf(message, sizeof(message), "great_khan coexistence hre_holder=%s china_holder=%s", hre, china);
    log_message(ANVIL_LOG_INFO, message);
}

static bool campaign_date(uint32_t *today) {
    AnvilGameDate date = { .size = sizeof(date) };
    if (!plugin.game || !plugin.game->date || !plugin.game->date(&date)) return false;
    *today = (uint32_t)date.year * 10000 + date.month * 100 + date.day;
    return true;
}

static bool validate_holder(PoliticalInstance *instance) {
    if (!instance->holder || live_holder_tag(instance->holder)) return false;
    instance->holder = 0;
    return true;
}

static void instance_history_error(const PoliticalInstanceSpec *spec, const PoliticalHistoryChange *change, const char *reason) {
    ScriptError error = { .line = change ? change->line : spec->line, .column = change ? change->column : 1 };
    snprintf(error.reason, sizeof(error.reason), "%s (%.90s)", reason, spec->id);
    log_file_error(spec->path, plugin.files.source ? plugin.files.source(spec->path) : NULL, &error);
}

static bool history_capacity(const PoliticalInstance *instance, const bool ready[PS_MAX_INSTANCES]) {
    const KhanDefinition *definition = &instance->type->definition;
    unsigned instances = 0, memberships[PS_MAX_EXPLICIT_MEMBERS] = {0};
    for (size_t i = 0; i < plugin.roster.count; ++i) {
        const PoliticalInstance *other = &plugin.instances[i];
        if (!ready[i] || other->type != instance->type) continue;
        ++instances;
        if (!definition->max_memberships) continue;
        for (unsigned a = 0; a < instance->members.count; ++a)
            memberships[a] += has_tag(other->members.tags, other->members.count, instance->members.tags[a]);
    }
    const char *reason = definition->max_instances && instances > definition->max_instances
        ? "eligible history exceeds max_instances" : NULL;
    for (unsigned a = 0; a < instance->members.count && !reason; ++a)
        if (memberships[a] > definition->max_memberships) reason = "eligible history exceeds max_per_country";
    if (!reason) return true;
    instance_history_error(instance->spec, NULL, reason);
    return false;
}

static bool change_history_list(unsigned *ids, unsigned *count, unsigned capacity, unsigned id, bool add) {
    if (!add) {
        remove_tag(ids, count, id);
        return true;
    }
    if (has_tag(ids, *count, id)) return true;
    if (*count >= capacity) return false;
    ids[(*count)++] = id;
    return true;
}

static const char *history_balance_problem(const KhanDefinition *definition, const char *id, int32_t amount) {
    const unsigned slot = resource_find(definition->resources, definition->resource_count, id);
    if (slot == definition->resource_count || definition->resources[slot].owner != RESOURCE_INSTANCE)
        return "history balance requires a defined instance account";
    if (resource_limit(&definition->resources[slot], amount) != amount)
        return "history balance must be in range at the account precision";
    return NULL;
}

static const char *set_history_balance(PoliticalInstance *instance, const char *id, int32_t amount) {
    const KhanDefinition *definition = &instance->type->definition;
    const char *reason = history_balance_problem(definition, id, amount);
    if (reason) return reason;
    const unsigned slot = resource_find(definition->resources, definition->resource_count, id);
    if (!store_balance(&instance->accounts[slot], 0, amount)) return "cannot store the historical balance";
    if (!strcmp(id, PS_AUTHORITY)) instance->reform.authority = amount / PS_RESOURCE_SCALE;
    return NULL;
}

static const PoliticalHistoryChange *last_role_change(const PoliticalInstanceSpec *spec, const char *role,
                                                      unsigned country, uint32_t today) {
    for (size_t i = spec->change_count; i > 0; --i) {
        const PoliticalHistoryChange *change = &spec->changes[i - 1];
        if (change->date <= today && change->kind == HISTORY_ROLE && change->key == country && !strcmp(change->id, role))
            return change;
    }
    return NULL;
}

static const char *add_historical_role(PoliticalInstance *instance, unsigned slot, unsigned country) {
    const PoliticalRole *role = &instance->type->definition.roles[slot];
    RoleHolders *holders = &instance->roles[slot];
    if (has_tag(holders->countries, holders->count, country)) return NULL;
    bool allowed;
    const char *reason = role_eligible(instance, role, country, true, &allowed);
    if (reason || !allowed) return reason;
    if (holders->count >= role->max_holders) return "historical office exceeds max_holders";
    holders->countries[holders->count++] = country;
    return NULL;
}

static bool seed_historical_roles(PoliticalInstance *instance, uint32_t today) {
    const PoliticalInstanceSpec *spec = instance->spec;
    const PoliticalType *type = instance->type;
    for (unsigned slot = 0; slot < type->definition.role_count; ++slot) {
        const char *role = type->definition.roles[slot].id;
        const KhanateHistoryFiles *markers = &type->role_history[slot];
        for (size_t i = 0; i < markers->count; ++i) {
            const KhanateFlag *file = &markers->items[i];
            if (!flag_at(&file->timeline, today) || last_role_change(spec, role, file->key, today)) continue;
            const char *reason = add_historical_role(instance, slot, file->key);
            if (reason) {
                instance_history_error(spec, NULL, reason);
                return false;
            }
        }
        for (size_t i = 0; i < spec->change_count && spec->changes[i].date <= today; ++i) {
            const PoliticalHistoryChange *change = &spec->changes[i];
            if (change->kind != HISTORY_ROLE || !change->enabled || strcmp(change->id, role) ||
                change != last_role_change(spec, role, change->key, today)) continue;
            const char *reason = add_historical_role(instance, slot, change->key);
            if (reason) {
                instance_history_error(spec, change, reason);
                return false;
            }
        }
    }
    return true;
}

static bool seed_instance_history(PoliticalInstance *instance, uint32_t today) {
    const PoliticalInstanceSpec *spec = instance->spec;
    const KhanDefinition *definition = &instance->type->definition;
    instance->starting_history.effective_date = spec->date;
    instance->history_date = today;
    instance->holder = election_tag(spec->leader);
    instance->members = spec->members;
    instance->has_members = records_members(definition->member_source);
    if (definition->territory_enabled) instance->territory = spec->territory;
    if (definition->territory_enabled && !spec->territory_written) {
        const KhanateHistoryFiles *markers = &instance->type->territory_history;
        for (size_t i = 0; i < markers->count; ++i) {
            const KhanateFlag *file = &markers->items[i];
            if ((plugin.province_limit > 0 && file->key >= (unsigned)plugin.province_limit) ||
                !flag_at(&file->timeline, today)) continue;
            if (!change_history_list(instance->territory.ids, &instance->territory.count, PS_MAX_TERRITORY, file->key, true)) {
                instance_history_error(spec, NULL, "province history exceeds territory capacity");
                return false;
            }
        }
    }
    for (unsigned i = 0; i < spec->balance_count; ++i) {
        const ResourceInitial *balance = &spec->balances[i];
        const char *reason = set_history_balance(instance, balance->id, balance->balance);
        if (reason) {
            instance_history_error(spec, NULL, reason);
            return false;
        }
    }
    for (size_t i = 0; i < spec->change_count && spec->changes[i].date <= today; ++i) {
        const PoliticalHistoryChange *change = &spec->changes[i];
        const char *reason = NULL;
        switch (change->kind) {
        case HISTORY_LEADER:
            instance->holder = change->key;
            instance->starting_history.effective_date = change->date;
            break;
        case HISTORY_CLEAR_MEMBERS: instance->members.count = 0; break;
        case HISTORY_MEMBER:
            if (!change_history_list(instance->members.tags, &instance->members.count, PS_MAX_EXPLICIT_MEMBERS,
                                     change->key, change->enabled)) reason = "historical member capacity exceeded";
            break;
        case HISTORY_CLEAR_TERRITORY: instance->territory.count = 0; break;
        case HISTORY_PROVINCE:
            if (plugin.province_limit > 0 && change->key >= (unsigned)plugin.province_limit)
                reason = "historical province is outside the mounted map";
            else if (!change_history_list(instance->territory.ids, &instance->territory.count, PS_MAX_TERRITORY,
                                          change->key, change->enabled)) reason = "historical territory capacity exceeded";
            break;
        case HISTORY_BALANCE: reason = set_history_balance(instance, change->id, change->amount); break;
        case HISTORY_ROLE: break;
        }
        if (reason) {
            instance_history_error(spec, change, reason);
            return false;
        }
    }
    election_text(instance->holder, instance->starting_history.tag);
    return seed_historical_roles(instance, today);
}

static int province_id_order(const void *left, const void *right) {
    const unsigned a = *(const unsigned *)left, b = *(const unsigned *)right;
    return a < b ? -1 : a != b;
}

static bool restore_structures(const NativeSaveFields *fields, const PoliticalSaveRecord *records, size_t count) {
    bool excluded_seen[PS_MAX_SAVE_RECORDS] = {0}, territory_seen[PS_MAX_SAVE_RECORDS] = {0};
    uint32_t territory_chunks[PS_MAX_SAVE_RECORDS] = {0};
    uint16_t territory_total[PS_MAX_SAVE_RECORDS] = {0};
    _Static_assert(PS_TERRITORY_CHUNKS < 32 && PS_MAX_TERRITORY <= UINT16_MAX, "province chunks must fit their saved layout");
    for (size_t i = 0; i < fields->count; ++i) {
        const NativeSaveField *field = &fields->entries[i];
        const char kind = field->value[0];
        if (field->legacy || !kind || !strchr("XRT", kind)) continue;
        char id[GK_SCRIPT_WORD_SIZE] = {0}, role[GK_SCRIPT_WORD_SIZE] = {0};
        ProvinceList territory = {0};
        TerritorySaveChunk chunk = {0};
        const bool chunked = kind == 'T' && field->value[1] == '2';
        RoleHolders holders = {0};
        MemberTags excluded = {0};
        if (kind == 'X') {
            char value[GK_SAVE_VALUE_SIZE];
            memcpy(value, field->value, sizeof(value));
            value[0] = 'M';
            if (!decode_explicit_members(value, id, &excluded)) return false;
        } else if (kind == 'R') {
            if (!decode_role(field->value, id, role, &holders)) return false;
        } else if (chunked) {
            if (!decode_territory_chunk(field->value, id, &chunk)) return false;
        } else if (!decode_territory(field->value, id, &territory)) return false;
        size_t j = 0;
        while (j < count && strcmp(records[j].instance, id)) ++j;
        if (j == count) return false;
        if (kind == 'X') {
            if (excluded_seen[j]) return false;
            excluded_seen[j] = true;
        } else if (kind == 'T') {
            if (chunked) {
                if (territory_seen[j] && territory_total[j] != chunk.total) return false;
                territory_total[j] = (uint16_t)chunk.total;
                const uint32_t mask = UINT32_C(1) << (chunk.offset / PS_TERRITORY_CHUNK);
                if (territory_chunks[j] & mask) return false;
                territory_chunks[j] |= mask;
            } else if (territory_seen[j]) return false;
            territory_seen[j] = true;
        } else {
            const size_t prefix = strlen(id) + strlen(role) + 5;
            for (size_t prior = 0; prior < i; ++prior)
                if (!fields->entries[prior].legacy && !strncmp(field->value, fields->entries[prior].value, prefix)) return false;
        }
        const size_t index = instance_index(id);
        if (index >= plugin.roster.count || !plugin.instances[index].enabled) {
            plugin.retained.entries[plugin.retained.count++] = *field;
            continue;
        }
        PoliticalInstance *instance = &plugin.instances[index];
        if (kind == 'X' && instance->type->definition.member_source == MEMBERS_MIXED) instance->excluded = excluded;
        else if (kind == 'T' && instance->type->definition.territory_enabled) {
            if (chunked) {
                instance->territory.count = chunk.total;
                memcpy(instance->territory.ids + chunk.offset, chunk.ids, chunk.count * sizeof(*chunk.ids));
            } else instance->territory = territory;
        }
        else if (kind == 'R') {
            const unsigned slot = role_index(instance, role);
            if (slot < instance->type->definition.role_count) instance->roles[slot] = holders;
        }
    }
    for (size_t j = 0; j < count; ++j) {
        if (!territory_total[j]) continue;
        const unsigned chunks = (territory_total[j] + PS_TERRITORY_CHUNK - 1) / PS_TERRITORY_CHUNK;
        if (territory_chunks[j] != (UINT32_C(1) << chunks) - 1) return false;
        const size_t index = instance_index(records[j].instance);
        if (index >= plugin.roster.count || !plugin.instances[index].enabled) continue;
        ProvinceList *list = &plugin.instances[index].territory;
        qsort(list->ids, list->count, sizeof(*list->ids), province_id_order);
        for (unsigned i = 1; i < list->count; ++i)
            if (list->ids[i - 1] == list->ids[i]) return false;
    }
    return true;
}

static bool has_account(const ResourceLedger *ledger, unsigned country) {
    for (unsigned i = 0; i < ledger->count; ++i)
        if (ledger->accounts[i].country == country) return true;
    return false;
}

static bool restore_resources(const NativeSaveFields *fields, const PoliticalSaveRecord *records, size_t count, unsigned today) {
    for (size_t i = 0; i < fields->count; ++i) {
        const NativeSaveField *field = &fields->entries[i];
        if (field->legacy || field->value[0] != 'P') continue;
        char id[GK_SCRIPT_WORD_SIZE], resource[GK_SCRIPT_WORD_SIZE];
        unsigned month;
        ResourceLedger incoming;
        if (!decode_resource_accounts(field->value, id, resource, &month, &incoming) || month > today / 100) return false;
        size_t base = 0;
        while (base < count && strcmp(records[base].instance, id)) ++base;
        if (base == count) return false;
        size_t index = instance_index(id);
        if (index == plugin.roster.count || !plugin.instances[index].enabled) {
            plugin.retained.entries[plugin.retained.count++] = *field;
            continue;
        }
        PoliticalInstance *instance = &plugin.instances[index];
        const KhanDefinition *d = &instance->type->definition;
        const unsigned slot = resource_find(d->resources, d->resource_count, resource);
        if (slot == d->resource_count) {
            plugin.retained.entries[plugin.retained.count++] = *field;
            continue;
        }
        if (month > instance->restored_month) instance->restored_month = month;
        ResourceLedger *ledger = &instance->accounts[slot];
        if ((instance->restored_accounts & (1u << slot)) && (!ledger->count || !incoming.count)) return false;
        instance->restored_accounts |= 1u << slot;
        const ResourceDefinition *def = &d->resources[slot];
        for (unsigned j = 0; j < incoming.count; ++j) {
            const ResourceAccount account = incoming.accounts[j];
            if ((def->owner == RESOURCE_INSTANCE) != !account.country || has_account(ledger, account.country) ||
                !store_balance(ledger, account.country, resource_limit(def, account.balance))) return false;
        }
    }
    return true;
}

static bool restore_actions(const NativeSaveFields *fields, const PoliticalSaveRecord *records, size_t count, unsigned today) {
    for (size_t i = 0; i < fields->count; ++i) {
        const NativeSaveField *field = &fields->entries[i];
        if (field->legacy || field->value[0] != 'A') continue;
        char id[GK_SCRIPT_WORD_SIZE], action[GK_SCRIPT_WORD_SIZE];
        ActionKind kind;
        PoliticalActionRun run;
        if (!decode_action_run(field->value, id, action, &kind, &run) || run.performed > today) return false;
        size_t base = 0;
        while (base < count && strcmp(records[base].instance, id)) ++base;
        if (base == count) return false;
        size_t index = instance_index(id);
        PoliticalInstance *instance = index < plugin.roster.count ? &plugin.instances[index] : NULL;
        if (!instance || !instance->enabled) { plugin.retained.entries[plugin.retained.count++] = *field; continue; }
        const KhanDefinition *type = &instance->type->definition;
        const unsigned slot = political_action_index(type->actions, type->action_count, action);
        if (slot == type->action_count) { plugin.retained.entries[plugin.retained.count++] = *field; continue; }
        const PoliticalAction *def = &type->actions[slot];
        if (def->kind != kind || def->country_target != (run.target != 0) || instance->actions.count == PS_MAX_ACTION_RUNS ||
            find_action_run(&instance->actions, slot, kind == ACTION_REFORM ? 0 : run.actor)) return false;
        run.slot = slot;
        instance->actions.runs[instance->actions.count++] = run;
    }
    return true;
}

static bool restore_processes(const NativeSaveFields *fields, const PoliticalSaveRecord *records, size_t count, unsigned today) {
    for (size_t i = 0; i < fields->count; ++i) {
        const NativeSaveField *field = &fields->entries[i];
        if (field->legacy || field->value[0] != 'Q') continue;
        char id[GK_SCRIPT_WORD_SIZE], action[GK_SCRIPT_WORD_SIZE];
        ActionKind kind;
        PoliticalProcess process;
        if (!decode_process(field->value, id, action, &kind, &process) || process.opened > today || process.resolved > today) return false;
        size_t base = 0;
        while (base < count && strcmp(records[base].instance, id)) ++base;
        if (base == count) return false;
        const size_t index = instance_index(id);
        PoliticalInstance *instance = index < plugin.roster.count ? &plugin.instances[index] : NULL;
        if (!instance || !instance->enabled) { plugin.retained.entries[plugin.retained.count++] = *field; continue; }
        const KhanDefinition *type = &instance->type->definition;
        const unsigned slot = political_action_index(type->actions, type->action_count, action);
        if (slot == type->action_count || type->actions[slot].kind != kind) {
            plugin.retained.entries[plugin.retained.count++] = *field;
            continue;
        }
        PoliticalProcesses *state = &instance->processes;
        if (state->count == PS_MAX_PROCESSES || political_process_index(state, slot, process.target) < state->count) return false;
        process.slot = slot;
        state->items[state->count++] = process;
    }
    return true;
}

static bool restore_selections(const NativeSaveFields *fields, const PoliticalSaveRecord *records, size_t count, unsigned today) {
    for (size_t i = 0; i < fields->count; ++i) {
        const NativeSaveField *field = &fields->entries[i];
        if (field->legacy || (field->value[0] != 'J' && field->value[0] != 'L')) continue;
        char id[GK_SCRIPT_WORD_SIZE], office[GK_SCRIPT_WORD_SIZE];
        const bool threshold = field->value[0] == 'L';
        PoliticalContest incoming;
        if (threshold) {
            if (strncmp(field->value, "L1|", 3)) return false;
            const char *end = strchr(field->value + 3, '|');
            if (!end || end[1] || end - field->value - 3 >= GK_SCRIPT_WORD_SIZE) return false;
            unsigned length = (unsigned)(end - field->value - 3);
            memcpy(id, field->value + 3, length);
            id[length] = 0;
            if (!ps_identifier(id)) return false;
        } else if (!decode_contest(field->value, id, office, &incoming) || incoming.opened > today ||
                   (incoming.settled && incoming.due > today)) return false;
        size_t base = 0;
        while (base < count && strcmp(records[base].instance, id)) ++base;
        if (base == count) return false;
        const size_t index = instance_index(id);
        PoliticalInstance *instance = index < plugin.roster.count ? &plugin.instances[index] : NULL;
        if (!instance || !instance->enabled) { plugin.retained.entries[plugin.retained.count++] = *field; continue; }
        const KhanDefinition *d = &instance->type->definition;
        if (threshold) {
            if (d->succession.event != SELECT_THRESHOLD) { plugin.retained.entries[plugin.retained.count++] = *field; continue; }
            if (instance->threshold_succession_pending) return false;
            instance->threshold_succession_pending = true;
            continue;
        }
        const unsigned slot = office[0] ? role_index(instance, office) : PS_MAX_ROLES;
        const bool compatible = office[0] ? slot < d->role_count && d->roles[slot].acquisition == OFFICE_COMPETITION :
                                d->succession.mode == LEADER_COMPETITION;
        if (!compatible) { plugin.retained.entries[plugin.retained.count++] = *field; continue; }
        PoliticalContest *contest = &instance->contests[slot];
        if (!contest->opened) *contest = incoming;
        else {
            if (!incoming.bids.count || !contest->bids.count || contest->opened != incoming.opened || contest->due != incoming.due ||
                contest->settled != incoming.settled || contest->winner != incoming.winner) return false;
            for (unsigned j = 0; j < incoming.bids.count; ++j) {
                const ResourceAccount *bid = &incoming.bids.accounts[j];
                if (has_account(&contest->bids, bid->country) || !store_balance(&contest->bids, bid->country, bid->balance)) return false;
            }
        }
    }
    return true;
}

static PoliticalInstance *register_dynamic(const PoliticalLifecycleRecord *record, const PoliticalType *type) {
    if (plugin.roster.count == PS_MAX_INSTANCES) return NULL;
    const size_t index = plugin.roster.count++;
    PoliticalInstanceSpec *spec = &plugin.roster.items[index];
    *spec = (PoliticalInstanceSpec){ .date = record->lifecycle.created, .starting_authority = -1 };
    strcpy(spec->id, record->instance);
    strcpy(spec->type, record->type);
    strcpy(spec->title, record->name);
    strcpy(spec->path, type->definition.source_path);
    PoliticalInstance *instance = &plugin.instances[index];
    memset(instance, 0, sizeof(*instance));
    instance->spec = spec;
    instance->type = type;
    instance->life = record->lifecycle;
    instance->configured = true;
    if (spec->title[0]) localize_key(instance->title, spec->title);
    return instance;
}

typedef enum WindowRetirement { RETIRE_FINISHED, RETIRE_PENDING, RETIRE_UNAVAILABLE } WindowRetirement;

static WindowRetirement retire_dynamic_windows(void) {
    size_t first = plugin.roster.count;
    while (first && plugin.instances[first - 1].life.dynamic) --first;
    bool pending = false;
    for (size_t i = first; i < plugin.roster.count; ++i) {
        const PoliticalPanel *panel = &plugin.panels[i];
        void *windows[] = { panel->window, panel->vote.window };
        for (size_t w = 0; w < sizeof(windows) / sizeof(*windows); ++w) {
            void *window = windows[w];
            if (!window) continue;
            bool registered;
            if (!fixed_window_registry_contains(plugin.gui, window, &registered)) return RETIRE_UNAVAILABLE;
            if (!registered) continue;
            if (!plugin.retire_window || method(window, 0x130) != (void *)plugin.retire_window) return RETIRE_UNAVAILABLE;
            if (!((const unsigned char *)window)[0xf4]) plugin.retire_window(window);
            pending = true;
        }
    }
    // CGui still owns installed click and tooltip references until its removal pass.
    if (pending) return RETIRE_PENDING;
    for (size_t i = first; i < plugin.roster.count; ++i) {
        plugin.panels[i] = (PoliticalPanel){0};
        release_roll(&plugin.instances[i].roll);
    }
    plugin.roster.count = first;
    return RETIRE_FINISHED;
}

static bool restore_lifecycle(const NativeSaveFields *fields, const PoliticalSaveRecord *records, size_t count, unsigned today) {
    PoliticalLifecycleRecord seen[PS_MAX_INSTANCES];
    size_t total = 0;
    for (size_t i = 0; i < fields->count; ++i) {
        const NativeSaveField *field = &fields->entries[i];
        if (field->legacy || field->value[0] != 'Y') continue;
        PoliticalLifecycleRecord entry;
        if (total == PS_MAX_INSTANCES || !decode_lifecycle(field->value, &entry) || entry.lifecycle.created > today)
            return false;
        for (size_t j = 0; j < total; ++j)
            if (!strcmp(seen[j].instance, entry.instance)) return false;
        seen[total++] = entry;
        size_t j = 0;
        while (j < count && strcmp(records[j].instance, entry.instance)) ++j;
        if (((j == count) != (entry.lifecycle.mode == POLITICAL_DISSOLVED)) ||
            (j < count && strcmp(records[j].type, entry.type))) return false;
        const size_t index = instance_index(entry.instance);
        const PoliticalType *type = known_type(entry.type);
        if (entry.lifecycle.dynamic && index < plugin.roster.count) return false;
        if (entry.lifecycle.dynamic && type) {
            if (!register_dynamic(&entry, type)) return false;
        } else if (!entry.lifecycle.dynamic && index < plugin.roster.count &&
                   plugin.instances[index].configured && plugin.instances[index].type == type) {
            plugin.instances[index].life = entry.lifecycle;
        } else {
            plugin.retained.entries[plugin.retained.count++] = *field;
        }
    }
    return true;
}

static bool restore_leader_reigns(const NativeSaveFields *fields, const PoliticalSaveRecord *records,
                                  size_t count, unsigned today) {
    for (size_t i = 0; i < fields->count; ++i) {
        const NativeSaveField *field = &fields->entries[i];
        if (field->legacy || field->value[0] != 'N') continue;
        char id[GK_SCRIPT_WORD_SIZE];
        LeaderReign reign;
        if (!decode_leader_reign(field->value, id, &reign) || reign.start > today || reign.end > today) return false;
        size_t base = 0;
        while (base < count && strcmp(records[base].instance, id)) ++base;
        if (base == count) return false;
        const size_t index = instance_index(id);
        if (index == plugin.roster.count || !plugin.instances[index].enabled) {
            plugin.retained.entries[plugin.retained.count++] = *field;
            continue;
        }
        LeaderHistory *history = &plugin.instances[index].leaders;
        if (history->count == PS_LEADER_REIGNS ||
            (history->count && (!history->items[history->count - 1].end ||
                               history->items[history->count - 1].end > reign.start))) return false;
        append_leader_reign(history, reign);
    }
    return true;
}

static bool apply_campaign_load(const KhanLoadedSave *loaded, const char *player, uint32_t today) {
    plugin.ready = false;
    plugin.province_limit = plugin.world && plugin.world->province_limit ? plugin.world->province_limit() : 0;
    plugin.enabled_count = 0;
    plugin.retained.count = 0;
    plugin.retained.incomplete = false;
    if (loaded->fields.count > PS_MAX_SAVE_RECORDS) {
        static const NativeSaveFields invalid = { .incomplete = true };
        publish_native_save(plugin.root, &invalid);
        log_message(ANVIL_LOG_WARNING, "political restore=too_many_input_records");
        return false;
    }
    static NativeSaveFields base;
    base.count = 0;
    base.incomplete = loaded->fields.incomplete;
    for (size_t i = 0; i < loaded->fields.count; ++i) {
        const NativeSaveField *field = &loaded->fields.entries[i];
        if (field->legacy || !field->value[0] || !strchr("ECMXRTPAJLQYN", field->value[0])) base.entries[base.count++] = *field;
    }
    static PoliticalSaveRecord records[PS_MAX_SAVE_RECORDS];
    const KhanLoadStatus status = decode_saved_instances(&base, records);
    if (status == GK_LOAD_INVALID || status == GK_LOAD_UNSUPPORTED) {
        publish_native_save(plugin.root, &loaded->fields);
        log_message(ANVIL_LOG_WARNING, status == GK_LOAD_UNSUPPORTED
            ? "political restore=unsupported_save_schema"
            : "political restore=invalid_save_record");
        return false;
    }
    if (retire_dynamic_windows() != RETIRE_FINISHED) {
        publish_native_save(plugin.root, &loaded->fields);
        log_message(ANVIL_LOG_WARNING, "political restore=native_window_retirement_failed");
        return false;
    }
    for (size_t i = 0; i < plugin.roster.count; ++i) plugin.instances[i].life = (PoliticalLifecycle){0};
    if (!restore_lifecycle(&loaded->fields, records, base.count, today)) {
        publish_native_save(plugin.root, &loaded->fields);
        log_message(ANVIL_LOG_WARNING, "political restore=invalid_lifecycle_record");
        return false;
    }

    const char *sources[PS_MAX_INSTANCES] = {0};
    bool history_ready[PS_MAX_INSTANCES] = {0};
    for (size_t i = 0; i < plugin.roster.count; ++i) {
        PoliticalInstance *instance = &plugin.instances[i];
        instance->enabled = false;
        instance->has_members = false;
        instance->holder = 0;
        instance->members = (MemberTags){0};
        instance->excluded = (MemberTags){0};
        instance->territory = (ProvinceList){0};
        memset(instance->roles, 0, sizeof(instance->roles));
        memset(instance->accounts, 0, sizeof(instance->accounts));
        instance->actions.count = 0;
        instance->processes.count = 0;
        instance->leaders.count = 0;
        memset(instance->contests, 0, sizeof(instance->contests));
        instance->threshold_succession_pending = false;
        instance->restored_month = instance->restored_accounts = instance->computed_values = 0;
        instance->resource_month = today / 100;
        if (!instance->configured) continue;
        instance->election = (ElectionState){ .round = (elective(instance) ||
            instance->type->definition.succession.event == SELECT_RULER_CHANGE) ? 1u : 0u };
        instance->election_checked_date = 0;
        instance->history_date = uses_capital_members(instance) ? today : 0;
        instance->starting_history = (KhanHistory){0};
        if (loaded->loaded || instance->life.mode || instance->spec->date > today) continue;
        const int authority = instance->spec->starting_authority;
        instance->reform = (ReformState){ .authority = authority >= 0
            ? authority : instance->type->definition.starting_authority };
        if (instance->spec->date) {
            if (!seed_instance_history(instance, today)) continue;
        } else {
            instance->starting_history = history_at(&instance->timeline, today);
            instance->holder = election_tag(instance->starting_history.tag);
        }
        history_ready[i] = true;
    }
    for (size_t i = 0; i < plugin.roster.count; ++i) {
        PoliticalInstance *instance = &plugin.instances[i];
        if (!history_ready[i] ||
            (instance->type->definition.script_type && !history_capacity(instance, history_ready))) continue;
        instance->enabled = true;
        sources[i] = "history";
    }

    for (size_t i = 0; i < base.count; ++i) {
        const PoliticalSaveRecord *record = &records[i];
        const size_t index = instance_index(record->instance);
        PoliticalInstance *instance = index < plugin.roster.count ? &plugin.instances[index] : NULL;
        if (!instance || !instance->configured || strcmp(instance->type->id, record->type)) {
            if (instance) instance->enabled = false;
            plugin.retained.entries[plugin.retained.count++] = base.entries[i];
            char message[360];
            snprintf(message, sizeof(message), "political restore=retained_unknown instance=%.127s type=%.127s",
                     record->instance, record->type);
            log_message(ANVIL_LOG_WARNING, message);
            continue;
        }
        instance->enabled = true;
        instance->holder = election_tag(record->data.leader);
        instance->reform = record->data.reform;
        if (record->history_date) instance->history_date = record->history_date;
        if (uses_capital_members(instance) && record->history_date) {
            instance->starting_history = history_at(&instance->timeline, instance->history_date);
        }
        sources[index] = base.entries[i].legacy ? "legacy_save" : "save";
    }

    bool members_seen[PS_MAX_SAVE_RECORDS] = {0};
    for (size_t i = 0; i < loaded->fields.count; ++i) {
        const NativeSaveField *field = &loaded->fields.entries[i];
        if (field->legacy || field->value[0] != 'M') continue;
        char id[GK_SCRIPT_WORD_SIZE];
        MemberTags members;
        const bool valid = decode_explicit_members(field->value, id, &members);
        size_t j = 0;
        if (valid) while (j < base.count && strcmp(records[j].instance, id)) ++j;
        if (!valid || j == base.count || members_seen[j]) {
            publish_native_save(plugin.root, &loaded->fields);
            log_message(ANVIL_LOG_WARNING, "political restore=invalid_member_record");
            return false;
        }
        members_seen[j] = true;
        size_t index = instance_index(id);
        if (index < plugin.roster.count && plugin.instances[index].enabled) {
            plugin.instances[index].members = members;
            plugin.instances[index].has_members = true;
        } else plugin.retained.entries[plugin.retained.count++] = *field;
    }

    if (!restore_leader_reigns(&loaded->fields, records, base.count, today)) {
        publish_native_save(plugin.root, &loaded->fields);
        log_message(ANVIL_LOG_WARNING, "political restore=invalid_leader_history");
        return false;
    }
    if (!restore_structures(&loaded->fields, records, base.count)) {
        publish_native_save(plugin.root, &loaded->fields);
        log_message(ANVIL_LOG_WARNING, "political restore=invalid_territory_role_or_exclusion_record");
        return false;
    }
    if (!restore_resources(&loaded->fields, records, base.count, today)) {
        publish_native_save(plugin.root, &loaded->fields);
        log_message(ANVIL_LOG_WARNING, "political restore=invalid_resource_record");
        return false;
    }
    if (!restore_actions(&loaded->fields, records, base.count, today)) {
        publish_native_save(plugin.root, &loaded->fields);
        log_message(ANVIL_LOG_WARNING, "political restore=invalid_action_record");
        return false;
    }
    if (!restore_selections(&loaded->fields, records, base.count, today)) {
        publish_native_save(plugin.root, &loaded->fields);
        log_message(ANVIL_LOG_WARNING, "political restore=invalid_selection_record");
        return false;
    }
    if (!restore_processes(&loaded->fields, records, base.count, today)) {
        publish_native_save(plugin.root, &loaded->fields);
        log_message(ANVIL_LOG_WARNING, "political restore=invalid_process_record");
        return false;
    }
    bool seen[PS_MAX_SAVE_RECORDS] = {0};
    for (size_t i = 0; i < loaded->fields.count; ++i) {
        const NativeSaveField *field = &loaded->fields.entries[i];
        if (field->legacy || field->value[0] != 'E') continue;
        char id[GK_SCRIPT_WORD_SIZE];
        ElectionState state;
        bool valid = decode_election_state(field->value, id, &state);
        size_t j = 0;
        if (valid) while (j < base.count && strcmp(records[j].instance, id)) ++j;
        if (!valid || j == base.count || seen[j]) {
            publish_native_save(plugin.root, &loaded->fields);
            log_message(ANVIL_LOG_WARNING, "political restore=invalid_or_unsupported_election_record");
            return false;
        }
        seen[j] = true;
        const size_t index = instance_index(id);
        if (index < plugin.roster.count && plugin.instances[index].enabled) {
            plugin.instances[index].election = state;
        } else plugin.retained.entries[plugin.retained.count++] = *field;
    }
    bool locks_seen[PS_MAX_SAVE_RECORDS] = {0};
    for (size_t i = 0; i < loaded->fields.count; ++i) {
        const NativeSaveField *field = &loaded->fields.entries[i];
        if (field->legacy || field->value[0] != 'C') continue;
        char id[GK_SCRIPT_WORD_SIZE];
        ElectionState temp = {0};
        bool valid = decode_election_cooldowns(field->value, id, &temp);
        size_t j = 0;
        if (valid) while (j < base.count && strcmp(records[j].instance,id)) ++j;
        if (!valid || j == base.count || !seen[j] || locks_seen[j]) {
            publish_native_save(plugin.root, &loaded->fields);
            log_message(ANVIL_LOG_WARNING, "political restore=invalid_election_cooldown");
            return false;
        }
        locks_seen[j] = true;
        size_t index = instance_index(id);
        if (index < plugin.roster.count && plugin.instances[index].enabled) {
            ElectionState *s = &plugin.instances[index].election;
            memcpy(s->cooldowns,temp.cooldowns,sizeof(s->cooldowns));
            s->cooldown_count = temp.cooldown_count;
        } else plugin.retained.entries[plugin.retained.count++] = *field;
    }
    size_t auxiliary_records = 0;
    for (size_t i = 0; i < plugin.roster.count; ++i) {
        PoliticalInstance *instance = &plugin.instances[i];
        if (!instance->enabled) continue;
        if (loaded->loaded && records_members(instance->type->definition.member_source) && !instance->has_members) {
            publish_native_save(plugin.root, &loaded->fields);
            log_message(ANVIL_LOG_WARNING, "political restore=missing_explicit_member_record");
            return false;
        }
        auxiliary_records += instance->has_members;
        const bool suspended = instance->life.mode == POLITICAL_SUSPENDED;
        if (!suspended && !plugin.enabled_count++) plugin.selected = i;
        const KhanDefinition *definition = &instance->type->definition;
        if (instance->restored_month) instance->resource_month = instance->restored_month;
        if (loaded->loaded && !instance->restored_month && definition->authority_enabled) {
            const unsigned slot = resource_find(definition->resources, definition->resource_count, PS_AUTHORITY);
            if (slot < definition->resource_count)
                store_balance(&instance->accounts[slot], 0, resource_limit(&definition->resources[slot],
                              (int64_t)instance->reform.authority * PS_RESOURCE_SCALE));
        }
        if (!suspended) {
            validate_holder(instance);
            prune_roles(instance);
            seed_leader_reigns(instance, today);
        }
        if (instance->election.round) {
            if (migrate_withdrawn_ballots(&instance->election))
                log_message(ANVIL_LOG_INFO,"election restore=legacy_abstention_to_automatic");
            ++auxiliary_records;
            auxiliary_records += instance->election.cooldown_count != 0;
            if (!suspended) {
                if (elective(instance)) sample_electoral_roll(instance, today);
                if (!instance->election.ruler || !elective(instance)) ruler_id(instance->holder, &instance->election.ruler);
            }
        }
        char holder[4] = {0};
        memcpy(holder, &instance->holder, 3);
        char message[690], authority[24];
        const unsigned authority_slot = resource_find(definition->resources, definition->resource_count, PS_AUTHORITY);
        if (authority_slot < definition->resource_count) {
            const ResourceDefinition *def = &definition->resources[authority_slot];
            format_fixed_number(resource_balance(def, &instance->accounts[authority_slot], 0), def->precision, authority);
        } else snprintf(authority, sizeof(authority), "%d", instance->reform.authority);
        unsigned offices = 0;
        for (unsigned role = 0; role < definition->role_count; ++role) offices += instance->roles[role].count;
        snprintf(message, sizeof(message), "political instance ready id=%s type=%s player=%.3s holder=%s "
                 "source=%s authority=%s passed=%s configured=%s explicit_members=%u state=%s history_date=%u leader_date=%u territory=%u office_holders=%u",
                 instance->spec->id, instance->type->id, player, holder[0] ? holder : "none", sources[i],
                 authority, instance->reform.passed_id[0] ? instance->reform.passed_id : "none",
                 instance->starting_history.tag[0] ? instance->starting_history.tag : "none", instance->members.count,
                 political_mode_name(suspended ? POLITICAL_SUSPENDED : POLITICAL_ACTIVE), instance->history_date,
                 instance->starting_history.effective_date, instance->territory.count, offices);
        log_message(ANVIL_LOG_INFO, message);
        instance->enabled = !suspended;
    }
    if (plugin.enabled_count + auxiliary_records + plugin.retained.count > PS_MAX_SAVE_RECORDS) {
        publish_native_save(plugin.root, &loaded->fields);
        log_message(ANVIL_LOG_WARNING, "political restore=too_many_retained_records");
        return false;
    }
    if (!plugin.enabled_count && loaded->loaded) log_message(ANVIL_LOG_INFO, "political restore=empty_campaign");
    plugin.ready = true;
    publish_campaign();
    return true;
}

static bool initialize_campaign(const char *player, uint32_t today) {
    static KhanLoadedSave loaded;
    take_native_load(plugin.root, &loaded);
    return apply_campaign_load(&loaded, player, today);
}

static unsigned reform_actor(void) {
    AnvilGamePlayer player = { .size = sizeof(player) };
    unsigned actor = 0;
    if (plugin.world->player_tags(&player) == ANVIL_OK) memcpy(&actor, player.own.text, 3);
    return actor;
}

static void reform_prestige(void *country, int32_t amount, const KhanReform *definition) {
    plugin.add_prestige(country, &amount, definition->path, (int)definition->effect_line);
}

static ReformStatus enact_reform_for(PoliticalInstance *instance, unsigned actor) {
    if (validate_holder(instance)) publish_campaign();
    const unsigned leader = instance->holder;
    const KhanReform *reform = &instance->type->reform;
    const ReformStatus status = evaluate_reform(&instance->reform, reform, actor, leader);
    if (status != REFORM_AVAILABLE) return status;

    char tag[4] = {0};
    memcpy(tag, &leader, 3);
    void *country = country_from_tag(live_holder_tag(leader));
    if (!country) return REFORM_NO_LEADER;
    int32_t before, after;
    plugin.get_prestige(country, &before);
    const int authority_before = instance->reform.authority;
    const ReformStatus result = pass_reform(&instance->reform, reform, actor, leader, country, reform_prestige);
    if (result == REFORM_ENACTED) {
        publish_campaign();
        plugin.get_prestige(country, &after);
        char message[520];
        snprintf(message, sizeof(message),
                 "political reform=passed instance=%s id=%s leader=%s authority_before=%d authority_after=%d "
                 "cost=%d prestige_before_milli=%d prestige_after_milli=%d effect_delta_milli=%d",
                 instance->spec->id, reform->id, tag, authority_before, instance->reform.authority,
                 reform->cost, before, after, after - before);
        log_message(ANVIL_LOG_INFO, message);
    }
    return result;
}

static bool political_campaign_active(void) {
    return plugin.active && plugin.ready && plugin.root && plugin.root == *plugin.root_slot;
}

static bool country_fact(unsigned country, CountryFact *out) {
    const uint64_t tag = live_holder_tag(country);
    if (!tag) return false;
    CountryFact *fact = &country_facts[(uint16_t)(tag >> 32)];
    char name[4];
    AnvilGameCountry data = { .size = sizeof(data) };
    if (plugin.world->country && plugin.world->country(election_text(country, name), &data) == ANVIL_OK) {
        *fact = (CountryFact){country, resource_fact_day, data.capital, data.manpower, data.values};
    } else {
        unsigned day;
        if (!campaign_date(&day) || fact->tag != country || fact->day != day) return false;
    }
    *out = *fact;
    return true;
}

static bool native_manpower(unsigned country, int32_t *value) {
    CountryFact fact;
    if (!country_fact(country, &fact) || !(fact.values & ANVIL_GAME_VALUE_MANPOWER)) return false;
    *value = fact.manpower;
    return true;
}

static bool count_by_strength(const PoliticalInstance *instance, const CalculationDefinition *def,
                              const Shogunate *members, int32_t *value) {
    int32_t leader;
    if (!native_manpower(instance->holder, &leader)) return false;
    int32_t count = 0;
    for (uint32_t i = 0; i < members->count; ++i) {
        const unsigned tag = election_tag(members->daimyo[i].text);
        if (tag == instance->holder) continue;
        int32_t strength;
        if (!native_manpower(tag, &strength)) return false;
        count += ps_compare(def->comparison, strength, leader);
    }
    *value = count * PS_RESOURCE_SCALE;
    return true;
}

static bool read_calculation(const PoliticalInstance *instance, unsigned slot, unsigned country, int32_t *value) {
    const CalculationDefinition *def = &instance->type->definition.calculations[slot];
    if (def->kind == VALUE_MANPOWER) return native_manpower(country, value);
    unsigned day;
    if (!(instance->computed_values & (1u << slot)) || !campaign_date(&day) || day != resource_fact_day) return false;
    *value = instance->computed[slot];
    return true;
}

static const char *resource_owner(const PoliticalInstance *instance, const ResourceDefinition *def, unsigned country, bool *active) {
    *active = false;
    if (def->owner == RESOURCE_INSTANCE) { *active = !country; return NULL; }
    if (!country || !live_holder_tag(country)) return NULL;
    if (def->owner == RESOURCE_LEADER) { *active = country == instance->holder; return NULL; }
    return instance_has_member(instance, country, active);
}

static const char *text_country(PoliticalText *context, unsigned country) {
    if (!country) return NULL;
    election_text(country, context->tag);
    const uint64_t tag = country_tag(context->tag);
    const GameString *name = tag && plugin.country_name ? plugin.country_name(&tag, false, false, false) : NULL;
    return name && name->size ? string_text(name) : context->tag;
}

static const char *text_localised(PoliticalText *context, const char *key) {
    plugin.free_string(&context->scratch);
    context->scratch = (GameString){.capacity = 15};
    return lookup_ui_label(&context->scratch, key) ? string_text(&context->scratch) : key;
}

static const ResourceDefinition *text_resource(const PoliticalText *context, const char *id) {
    if (!*id) return context->resource;
    if (!context->instance) return NULL;
    const KhanDefinition *d = &context->instance->type->definition;
    const unsigned slot = resource_find(d->resources, d->resource_count, id);
    return slot < d->resource_count ? &d->resources[slot] : NULL;
}

static bool political_text_source(void *argument, const char *source, const char *id, TextValue *out) {
    PoliticalText *context = argument;
    const PoliticalInstance *instance = context->instance;
    if (!_stricmp(source, "val")) source = "value";
    if (!_stricmp(source, "why")) source = "breakdown";
    if (!_stricmp(source, "realm")) source = "system";
    if (!*id) {
        for (size_t i = 0; i < context->count; ++i) {
            const TextField *field = &context->fields[i];
            if (!_stricmp(field->name, source)) { *out = field->value; return out->kind != TEXT_NONE; }
        }
    }
    const char *name = NULL;
    if (!_stricmp(source, "localisation") && *id) name = text_localised(context, id);
    else if (!_stricmp(source, "country")) name = text_country(context, context->country);
    else if (instance) {
        if (!_stricmp(source, "system")) name = instance_title(instance);
        else if (!_stricmp(source, "instance")) name = instance->spec->id;
        else if (!_stricmp(source, "type")) name = instance->spec->type;
        else if (!_stricmp(source, "leader")) name = text_country(context, instance->holder);
        else if (!_stricmp(source, "member_count")) {
            Shogunate members = {0};
            AnvilGameTag holder = {0};
            election_text(instance->holder, holder.text);
            if (!instance_members(instance, &holder, &members)) return false;
            *out = text_integer((int32_t)members.count);
            release_shogunate(&members);
            return true;
        } else if (!_stricmp(source, "role_count") && *id) {
            const unsigned index = role_index(instance, id);
            if (index >= instance->type->definition.role_count) return false;
            const RoleHolders *holders = &instance->roles[index];
            int count = 0;
            for (unsigned i = 0; i < holders->count; ++i) count += live_holder_tag(holders->countries[i]) != 0;
            *out = text_integer(count);
            return true;
        } else if (!_stricmp(source, "province_count") && instance->type->definition.territory_enabled) {
            *out = text_integer((int32_t)instance->territory.count);
            return true;
        } else {
            const ResourceDefinition *resource = text_resource(context, id);
            if (!resource) return false;
            if (!_stricmp(source, "resource") || !_stricmp(source, "name"))
                name = resource->name[0] ? text_localised(context, resource->name) : resource->id;
            else if (!_stricmp(source, "minimum")) { *out = text_fixed(resource->minimum); return true; }
            else if (!_stricmp(source, "maximum")) { *out = text_fixed(resource->maximum); return true; }
            else if (!_stricmp(source, "initial")) { *out = text_fixed(resource->initial); return true; }
            else if (!_stricmp(source, "balance")) {
                const KhanDefinition *d = &instance->type->definition;
                const unsigned owner = resource->owner == RESOURCE_INSTANCE ? 0 : context->country;
                bool active;
                if (resource_owner(instance, resource, owner, &active) || !active) return false;
                *out = text_fixed(resource_balance(resource, &instance->accounts[resource - d->resources], owner));
                return true;
            }
        }
    }
    if (!name) return false;
    *out = text_value(name);
    return true;
}

static bool append_political_text(GameString *out, const char *key, PoliticalText context, bool *used_breakdown) {
    if (used_breakdown) *used_breakdown = false;
    if (!plugin.text_engine.lookup) return false;
    context.scratch.capacity = 15;
    GameString translated = {.capacity = 15};
    const bool success = render_political_text(&plugin.text_engine, &plugin.text_templates, key,
                                               political_text_source, &context, &translated, used_breakdown);
    if (success && translated.size) plugin.append_string(out, string_text(&translated), translated.size);
    plugin.free_string(&translated);
    plugin.free_string(&context.scratch);
    return success;
}

static void append_instance_text(GameString *out, const char *key, const PoliticalInstance *instance, unsigned country) {
    if (!append_political_text(out, key, (PoliticalText){.instance = instance, .country = country}, NULL))
        append_label(out, key);
}

static void render_instance_text(void *control, const char *key, const char *fallback,
                                 const PoliticalInstance *instance) {
    if (!control) return;
    GameString text = {.capacity = 15};
    const bool rendered = key && append_political_text(&text, key,
        (PoliticalText){.instance = instance, .country = reform_actor()}, NULL);
    native_text(control, rendered ? string_text(&text) : fallback);
    plugin.free_string(&text);
}

static const char *text_scope_country(PoliticalText *scope, unsigned country) {
    switch (country) {
    case PS_COUNTRY_ROOT: return "ROOT";
    case PS_COUNTRY_THIS: return "THIS";
    case PS_COUNTRY_LEADER: return "LEADER";
    case PS_COUNTRY_FROM: return "FROM";
    case PS_COUNTRY_PREV: return "PREV";
    default: return country ? text_country(scope, country) : "";
    }
}

static const char *queue_custom_notice(PoliticalInstance *instance, const PsArguments *args, unsigned current) {
    const KhanDefinition *type = &instance->type->definition;
    unsigned slot = 0;
    while (slot < type->message_count && strcmp(type->messages[slot].id, args->message)) ++slot;
    if (slot == type->message_count) return "unknown message for this political type";
    const PoliticalMessageDefinition *definition = &type->messages[slot];
    if (!current || !live_holder_tag(current)) return "expected a current country scope";
    const unsigned recipient = args->target ? args->target : current;
    if (!live_holder_tag(recipient)) return "message target country does not exist";
    if (recipient != reform_actor()) return NULL;
    const unsigned sender = args->actor ? args->actor : current;
    if (!live_holder_tag(sender)) return "message sender country does not exist";
    if (!plugin.messages.custom_buttons || !plugin.text_engine.lookup) return "native custom messages are unavailable";
    if (plugin.custom_notice_count == 64) return "pending message queue is full";
    CustomNotice *message = calloc(1, sizeof(*message));
    CrownNotice *notice = calloc(1, sizeof(*notice));
    if (!message || !notice) {
        free(message);
        free(notice);
        return "out of memory queuing a native message";
    }
    message->definition = definition;
    message->sender = sender;
    message->recipient = recipient;
    const char *keys[PS_MESSAGE_BODY_LINES + 5] = {
        definition->title, definition->subtitle, definition->log[0] ? definition->log : definition->body[0],
        definition->close, definition->secondary,
    };
    GameString *strings[PS_MESSAGE_BODY_LINES + 5] = {
        &message->title, &message->subtitle, &message->log, &message->close, &message->secondary,
    };
    for (unsigned i = 0; i < PS_MESSAGE_BODY_LINES; ++i) {
        keys[i + 5] = definition->body[i];
        strings[i + 5] = &message->body[i];
    }
    PoliticalText from = {0}, to = {0};
    const char *sender_name = text_scope_country(&from, sender), *recipient_name = text_scope_country(&to, recipient);
    const TextField fields[] = {
        {"sender", text_value(sender_name)}, {"actor", text_value(sender_name)},
        {"recipient", text_value(recipient_name)}, {"target", text_value(recipient_name)},
        {"message", text_value(definition->id)},
    };
    const PoliticalText context = {.instance = instance, .country = recipient, .fields = fields,
                                   .count = sizeof(fields) / sizeof(*fields)};
    for (unsigned i = 0; i < sizeof(keys) / sizeof(*keys); ++i) {
        strings[i]->capacity = 15;
        if (!keys[i][0] || append_political_text(strings[i], keys[i], context, NULL)) continue;
        char line[650];
        snprintf(line, sizeof(line), "political message %s:%u message=%s localisation=%s missing or could not render",
                 definition->path, definition->line, definition->id, keys[i]);
        log_message(ANVIL_LOG_WARNING, line);
        free_custom_notice(message);
        free(notice);
        return "message localisation is missing or invalid";
    }
    notice->instance = instance;
    notice->custom = message;
    queue_notice(notice);
    ++plugin.custom_notice_count;
    return NULL;
}

static bool describe_operation(PsOperation operation, const PsArguments *args, unsigned current,
                               const char *key, GameString *out) {
    const size_t slot = instance_index(args->instance);
    const PoliticalInstance *instance = slot < plugin.roster.count && plugin.instances[slot].enabled &&
        !ps_selects_by_type(operation, args) ? &plugin.instances[slot] : NULL;
    const unsigned country = args->country && args->country <= 0xffffffu ? args->country : current;
    PoliticalText text = {.instance = instance, .country = country};
    if (args->fields & PS_FIELD(PS_RESOURCE_FIELD)) text.resource = text_resource(&text, args->resource);
    const bool decimal = operation == PS_COMPARE_RESOURCE || operation == PS_SET_RESOURCE ||
                         operation == PS_CHANGE_RESOURCE || operation == PS_COMPARE_CALCULATED;
    const bool has_amount = (args->fields & PS_FIELD(PS_AMOUNT_FIELD)) || operation == PS_COMPARE_CALCULATED;
    const TextValue amount = !has_amount ? (TextValue){0} : decimal ? text_fixed(args->fixed_amount) : text_integer(args->amount);
    static const char *const comparisons[] = {">=", "=", "<=", ">", "<", "!="};
    PoliticalText target = {0}, actor = {0};
    GameString details = {.capacity = 15};
    if (decimal && has_amount) {
        const char *description = operation == PS_COMPARE_RESOURCE || operation == PS_COMPARE_CALCULATED ?
            "PS_AMOUNT_REQUIRED" : operation == PS_CHANGE_RESOURCE ? "PS_AMOUNT_SCRIPT_DELTA" : "PS_AMOUNT_SCRIPT_SET";
        const char *fallback = operation == PS_COMPARE_RESOURCE || operation == PS_COMPARE_CALCULATED ?
            "Requested comparison" : operation == PS_CHANGE_RESOURCE ? "Scripted change" : "Requested balance";
        if (operation == PS_COMPARE_RESOURCE || operation == PS_COMPARE_CALCULATED) {
            append_ui_label(&details, description, fallback);
            char expression[48];
            snprintf(expression, sizeof(expression), ": %s %.*f", comparisons[args->comparison],
                     text.resource ? (int)text.resource->precision : 3, (double)args->fixed_amount / PS_RESOURCE_SCALE);
            plugin.append_string(&details, expression, strlen(expression));
        } else append_amount(&details, description, fallback, args->fixed_amount,
                             text.resource ? text.resource->precision : 3, operation == PS_CHANGE_RESOURCE, "");
    }
    const unsigned recipient = operation == PS_SEND_MESSAGE && !args->target ? (current ? current : PS_COUNTRY_THIS) : args->target;
    const TextField fields[] = {
        {"instance", text_value(instance ? instance->spec->id : args->instance)},
        {"operation", text_value(ps_definitions[operation].name)},
        {"amount", amount}, {"value", amount}, {"breakdown", text_value(string_text(&details))},
        {"comparison", text_value(comparisons[args->comparison])},
        {"requested", text_value(args->value ? "yes" : "no")},
        {"round", text_integer(args->round)}, {"province", text_integer(args->province)},
        {"role", text_value(args->fields & PS_FIELD(PS_ROLE_FIELD) ? args->role : "")},
        {"action", text_value(args->fields & PS_FIELD(PS_ACTION_FIELD) ? args->action : "")},
        {"message", text_value(args->fields & PS_FIELD(PS_MESSAGE_FIELD) ? args->message : "")},
        {"target", text_value(text_scope_country(&target, recipient))},
        {"actor", text_value(text_scope_country(&actor, args->actor ? args->actor : current))},
    };
    text.fields = fields;
    text.count = sizeof(fields) / sizeof(*fields);
    bool rendered = text.resource && text.resource->script_description[0] &&
        append_political_text(out, text.resource->script_description, text, NULL);
    if (!rendered) rendered = append_political_text(out, key, text, NULL);
    plugin.free_string(&details);
    return rendered;
}

static bool prune_resource_accounts(PoliticalInstance *instance) {
    const KhanDefinition *definition = &instance->type->definition;
    bool removed = false;
    for (unsigned i = 0; i < definition->resource_count; ++i) {
        const ResourceDefinition *def = &definition->resources[i];
        ResourceLedger *store = &instance->accounts[i];
        if (def->owner == RESOURCE_INSTANCE || def->retain || !store->count) continue;
        unsigned kept = 0;
        for (unsigned j = 0; j < store->count; ++j) {
            bool active;
            if (resource_owner(instance, def, store->accounts[j].country, &active) || active)
                store->accounts[kept++] = store->accounts[j];
        }
        removed |= kept != store->count;
        store->count = kept;
    }
    return removed;
}

static const PoliticalContestRule *contest_rules(const PoliticalInstance *instance, unsigned slot) {
    const KhanDefinition *d = &instance->type->definition;
    if (slot == PS_MAX_ROLES) return d->succession.mode == LEADER_COMPETITION ? &d->succession.contest : NULL;
    if (slot >= d->role_count || d->roles[slot].acquisition != OFFICE_COMPETITION) return NULL;
    return &d->roles[slot].contest;
}

static const char *contest_qualification(const PoliticalInstance *instance, unsigned slot, unsigned country, bool *eligible) {
    return slot == PS_MAX_ROLES ? leader_eligible(instance, country, true, eligible) :
           role_eligible(instance, &instance->type->definition.roles[slot], country, true, eligible);
}

static const char *contest_office(const PoliticalInstance *instance, unsigned slot) {
    return slot == PS_MAX_ROLES ? "leader" : instance->type->definition.roles[slot].id;
}

static bool select_highest_leader(PoliticalInstance *instance) {
    const KhanDefinition *d = &instance->type->definition;
    const unsigned value = calculation_find(d->calculations, d->calculation_count, d->succession.value);
    if (value == d->calculation_count) return false;
    Shogunate members = {0};
    AnvilGameTag leader = {0};
    election_text(instance->holder, leader.text);
    if (!instance_members(instance, &leader, &members)) return false;
    unsigned winner = 0;
    int32_t best = 0;
    bool valid = true;
    for (uint32_t i = 0; i < members.count; ++i) {
        const unsigned tag = election_tag(members.daimyo[i].text);
        bool eligible;
        if (leader_eligible(instance, tag, true, &eligible)) { valid = false; break; }
        if (!eligible) continue;
        int32_t score;
        if (!read_calculation(instance, value, tag, &score)) { valid = false; break; }
        if (better_candidate(tag, score, winner, best, instance->holder, d->succession.contest.incumbent_ties)) {
            winner = tag;
            best = score;
        }
    }
    release_shogunate(&members);
    if (!valid) return false;
    if (winner != instance->holder && !change_holder(instance, winner)) return false;
    char line[305], tag[4];
    snprintf(line, sizeof(line), "political leader selection instance=%s mode=highest_value winner=%s score_milli=%d",
             instance->spec->id, winner ? election_text(winner, tag) : "none", best);
    log_message(ANVIL_LOG_INFO, line);
    return true;
}

static const char *open_contest(PoliticalInstance *instance, unsigned slot, unsigned actor, unsigned today) {
    const PoliticalContestRule *rule = contest_rules(instance, slot);
    if (!rule) return "office does not use competition";
    if (!actor || actor != instance->holder || !live_holder_tag(actor)) {
        bool eligible;
        if (slot != PS_MAX_ROLES || instance->holder || leader_eligible(instance, actor, true, &eligible) || !eligible)
            return "only the current leader or a candidate for a vacant leadership may open this competition";
    }
    PoliticalContest *contest = &instance->contests[slot];
    if (contest->opened && !contest->settled) return "competition already open";
    const unsigned due = election_after_days(today, rule->period);
    if (due <= today) return "competition deadline unavailable";
    *contest = (PoliticalContest){ .opened = today, .due = due };
    publish_campaign();
    char line[395];
    snprintf(line, sizeof(line), "political contest instance=%s office=%s action=open start=%u due=%u",
             instance->spec->id, contest_office(instance, slot), today, due);
    log_message(ANVIL_LOG_INFO, line);
    return NULL;
}

static const char *invest_contest(PoliticalInstance *instance, unsigned slot, unsigned actor, unsigned today) {
    const PoliticalContestRule *rule = contest_rules(instance, slot);
    if (!rule) return "office does not use competition";
    PoliticalContest *contest = &instance->contests[slot];
    if (!contest->opened || contest->settled || today >= contest->due) return "competition is closed";
    bool eligible;
    const char *error = contest_qualification(instance, slot, actor, &eligible);
    if (error || !eligible) return error ? error : "country does not qualify for the office";
    const ResourceDefinition zero = {0};
    const int32_t previous = resource_balance(&zero, &contest->bids, actor);
    if ((int64_t)previous + rule->cost > INT32_MAX ||
        (!has_account(&contest->bids, actor) && contest->bids.count == PS_MAX_EXPLICIT_MEMBERS))
        return "competition score capacity exceeded";
    const KhanDefinition *d = &instance->type->definition;
    const unsigned index = resource_find(d->resources, d->resource_count, rule->investment);
    if (rule->cost) {
        if (index == d->resource_count) return "investment resource unavailable";
        const ResourceDefinition *resource = &d->resources[index];
        ResourceLedger *ledger = &instance->accounts[index];
        bool owns;
        error = resource_owner(instance, resource, actor, &owns);
        if (error || !owns) return error ? error : "candidate has no investment points";
        const int32_t balance = resource_balance(resource, ledger, actor);
        if ((int64_t)balance - rule->cost < resource->minimum) return "not enough investment points";
        if (!store_balance(ledger, actor, balance - rule->cost)) return "resource account capacity exhausted";
        resource_crossing(instance, resource, actor, balance, balance - rule->cost);
    }
    store_balance(&contest->bids, actor, previous + rule->cost);
    publish_campaign();
    char line[465], tag[4];
    snprintf(line, sizeof(line), "political contest instance=%s office=%s action=invest country=%s paid_milli=%d total_milli=%d",
             instance->spec->id, contest_office(instance, slot), election_text(actor, tag), rule->cost, previous + rule->cost);
    log_message(ANVIL_LOG_INFO, line);
    return NULL;
}

static void settle_contest(PoliticalInstance *instance, unsigned slot, unsigned today) {
    PoliticalContest *contest = &instance->contests[slot];
    if (!contest->opened || contest->settled || today < contest->due) return;
    const PoliticalContestRule *rule = contest_rules(instance, slot);
    if (!rule) return;
    const KhanDefinition *d = &instance->type->definition;
    const bool uses_bids = !strcmp(rule->score, "invested");
    const unsigned score_slot = calculation_find(d->calculations, d->calculation_count, rule->score);
    const unsigned incumbent = slot == PS_MAX_ROLES ? instance->holder :
         instance->roles[slot].count ? instance->roles[slot].countries[0] : 0;
    unsigned winner = 0;
    int32_t best = 0;
    for (unsigned i = 0; i < contest->bids.count; ++i) {
        const ResourceAccount bid = contest->bids.accounts[i];
        bool eligible;
        if (contest_qualification(instance, slot, bid.country, &eligible)) return;
        if (!eligible) continue;
        int32_t score = bid.balance;
        if (!uses_bids && (score_slot == d->calculation_count || !read_calculation(instance, score_slot, bid.country, &score))) return;
        if (better_candidate(bid.country, score, winner, best, incumbent, rule->incumbent_ties)) {
            winner = bid.country;
            best = score;
        }
    }
    contest->settled = true;
    contest->winner = winner;
    if (slot == PS_MAX_ROLES) {
        if (winner != incumbent && !change_holder(instance, winner)) {
            contest->settled = false;
            contest->winner = 0;
            return;
        }
    } else instance->roles[slot] = (RoleHolders){ .count = winner != 0, .countries = {winner} };
    publish_campaign();
    char line[442], tag[4];
    snprintf(line, sizeof(line), "political contest instance=%s office=%s action=settle winner=%s date=%u score_milli=%d",
             instance->spec->id, contest_office(instance, slot), winner ? election_text(winner, tag) : "none", today, best);
    log_message(ANVIL_LOG_INFO, line);
    plugin.next_update = 0;
}

static const char *submit_selection(size_t index, unsigned office, unsigned actor, unsigned target, PsOperation op) {
    if (!actor) return "expected a country scope";
    if (selection_requests.count == sizeof(selection_requests.items) / sizeof(*selection_requests.items)) return "selection queue is full";
    selection_requests.items[selection_requests.count++] = (SelectionRequest){index, office, actor, target, op};
    return NULL;
}

static void drain_selections(unsigned today) {
    SelectionRequest requests[sizeof(selection_requests.items) / sizeof(*selection_requests.items)];
    const unsigned count = selection_requests.count;
    memcpy(requests, selection_requests.items, count * sizeof(*requests));
    selection_requests.count = 0;
    for (unsigned i = 0; i < count; ++i) {
        const SelectionRequest *req = &requests[i];
        PoliticalInstance *instance = &plugin.instances[req->instance];
        if (!instance->enabled) continue;
        const LeaderSelection *rule = &instance->type->definition.succession;
        const char *error = NULL;
        if (req->operation == PS_START_CONTEST) error = open_contest(instance, req->office, req->actor, today);
        else if (req->operation == PS_INVEST_CONTEST) error = invest_contest(instance, req->office, req->actor, today);
        else if (req->operation == PS_SET_LEADER) {
            bool allowed;
            if ((rule->mode != LEADER_EXPLICIT && rule->mode != LEADER_APPOINTED) ||
                (rule->mode == LEADER_APPOINTED && req->actor != instance->holder)) error = "this leader uses a different selection method";
            else if (leader_eligible(instance, req->target, true, &allowed) || !allowed || !change_holder(instance, req->target))
                error = "country does not qualify as leader";
        } else {
            bool member = false;
            if (!instance->holder) instance_has_member(instance, req->actor, &member);
            if (rule->mode != LEADER_HIGHEST || rule->event != SELECT_EXPLICIT ||
                (req->actor != instance->holder && !member)) error = "no explicit ranked succession is available";
            else if (!select_highest_leader(instance)) error = "leader ranking is unavailable";
        }
        if (error) {
            char line[352], actor[4];
            snprintf(line, sizeof(line), "political selection instance=%s actor=%s op=%s result=%s", instance->spec->id,
                     election_text(req->actor, actor), ps_definitions[req->operation].name, error);
            log_message(ANVIL_LOG_INFO, line);
        }
    }
    if (count) plugin.next_update = 0;
}

static void resource_crossing(PoliticalInstance *instance, const ResourceDefinition *def,
                              unsigned country, int32_t before, int32_t after) {
    const unsigned crossed = crossed_thresholds(def, before, after);
    for (unsigned i = 0; i < def->threshold_count; ++i) {
        if (!(crossed & (1u << i))) continue;
        const ResourceThreshold *threshold = &def->thresholds[i];
        const LeaderSelection *selection = &instance->type->definition.succession;
        if (selection->defined && selection->event == SELECT_THRESHOLD && !country &&
            !strcmp(selection->threshold_resource, def->id) && !strcmp(selection->threshold, threshold->id))
            instance->threshold_succession_pending = true;
        char line[560], tag[4];
        snprintf(line, sizeof(line), "political resource_threshold instance=%s resource=%s threshold=%s country=%s from_milli=%d to_milli=%d",
                 instance->spec->id, def->id, threshold->id, country ? election_text(country, tag) : "instance", before, after);
        log_message(ANVIL_LOG_INFO, line);
        CrownNotice *notice = calloc(1, sizeof(*notice));
        if (!notice) continue;
        notice->instance = instance;
        notice->resource = def;
        notice->threshold = threshold;
        notice->winner = country;
        notice->threshold_value = after;
        notice->previous_value = before;
        queue_notice(notice);
    }
}

static bool resource_growth(const PoliticalInstance *instance, const ResourceDefinition *def, unsigned country,
                            int64_t *amount, bool report, ResourceGrowth *trace) {
    *amount = def->monthly;
    int64_t growth_factor = PS_RESOURCE_SCALE;
    if (trace) *trace = (ResourceGrowth){0};
    const KhanDefinition *type = &instance->type->definition;
    for (unsigned i = 0; i < def->term_count; ++i) {
        const ResourceTerm *term = &def->terms[i];
        const unsigned actor = term->leader_scope ? instance->holder : country;
        bool applies = true;
        if (term->political_test == PS_CONTENT_REFORM) {
            applies = passed_action(instance, term->operand) == term->expected;
        } else if (term->political_test == PS_HAS_ROLE) {
            const unsigned role = role_index(instance, term->operand);
            if (role == type->role_count) return false;
            applies = actor && (holds_role(instance, role, actor) == term->expected);
        } else if (term->test == ENTRY_QUERY) {
            const unsigned slot = calculation_find(type->calculations, type->calculation_count, term->calculation);
            int32_t value;
            if (slot == type->calculation_count) return false;
            applies = read_calculation(instance, slot, actor, &value) && ps_compare(term->comparison, value, term->compare_to);
        } else if (term->test == ENTRY_COUNTRY_FLAG) {
            applies = false;
            if (actor && country_has_flag(actor, term->operand, &applies)) return false;
        } else if (term->test == ENTRY_NATIVE) {
            applies = false;
            if (actor) {
                const unsigned leader = live_holder_tag(instance->holder) ? instance->holder : 0;
                const NativeScope scope = {.root = actor, .from = leader,
                                          .instance = instance->spec->id};
                if (test_native_condition(&term->native, &scope, &applies)) return false;
            }
        } else if (term->test == ENTRY_TAG) applies = actor == election_tag(term->operand);
        else if (term->test == ENTRY_ALWAYS) applies = !strcmp(term->operand, "yes");
        if (applies && (!term->leader_scope || actor)) {
            *amount += term->add;
            growth_factor += term->modifier;
            if (trace) trace->terms |= 1u << i;
        }
    }
    if (def->growth_modifier[0]) {
        const NativeModifier *modifier = find_native_modifier(&plugin.modifiers, def->growth_modifier);
        if (!modifier || modifier->id < 0) return false;
        const bool use_account = def->modifier_source == GROWTH_ACCOUNT || def->modifier_source == GROWTH_ACCOUNT_CAPITAL;
        const unsigned source = use_account ? country : instance->holder;
        int32_t native_amount = 0;
        if (source) {
            AnvilResult result;
            if (growth_modifier_is_local(def)) {
                int32_t capital;
                if (!current_capital(source, &capital) || capital <= 0) return false;
                result = plugin.world->province_modifier(capital, modifier->id, &native_amount);
            } else {
                char tag[4];
                result = plugin.world->country_modifier(election_text(source, tag), modifier->id, &native_amount);
            }
            if (result != ANVIL_OK) return false;
        }
        growth_factor += native_amount;
        if (trace) { trace->native_country = source; trace->native_modifier = native_amount; }
        if (report) {
            char line[520], tag[4];
            snprintf(line, sizeof(line), "political growth modifier instance=%s resource=%s key=%s country=%.3s native_milli=%d",
                     instance->spec->id, def->id, modifier->key, source ? election_text(source, tag) : "-", native_amount);
            log_message(ANVIL_LOG_INFO, line);
        }
    }
    *amount = growth_factor > 0 ? (*amount / PS_RESOURCE_SCALE) * growth_factor +
              (*amount % PS_RESOURCE_SCALE) * growth_factor / PS_RESOURCE_SCALE : 0;
    if (trace) { trace->factor = growth_factor > 0 ? growth_factor : 0; trace->uncapped = *amount; }
    return true;
}

static bool grow_account(const PoliticalInstance *instance, const ResourceDefinition *def, ResourceLedger *ledger, unsigned country) {
    int64_t growth;
    if (!resource_growth(instance, def, country, &growth, true, NULL)) return false;
    const int32_t before = resource_balance(def, ledger, country);
    const int32_t after = resource_limit(def, before + growth);
    return before == after || store_balance(ledger, country, after);
}

static bool settle_resource_month(PoliticalInstance *instance, const Shogunate *members, unsigned month) {
    const KhanDefinition *type = &instance->type->definition;
    ResourceLedger next[PS_MAX_RESOURCES] = {0};
    memcpy(next, instance->accounts, type->resource_count * sizeof(*next));
    for (unsigned i = 0; i < type->resource_count; ++i) {
        const ResourceDefinition *def = &type->resources[i];
        if (def->owner == RESOURCE_MEMBER) {
            for (uint32_t j = 0; j < members->count; ++j)
                if (!grow_account(instance, def, &next[i], election_tag(members->daimyo[j].text))) return false;
        } else if (def->owner == RESOURCE_INSTANCE || instance->holder) {
            const unsigned country = def->owner == RESOURCE_LEADER ? instance->holder : 0;
            if (!grow_account(instance, def, &next[i], country)) return false;
        }
    }
    instance->resource_month = month;
    for (unsigned i = 0; i < type->resource_count; ++i) {
        const ResourceDefinition *def = &type->resources[i];
        for (unsigned j = 0; j < next[i].count; ++j) {
            const ResourceAccount *account = &next[i].accounts[j];
            resource_crossing(instance, def, account->country,
                              resource_balance(def, &instance->accounts[i], account->country), account->balance);
        }
        instance->accounts[i] = next[i];
    }
    char line[240];
    snprintf(line, sizeof(line), "political resource_month instance=%s month=%u result=applied", instance->spec->id, month);
    log_message(ANVIL_LOG_INFO, line);
    return true;
}

static void refresh_resource_world(unsigned day) {
    const bool sample = day != resource_fact_day;
    resource_fact_day = day;
    bool dirty = false;
    for (size_t i = 0; i < plugin.roster.count; ++i) {
        PoliticalInstance *instance = &plugin.instances[i];
        if (!instance->enabled) continue;
        const KhanDefinition *type = &instance->type->definition;
        if (!type->calculation_count && !type->resource_count && !type->territory_enabled) continue;
        const unsigned month = day / 100;
        const bool monthly = type->resource_count && month > instance->resource_month;
        if (!sample && !monthly) continue;
        instance->computed_values = 0;
        Shogunate members = {0};
        AnvilGameTag leader = {0};
        if (instance->holder) election_text(instance->holder, leader.text);
        bool need_members = type->calculation_count || type->territory_enabled;
        for (unsigned j = 0; j < type->resource_count; ++j) need_members |= type->resources[j].owner == RESOURCE_MEMBER;
        if (need_members && !instance_members(instance, &leader, &members)) continue;
        CountryFact data;
        if (type->calculation_count || type->territory_enabled) {
            country_fact(instance->holder, &data);
            for (uint32_t j = 0; j < members.count; ++j) country_fact(election_tag(members.daimyo[j].text), &data);
        }
        for (unsigned j = 0; j < type->calculation_count; ++j) {
            if (type->calculations[j].kind == VALUE_MEMBER_COUNT &&
                count_by_strength(instance, &type->calculations[j], &members, &instance->computed[j]))
                instance->computed_values |= 1u << j;
        }
        dirty |= prune_resource_accounts(instance);
        if (monthly) dirty |= settle_resource_month(instance, &members, month);
        release_shogunate(&members);
    }
    if (dirty) publish_campaign();
}

static const char *resolve_type_instance(const char *id, char out[GK_SCRIPT_WORD_SIZE]) {
    if (!political_campaign_active()) return "no active campaign";
    const PoliticalType *type = known_type(id);
    if (!type) return "unknown political type";
    const PoliticalInstanceSpec *match = NULL;
    for (size_t i = 0; i < plugin.roster.count; ++i) {
        if (!plugin.instances[i].enabled || plugin.instances[i].type != type) continue;
        if (match) return "political type has multiple active instances; specify instance";
        match = plugin.instances[i].spec;
    }
    if (!match) return "political type has no active instance";
    strcpy(out, match->id);
    return NULL;
}

static PsQueryResult query_political_state(const PsArguments *args, PsQueryState *out) {
    const size_t index = instance_index(args->instance);
    if (index == plugin.roster.count) return PS_QUERY_UNKNOWN_INSTANCE;
    const PoliticalInstance *instance = &plugin.instances[index];
    if (!political_campaign_active() || !instance->enabled) return PS_QUERY_INACTIVE;
    const bool asks_reform = (args->fields & PS_FIELD(PS_REFORM_FIELD)) != 0;
    if (asks_reform && strcmp(args->reform, instance->type->reform.id)) return PS_QUERY_UNKNOWN_REFORM;
    const unsigned holder = instance->holder;
    *out = (PsQueryState){ .leader = live_holder_tag(holder) ? holder : 0, .authority = instance->reform.authority,
                           .passed = asks_reform && !strcmp(args->reform, instance->reform.passed_id) };
    return PS_QUERY_READY;
}

static const char *query_resource(PsOperation op, const PsArguments *args, unsigned actor, bool *result) {
    const size_t index = instance_index(args->instance);
    if (index >= plugin.roster.count || !plugin.instances[index].enabled) return "unknown or disabled resource instance";
    const PoliticalInstance *instance = &plugin.instances[index];
    const KhanDefinition *d = &instance->type->definition;
    int32_t balance;
    if (op == PS_COMPARE_CALCULATED) {
        const unsigned slot = calculation_find(d->calculations, d->calculation_count, args->resource);
        if (slot == d->calculation_count) return "unknown calculated value";
        const bool national = d->calculations[slot].kind == VALUE_MANPOWER;
        if (!national && (args->fields & PS_FIELD(PS_COUNTRY_FIELD))) return "instance calculations do not take a country";
        if (!read_calculation(instance, slot, national ? (args->country ? args->country : actor) : 0, &balance))
            return "calculated value unavailable in the current scope";
    } else {
        const unsigned slot = resource_find(d->resources, d->resource_count, args->resource);
        if (slot == d->resource_count) {
            if (d->script_type || !d->authority_enabled || strcmp(args->resource, PS_AUTHORITY))
                return "resource not enabled for this instance";
            if (args->fields & PS_FIELD(PS_COUNTRY_FIELD)) return "instance accounts do not take a country";
            balance = instance->reform.authority * PS_RESOURCE_SCALE;
        } else {
            const ResourceDefinition *def = &d->resources[slot];
            if (def->owner == RESOURCE_INSTANCE && (args->fields & PS_FIELD(PS_COUNTRY_FIELD))) return "instance accounts do not take a country";
            const unsigned country = def->owner == RESOURCE_INSTANCE ? 0 : args->country ? args->country : actor;
            bool active;
            const char *error = resource_owner(instance, def, country, &active);
            if (error || !active) return error ? error : "country does not hold this resource account";
            balance = resource_balance(def, &instance->accounts[slot], country);
        }
    }
    *result = ps_compare(args->comparison, balance, args->fixed_amount);
    return NULL;
}

static const char *change_resource(PoliticalInstance *instance, PsOperation op, const PsArguments *args, unsigned actor) {
    const KhanDefinition *d = &instance->type->definition;
    const unsigned slot = resource_find(d->resources, d->resource_count, args->resource);
    if (slot == d->resource_count) {
        if (d->script_type || !d->authority_enabled || strcmp(args->resource, PS_AUTHORITY) ||
            (args->fields & PS_FIELD(PS_COUNTRY_FIELD))) return "resource not enabled for this instance";
        if (args->fixed_amount % PS_RESOURCE_SCALE) return "legacy authority uses whole numbers";
        int amount = args->amount;
        if (op == PS_CHANGE_RESOURCE) amount += instance->reform.authority;
        if (amount < 0 || amount > GK_MAX_AUTHORITY) return "resulting authority outside 0..100";
        instance->reform.authority = amount;
    } else {
        const ResourceDefinition *def = &d->resources[slot];
        if (def->owner == RESOURCE_INSTANCE && (args->fields & PS_FIELD(PS_COUNTRY_FIELD))) return "instance accounts do not take a country";
        const unsigned country = def->owner == RESOURCE_INSTANCE ? 0 : args->country ? args->country : actor;
        bool active;
        const char *error = resource_owner(instance, def, country, &active);
        if (error || !active) return error ? error : "country does not hold this resource account";
        ResourceLedger *ledger = &instance->accounts[slot];
        const int32_t before = resource_balance(def, ledger, country);
        const int64_t amount = (int64_t)args->fixed_amount + (op == PS_CHANGE_RESOURCE ? before : 0);
        if (op == PS_SET_RESOURCE && (amount < def->minimum || amount > def->maximum)) return "resource amount outside its bounds";
        const int32_t after = resource_limit(def, amount);
        if (before == after) return NULL;
        if (!store_balance(ledger, country, after)) return "country account capacity exhausted";
        resource_crossing(instance, def, country, before, after);
    }
    publish_campaign();
    return NULL;
}

typedef struct ActionPlan {
    unsigned today, expires, cooldown_until, payer, cost_slot;
    int32_t before, after;
    const PoliticalActionRun *existing;
    void *effect_country;
} ActionPlan;

typedef struct ActionRecipients {
    unsigned single;
    Shogunate members;
} ActionRecipients;

static bool action_recipients(const PoliticalInstance *instance, const PoliticalAction *action,
                              unsigned actor, unsigned target, ActionRecipients *out) {
    *out = (ActionRecipients){0};
    if (action->beneficiaries == ACTION_BENEFIT_MEMBERS) {
        AnvilGameTag leader = {0};
        election_text(instance->holder, leader.text);
        return instance_members(instance, &leader, &out->members);
    }
    const unsigned country = action->beneficiaries == ACTION_BENEFIT_LEADER ? instance->holder :
                             action->beneficiaries == ACTION_BENEFIT_TARGET ? target : actor;
    if (!live_holder_tag(country)) return false;
    out->single = country;
    return true;
}

static unsigned action_recipient_count(const ActionRecipients *recipients) {
    return recipients->single ? 1 : recipients->members.count;
}

static unsigned action_recipient(const ActionRecipients *recipients, unsigned i) {
    return recipients->single ? recipients->single : election_tag(recipients->members.daimyo[i].text);
}

static bool collect_passive_recipients(NativePassives *state, const ActionRecipients *recipients,
                                       const void *modifier) {
    if (!modifier) return true;
    for (unsigned i = 0; i < action_recipient_count(recipients); ++i) {
        const uint64_t tag = live_holder_tag(action_recipient(recipients, i));
        if (!append_native_passive(state, false, tag, modifier)) return false;
    }
    return true;
}

static bool collect_instance_passives(NativePassives *state, const PoliticalInstance *instance, unsigned today) {
    const KhanDefinition *definition = &instance->type->definition;
    for (unsigned i = 0; i < instance->actions.count; ++i) {
        const PoliticalActionRun *run = &instance->actions.runs[i];
        const PoliticalAction *action = &definition->actions[run->slot];
        if (!political_action_running(action, run, today)) continue;
        const void *modifier = native_eu4_modifier(&action->modifiers[ACTION_MODIFIER_COUNTRY]);
        const void *member_modifier = native_eu4_modifier(&action->modifiers[ACTION_MODIFIER_MEMBER]);
        const void *leader_modifier = native_eu4_modifier(&action->modifiers[ACTION_MODIFIER_LEADER]);
        const void *province_modifier = native_eu4_modifier(&action->modifiers[ACTION_MODIFIER_PROVINCE]);
        if (leader_modifier) {
            if (!append_native_passive(state, false, live_holder_tag(instance->holder), leader_modifier)) return false;
        }
        if (member_modifier || (modifier && action->beneficiaries == ACTION_BENEFIT_MEMBERS)) {
            AnvilGameTag leader = {0};
            election_text(instance->holder, leader.text);
            ActionRecipients recipients = {0};
            if (!instance_members(instance, &leader, &recipients.members)) return false;
            bool collected = collect_passive_recipients(state, &recipients, member_modifier);
            if (collected && action->beneficiaries == ACTION_BENEFIT_MEMBERS)
                collected = collect_passive_recipients(state, &recipients, modifier);
            release_shogunate(&recipients.members);
            if (!collected) return false;
        }
        if (modifier && action->beneficiaries != ACTION_BENEFIT_MEMBERS) {
            ActionRecipients recipients = {0};
            if (action_recipients(instance, action, run->actor, run->target, &recipients) &&
                !collect_passive_recipients(state, &recipients, modifier)) return false;
        }
        if (province_modifier)
            for (unsigned j = 0; j < instance->territory.count; ++j)
                if (!append_native_passive(state, true, instance->territory.ids[j], province_modifier)) return false;
    }
    return true;
}

static void refresh_changed_modifier(bool province, uintptr_t recipient) {
    if (!plugin.root || plugin.root != *plugin.root_slot) return;
    if (province) {
        if (!refresh_province_modifiers || recipient >= (unsigned)plugin.province_limit) return;
        unsigned char *array = pointer_at(plugin.root, SYMVAL_PROVINCE_DISCOVERY_ARRAY_ARRAY);
        if (array) refresh_province_modifiers(array + recipient * SYMVAL_PROVINCE_DISCOVERY_ARRAY_STRIDE);
    } else if (refresh_country_modifiers) {
        void *country = country_from_tag(recipient);
        if (country) refresh_country_modifiers(country, "Political Framework", false);
    }
}

static void sync_native_passives(unsigned today) {
    if (!have_native_passives || !today || (!native_passives_dirty && native_passives_day == today)) return;
    NativePassives next = {0};
    native_passives_day = today;
    native_passives_dirty = false;
    for (size_t i = 0; i < plugin.roster.count; ++i) {
        const PoliticalInstance *instance = &plugin.instances[i];
        if (instance->enabled && instance->type->native_passives &&
            !collect_instance_passives(&next, instance, today)) {
            discard_native_passives(&next);
            log_message(ANVIL_LOG_WARNING, "political passive modifiers not refreshed: recipient snapshot unavailable");
            return;
        }
    }
    replace_native_passives(next, refresh_changed_modifier);
}

static void run_action_clause(const PoliticalInstance *instance, const NativeClause *clause, const ActionRecipients *recipients,
                              unsigned actor, unsigned target) {
    for (unsigned i = 0; i < action_recipient_count(recipients); ++i) {
        const unsigned root = action_recipient(recipients, i);
        const unsigned from = root == actor && target ? target : actor;
        run_native_country(instance, clause, root, live_holder_tag(from) ? from : 0);
    }
}

static bool action_country_condition(const ResourceTerm *condition, unsigned actor) {
    if (condition->test == ENTRY_TAG) return actor == election_tag(condition->operand);
    if (condition->test == ENTRY_ALWAYS) return !strcmp(condition->operand, "yes");
    if (condition->test != ENTRY_COUNTRY_FLAG) return true;
    bool flag;
    return !country_has_flag(actor, condition->operand, &flag) && flag;
}

static bool passed_action(const PoliticalInstance *instance, const char *id) {
    if (!strcmp(instance->reform.passed_id, id)) return true;
    const KhanDefinition *d = &instance->type->definition;
    const unsigned slot = political_action_index(d->actions, d->action_count, id);
    return slot < d->action_count && d->actions[slot].kind == ACTION_REFORM && find_action_run(&instance->actions, slot, 0);
}

static const char *reform_links(const PoliticalInstance *instance, const PoliticalAction *reform, bool enact) {
    if (enact) {
        for (unsigned i = 0; i < reform->require_count; ++i)
            if (!passed_action(instance, reform->requires[i])) return "PS_ACTION_PREREQUISITE";
        for (unsigned i = 0; i < reform->exclude_count; ++i)
            if (passed_action(instance, reform->excludes[i])) return "PS_ACTION_CONFLICT";
    }
    const KhanDefinition *d = &instance->type->definition;
    for (unsigned i = 0; i < instance->actions.count; ++i) {
        const PoliticalAction *other = &d->actions[instance->actions.runs[i].slot];
        if (other->kind != ACTION_REFORM) continue;
        const unsigned count = enact ? other->exclude_count : other->require_count;
        for (unsigned j = 0; j < count; ++j) {
            const char *id = enact ? other->excludes[j] : other->requires[j];
            if (!strcmp(id, reform->id)) return enact ? "PS_ACTION_CONFLICT" : "revoke dependent reform first";
        }
    }
    return NULL;
}

static const char *set_reform_state(PoliticalInstance *instance, const PsArguments *args, unsigned actor) {
    const KhanDefinition *d = &instance->type->definition;
    const unsigned slot = political_action_index(d->actions, d->action_count, args->reform);
    if (slot == d->action_count) {
        if (!instance->type->reform.id[0] || strcmp(instance->type->reform.id, args->reform)) return "unknown reform";
        if (args->target) return "this reform does not take a target";
        strcpy(instance->reform.passed_id, args->value ? args->reform : "");
    } else {
        const PoliticalAction *action = &d->actions[slot];
        if (action->kind != ACTION_REFORM) return "expected a reform ID";
        PoliticalActionState *state = &instance->actions;
        const PoliticalActionRun *run = find_action_run(state, slot, 0);
        if (!args->value && args->target) return "revocation selects the reform, without a target";
        if (args->value && (action->country_target != (args->target != 0) ||
            (args->target && !live_holder_tag(args->target)))) return "expected the reform's configured target";
        if (args->value == (run != NULL)) return NULL;
        const char *reason = reform_links(instance, action, args->value);
        if (reason) return reason;
        if (args->value) {
            unsigned today;
            if (!live_holder_tag(actor) || !campaign_date(&today)) return "reform grant requires a live country scope";
            if (state->count == PS_MAX_ACTION_RUNS) return "reform record capacity exhausted";
            state->runs[state->count++] = (PoliticalActionRun){ .slot = slot, .actor = actor, .target = args->target,
                                                               .performed = today };
        } else {
            const PoliticalActionRun former = *run;
            ActionRecipients recipients = {0};
            if (action->off_effect.program)
                action_recipients(instance, action, former.actor, former.target, &recipients);
            const size_t offset = (size_t)(run - state->runs);
            --state->count;
            memmove(&state->runs[offset], &state->runs[offset + 1], (state->count - offset) * sizeof(*run));
            if (action->off_effect.program) {
                publish_campaign();
                run_action_clause(instance, &action->off_effect, &recipients, former.actor, former.target);
            }
            release_shogunate(&recipients.members);
        }
    }
    publish_campaign();
    plugin.next_update = 0;
    return NULL;
}

static const PoliticalActionRun *decree_in_force(const PoliticalInstance *instance, unsigned slot, unsigned today) {
    const PoliticalAction *actions = instance->type->definition.actions;
    const PoliticalAction *action = &actions[slot];
    if (action->kind != ACTION_DECREE) return NULL;
    const PoliticalActionRun *group_member = NULL;
    for (unsigned i = 0; i < instance->actions.count; ++i) {
        const PoliticalActionRun *run = &instance->actions.runs[i];
        const PoliticalAction *other = &actions[run->slot];
        if (other->kind != ACTION_DECREE || !political_action_running(other, run, today)) continue;
        if (run->slot == slot) return run;
        if (action->exclusive_group[0] && !strcmp(other->exclusive_group, action->exclusive_group))
            group_member = run;
    }
    return group_member;
}

static const char *action_scope(const PoliticalInstance *instance, const PoliticalAction *a, unsigned actor, unsigned target) {
    if (!live_holder_tag(actor)) return "PS_ACTION_INVALID_ACTOR";
    if (a->actor_rule == ACTION_LEADER && actor != instance->holder) return "PS_ACTION_LEADER_ONLY";
    if (a->actor_rule == ACTION_PERMISSION && !has_political_permission(instance, actor, a->permission)) return "PS_ACTION_PERMISSION";
    if (a->actor_rule == ACTION_MEMBER) {
        bool member;
        if (instance_has_member(instance, actor, &member) || !member) return "PS_ACTION_MEMBER_ONLY";
    }
    if (a->country_target != (target != 0) || (target && (!live_holder_tag(target) ||
        !action_country_condition(&a->target_trigger, target) ||
        !native_country_allows(instance, &a->native_target, target, actor)))) return "PS_ACTION_INVALID_TARGET";
    if (!action_country_condition(&a->actor_trigger, actor) ||
        !native_country_allows(instance, &a->potential, actor, target) ||
        !native_country_allows(instance, &a->trigger, actor, target) ||
        !native_country_allows(instance, &a->native_actor, actor, target)) return "PS_ACTION_CONDITION";
    return NULL;
}

static const char *invitation_target(const PoliticalInstance *instance, unsigned issuer, unsigned target) {
    if (issuer == target) return "PS_ACTION_INVALID_TARGET";
    bool member;
    const char *reason = instance_has_member(instance, target, &member);
    if (reason) return "PS_REQUEST_MEMBER_CONDITION";
    if (member) return "PS_REQUEST_ALREADY_MEMBER";
    bool changes;
    reason = check_member_change(instance, target, true, &changes);
    return reason || !changes ? "PS_REQUEST_MEMBER_CONDITION" : NULL;
}

static const char *action_payment(const PoliticalInstance *instance, const PoliticalAction *a,
                                  unsigned actor, unsigned target, ActionPlan *plan) {
    const KhanDefinition *d = &instance->type->definition;
    if (!a->resource[0]) return NULL;
    const unsigned cost_slot = resource_find(d->resources, d->resource_count, a->resource);
    if (cost_slot == d->resource_count) return "PS_ACTION_PAYMENT_UNAVAILABLE";
    const ResourceDefinition *def = &d->resources[cost_slot];
    const unsigned payer = a->payer == ACTION_ACTOR_PAYS ? actor : a->payer == ACTION_TARGET_PAYS ? target : 0;
    bool active;
    if (resource_owner(instance, def, payer, &active) || !active) return "PS_ACTION_PAYMENT_UNAVAILABLE";
    const ResourceLedger *ledger = &instance->accounts[cost_slot];
    plan->before = resource_balance(def, ledger, payer);
    if ((int64_t)plan->before - a->cost < def->minimum) return "PS_ACTION_LOW_BALANCE";
    if (a->cost && ledger->count == PS_MAX_EXPLICIT_MEMBERS && !has_account(ledger, payer)) return "PS_ACTION_CAPACITY";
    plan->after = plan->before - a->cost;
    plan->cost_slot = cost_slot;
    plan->payer = payer;
    return NULL;
}

static const char *prepare_action(const PoliticalInstance *instance, unsigned slot, unsigned actor,
                                 unsigned target, unsigned today, ActionPlan *plan) {
    const KhanDefinition *d = &instance->type->definition;
    *plan = (ActionPlan){ .today = today, .cost_slot = d->resource_count };
    if (!instance->enabled || slot >= d->action_count || !today) return "PS_ACTION_UNAVAILABLE";
    const PoliticalAction *a = &d->actions[slot];
    const char *reason = action_scope(instance, a, actor, target);
    if (reason) return reason;
    if (a->kind >= ACTION_INVITATION) {
        const PoliticalProcesses *state = &instance->processes;
        const unsigned index = political_process_index(state, slot, target);
        if (index < state->count) {
            const PoliticalProcess *process = &state->items[index];
            if (process->status == PROCESS_PENDING) return "PS_PROCESS_PENDING";
            if (a->kind == ACTION_INCIDENT) return "PS_ACTION_ALREADY_DONE";
            if (process->round == INT_MAX) return "PS_ACTION_CAPACITY";
        } else if (state->count == PS_MAX_PROCESSES) return "PS_ACTION_CAPACITY";
        if (a->kind == ACTION_INVITATION && (reason = invitation_target(instance, actor, target))) return reason;
        plan->expires = election_after_days(today, a->duration);
        if (!plan->expires) return "PS_ACTION_UNAVAILABLE";
        return action_payment(instance, a, actor, target, plan);
    }
    plan->existing = find_action_run(&instance->actions, slot, a->kind == ACTION_REFORM ? 0 : actor);
    if (plan->existing && (a->kind == ACTION_REFORM || (a->kind == ACTION_DECISION && !a->repeatable)))
        return "PS_ACTION_ALREADY_DONE";
    if (a->kind == ACTION_REFORM && (reason = reform_links(instance, a, true))) return reason;
    if (decree_in_force(instance, slot, today)) return "PS_ACTION_IN_FORCE";
    if (plan->existing && plan->existing->cooldown_until > today) return "PS_ACTION_COOLDOWN";
    if (!plan->existing && instance->actions.count == PS_MAX_ACTION_RUNS) return "PS_ACTION_CAPACITY";
    if (a->kind == ACTION_DECREE && !(plan->expires = election_after_days(today, a->duration))) return "PS_ACTION_UNAVAILABLE";
    if (a->cooldown && !(plan->cooldown_until = election_after_days(today, a->cooldown))) return "PS_ACTION_UNAVAILABLE";
    if (a->prestige) {
        plan->effect_country = country_from_tag(live_holder_tag(actor));
        if (!plan->effect_country || !plugin.add_prestige) return "PS_ACTION_EFFECT_UNAVAILABLE";
    }
    if (native_clause_present(&a->effect) && !a->effect.program) return "PS_ACTION_EFFECT_UNAVAILABLE";
    return action_payment(instance, a, actor, target, plan);
}

static const char *enqueue_action(ActionRequest request) {
    for (unsigned i = 0; i < queued_actions.count; ++i) {
        const ActionRequest *queued = &queued_actions.items[i];
        if (queued->instance == request.instance && queued->slot == request.slot &&
            queued->actor == request.actor && queued->target == request.target && queued->step == request.step &&
            queued->round == request.round && queued->option == request.option) return NULL;
    }
    if (queued_actions.count == sizeof(queued_actions.items) / sizeof(*queued_actions.items)) return "action queue is full";
    queued_actions.items[queued_actions.count++] = request;
    return NULL;
}

static bool debit_action(PoliticalInstance *instance, const PoliticalAction *action, const ActionPlan *plan) {
    if (!action->cost) return true;
    if (!store_balance(&instance->accounts[plan->cost_slot], plan->payer, plan->after)) return false;
    resource_crossing(instance, &instance->type->definition.resources[plan->cost_slot], plan->payer, plan->before, plan->after);
    return true;
}

static void report_process(const PoliticalInstance *instance, const PoliticalProcess *process,
                            const char *source, const char *reason) {
    const PoliticalAction *action = &instance->type->definition.actions[process->slot];
    char message[680], issuer[4], receiver[4];
    snprintf(message, sizeof(message), "political process instance=%s id=%s round=%u issuer=%s target=%s status=%s "
        "opened=%u deadline=%u resolved=%u option=%s source=%s reason=%s", instance->spec->id, action->id,
        process->round, election_text(process->issuer, issuer), process->target ? election_text(process->target, receiver) : "none",
        process_status_name(process->status), process->opened, process->deadline, process->resolved,
        process->option[0] ? process->option : "none", source, reason ? reason : "none");
    log_message(ANVIL_LOG_INFO, message);
}

static void conclude_process(PoliticalInstance *instance, PoliticalProcess *process, ProcessStatus status,
                            unsigned today, const char *source, const char *reason) {
    process->status = status;
    process->resolved = today;
    publish_campaign();
    plugin.next_update = 0;
    report_process(instance, process, source, reason);
}

static const char *update_pending_process(PoliticalInstance *instance, PoliticalProcess *process, unsigned today) {
    if (process->status != PROCESS_PENDING) return "PS_PROCESS_CLOSED";
    const PoliticalAction *action = &instance->type->definition.actions[process->slot];
    if (action->can_stop.program && native_country_allows(instance, &action->can_stop,
        instance->holder, live_holder_tag(process->issuer) ? process->issuer : 0)) {
        conclude_process(instance, process, PROCESS_INVALID, today, "can_stop", NULL);
        return "PS_PROCESS_CLOSED";
    }
    if (today >= process->deadline) {
        conclude_process(instance, process, PROCESS_EXPIRED, today, "calendar", NULL);
        return "PS_PROCESS_EXPIRED";
    }
    if (action->kind != ACTION_INVITATION) return NULL;
    const char *reason = action_scope(instance, action, process->issuer, process->target);
    if (!reason) reason = invitation_target(instance, process->issuer, process->target);
    if (reason) conclude_process(instance, process, PROCESS_INVALID, today, "eligibility", reason);
    return reason;
}

static const char *const action_sources[] = { "script", "ui", "ai" };

static const char *commit_process(PoliticalInstance *instance, const ActionRequest *request, unsigned today, ActionPlan *plan) {
    const KhanDefinition *definition = &instance->type->definition;
    const PoliticalAction *action = &definition->actions[request->slot];
    PoliticalProcesses *state = &instance->processes;
    const unsigned index = political_process_index(state, request->slot, request->target);
    const char *source = action_sources[request->source];
    PoliticalProcess *process = index < state->count ? &state->items[index] : NULL;
    if (request->step == ACTION_START) {
        if (process && process->status == PROCESS_PENDING) update_pending_process(instance, process, today);
        const char *reason = prepare_action(instance, request->slot, request->actor, request->target, today, plan);
        if (reason) return reason;
        if (action->kind == ACTION_INCIDENT && !native_effect_callable(&action->effect))
            return "PS_ACTION_EFFECT_UNAVAILABLE";
        const unsigned round = process ? process->round + 1 : 1;
        if (action->kind == ACTION_INCIDENT && !debit_action(instance, action, plan)) return "PS_ACTION_CAPACITY";
        if (action->kind == ACTION_INVITATION) plan->after = plan->before;
        if (!process) process = &state->items[state->count++];
        *process = (PoliticalProcess){ .slot = request->slot, .round = round, .issuer = request->actor,
            .target = request->target, .opened = today, .deadline = plan->expires, .status = PROCESS_PENDING };
        publish_campaign();
        report_process(instance, process, source, NULL);
        if (action->kind == ACTION_INCIDENT)
            run_native_country(instance, &action->effect, request->actor, 0);
        return NULL;
    }
    if (!process || process->round != request->round) return "PS_PROCESS_STALE";
    if (action->kind == ACTION_INCIDENT) {
        if (request->step != ACTION_RESOLVE || request->option >= action->option_count) return "PS_PROCESS_BAD_OPTION";
        if (request->actor != instance->holder || !live_holder_tag(request->actor)) return "PS_ACTION_LEADER_ONLY";
        const char *reason = update_pending_process(instance, process, today);
        if (reason) return reason;
        const IncidentOption *option = &action->options[request->option];
        const unsigned from = live_holder_tag(process->issuer) ? process->issuer : 0;
        if (!native_country_allows(instance, &option->trigger, request->actor, from)) return "PS_ACTION_CONDITION";
        if (!native_effect_callable(&option->effect)) return "PS_ACTION_EFFECT_UNAVAILABLE";
        void *country = option->prestige ? country_from_tag(live_holder_tag(request->actor)) : NULL;
        if (option->prestige && (!country || !plugin.add_prestige)) return "PS_ACTION_EFFECT_UNAVAILABLE";
        strcpy(process->option, option->id);
        conclude_process(instance, process, PROCESS_SETTLED, today, source, NULL);
        if (country) plugin.add_prestige(country, &option->prestige, definition->source_path, (int)definition->source_line);
        run_native_country(instance, &option->effect, request->actor, from);
        return NULL;
    }
    if (request->step == ACTION_WITHDRAW) {
        if (request->actor != process->issuer) return "PS_REQUEST_ISSUER_ONLY";
    } else if ((request->step != ACTION_ACCEPT && request->step != ACTION_DECLINE) ||
               request->actor != process->target || !live_holder_tag(request->actor)) return "PS_REQUEST_RECIPIENT_ONLY";
    if (request->source == ACTION_AI && reform_actor() == process->target) return "PS_REQUEST_RECIPIENT_ONLY";
    const char *reason = update_pending_process(instance, process, today);
    if (reason) return reason;
    if (request->step == ACTION_ACCEPT) {
        if (!native_effect_callable(&action->effect)) return "PS_ACTION_EFFECT_UNAVAILABLE";
        reason = action_payment(instance, action, process->issuer, process->target, plan);
        if (reason) return reason;
        if (!debit_action(instance, action, plan)) return "PS_ACTION_CAPACITY";
        apply_member_change(instance, process->target, true);
        conclude_process(instance, process, PROCESS_ACCEPTED, today, source, NULL);
        run_native_country(instance, &action->effect, process->target, process->issuer);
    } else {
        conclude_process(instance, process, request->step == ACTION_WITHDRAW ? PROCESS_WITHDRAWN : PROCESS_REJECTED,
                         today, source, NULL);
    }
    return NULL;
}

static void pulse_processes(unsigned today, bool ai_day) {
    const unsigned player = ai_day ? reform_actor() : 0;
    for (size_t i = 0; i < plugin.roster.count; ++i) {
        PoliticalInstance *instance = &plugin.instances[i];
        if (!instance->enabled) continue;
        for (unsigned j = 0; j < instance->processes.count; ++j) {
            PoliticalProcess *process = &instance->processes.items[j];
            if (process->status != PROCESS_PENDING || update_pending_process(instance, process, today)) continue;
            const PoliticalAction *action = &instance->type->definition.actions[process->slot];
            if (player && process->target && process->target != player && action->ai_accept)
                enqueue_action((ActionRequest){.instance=i, .slot=process->slot, .actor=process->target, .target=process->target,
                    .source=ACTION_AI, .step=ACTION_ACCEPT, .round=process->round});
        }
    }
}

static void queue_action_widget(void *owner) {
    const struct ActionDisplayRow *row = owner;
    const PoliticalInstance *instance = selected_instance();
    if (!political_campaign_active() || !plugin.visible || active_selector()->open || !game_gui_context() ||
        !instance || row->instance != instance->spec || !row->visible || row->reason || !row->actor) return;
    const ActionRowEntry *entry = &row->entry;
    enqueue_action((ActionRequest){.instance=plugin.selected, .slot=entry->slot, .actor=row->actor, .target=entry->target,
        .source=ACTION_UI, .step=entry->step, .round=entry->round, .option=entry->option});
}

static const char *commit_action(PoliticalInstance *instance, const ActionRequest *request, unsigned today, ActionPlan *plan) {
    const char *reason = prepare_action(instance, request->slot, request->actor, request->target, today, plan);
    if (reason) return reason;
    const KhanDefinition *d = &instance->type->definition;
    const PoliticalAction *a = &d->actions[request->slot];
    if (!native_effect_callable(&a->effect)) return "PS_ACTION_EFFECT_UNAVAILABLE";
    ActionRecipients recipients = {0};
    if (a->effect.program && !action_recipients(instance, a, request->actor, request->target, &recipients)) {
        release_shogunate(&recipients.members);
        return "PS_ACTION_EFFECT_UNAVAILABLE";
    }
    if (!debit_action(instance, a, plan)) {
        release_shogunate(&recipients.members);
        return "PS_ACTION_CAPACITY";
    }
    PoliticalActionRun *run = plan->existing ? &instance->actions.runs[plan->existing - instance->actions.runs] :
                              &instance->actions.runs[instance->actions.count++];
    *run = (PoliticalActionRun){ .slot = request->slot, .actor = request->actor, .target = request->target,
        .performed = today, .expires = plan->expires, .cooldown_until = plan->cooldown_until };
    publish_campaign();
    if (plan->effect_country) plugin.add_prestige(plan->effect_country, &a->prestige, d->source_path, (int)d->source_line);
    run_action_clause(instance, &a->effect, &recipients, request->actor, request->target);
    release_shogunate(&recipients.members);
    return NULL;
}

static void pulse_decree_removals(unsigned today) {
    for (size_t i = 0; i < plugin.roster.count; ++i) {
        PoliticalInstance *instance = &plugin.instances[i];
        if (!instance->enabled) continue;
        const KhanDefinition *def = &instance->type->definition;
        for (unsigned j = 0; j < instance->actions.count; ++j) {
            PoliticalActionRun *run = &instance->actions.runs[j];
            const PoliticalAction *action = &def->actions[run->slot];
            if (action->kind != ACTION_DECREE || !run->expires || run->expires > today || !action->off_effect.program)
                continue;
            const PoliticalActionRun former = *run;
            ActionRecipients recipients = {0};
            action_recipients(instance, action, former.actor, former.target, &recipients);
            run->expires = 0;
            publish_campaign();
            run_action_clause(instance, &action->off_effect, &recipients, former.actor, former.target);
            release_shogunate(&recipients.members);
        }
    }
}

static void drain_actions(void) {
    unsigned today;
    if (!queued_actions.count || !campaign_date(&today)) return;
    pulse_decree_removals(today);
    ActionRequest requests[sizeof(queued_actions.items) / sizeof(*queued_actions.items)];
    const unsigned count = queued_actions.count;
    memcpy(requests, queued_actions.items, count * sizeof(*requests));
    queued_actions.count = 0;
    for (unsigned i = 0; i < count; ++i) {
        const ActionRequest *request = &requests[i];
        if (request->instance >= plugin.roster.count) continue;
        PoliticalInstance *instance = &plugin.instances[request->instance];
        if (!instance->enabled || request->slot >= instance->type->definition.action_count) continue;
        const PoliticalAction *a = &instance->type->definition.actions[request->slot];
        ActionPlan plan = {0};
        const char *result;
        if (request->source == ACTION_UI && (reform_actor() != request->actor ||
            !instance_access(request->instance, request->actor).view)) result = "PS_ACTION_UNAVAILABLE";
        else if (a->kind >= ACTION_INVITATION) result = commit_process(instance, request, today, &plan);
        else result = commit_action(instance, request, today, &plan);
        static const char *const steps[] = { "start", "accept", "decline", "withdraw", "resolve" };
        char line[680], actor[4], target[4], payer[4];
        snprintf(line, sizeof(line), "political action instance=%s id=%s source=%s actor=%s target=%s date=%u result=%s "
                 "resource=%s payer=%s before_milli=%d after_milli=%d prestige_milli=%d expires=%u step=%s round=%u",
                 instance->spec->id, a->id, action_sources[request->source], election_text(request->actor, actor),
                 request->target ? election_text(request->target, target) : "none", today, result ? result : "applied",
                 a->resource[0] ? a->resource : "none", plan.payer ? election_text(plan.payer, payer) : "instance",
                 plan.before, result ? plan.before : plan.after, result ? 0 : a->prestige, result ? 0 : plan.expires,
                 steps[request->step], request->round);
        log_message(ANVIL_LOG_INFO, line);
    }
    plugin.next_update = 0;
}

static const char *query_action(PsOperation op, const PsArguments *args, unsigned actor, bool *result) {
    const size_t index = instance_index(args->instance);
    if (index >= plugin.roster.count || !plugin.instances[index].enabled) return "unknown or inactive action instance";
    const PoliticalInstance *instance = &plugin.instances[index];
    const KhanDefinition *d = &instance->type->definition;
    if (op == PS_REFORM_LEVEL) {
        int total = instance->reform.passed_id[0] != 0;
        for (unsigned i = 0; i < d->action_count; ++i)
            if (d->actions[i].kind == ACTION_REFORM &&
                strcmp(d->actions[i].id, instance->reform.passed_id) && passed_action(instance, d->actions[i].id)) ++total;
        *result = ps_compare(args->comparison, total, args->amount);
        return NULL;
    }
    if (op == PS_CONTENT_REFORM) {
        const unsigned slot = political_action_index(d->actions, d->action_count, args->reform);
        const bool legacy = instance->type->reform.id[0] && !strcmp(instance->type->reform.id, args->reform);
        if (!legacy && (slot == d->action_count || d->actions[slot].kind != ACTION_REFORM)) return "unknown reform";
        *result = passed_action(instance, args->reform) == (!(args->fields & PS_FIELD(PS_VALUE_FIELD)) || args->value);
        return NULL;
    }
    if (op == PS_HAS_PERMISSION || op == PS_CONTEST_WON) {
        const unsigned country = args->country ? args->country : actor;
        if (!country) return "expected a country scope";
        bool yes;
        if (op == PS_HAS_PERMISSION) {
            bool declared = grants_permission(&d->succession.permissions, args->permission);
            for (unsigned i = 0; i < d->role_count && !declared; ++i)
                declared = grants_permission(&d->roles[i].permissions, args->permission);
            if (!declared) return "unknown permission";
            yes = has_political_permission(instance, country, args->permission);
        } else {
            const unsigned position = args->role[0] ? role_index(instance, args->role) : PS_MAX_ROLES;
            if (!contest_rules(instance, position)) return "unknown competition";
            const PoliticalContest *contest = &instance->contests[position];
            yes = contest->settled && contest->winner == country;
        }
        *result = yes == (!(args->fields & PS_FIELD(PS_VALUE_FIELD)) || args->value);
        return NULL;
    }
    const unsigned slot = political_action_index(d->actions, d->action_count, args->action);
    if (slot == d->action_count) return "unknown action";
    const PoliticalAction *a = &d->actions[slot];
    if (op == PS_PROCESS_STATUS) {
        if (a->kind < ACTION_INVITATION || a->country_target != (args->target != 0) || (args->option[0] && a->kind != ACTION_INCIDENT))
            return "expected a configured process with its target when required";
        const PoliticalProcesses *state = &instance->processes;
        const unsigned i = political_process_index(state, slot, args->target);
        *result = i < state->count && state->items[i].status == args->status &&
                  (!args->round || args->round == state->items[i].round) &&
                  (!args->option[0] || !strcmp(args->option, state->items[i].option));
        return NULL;
    }
    bool found = false;
    if (a->kind != ACTION_REFORM && a->kind != ACTION_DECREE) return "action does not have a continuous benefit";
    const unsigned country = args->country ? args->country : actor;
    if (!country) return "benefit requires a country scope";
    unsigned today;
    if (!campaign_date(&today)) return "game date unavailable";
    for (unsigned i = 0; i < instance->actions.count && !found; ++i) {
        const PoliticalActionRun *run = &instance->actions.runs[i];
        if (run->slot != slot || !political_action_running(a, run, today)) continue;
        switch (a->beneficiaries) {
        case ACTION_BENEFIT_ACTOR: found = country == run->actor; break;
        case ACTION_BENEFIT_TARGET: found = country == run->target; break;
        case ACTION_BENEFIT_LEADER: found = country == instance->holder; break;
        case ACTION_BENEFIT_MEMBERS: {
            const char *error = instance_has_member(instance, country, &found);
            if (error) return error;
            break;
        }
        }
    }
    *result = found == (!(args->fields & PS_FIELD(PS_VALUE_FIELD)) || args->value);
    return NULL;
}

static const char *query_member(const PsArguments *args, unsigned actor, bool *result) {
    unsigned country = args->country ? args->country : actor;
    if (!country) return "membership requires a country or ROOT in a country scope";
    const size_t index = instance_index(args->instance);
    if (index == plugin.roster.count) return "unknown member instance";
    const PoliticalInstance *instance = &plugin.instances[index];
    bool found;
    const char *reason = instance_has_member(instance, country, &found);
    if (!reason) *result = found == (!(args->fields & PS_FIELD(PS_VALUE_FIELD)) || args->value);
    return reason;
}

static bool known_province(unsigned id) {
    if (!id) return false;
    if (plugin.province_limit > 0) return id < (unsigned)plugin.province_limit;
    AnvilGameProvince province = {.size = sizeof(province)};
    return plugin.world->province && plugin.world->province((int32_t)id, &province) == ANVIL_OK;
}

static const char *query_structure(PsOperation operation, const PsArguments *args, unsigned actor, bool *result) {
    const size_t index = instance_index(args->instance);
    if (index >= plugin.roster.count || !plugin.instances[index].enabled) return "unknown political instance";
    const PoliticalInstance *instance = &plugin.instances[index];
    const KhanDefinition *definition = &instance->type->definition;
    const unsigned country = args->country ? args->country : actor;
    bool found;
    if (operation == PS_MEMBER_COUNT) {
        AnvilGameTag leader = {0};
        election_text(instance->holder, leader.text);
        Shogunate members = {0};
        if (!instance_members(instance, &leader, &members)) return "member list unavailable";
        *result = ps_compare(args->comparison, (int)members.count, args->amount);
        release_shogunate(&members);
        return NULL;
    }
    if (operation == PS_HAS_ROLE || operation == PS_ROLE_COUNT) {
        const unsigned role = role_index(instance, args->role);
        if (role == definition->role_count) return "unknown role";
        if (operation == PS_ROLE_COUNT) {
            const RoleHolders *holders = &instance->roles[role];
            int count = 0;
            for (unsigned i = 0; i < holders->count; ++i) count += live_holder_tag(holders->countries[i]) != 0;
            *result = ps_compare(args->comparison, count, args->amount);
            return NULL;
        }
        if (!country) return "role query requires a country";
        found = holds_role(instance, role, country);
    } else {
        if (!definition->territory_enabled) return "territory module is disabled";
        if (operation == PS_PROVINCE_COUNT) {
            *result = ps_compare(args->comparison, (int)instance->territory.count, args->amount);
            return NULL;
        }
        unsigned province = args->province;
        if (operation == PS_HAS_CAPITAL) {
            if (!country) return "capital query requires a country";
            int32_t capital;
            if (!current_capital(country, &capital)) return "country capital is unavailable";
            province = capital > 0 ? (unsigned)capital : 0;
        } else if (!known_province(province)) return "unknown province";
        found = has_tag(instance->territory.ids, instance->territory.count, province);
    }
    *result = found == (!(args->fields & PS_FIELD(PS_VALUE_FIELD)) || args->value);
    return NULL;
}

static const char *check_member_change(const PoliticalInstance *instance, unsigned country, bool include, bool *changes) {
    *changes = false;
    const KhanDefinition *definition = &instance->type->definition;
    if (!records_members(definition->member_source)) return "member source does not accept an explicit change";
    if (!country || !live_holder_tag(country)) return "country does not exist in the campaign";
    bool current, allowed;
    const char *reason = instance_has_member(instance, country, &current);
    if (reason) return reason;
    const bool explicit_member = has_tag(instance->members.tags, instance->members.count, country);
    const bool excluded = has_tag(instance->excluded.tags, instance->excluded.count, country);
    if ((include && explicit_member && !excluded) || (!include && !explicit_member && (!current || excluded))) return NULL;
    reason = country_rule_matches(instance, include ? &definition->member_join : &definition->member_leave, country, &allowed);
    if (reason || !allowed) return reason ? reason : "member change condition failed";
    if (include) {
        reason = country_rule_matches(instance, &definition->member_retain, country, &allowed);
        if (reason || !allowed) return reason ? reason : "country cannot retain membership";
        if (definition->max_memberships) {
            unsigned memberships = 0;
            for (size_t i = 0; i < plugin.roster.count; ++i) {
                const PoliticalInstance *other = &plugin.instances[i];
                const PoliticalMode mode = instance_mode(other);
                if (other != instance && (mode == POLITICAL_ACTIVE || mode == POLITICAL_SUSPENDED) &&
                    other->type == instance->type &&
                    has_tag(other->members.tags, other->members.count, country)) ++memberships;
            }
            if (memberships >= definition->max_memberships) return "country has reached max_per_country";
        }
        if (!explicit_member && instance->members.count == PS_MAX_EXPLICIT_MEMBERS) return "explicit member capacity exhausted";
    } else if (definition->member_source == MEMBERS_MIXED && instance->excluded.count == PS_MAX_EXPLICIT_MEMBERS) {
        return "member exclusion capacity exhausted";
    }
    *changes = true;
    return NULL;
}

static void apply_member_change(PoliticalInstance *instance, unsigned country, bool include) {
    if (include) {
        if (!has_tag(instance->members.tags, instance->members.count, country))
            instance->members.tags[instance->members.count++] = country;
        remove_tag(instance->excluded.tags, &instance->excluded.count, country);
    } else {
        if (instance->type->definition.member_source == MEMBERS_MIXED)
            instance->excluded.tags[instance->excluded.count++] = country;
        remove_tag(instance->members.tags, &instance->members.count, country);
        prune_roles(instance);
        prune_resource_accounts(instance);
    }
    resource_fact_day = 0;
}

static const char *set_member(PoliticalInstance *instance, unsigned country, bool include) {
    bool changes;
    const char *reason = check_member_change(instance, country, include, &changes);
    if (reason || !changes) return reason;
    apply_member_change(instance, country, include);
    publish_campaign();
    return NULL;
}

static const char *set_territory_province(PoliticalInstance *instance, unsigned id, bool include) {
    if (!instance->type->definition.territory_enabled) return "territory module is disabled";
    if (!known_province(id)) return "unknown province";
    ProvinceList *list = &instance->territory;
    const bool present = has_tag(list->ids, list->count, id);
    if (present == include) return NULL;
    if (include) {
        if (list->count == PS_MAX_TERRITORY) return "territory province capacity exhausted";
        list->ids[list->count++] = id;
    } else remove_tag(list->ids, &list->count, id);
    publish_campaign();
    plugin.next_update = 0;
    return NULL;
}

static bool reserved_instance_id(const char *id) {
    if (instance_known(id)) return true;
    for (size_t i = 0; i < plugin.retained.count; ++i) {
        const NativeSaveField *field = &plugin.retained.entries[i];
        PoliticalLifecycleRecord lifecycle;
        PoliticalSaveRecord record;
        if (!field->legacy && field->value[0] == 'Y') {
            if (decode_lifecycle(field->value, &lifecycle) && !strcmp(lifecycle.instance, id)) return true;
        } else if (decode_political_state(field, &record) == GK_LOAD_VALID && !strcmp(record.instance, id)) return true;
    }
    return false;
}

static const char *lifecycle_condition(const PoliticalInstance *instance, bool *allowed) {
    const EntryCondition *rule = &instance->type->definition.continue_trigger;
    *allowed = true;
    if (!rule->count) return NULL;
    const unsigned leader = live_holder_tag(instance->holder) ? instance->holder : instance->life.founder;
    *allowed = false;
    return leader ? country_rule_matches(instance, rule, leader, allowed) : NULL;
}

static void report_lifecycle(const PoliticalInstance *instance, const char *operation, unsigned today) {
    char message[450];
    snprintf(message, sizeof(message), "political lifecycle instance=%s type=%s operation=%s state=%s date=%u",
             instance->spec->id, instance->type->id, operation, political_mode_name(instance_mode(instance)), today);
    log_message(ANVIL_LOG_INFO, message);
}

static const char *create_instance(const PsArguments *args, unsigned actor, unsigned today) {
    const PoliticalType *type = known_type(args->type);
    if (!type || !type->definition.dynamic_allowed) return "type does not allow dynamic creation";
    if (reserved_instance_id(args->instance)) return "instance ID is already reserved in this campaign";
    const KhanDefinition *definition = &type->definition;
    unsigned present = 0, lifecycle_records = 0;
    for (size_t i = 0; i < plugin.roster.count; ++i) {
        const PoliticalInstance *other = &plugin.instances[i];
        const PoliticalMode mode = instance_mode(other);
        if (other->type == type && (mode == POLITICAL_ACTIVE || mode == POLITICAL_SUSPENDED)) ++present;
        if (other->life.mode) ++lifecycle_records;
    }
    for (size_t i = 0; i < plugin.retained.count; ++i)
        if (!plugin.retained.entries[i].legacy && plugin.retained.entries[i].value[0] == 'Y') ++lifecycle_records;
    if (lifecycle_records >= PS_MAX_INSTANCES) return "campaign lifetime instance registry is full";
    if (definition->max_instances && present >= definition->max_instances) return "type has reached max_instances";
    EntryContext context = country_context(args->instance, actor, ps_test_condition);
    context.from = args->country;
    bool allowed;
    const char *reason = entry_evaluate(&definition->create_trigger, &context, &allowed);
    if (reason || !allowed) return reason ? reason : "create_trigger failed";
    const unsigned holder = args->country ? args->country : actor;
    if (args->country && !definition->leader_enabled) return "leader module is disabled";
    if (definition->leader_enabled && !live_holder_tag(holder)) return "proposed leader does not exist";
    PoliticalLifecycleRecord entry = {.lifecycle = {
        .mode = POLITICAL_ACTIVE, .created = today, .founder = actor, .dynamic = true
    }};
    strcpy(entry.instance, args->instance);
    strcpy(entry.type, args->type);
    strcpy(entry.name, args->name);
    PoliticalInstance *instance = register_dynamic(&entry, type);
    if (!instance) return "campaign instance registry is full";
    instance->holder = definition->leader_enabled ? holder : 0;
    instance->has_members = records_members(definition->member_source);
    instance->history_date = uses_capital_members(instance) ? today : 0;
    instance->resource_month = today / 100;
    instance->reform.authority = definition->starting_authority;
    instance->election.round = (elective(instance) || definition->succession.event == SELECT_RULER_CHANGE) ? 1u : 0u;
    if (instance->has_members) {
        bool changes;
        const unsigned first = instance->holder ? instance->holder : actor;
        reason = check_member_change(instance, first, true, &changes);
        if (!reason && changes) apply_member_change(instance, first, true);
    }
    if (!reason && instance->holder) {
        reason = leader_eligible(instance, instance->holder, true, &allowed);
        if (!reason && !allowed) reason = "proposed leader fails the configured leadership rules";
    }
    if (!reason) {
        reason = lifecycle_condition(instance, &allowed);
        if (!reason && !allowed) reason = "continue_trigger failed";
    }
    if (reason) {
        --plugin.roster.count;
        memset(instance, 0, sizeof(*instance));
        return reason;
    }
    if (instance->election.round) ruler_id(instance->holder, &instance->election.ruler);
    instance->enabled = true;
    if (!plugin.enabled_count++) plugin.selected = (size_t)(instance - plugin.instances);
    report_lifecycle(instance, "create", today);
    publish_campaign();
    plugin.next_entry_check = plugin.next_update = 0;
    return NULL;
}

static void stop_instance_actions(size_t index) {
    unsigned keep = 0;
    for (unsigned i = 0; i < queued_actions.count; ++i)
        if (queued_actions.items[i].instance != index) queued_actions.items[keep++] = queued_actions.items[i];
    queued_actions.count = keep;
    keep = 0;
    for (unsigned i = 0; i < selection_requests.count; ++i)
        if (selection_requests.items[i].instance != index) selection_requests.items[keep++] = selection_requests.items[i];
    selection_requests.count = keep;
}

static void deactivate_instance(PoliticalInstance *instance, PoliticalMode mode, bool automatic, unsigned today) {
    const size_t index = (size_t)(instance - plugin.instances);
    if (plugin.visible && plugin.selected == index) close_instance_panel(political_mode_name(mode));
    stop_instance_actions(index);
    if (instance->enabled) --plugin.enabled_count;
    instance->enabled = false;
    instance->life.mode = mode;
    instance->life.automatic = automatic;
    if (mode == POLITICAL_DISSOLVED) {
        release_roll(&instance->roll);
        const PoliticalInstance retired = { .spec = instance->spec, .type = instance->type, .life = instance->life,
            .configured = instance->configured };
        *instance = retired;
    }
    report_lifecycle(instance, automatic ? "continue_trigger" : "script", today);
    publish_campaign();
    plugin.next_entry_check = plugin.next_update = 0;
}

static void resume_instance(PoliticalInstance *instance, unsigned today) {
    instance->enabled = true;
    ++plugin.enabled_count;
    instance->life.mode = POLITICAL_ACTIVE;
    instance->life.automatic = false;
    instance->resource_month = today / 100;
    validate_holder(instance);
    prune_roles(instance);
    prune_resource_accounts(instance);
    resource_fact_day = 0;
    refresh_resource_world(today);
    for (unsigned i = 0; i < instance->processes.count; ++i)
        if (instance->processes.items[i].status == PROCESS_PENDING)
            update_pending_process(instance, &instance->processes.items[i], today);
    for (unsigned i = 0; i < instance->type->definition.role_count; ++i) settle_contest(instance, i, today);
    settle_contest(instance, PS_MAX_ROLES, today);
    report_lifecycle(instance, "resume", today);
    publish_campaign();
    plugin.next_entry_check = plugin.next_update = 0;
}

static const char *change_instance_lifecycle(PsOperation operation, const PsArguments *args, unsigned actor) {
    unsigned today;
    if (!live_holder_tag(actor)) return "lifecycle operation requires a live country scope";
    if (!campaign_date(&today)) return "game date unavailable";
    if (operation == PS_CREATE_INSTANCE) return create_instance(args, actor, today);
    const size_t index = instance_index(args->instance);
    if (index >= plugin.roster.count) return "unknown instance";
    PoliticalInstance *instance = &plugin.instances[index];
    const PoliticalMode mode = instance_mode(instance);
    if (mode != POLITICAL_ACTIVE && mode != POLITICAL_SUSPENDED) return "instance is absent or already dissolved";
    const unsigned owner = live_holder_tag(instance->holder) ? instance->holder : instance->life.founder;
    if (owner != actor) return "only the current leader or founding country during a vacancy may change lifecycle";
    if (operation == PS_DISSOLVE_INSTANCE) {
        if (!instance->type->definition.dissolution_allowed) return "type does not allow dissolution";
        deactivate_instance(instance, POLITICAL_DISSOLVED, false, today);
    } else if (operation == PS_SUSPEND_INSTANCE) {
        if (mode != POLITICAL_ACTIVE) return "instance is already suspended";
        deactivate_instance(instance, POLITICAL_SUSPENDED, false, today);
    } else {
        if (mode != POLITICAL_SUSPENDED) return "instance is already active";
        bool allowed;
        const char *reason = lifecycle_condition(instance, &allowed);
        if (reason || !allowed) return reason ? reason : "continue_trigger failed";
        resume_instance(instance, today);
    }
    return NULL;
}

static const char *query_instance_mode(const PsArguments *args, bool *result) {
    if (!political_campaign_active()) return NULL;
    const size_t index = instance_index(args->instance);
    *result = index < plugin.roster.count && instance_mode(&plugin.instances[index]) == (PoliticalMode)args->status;
    return index < plugin.roster.count ? NULL : "unknown instance";
}

static void pulse_lifecycle(unsigned today) {
    for (size_t i = 0; i < plugin.roster.count; ++i) {
        PoliticalInstance *instance = &plugin.instances[i];
        if ((!instance->enabled && !instance->life.automatic) || !instance->type->definition.continue_trigger.count) continue;
        bool allowed;
        if (lifecycle_condition(instance, &allowed)) continue;
        if (instance->enabled && !allowed) deactivate_instance(instance, POLITICAL_SUSPENDED, true, today);
        else if (instance->life.automatic && allowed) resume_instance(instance, today);
    }
}

static const char *query_direct_war(unsigned attacker, unsigned defender, bool *allowed) {
    *allowed = true;
    if (!political_campaign_active()) return NULL;
    if (!attacker || !defender) return "direct declaration requires an attacker and defender country scope";
    for (size_t i = 0; i < plugin.roster.count; ++i) {
        const PoliticalInstance *instance = &plugin.instances[i];
        if (!instance->enabled || !instance->type->definition.block_direct_war) continue;
        bool member;
        const char *reason = instance_has_member(instance, attacker, &member);
        if (reason) return reason;
        if (!member) continue;
        reason = instance_has_member(instance, defender, &member);
        if (reason) return reason;
        if (member) {
            *allowed = false;
            const uint64_t pair = (uint64_t)attacker << 32 | defender;
            if (pair != last_player_war_block && attacker == reform_actor()) {
                char message[270], from[4], to[4];
                snprintf(message, sizeof(message), "political declarewar instance=%s attacker=%s defender=%s result=blocked",
                         instance->spec->id, election_text(attacker, from), election_text(defender, to));
                log_message(ANVIL_LOG_INFO, message);
                last_player_war_block = pair;
            }
            return NULL;
        }
    }
    return NULL;
}

static bool read_war_side(uint64_t id, uint32_t side, Shogunate *out) {
    uint32_t count = 0;
    if (plugin.world->war_side(id, side, NULL, 0, &count) != ANVIL_OK || count > INT16_MAX) return false;
    if (!count) return true;
    out->daimyo = malloc(count * sizeof(*out->daimyo));
    if (!out->daimyo) return false;
    if (plugin.world->war_side(id, side, out->daimyo, count, &out->count) != ANVIL_OK || out->count > count) {
        release_shogunate(out);
        return false;
    }
    return true;
}

static bool war_side_contains(const Shogunate *side, unsigned country) {
    for (uint32_t i = 0; i < side->count; ++i)
        if (election_tag(side->daimyo[i].text) == country) return true;
    return false;
}

static bool unique_defensive_war(const AnvilGameWar *war, unsigned attacker, unsigned defender) {
    uint32_t count = 0;
    if (plugin.world->war_ids(NULL, 0, &count) != ANVIL_OK || !count || count > 4096) return false;
    uint64_t *ids = malloc(count * sizeof(*ids));
    if (!ids) return false;
    uint32_t loaded = 0;
    bool unique = plugin.world->war_ids(ids, count, &loaded) == ANVIL_OK && loaded <= count;
    for (uint32_t i = 0; unique && i < loaded; ++i) {
        if (ids[i] == war->id) continue;
        Shogunate offense = {0}, defense = {0};
        if (!read_war_side(ids[i], ANVIL_GAME_WAR_ATTACKERS, &offense) ||
            !read_war_side(ids[i], ANVIL_GAME_WAR_DEFENDERS, &defense)) unique = false;
        else unique = !(war_side_contains(&offense, attacker) && war_side_contains(&defense, defender)) &&
                      !(war_side_contains(&offense, defender) && war_side_contains(&defense, attacker));
        release_shogunate(&offense);
        release_shogunate(&defense);
    }
    free(ids);
    return unique;
}

static void report_defense(const PoliticalInstance *instance, const AnvilGameWar *war, const char *member, const char *result) {
    char line[430];
    snprintf(line, sizeof(line), "political defense instance=%s war=%016llx attacker=%.3s defender=%.3s member=%s result=%s",
             instance->spec->id, (unsigned long long)war->id, war->attacker.text, war->defender.text, member, result);
    log_message(ANVIL_LOG_INFO, line);
}

static void apply_defensive_obligations(const AnvilGameWar *war, int32_t date) {
    const unsigned attacker = election_tag(war->attacker.text), defender = election_tag(war->defender.text);
    const uint64_t opposing = live_holder_tag(attacker), protected = live_holder_tag(defender);
    if (!protected || !opposing || protected == opposing) return;
    Shogunate offense = {0}, defense = {0};
    bool sides_read = false;
    unsigned char attempted[INT16_MAX + 1] = {0};
    for (size_t i = 0; i < plugin.roster.count; ++i) {
        const PoliticalInstance *instance = &plugin.instances[i];
        if (!instance->enabled || !instance->type->definition.defensive_obligation) continue;
        bool inside, attacker_member;
        if (instance_has_member(instance, defender, &inside) || !inside ||
            instance_has_member(instance, attacker, &attacker_member) || attacker_member) continue;
        const KhanDefinition *definition = &instance->type->definition;
        EntryContext context = country_context(instance->spec->id, defender, ps_test_condition);
        context.from = attacker;
        bool enabled;
        const char *error = entry_evaluate(&definition->defensive_trigger, &context, &enabled);
        if (error || !enabled) {
            report_defense(instance, war, "-", error ? error : "condition_failed");
            continue;
        }
        if (!sides_read) {
            sides_read = read_war_side(war->id, ANVIL_GAME_WAR_ATTACKERS, &offense) &&
                         read_war_side(war->id, ANVIL_GAME_WAR_DEFENDERS, &defense);
            if (!sides_read || !war_side_contains(&offense, attacker) || !war_side_contains(&defense, defender) ||
                !unique_defensive_war(war, attacker, defender)) {
                report_defense(instance, war, "-", "ambiguous_or_unavailable_war");
                break;
            }
        }
        AnvilGameTag holder = {0};
        election_text(instance->holder, holder.text);
        Shogunate members = {0};
        if (!instance_members(instance, &holder, &members)) continue;
        for (uint32_t j = 0; j < members.count; ++j) {
            const char *name = members.daimyo[j].text;
            const unsigned country = election_tag(name);
            const uint64_t member = live_holder_tag(country);
            const uint16_t idx = (uint16_t)(member >> 32);
            if (!member || idx > INT16_MAX || attempted[idx]) continue;
            attempted[idx] = 1;
            if (war_side_contains(&offense, country) || war_side_contains(&defense, country)) {
                if (country != defender) report_defense(instance, war, name, "vanilla_participant");
                continue;
            }
            if (!summon_defender(&plugin.defense, protected, member, opposing, date)) {
                report_defense(instance, war, name, "native_check_rejected");
                continue;
            }
            // Stock execution can add subjects of the summoned country. Read back
            // both sides before considering the next candidate from any instance.
            release_shogunate(&offense);
            release_shogunate(&defense);
            sides_read = read_war_side(war->id, ANVIL_GAME_WAR_ATTACKERS, &offense) &&
                         read_war_side(war->id, ANVIL_GAME_WAR_DEFENDERS, &defense);
            report_defense(instance, war, name, !sides_read ? "submitted_unverified" :
                           war_side_contains(&defense, country) ? "joined" : "submitted_not_joined");
            if (!sides_read) break;
        }
        release_shogunate(&members);
        if (!sides_read) break;
    }
    release_shogunate(&offense);
    release_shogunate(&defense);
}

static void on_war_started(const char *event, const void *payload, void *context) {
    (void)event;
    (void)context;
    const AnvilGameWarChange *change = payload;
    if (!change || change->size < sizeof(*change) || change->change != ANVIL_GAME_WAR_STARTED ||
        !political_campaign_active() || !plugin.defense.check || !game_gui_context()) return;
    AnvilGameWar war = {.size = sizeof(war)};
    if (plugin.world->war(change->war, &war) != ANVIL_OK ||
        strcmp(change->attacker.text, war.attacker.text) || strcmp(change->defender.text, war.defender.text)) return;
    int32_t date;
    memcpy(&date, (const unsigned char *)plugin.root + 0x1DD0, sizeof(date));
    apply_defensive_obligations(&war, date);
}

static const char *set_role(PoliticalInstance *instance, const PsArguments *args, unsigned actor) {
    if (!actor || actor != instance->holder || !live_holder_tag(actor)) return "only the instance leader may appoint or revoke roles";
    const KhanDefinition *definition = &instance->type->definition;
    const unsigned index = role_index(instance, args->role);
    if (index == definition->role_count) return "unknown role";
    if (args->value && definition->roles[index].acquisition == OFFICE_COMPETITION) return "this office is filled by competition";
    RoleHolders *holders = &instance->roles[index];
    if (args->value) {
        if (has_tag(holders->countries, holders->count, args->country)) return NULL;
        if (holders->count >= definition->roles[index].max_holders) return "role has no open seats";
        bool allowed;
        const char *reason = role_eligible(instance, &definition->roles[index], args->country, true, &allowed);
        if (reason || !allowed) return reason ? reason : "country is ineligible for this role";
        holders->countries[holders->count++] = args->country;
    } else if (!remove_tag(holders->countries, &holders->count, args->country)) return NULL;
    if (definition->succession.defined && !strcmp(definition->succession.elector_role, args->role)) instance->roll.date = 0;
    publish_campaign();
    return NULL;
}

static const char *query_election(PsOperation op, const PsArguments *args, unsigned actor, bool *result) {
    const size_t index = instance_index(args->instance);
    if (index >= plugin.roster.count || !plugin.instances[index].enabled) return "unknown election instance";
    const PoliticalInstance *instance = &plugin.instances[index];
    if (!elective(instance)) return "instance does not elect its leader";
    if (op == PS_ELECTION_ROUND) {
        *result = args->round == instance->election.round;
        return NULL;
    }
    const unsigned country = args->country ? args->country : actor;
    if (!country) return "expected a country scope";
    ElectionTable table = {0};
    if (!electoral_table(instance, &table)) return "electoral roll unavailable";
    const ElectionCountry *row = election_country(&table, country);
    switch (op) {
    case PS_IS_CANDIDATE: *result = (!!row && row->candidate) == args->value; break;
    case PS_IS_ELECTOR: *result = (!!row && row->weight > 0) == args->value; break;
    case PS_VOTE_WEIGHT: *result = ps_compare(args->comparison, row ? row->weight : 0, args->amount); break;
    case PS_VOTE_TOTAL: *result = ps_compare(args->comparison, row ? (int)row->votes : 0, args->amount); break;
    case PS_SUPPORTS_CANDIDATE:
        *result = (!!row && row->support == args->target) == (!(args->fields & PS_FIELD(PS_VALUE_FIELD)) || args->value);
        break;
    default: free(table.rows); return "unknown election query";
    }
    free(table.rows);
    return NULL;
}

static const char *electoral_action(PoliticalInstance *instance, PsOperation op, const PsArguments *args, unsigned actor) {
    if (!elective(instance)) return "instance does not elect its leader";
    if (!actor) return "expected a country scope";
    if (op == PS_ABSTAIN) return "an elector must choose a candidate";
    if (instance->holder) {
        uint64_t current = 0;
        if (!live_holder_tag(instance->holder)) return "succession is pending";
        if (!current_ruler(instance, instance->holder, &current)) return "ruler information unavailable";
        if (instance->election.ruler && instance->election.ruler != current) return "succession is pending";
    }
    ElectionTable table = {0};
    if (!electoral_table(instance, &table)) return "electoral roll unavailable";
    const ElectionCountry *row = election_country(&table, actor);
    ElectionState *state = &instance->election;
    const char *error = NULL;
    if (op == PS_SET_ELECTOR) {
        const bool amount = (args->fields & PS_FIELD(PS_AMOUNT_FIELD)) != 0;
        if (instance->type->definition.succession.defined) error = "electors are assigned through the named office";
        else if (!row && args->value) error = "elector must be a member";
        else if (amount && (!args->value || args->amount < 1)) error = "elector grant requires a positive weight";
        else {
            const int weight = args->value ? (amount ? args->amount : instance->type->definition.election.weight) : 0;
            unsigned choice = 0;
            for (unsigned i = 0; i < state->count; ++i)
                if (state->entries[i].voter == actor && weight) choice = state->entries[i].choice;
            error = election_prefer(state, actor, choice, weight);
        }
    } else if (op == PS_RESOLVE_ELECTION) {
        if (instance->type->definition.succession.defined && instance->type->definition.succession.event != SELECT_EXPLICIT)
            error = "this election settles through its configured event";
        else if (actor != instance->holder && (instance->holder || !row || !row->weight)) error = "only the holder can call succession, or an elector during vacancy";
        else error = settle_election(instance, &table, args->round, "script");
    } else {
        unsigned today = instance->roll.date;
        campaign_date(&today);
        error = election_cast(state, &table, args->round, actor, args->country,
                              today, (unsigned)instance->type->definition.election.cooldown_days);
    }
    char voter[4], target[4], message[420];
    snprintf(message, sizeof(message), "election action instance=%s op=%s round=%u actor=%s target=%s weight=%d result=%s",
             instance->spec->id, ps_definitions[op].name, args->round, election_text(actor, voter),
             args->country ? election_text(args->country, target) : "-", row ? row->weight : 0, error ? error : "applied");
    log_message(error ? ANVIL_LOG_WARNING : ANVIL_LOG_INFO, message);
    free(table.rows);
    if (!error && op != PS_RESOLVE_ELECTION) {
        if (op == PS_SET_ELECTOR) {
            ElectionTable updated = {0};
            if (electoral_table(instance, &updated)) {
                election_prune(state, &updated);
                free(updated.rows);
            }
        }
        publish_campaign();
    }
    return error;
}

static const char *execute_political_action(PsOperation operation, const PsArguments *args, unsigned actor) {
    if (!political_campaign_active()) return "no active campaign instance";
    if (operation >= PS_CREATE_INSTANCE && operation <= PS_DISSOLVE_INSTANCE)
        return change_instance_lifecycle(operation, args, actor);
    const size_t index = instance_index(args->instance);
    if (index == plugin.roster.count) return "unknown instance";
    PoliticalInstance *instance = &plugin.instances[index];
    if (!instance->enabled) return "campaign instance is unavailable";
    if (operation >= PS_SET_ELECTOR && operation <= PS_RESOLVE_ELECTION)
        return electoral_action(instance, operation, args, actor);
    switch (operation) {
    case PS_SEND_MESSAGE:
        return queue_custom_notice(instance, args, actor);
    case PS_START_CONTEST:
    case PS_INVEST_CONTEST: {
        const unsigned slot = args->role[0] ? role_index(instance, args->role) : PS_MAX_ROLES;
        if (!contest_rules(instance, slot)) return "unknown competition";
        return submit_selection(index, slot, actor, 0, operation);
    }
    case PS_RESOLVE_LEADER:
        return submit_selection(index, PS_MAX_ROLES, actor, 0, operation);
    case PS_RUN_ACTION:
    case PS_RESPOND_REQUEST:
    case PS_WITHDRAW_REQUEST:
    case PS_RESOLVE_INCIDENT: {
        if (!actor || (args->actor && args->actor != actor)) return "actor must match the country scope";
        const KhanDefinition *d = &instance->type->definition;
        const unsigned slot = political_action_index(d->actions, d->action_count, args->action);
        if (slot == d->action_count) return "unknown action";
        const PoliticalAction *action = &d->actions[slot];
        ActionRequest request = { .instance = index, .slot = slot, .actor = actor, .target = args->target,
            .source = ACTION_SCRIPT, .round = args->round };
        if (operation == PS_RESOLVE_INCIDENT) {
            if (action->kind != ACTION_INCIDENT) return "expected an incident ID";
            request.step = ACTION_RESOLVE;
            request.option = incident_option_index(action, args->option);
            if (request.option == action->option_count) return "unknown incident option";
        } else if (operation == PS_RESPOND_REQUEST || operation == PS_WITHDRAW_REQUEST) {
            if (action->kind != ACTION_INVITATION) return "expected an invitation action ID";
            if (operation == PS_RESPOND_REQUEST) {
                request.step = args->value ? ACTION_ACCEPT : ACTION_DECLINE;
                request.target = actor;
            } else request.step = ACTION_WITHDRAW;
        }
        return enqueue_action(request);
    }
    case PS_SET_MEMBER:
        return set_member(instance, args->country ? args->country : actor, args->value);
    case PS_SET_ROLE:
        return set_role(instance, args, actor);
    case PS_SET_PROVINCE:
        return set_territory_province(instance, args->province, args->value);
    case PS_SET_REFORM:
        return set_reform_state(instance, args, actor);
    case PS_SET_LEADER: {
        if (!instance->type->definition.leader_enabled) return "leader module is disabled";
        if ((args->fields & PS_FIELD(PS_VALUE_FIELD)) && !args->value)
            return "setting a leader requires yes; use political_clear_leader to vacate the office";
        const unsigned country = args->country ? args->country : args->target ? args->target : actor;
        if (!country) return "leadership requires a target country or a current country scope";
        if (instance->type->definition.succession.defined)
            return submit_selection(index, PS_MAX_ROLES, actor, country, operation);
        return change_holder(instance, country) ? NULL : "country does not exist in the campaign";
    }
    case PS_CLEAR_LEADER:
        if (!(args->fields & PS_FIELD(PS_VALUE_FIELD)) || args->value) change_holder(instance, 0);
        return NULL;
    case PS_SET_RESOURCE:
    case PS_CHANGE_RESOURCE: return change_resource(instance, operation, args, actor);
    case PS_EXECUTE_ACTION: {
        if (!actor) return "expected a country scope";
        if (!instance->type->reform.id[0] || strcmp(args->reform, instance->type->reform.id)) return "unknown reform";
        const ReformStatus status = enact_reform_for(instance, actor);
        return status == REFORM_ENACTED ? NULL : reform_status_text[status];
    }
    default: return "operation has no effect implementation";
    }
}

static bool process_reform_input(void) {
    ReformView *view = &panel_view()->reform_view;
    PoliticalInstance *instance = selected_instance();
    const ReformInput input = view->pending;
    view->pending = REFORM_INPUT_NONE;
    if (!instance || !instance->type->reform.id[0] || input == REFORM_INPUT_NONE) return false;
    if (input == REFORM_INPUT_CANCEL) {
        if (view->confirmed_for) log_message(ANVIL_LOG_INFO, "great_khan reform_confirmation=cancelled");
        view->confirmed_for = 0;
        view->confirmed_instance = NULL;
        return true;
    }

    if (validate_holder(instance)) publish_campaign();
    const unsigned leader = instance->holder;
    const unsigned actor = reform_actor();
    const ReformStatus status = evaluate_reform(&instance->reform, &instance->type->reform, actor, leader);
    if (input == REFORM_INPUT_REVIEW) {
        if (status == REFORM_AVAILABLE && !view->confirmed_for) {
            view->confirmed_for = leader;
            view->confirmed_instance = instance->spec;
            log_message(ANVIL_LOG_INFO, "great_khan reform_confirmation=opened");
        }
        return true;
    }
    const unsigned confirmed_for = view->confirmed_for;
    const PoliticalInstanceSpec *confirmed_instance = view->confirmed_instance;
    view->confirmed_for = 0;
    view->confirmed_instance = NULL;
    if (!confirmed_for || confirmed_instance != instance->spec || leader != confirmed_for ||
        actor != confirmed_for || status != REFORM_AVAILABLE) {
        if (confirmed_for) log_message(ANVIL_LOG_INFO, "great_khan reform_confirmation=expired");
        return true;
    }

    if (enact_reform_for(instance, actor) == REFORM_NO_LEADER)
        log_message(ANVIL_LOG_INFO, "great_khan reform_confirmation=leader_unavailable");
    return true;
}

static void render_reform_view(bool opened) {
    ReformView *view = &panel_view()->reform_view;
    const PoliticalInstance *instance = selected_instance();
    if (!view->bound || !instance) return;
    const KhanReform *reform = &instance->type->reform;
    char authority[16];
    snprintf(authority, sizeof(authority), "%d", instance->reform.authority);
    native_text(view->authority, authority);
    set_number(&panel_view()->authority_value, instance, instance->reform.authority * PS_RESOURCE_SCALE, 0);
    const bool has_reform = reform->id[0] != 0;
    control_visible(view->buttons[0], has_reform);
    if (!has_reform) {
        control_visible(view->buttons[1], false);
        control_visible(view->buttons[2], false);
        native_text(view->title, "");
        native_text(view->terms, "");
        native_text(view->status_label, "");
        return;
    }

    const unsigned leader = instance->holder;
    const unsigned actor = reform_actor();
    view->status = evaluate_reform(&instance->reform, reform, actor, leader);
    const bool available = view->status == REFORM_AVAILABLE;
    if (!available || view->confirmed_for != leader || view->confirmed_instance != instance->spec) {
        view->confirmed_for = 0;
        view->confirmed_instance = NULL;
    }
    const bool confirming = view->confirmed_for != 0;
    button_enabled(view->buttons[0], available && !confirming);
    control_visible(view->buttons[1], confirming);
    control_visible(view->buttons[2], confirming);
    if (confirming) native_ui_label(view->status_label, "PS_REFORM_CONFIRM", "Pass this reform?");
    else if (available) native_text(view->status_label, "");
    else native_ui_label(view->status_label, reform_status_key[view->status], reform_status_text[view->status]);

    if (opened) {
        GameString title = {0};
        plugin.localize(&title, reform->title, NULL, 0);
        native_text(view->title, string_text(&title));
        plugin.free_string(&title);
        GameString terms = {.capacity = 15};
        append_reform_terms(&terms, reform, false);
        native_text(view->terms, string_text(&terms));
        plugin.free_string(&terms);
        char message[445];
        char tag[4] = {0};
        memcpy(tag, &leader, 3);
        void *country = country_from_tag(country_tag(tag));
        int32_t prestige = 0;
        if (country) plugin.get_prestige(country, &prestige);
        snprintf(message, sizeof(message), "political reform_view instance=%s id=%s authority=%d status=%s "
                 "leader=%s prestige_milli=%d",
                 instance->spec->id, reform->id, instance->reform.authority,
                 available ? "Available" : reform_status_text[view->status], tag, prestige);
        log_message(ANVIL_LOG_INFO, message);
    }
}

static void render_count_heading(void *control, const char *key, const char *fallback, unsigned count) {
    if (!control) return;
    GameString heading = {.capacity = 15};
    const TextField fields[] = {{"value", text_integer(count)}, {"count", text_integer(count)}};
    if (key) {
        if (!append_political_text(&heading, key, (PoliticalText){.instance = selected_instance(),
            .fields = fields, .count = sizeof(fields) / sizeof(*fields)}, NULL)) append_ui_label(&heading, key, fallback);
    } else plugin.append_string(&heading, fallback, strlen(fallback));
    char suffix[24];
    snprintf(suffix, sizeof(suffix), " (%u)", count);
    plugin.append_string(&heading, suffix, strlen(suffix));
    native_text(control, string_text(&heading));
    plugin.free_string(&heading);
}

static void render_member_heading(const KhanDefinition *definition, uint32_t count) {
    void *control = panel_view()->daimyo_label;
    const char *title = definition->member_title;
    if (title[0]) {
        render_count_heading(control, ps_identifier(title) ? title : NULL, title, count);
    } else if (!definition->subject_type[0] || definition->member_source == MEMBERS_CAPITAL_PROVINCES) {
        render_count_heading(control, "PS_MEMBERS_TITLE", "Members", count);
    } else {
        char key[GK_SCRIPT_WORD_SIZE + sizeof("_title")], fallback[GK_SCRIPT_WORD_SIZE];
        snprintf(key, sizeof(key), "%s_title", definition->subject_type);
        format_title(fallback, definition->subject_type);
        render_count_heading(control, key, fallback, count);
    }
}

static void set_number(NumberDisplay *number, const PoliticalInstance *instance, int32_t value, unsigned country) {
    number->instance = instance ? instance->spec : NULL;
    number->value = value;
    number->country = country;
}

static void render_tumen(const PoliticalInstance *instance, const Shogunate *members) {
    const char *tags[TUMEN_SLOTS] = {0};
    unsigned count = 0;
    for (uint32_t i = 0; i < members->count; ++i) {
        const char *tag = members->daimyo[i].text;
        if (!has_khan_tumen(&instance->khanate_history, instance->history_date, tag)) continue;
        if (count < TUMEN_SLOTS) tags[count] = tag;
        ++count;
    }
    render_count_heading(panel_view()->tumen_caption, "PS_TUMEN_TITLE", "Tumen", count);
    set_number(&panel_view()->electors_count, instance, (int32_t)count, 0);
    char summary[TUMEN_SLOTS * 4 + 1] = {0};
    for (unsigned i = 0; i < TUMEN_SLOTS; ++i) {
        if (panel_view()->show_shields) render_shield(&panel_view()->tumen_shields[i], tags[i]);
        else native_text(panel_view()->tumen_labels[i], tags[i] ? tags[i] : "");
        if (!tags[i]) continue;
        if (i) summary[i * 4 - 1] = ',';
        memcpy(summary + i * 4, tags[i], 3);
    }
    char message[270];
    snprintf(message, sizeof(message), "khanate tumen instance=%s count=%u tags=%s",
             instance->spec->id, count, summary);
    log_message(ANVIL_LOG_INFO, message);
}

static void set_list_top(ListScroll *list, unsigned top) {
    list->top = top < list->last ? top : list->last;
    position_scroll_rail(list->bar, list->top);
}

static void size_list(ListScroll *list, unsigned last, bool show_empty_rail) {
    if (!list->initialized || last != list->last) {
        list->last = last;
        list->initialized = true;
        resize_scroll_rail(list->bar, last);
        set_list_top(list,list->top);
    }
    control_visible(list->bar, show_empty_rail || last != 0);
}

static bool read_list(ListScroll *list) {
    if (!list->bar || !list->last) { list->wheel = 0; return false; }
    const unsigned top = read_scroll_row(list->bar, list->last, &list->wheel);
    if (top == list->top) return false;
    list->top = top;
    return true;
}

static void append_office_title(GameString *text, const PoliticalRole *role) {
    if (role->title_key[0]) append_label(text, role->title_key);
    else plugin.append_string(text, role->id, strlen(role->id));
}

static void draw_office_group(struct OfficeGroupHeading *group, const PoliticalRole *role) {
    group->role = role;
    GameString text = {.capacity = 15};
    if (role) append_office_title(&text, role);
    native_text(group->label, string_text(&text));
    plugin.free_string(&text);
    control_visible(group->hover.button, role != NULL);
}

static unsigned office_slots(const MemberLayout *layout, const PoliticalRole *role) {
    if (!layout->group_heading[0]) return role->max_holders;
    const unsigned columns = layout->columns;
    return (role->max_holders + columns - 1) / columns * columns;
}

static void clear_office_slot(PoliticalPanel *panel, unsigned slot) {
    native_text(panel->roles.labels[slot], "");
    render_nomination(NULL, NULL, NULL, 0, false, &panel->roles.scores[slot]);
    Shield *support = &panel->roles.supports[slot];
    if (support->tooltip.button) {
        support->voter = 0;
        render_shield(support, NULL);
    }
    panel->roles.shields[slot].role_tooltip = NULL;
    render_shield(&panel->roles.shields[slot], NULL);
}

static void render_roles(const PoliticalInstance *instance, bool opened) {
    PoliticalPanel *panel = panel_view();
    ListScroll *list = &panel->roles.list;
    const PoliticalLayout *layout = instance->type->layout;
    if (!layout || !list->bar) return;
    const KhanDefinition *definition = &instance->type->definition;
    ElectionTable table = {0};
    if ((layout->roles.nomination_score[0] || layout->roles.support_shield[0]) && elective(instance))
        electoral_table(instance, &table);
    const unsigned viewer = table.rows ? reform_actor() : 0;
    const ElectionCountry *viewer_row = viewer ? election_country(&table, viewer) : NULL;
    const bool can_stand = viewer_row && viewer_row->candidate;
    unsigned total = 0;
    for (unsigned i = 0; i < definition->role_count; ++i)
        total += office_slots(&layout->roles, &definition->roles[i]);
    if (opened) {
        list->initialized = false;
        GameString text = { .capacity = 15 };
        if (definition->role_count) append_label(&text, "PS_ROLE_HEADING");
        native_text(panel->roles.caption, string_text(&text));
        plugin.free_string(&text);
    }
    const unsigned columns = layout->roles.columns;
    const unsigned slots = layout->roles.rows * columns;
    const unsigned rows = (total + columns - 1) / columns;
    const bool grouped = layout->roles.group_heading[0] != 0;
    size_list(list, rows > layout->roles.rows ? rows - layout->roles.rows : 0, false);
    unsigned skip = list->top * columns, visible = 0;
    for (unsigned i = 0; i < definition->role_count && visible < slots; ++i) {
        const PoliticalRole *role = &definition->roles[i];
        const RoleHolders *holders = &instance->roles[i];
        const unsigned seats = office_slots(&layout->roles, role);
        for (unsigned row = 0; row < seats && visible < slots; ++row) {
            if (skip) { --skip; continue; }
            if (grouped && visible % columns == 0)
                draw_office_group(&panel->roles.groups[visible / columns], !row || !visible ? role : NULL);
            if (row >= role->max_holders) {
                clear_office_slot(panel, visible++);
                continue;
            }
            const bool occupied = row < holders->count;
            if (panel->roles.labels[visible]) {
                GameString text = { .capacity = 15 };
                append_office_title(&text, role);
                if (!occupied) {
                    plugin.append_string(&text, "  ", 2);
                    const PoliticalContest *contest = &instance->contests[i];
                    append_label(&text, contest->opened && !contest->settled ? "PS_ROLE_COMPETITION" : "PS_ROLE_EMPTY");
                }
                native_text(panel->roles.labels[visible], string_text(&text));
                plugin.free_string(&text);
            }
            Shield *shield = &panel->roles.shields[visible];
            shield->role_tooltip = office_tooltip_key(&layout->roles, role);
            char country[4];
            render_shield_slot(shield, occupied ? election_text(holders->countries[row], country) : NULL, true);
            const ElectionCountry *elector = occupied && viewer && !strcmp(role->id, definition->succession.elector_role)
                ? election_country(&table, holders->countries[row]) : NULL;
            render_nomination(instance, &table, elector, viewer, can_stand, &panel->roles.scores[visible]);
            Shield *support = &panel->roles.supports[visible];
            if (support->tooltip.button) {
                support->voter = elector && elector->weight ? elector->tag : 0;
                const unsigned target = support->voter ? elector->support : 0;
                render_shield(support, target ? election_text(target, country) : NULL);
            }
            ++visible;
        }
    }
    while (visible < slots) {
        if (grouped && visible % columns == 0)
            draw_office_group(&panel->roles.groups[visible / columns], NULL);
        clear_office_slot(panel, visible++);
    }
    free(table.rows);
}

static unsigned primary_resource(const KhanDefinition *type) {
    for (unsigned i = 0; i < type->resource_count; ++i)
        if (type->resources[i].owner == RESOURCE_INSTANCE) return i;
    return 0;
}

static unsigned display_resource(struct ResourceDisplayRow *row, const PoliticalInstance *instance,
                                 unsigned slot, unsigned viewer, char value[24]) {
    const ResourceDefinition *def = &instance->type->definition.resources[slot];
    const unsigned owner = def->owner == RESOURCE_INSTANCE ? 0 : viewer;
    bool active;
    const char *error = resource_owner(instance, def, owner, &active);
    row->name = def->name[0] ? def->name : def->id;
    row->detail = def->tooltip[0] ? def->tooltip : NULL;
    row->definition = def;
    row->viewer = viewer;
    row->owner = UINT_MAX;
    row->has_growth = row->stored = false;
    strcpy(value, "--");
    if (error || !active) {
        row->ownership = "PS_RESOURCE_NO_ACCOUNT";
        return UINT_MAX;
    }
    row->ownership = def->owner == RESOURCE_INSTANCE ? "PS_RESOURCE_SYSTEM_TOOLTIP" : "PS_RESOURCE_COUNTRY_TOOLTIP";
    const ResourceLedger *ledger = &instance->accounts[slot];
    row->balance = resource_balance(def, ledger, owner);
    row->owner = owner;
    for (unsigned i = 0; i < ledger->count; ++i) row->stored |= ledger->accounts[i].country == owner;
    int64_t growth;
    row->has_growth = resource_growth(instance, def, owner, &growth, false, &row->growth);
    if (row->has_growth) {
        const int64_t delta = (int64_t)resource_limit(def, (int64_t)row->balance + growth) - row->balance;
        row->change = delta < INT32_MIN ? INT32_MIN : delta > INT32_MAX ? INT32_MAX : (int32_t)delta;
    }
    format_fixed_number(row->balance, def->precision, value);
    return owner;
}

static void render_primary_resource(PoliticalPanel *panel, const PoliticalInstance *instance, unsigned slot, unsigned viewer) {
    struct ResourceDisplayRow *row = &panel->resources.summary;
    row->name = row->ownership = row->detail = NULL;
    row->definition = NULL;
    const KhanDefinition *type = &instance->type->definition;
    const bool present = slot < type->resource_count;
    if (row->hover.button) control_visible(row->hover.button, present);
    if (!present) {
        native_text(row->value, "");
        native_text(panel->resources.growth, "");
        return;
    }
    const ResourceDefinition *def = &type->resources[slot];
    char value[24];
    display_resource(row, instance, slot, viewer, value);
    native_text(row->value, value);
    char monthly[64] = "--";
    if (row->has_growth) {
        const int64_t delta = row->change;
        const int32_t magnitude = (int32_t)(llabs(delta) > INT32_MAX ? INT32_MAX : llabs(delta));
        char amount[24];
        format_fixed_number(magnitude, def->precision, amount);
        snprintf(monthly, sizeof(monthly), "%s%s%s%s", delta > 0 ? "\xa7G" : delta < 0 ? "\xa7R" : "",
                 delta < 0 ? "-" : "+", amount, delta ? "\xa7!" : "");
    }
    native_text(panel->resources.growth, monthly);
}

static void render_resources(const PoliticalInstance *instance, unsigned viewer, bool opened) {
    PoliticalPanel *panel = panel_view();
    ListScroll *list = &panel->resources.list;
    const PoliticalLayout *layout = instance->type->layout;
    if (!layout) return;
    const KhanDefinition *type = &instance->type->definition;
    const unsigned primary = panel->resources.summary.value ? primary_resource(type) : type->resource_count;
    if (panel->resources.summary.value) render_primary_resource(panel, instance, primary, viewer);
    if (!list->bar) return;
    const unsigned rows = layout->resources.rows;
    const unsigned total = type->resource_count - (primary < type->resource_count);
    if (opened) list->initialized = false;
    size_list(list, total > rows ? total - rows : 0, false);
    for (unsigned row = 0; row < rows; ++row) {
        const unsigned index = row + list->top;
        struct ResourceDisplayRow *display = &panel->resources.rows[row];
        display->ownership = display->detail = display->name = NULL;
        display->definition = NULL;
        if (display->hover.button) control_visible(display->hover.button, index < total);
        if (index >= total) {
            native_text(display->label, "");
            if (display->value) native_text(display->value, "");
            continue;
        }
        const unsigned i = index + (index >= primary);
        const ResourceDefinition *def = &type->resources[i];
        char value[24];
        display_resource(display, instance, i, viewer, value);
        GameString text = { .capacity = 15 };
        if (def->name[0]) append_label(&text, def->name);
        else plugin.append_string(&text, def->id, strlen(def->id));
        if (display->value) native_text(display->value, value);
        else {
            plugin.append_string(&text, "   ", 3);
            plugin.append_string(&text, value, strlen(value));
        }
        native_text(display->label, string_text(&text));
        plugin.free_string(&text);
    }
}

static void append_action_name(GameString *text, const PoliticalAction *action) {
    if (action->name[0]) append_label(text, action->name);
    else plugin.append_string(text, action->id, strlen(action->id));
}

static void append_political_date(GameString *text, unsigned date) {
    char day[16];
    snprintf(day, sizeof(day), "%u.%02u.%02u", date / 10000, date / 100 % 100, date % 100);
    plugin.append_string(text, day, strlen(day));
}

static void append_action_reason(GameString *text, const PoliticalInstance *instance,
                                 const struct ActionDisplayRow *row, unsigned today) {
    const KhanDefinition *d = &instance->type->definition;
    const unsigned slot = row->entry.slot;
    const PoliticalAction *action = &d->actions[slot];
    const PoliticalActionRun *decree = decree_in_force(instance, slot, today);
    if (decree) {
        plugin.append_string(text, "\n", 1);
        append_label(text, decree->slot == slot ? "PS_ACTION_ACTIVE_DECREE" : "PS_ACTION_BLOCKING_DECREE");
        append_action_name(text, &d->actions[decree->slot]);
        append_label(text, "PS_ACTION_EXPIRES_ON");
        append_political_date(text, decree->expires);
    }
    if (!row->reason && action->kind == ACTION_DECREE) {
        const unsigned expires = election_after_days(today, action->duration);
        if (expires) {
            plugin.append_string(text, "\n", 1);
            append_label(text, "PS_ACTION_DECREE_IF_ENACTED");
            append_political_date(text, expires);
        }
    }
    if (!row->reason || (decree && !strcmp(row->reason, "PS_ACTION_IN_FORCE"))) return;
    plugin.append_string(text, "\n", 1);
    const char *key = row->reason;
    if (!strcmp(key, "PS_ACTION_CONDITION")) {
        if (action->condition_tooltip[0]) key = action->condition_tooltip;
        else if (action->actor_trigger.test == ENTRY_TAG || action->actor_trigger.test == ENTRY_COUNTRY_FLAG) {
            append_label(text, action->actor_trigger.test == ENTRY_TAG ? "PS_ACTION_REQUIRED_COUNTRY" : "PS_ACTION_REQUIRED_FLAG");
            if (action->actor_trigger.test == ENTRY_TAG) append_label(text, action->actor_trigger.operand);
            else plugin.append_string(text, action->actor_trigger.operand, strlen(action->actor_trigger.operand));
            return;
        }
    }
    append_label(text, key);
    if (!strcmp(row->reason, "PS_ACTION_COOLDOWN")) {
        const PoliticalActionRun *run = find_action_run(&instance->actions, slot, row->actor);
        if (run) {
            append_label(text, "PS_ACTION_REOPENS_ON");
            append_political_date(text, run->cooldown_until);
        }
    }
    if (!strcmp(row->reason, "PS_ACTION_CONFLICT")) {
        for (unsigned i = 0; i < d->action_count; ++i) {
            const PoliticalAction *other = &d->actions[i];
            if (other->kind != ACTION_REFORM || !find_action_run(&instance->actions, i, 0)) continue;
            bool blocks = false;
            for (unsigned j = 0; j < action->exclude_count; ++j) blocks |= !strcmp(action->excludes[j], other->id);
            for (unsigned j = 0; j < other->exclude_count; ++j) blocks |= !strcmp(other->excludes[j], action->id);
            if (!blocks) continue;
            plugin.append_string(text, "\n- ", 3);
            append_action_name(text, other);
        }
    }
    if (strcmp(row->reason, "PS_ACTION_PREREQUISITE")) return;
    for (unsigned i = 0; i < action->require_count; ++i) {
        if (passed_action(instance, action->requires[i])) continue;
        const unsigned required = political_action_index(d->actions, d->action_count, action->requires[i]);
        if (required == d->action_count) continue;
        plugin.append_string(text, "\n- ", 3);
        append_action_name(text, &d->actions[required]);
    }
}

static const char *process_tooltip(const PoliticalProcess *process) {
    static const char *const keys[] = {
        [PROCESS_ACCEPTED] = "PS_REQUEST_ACCEPTED",
        [PROCESS_REJECTED] = "PS_REQUEST_REJECTED", [PROCESS_EXPIRED] = "PS_PROCESS_EXPIRED",
        [PROCESS_INVALID] = "PS_REQUEST_INVALID", [PROCESS_WITHDRAWN] = "PS_REQUEST_WITHDRAWN",
        [PROCESS_SETTLED] = "PS_INCIDENT_SETTLED"
    };
    if (process->status == PROCESS_PENDING) return process->target ? "PS_REQUEST_PENDING" : "PS_INCIDENT_PENDING";
    return keys[process->status];
}

static const PoliticalProcess *row_process(const PoliticalInstance *instance, const ActionRowEntry *entry) {
    if (!entry->round) return NULL;
    const PoliticalProcesses *state = &instance->processes;
    const unsigned i = political_process_index(state, entry->slot, entry->target);
    return i < state->count && state->items[i].round == entry->round ? &state->items[i] : NULL;
}

static void append_option_name(GameString *text, const IncidentOption *option) {
    if (option->name[0]) append_label(text, option->name);
    else plugin.append_string(text, option->id, strlen(option->id));
}

static void append_row_name(GameString *text, const PoliticalAction *action, const struct ActionDisplayRow *row,
                            const PoliticalProcess *process) {
    const ActionRowEntry *entry = &row->entry;
    if (entry->step == ACTION_RESOLVE) {
        append_option_name(text, &action->options[entry->option]);
        return;
    }
    const char *verb = entry->step == ACTION_ACCEPT ? "PS_REQUEST_ACCEPT" : entry->step == ACTION_DECLINE ? "PS_REQUEST_DECLINE" :
                      entry->step == ACTION_WITHDRAW ? "PS_REQUEST_WITHDRAW" : NULL;
    if (verb) append_label(text, verb);
    else append_action_name(text, action);
    if (entry->target) {
        char tag[4];
        plugin.append_string(text, " ", 1);
        const bool receiving = process && (entry->step == ACTION_ACCEPT || entry->step == ACTION_DECLINE);
        plugin.append_string(text, election_text(receiving ? process->issuer : entry->target, tag), 3);
    }
}

static void append_process_details(GameString *text, const PoliticalAction *action,
                                   const PoliticalProcess *process) {
    plugin.append_string(text, "\n", 1);
    if (process->target) {
        char issuer[4], target[4];
        append_label(text, "PS_REQUEST_FROM");
        plugin.append_string(text, election_text(process->issuer, issuer), 3);
        append_label(text, "PS_REQUEST_TO");
        plugin.append_string(text, election_text(process->target, target), 3);
        plugin.append_string(text, "\n", 1);
    }
    append_label(text, "PS_PROCESS_DEADLINE");
    append_political_date(text, process->deadline);
    if (process->status == PROCESS_SETTLED) {
        const unsigned option = incident_option_index(action, process->option);
        plugin.append_string(text, "\n", 1);
        append_label(text, "PS_INCIDENT_RESULT");
        if (option < action->option_count) append_option_name(text, &action->options[option]);
        else plugin.append_string(text, process->option, strlen(process->option));
    }
}

static bool describe_action(const ButtonTooltip *tooltip, const void *button, GameString *text) {
    const PoliticalInstance *instance = selected_instance();
    if (!political_campaign_active() || !plugin.visible || active_selector()->open || !game_gui_context() ||
        !instance || button != tooltip->button) return false;
    const PoliticalLayout *layout = instance->type->layout;
    if (!layout) return false;
    const struct ActionDisplayRow *row = NULL;
    for (unsigned i = 0; i < layout->actions.rows; ++i) {
        const struct ActionDisplayRow *candidate = &panel_view()->actions.rows[i];
        const ButtonTooltip *shown = candidate->reason && candidate->read_only.button ? &candidate->read_only : &candidate->hover;
        if (tooltip == shown) { row = candidate; break; }
    }
    if (!row || !row->visible || row->instance != instance->spec) return false;
    const KhanDefinition *d = &instance->type->definition;
    const PoliticalAction *a = &d->actions[row->entry.slot];
    const PoliticalProcess *process = row_process(instance, &row->entry);
    append_row_name(text, a, row, process);
    unsigned today = 0;
    campaign_date(&today);
    append_action_reason(text, instance, row, today);
    if (process) append_process_details(text, a, process);
    const char *tip = row->entry.step == ACTION_RESOLVE ? a->options[row->entry.option].tooltip : a->tooltip;
    const unsigned cost_slot = resource_find(d->resources, d->resource_count, a->resource);
    const ResourceDefinition *resource = cost_slot < d->resource_count ? &d->resources[cost_slot] : NULL;
    if (a->cost && cost_slot < d->resource_count && (row->entry.step == ACTION_START || row->entry.step == ACTION_ACCEPT)) {
        const ResourceDefinition *r = &d->resources[cost_slot];
        char amount[24];
        format_fixed_number(a->cost, r->precision, amount);
        plugin.append_string(text, "\n", 1);
        append_label(text, "PS_ACTION_COST");
        plugin.append_string(text, amount, strlen(amount));
        plugin.append_string(text, " ", 1);
        if (r->name[0]) append_label(text, r->name);
        else plugin.append_string(text, r->id, strlen(r->id));
        plugin.append_string(text, "\n", 1);
        append_label(text, r->owner == RESOURCE_INSTANCE ? "PS_RESOURCE_SYSTEM_TOOLTIP" : "PS_RESOURCE_COUNTRY_TOOLTIP");
    }
    if (tip[0]) {
        char deadline[16] = "";
        if (process) snprintf(deadline, sizeof(deadline), "%u.%02u.%02u", process->deadline / 10000,
                              process->deadline / 100 % 100, process->deadline % 100);
        PoliticalText target = {0};
        const TextField fields[] = {{"cost", text_fixed(a->cost)}, {"action", text_value(a->id)},
            {"deadline", text_value(deadline)}, {"target", text_value(text_scope_country(&target, row->entry.target))},
            {"breakdown", text_value(string_text(text))}};
        GameString custom = {.capacity = 15};
        bool included;
        const bool rendered = append_political_text(&custom, tip, (PoliticalText){.instance = instance,
            .resource = resource, .country = row->actor, .fields = fields, .count = sizeof(fields) / sizeof(*fields)}, &included);
        if (rendered && custom.size) {
            if (included) plugin.assign_string(text, string_text(&custom), custom.size);
            else {
                plugin.append_string(text, "\n", 1);
                plugin.append_string(text, string_text(&custom), custom.size);
            }
        } else if (!plugin.text_engine.lookup) {
            plugin.append_string(text, "\n", 1);
            append_label(text, tip);
        }
        plugin.free_string(&custom);
    }
    return true;
}

static const TooltipMethods action_tooltip = {describe_action, release_button_tooltip};

static void show_action_row(struct ActionDisplayRow *row) {
    const bool clickable = row->visible && !row->reason;
    void *active = row->hover.button;
    if (clickable) connect_click(active, &row->listener, row, queue_action_widget);
    else disconnect_click(active, &row->listener);
    button_enabled(active, clickable);
    control_visible(active, row->visible && (clickable || !row->read_only.button));
    if (row->read_only.button) {
        control_visible(row->read_only.button, row->visible && !clickable);
        control_visible(row->seal, clickable);
        control_visible(row->check, row->visible && row->enacted);
    }
}

static unsigned list_action_rows(const PoliticalInstance *instance, unsigned actor, ActionRowEntry *choices) {
    const KhanDefinition *definition = &instance->type->definition;
    const PoliticalProcesses *state = &instance->processes;
    unsigned count = 0;
    for (unsigned i = 0; i < definition->action_count; ++i) {
        const PoliticalAction *action = &definition->actions[i];
        if (!action->shown || !native_country_allows(instance, &action->potential, actor, 0)) continue;
        if (action->kind == ACTION_INVITATION) {
            for (unsigned j = 0; j < state->count; ++j) {
                const PoliticalProcess *process = &state->items[j];
                if (process->slot != i || (actor != process->target && actor != process->issuer)) continue;
                ActionRowEntry entry = { .slot=i, .target=process->target, .round=process->round, .step=ACTION_VIEW };
                choices[count++] = entry;
                if (process->status != PROCESS_PENDING) continue;
                if (actor == process->issuer) {
                    entry.step = ACTION_WITHDRAW;
                    choices[count++] = entry;
                } else {
                    entry.step = ACTION_ACCEPT;
                    choices[count++] = entry;
                    entry.step = ACTION_DECLINE;
                    choices[count++] = entry;
                }
            }
            continue;
        }
        ActionRowEntry entry = { .slot = i, .step = ACTION_START };
        if (action->kind == ACTION_INCIDENT) {
            const unsigned j = political_process_index(state, i, 0);
            if (j < state->count) {
                bool member = false;
                if (actor != instance->holder && (instance_has_member(instance, actor, &member) || !member)) continue;
                entry.step = ACTION_VIEW;
                entry.round = state->items[j].round;
                choices[count++] = entry;
                if (state->items[j].status == PROCESS_PENDING && actor == instance->holder) {
                    entry.step = ACTION_RESOLVE;
                    for (entry.option = 0; entry.option < action->option_count; ++entry.option)
                        choices[count++] = entry;
                }
                continue;
            }
        }
        choices[count++] = entry;
    }
    return count;
}

static void update_action_row(const PoliticalInstance *instance, struct ActionDisplayRow *row,
                              const PoliticalProcess *process, unsigned today) {
    const ActionRowEntry *entry = &row->entry;
    const PoliticalAction *action = &instance->type->definition.actions[entry->slot];
    row->reason = NULL;
    row->enacted = false;
    ActionPlan plan = {0};
    if (entry->step == ACTION_VIEW) {
        row->reason = process ? process_tooltip(process) : "PS_PROCESS_STALE";
        row->enacted = process && (process->status == PROCESS_ACCEPTED || process->status == PROCESS_SETTLED);
        return;
    }
    if (entry->step != ACTION_START) {
        if (!process || today >= process->deadline || process->status != PROCESS_PENDING)
            row->reason = "PS_PROCESS_CLOSED";
        else if (entry->step == ACTION_ACCEPT)
            row->reason = action_payment(instance, action, process->issuer, process->target, &plan);
        else if (entry->step == ACTION_RESOLVE &&
                 !native_country_allows(instance, &action->options[entry->option].trigger, row->actor,
                     live_holder_tag(process->issuer) ? process->issuer : 0))
            row->reason = "PS_ACTION_CONDITION";
        return;
    }
    row->reason = prepare_action(instance, entry->slot, row->actor, 0, today, &plan);
    if (action->kind >= ACTION_INVITATION) return;
    for (unsigned j = 0; j < instance->actions.count && !row->enacted; ++j) {
        const PoliticalActionRun *run = &instance->actions.runs[j];
        if (run->slot != entry->slot) continue;
        row->enacted = political_action_running(action, run, today) ||
                       (run->actor == row->actor && action->kind == ACTION_DECISION && !action->repeatable);
    }
    if (row->enacted) row->reason = action->kind == ACTION_DECREE ? "PS_ACTION_IN_FORCE" : "PS_ACTION_ALREADY_DONE";
}

static void render_actions(const PoliticalInstance *instance, unsigned actor, bool opened) {
    PoliticalPanel *panel = panel_view();
    ListScroll *list = &panel->actions.list;
    const PoliticalLayout *layout = instance->type->layout;
    if (!layout || !list->bar) return;
    const KhanDefinition *d = &instance->type->definition;
    ActionRowEntry choices[PS_MAX_ACTIONS * (1 + PS_MAX_INCIDENT_OPTIONS) + PS_MAX_PROCESSES * 3];
    const unsigned count = list_action_rows(instance, actor, choices);
    unsigned today = 0;
    campaign_date(&today);
    if (opened) list->initialized = false;
    const unsigned rows = layout->actions.rows;
    size_list(list, count > rows ? count - rows : 0, false);
    for (unsigned i = 0; i < rows; ++i) {
        struct ActionDisplayRow *row = &panel->actions.rows[i];
        const unsigned position = list->top + i;
        const bool visible = position < count;
        row->visible = visible;
        row->instance = visible ? instance->spec : NULL;
        if (!visible) {
            show_action_row(row);
            native_text(row->label, "");
            continue;
        }
        row->entry = choices[position];
        row->actor = actor;
        const PoliticalAction *action = &d->actions[row->entry.slot];
        const PoliticalProcess *process = row_process(instance, &row->entry);
        update_action_row(instance, row, process, today);
        show_action_row(row);
        GameString label = { .capacity = 15 };
        if (row->reason) append_label(&label, "PS_ACTION_GRAY");
        append_row_name(&label, action, row, process);
        if (row->reason) append_label(&label, "PS_ACTION_COLOR_RESET");
        native_text(row->label, string_text(&label));
        plugin.free_string(&label);
    }
}

static bool describe_selector_row(const ButtonTooltip *tooltip, const void *button, GameString *text) {
    const SelectorRow *row = (const void *)((const char *)tooltip - offsetof(SelectorRow, tooltip));
    if (!active_selector()->open || row->selector_slot != plugin.selector_slot ||
        tooltip->button != button || row->instance >= plugin.roster.count) return false;
    const PoliticalInstance *instance = &plugin.instances[row->instance];
    append_label(text, row->enabled ? "PS_SELECTOR_ROW_TOOLTIP" : disabled_hint(&instance->type->definition.entry));
    const char *id = instance->spec->id;
    plugin.append_string(text, "\n", 1);
    plugin.append_string(text, id, strlen(id));
    return true;
}

static const TooltipMethods selector_tooltip = {describe_selector_row, release_button_tooltip};

static void queue_selector_choice(void *owner) {
    const SelectorRow *row = owner;
    if (plugin.ready && active_selector()->open && row->selector_slot == plugin.selector_slot &&
        row->enabled && row->instance < plugin.roster.count && game_gui_context()) {
        plugin.pending_panel = PANEL_REQUEST_CHOOSE_INSTANCE;
        plugin.pending_instance = row->instance;
    }
}

static void queue_selector_cancel(void *owner) {
    if (owner == active_selector() && active_selector()->open && game_gui_context())
        plugin.pending_panel = PANEL_REQUEST_CANCEL_SELECTOR;
}

static bool bind_selector(size_t slot) {
    InstanceSelector *selector = &plugin.selectors[slot];
    if (selector->failed) return false;
    if (selector->window) return true;
    if (!plugin.selector_gui_valid) goto failed;
    selector->window = create_named_window(PS_SELECTOR_WINDOW);
    if (!selector->window) goto failed;
    native_window_visible(selector->window, false);
    void *(*find_text)(void *, const char *) = method(selector->window, 0x98);
    selector->title = find_text(selector->window, "ps_selector_title");
    selector->list.bar = named_scrollbar(selector->window, "ps_instance_scroll");
    void *close = find_standard_button(selector->window, "ps_selector_close");
    if (!selector->title || !selector->list.bar || !close) goto failed;
    connect_click(close, &selector->close, selector, queue_selector_cancel);
    for (unsigned i = 0; i < SELECTOR_ROWS; ++i) {
        char name[48];
        SelectorRow *row = &selector->rows[i];
        snprintf(name, sizeof(name), "ps_instance_row_%02u", i);
        void *button = find_standard_button(selector->window, name);
        snprintf(name, sizeof(name), "ps_instance_label_%02u", i);
        row->label = find_text(selector->window, name);
        if (!button || !row->label) goto failed;
        row->instance = PS_MAX_INSTANCES;
        row->selector_slot = slot;
        attach_tooltip(button, &row->tooltip, &selector_tooltip);
        connect_click(button, &row->listener, row, queue_selector_choice);
    }
    return true;
failed:
    selector->failed = true;
    log_message(ANVIL_LOG_ERROR, "political instance selector could not bind its native controls");
    return false;
}

static void close_selector(void) {
    InstanceSelector *selector = active_selector();
    if (!selector->open) return;
    native_window_visible(selector->window, false);
    selector->open = false;
    selector->list.wheel = 0;
    selector->choices = (InstanceChoices){0};
    log_message(ANVIL_LOG_INFO, "political instance_selector action=close");
}

static void draw_selector(void) {
    InstanceSelector *selector = active_selector();
    const InstanceChoices *choices = &selector->choices;
    for (unsigned i = 0; i < SELECTOR_ROWS; ++i) {
        SelectorRow *row = &selector->rows[i];
        unsigned index = selector->list.top + i;
        const bool show = index < choices->count;
        row->instance = show ? choices->items[index] : PS_MAX_INSTANCES;
        row->enabled = show && (choices->enabled & (UINT32_C(1) << index)) != 0;
        button_enabled(row->tooltip.button, row->enabled);
        control_visible(row->tooltip.button, show);
        if (!show) {
            native_text(row->label, "");
            continue;
        }
        const PoliticalInstance *instance = &plugin.instances[row->instance];
        char caption[2 * GK_SCRIPT_WORD_SIZE + 16], tag[4];
        snprintf(caption, sizeof(caption), "%s   %s   [%s]",
                 instance_title(instance), election_text(instance->holder, tag), instance->spec->id);
        native_text(row->label, caption);
    }
}

static void update_selector(unsigned actor) {
    InstanceSelector *selector = active_selector();
    if (!selector->open) return;
    InstanceChoices choices = type_choices(&plugin.types[selector->type_index], actor);
    if (!choices.count) {
        close_selector();
        return;
    }
    const bool changed = choices.count != selector->choices.count || choices.enabled != selector->choices.enabled ||
        memcmp(choices.items, selector->choices.items, choices.count * sizeof(*choices.items));
    selector->choices = choices;
    size_list(&selector->list, choices.count > SELECTOR_ROWS ? choices.count - SELECTOR_ROWS : 0, false);
    if (changed || read_list(&selector->list)) draw_selector();
}

static bool open_selector(size_t type_index, const InstanceChoices *choices) {
    // Native windows keep their creation order when shown. Create a selector after its panel.
    const size_t slot = plugin.visible ? plugin.selected + 1 : 0;
    if (!bind_selector(slot)) return false;
    if (plugin.selector_slot != slot) close_selector();
    plugin.selector_slot = slot;
    InstanceSelector *selector = active_selector();
    selector->type_index = type_index;
    selector->choices = *choices;
    selector->list.top = 0;
    selector->list.wheel = 0;
    selector->list.initialized = false;
    size_list(&selector->list, choices->count > SELECTOR_ROWS ? choices->count - SELECTOR_ROWS : 0, false);
    native_window_visible(selector->window, true);
    selector->open = true;
    native_text(selector->title, plugin.types[type_index].definition.title);
    draw_selector();
    enable_ballot_wheel();
    char message[360];
    snprintf(message, sizeof(message), "political instance_selector action=open type=%s visible=%u enabled=%u parent=%s",
             plugin.types[type_index].id, choices->count, choices->enabled_count,
             plugin.visible ? plugin.instances[plugin.selected].spec->id : "none");
    log_message(ANVIL_LOG_INFO, message);
    return true;
}

static bool show_ballot(VoteView *view, bool show) {
    if (show && !create_ballot(view)) { log_message(ANVIL_LOG_WARNING,"election ballot=unavailable"); return false; }
    if (view->window) native_window_visible(view->window,show);
    view->open = show;
    if (show) { view->list.top = 0; view->list.wheel = 0; view->list.initialized = false; enable_ballot_wheel(); }
    return true;
}

static void render_elector_support(PoliticalPanel *panel, unsigned slot, const ElectionCountry *voter) {
    char target[4];
    const unsigned support = voter ? voter->support : 0;
    const char *tag = election_text(support,target);
    Shield *shield = &panel->supports[slot];
    if (panel->show_shields) {
        if (voter && (shield->voter != voter->tag || (shield->tooltip.tag & 0xffffffu) != support)) {
            char who[4], line[160];
            snprintf(line,sizeof(line),"election seat elector=%s support=%s ballot=%s weight=%d",
                     election_text(voter->tag,who),support ? tag : "-",
                     support ? (voter->manual ? "manual" : "automatic") : "no_candidate",
                     voter->weight);
            log_message(ANVIL_LOG_INFO,line);
        }
        shield->voter = voter ? voter->tag : 0;
        render_shield(shield,support ? tag : NULL);
    }
    void *label = panel->support_labels[slot];
    if (voter && !support) {
        GameString text = { .capacity = 15 };
        plugin.localize(&text,"GK_VOTE_PENDING",NULL,0);
        plugin.set_text(label,&text,false);
        plugin.free_string(&text);
    } else {
        native_text(label,panel->show_shields ? "" : tag);
    }
}

static void render_electors(const PoliticalInstance *instance, const ElectionTable *table) {
    PoliticalPanel *panel = panel_view();
    const unsigned viewer = reform_actor();
    const ElectionCountry *viewer_row = election_country(table, viewer);
    const bool can_stand = viewer_row && viewer_row->candidate;
    unsigned seen = 0, total = 0;
    const unsigned rows = (table->voters + 1)/2;
    size_list(&panel->vote.tumen, rows > TUMEN_SLOTS/2 ? rows - TUMEN_SLOTS/2 : 0, false);
    for (size_t i = 0; i < table->count; ++i) {
        const ElectionCountry *v = &table->rows[i];
        if (!v->weight) continue;
        ++total;
        if (total <= panel->vote.tumen.top*2 || seen == TUMEN_SLOTS) continue;
        char voter[4];
        election_text(v->tag, voter);
        if (panel->show_shields) render_shield(&panel->tumen_shields[seen], voter);
        else native_text(panel->tumen_labels[seen], voter);
        render_elector_support(panel,seen,v);
        render_nomination(instance, table, v, viewer, can_stand, &panel->tumen_scores[seen]);
        ++seen;
    }
    for (; seen < TUMEN_SLOTS; ++seen) {
        if (panel->show_shields) render_shield(&panel->tumen_shields[seen], NULL);
        else native_text(panel->tumen_labels[seen], "");
        render_elector_support(panel,seen,NULL);
        render_nomination(instance, table, NULL, viewer, false, &panel->tumen_scores[seen]);
    }
    render_count_heading(panel->tumen_caption, "PS_TUMEN_TITLE", "Tumen", total);
    set_number(&panel->electors_count, instance, (int32_t)total, 0);
}

typedef struct BallotCountry {
    unsigned tag;
    char *name;
    int opinion;
} BallotCountry;

static int vote_name_order(const void *left, const void *right) {
    const BallotCountry *a = left, *b = right;
    int cmp = strcmp(a->name,b->name);
    return cmp ? cmp : memcmp(&a->tag,&b->tag,3);
}

static int vote_opinion_order(const void *left, const void *right) {
    const BallotCountry *a = left, *b = right;
    if (a->opinion != b->opinion) return a->opinion < b->opinion ? 1 : -1;
    return vote_name_order(a,b);
}

static void render_ballot(PoliticalInstance *instance) {
    VoteView *v = &panel_view()->vote;
    if (!v->bound || !elective(instance)) return;
    ElectionTable table = {0};
    if (!electoral_table(instance, &table)) return;
    if (v->tumen.bar) render_electors(instance, &table);
    else {
        const unsigned viewer = reform_actor();
        const ElectionCountry *voter = election_country(&table, viewer);
        unsigned today;
        const bool eligible = voter && voter->weight && campaign_date(&today) &&
                              !election_locked_until(&instance->election, viewer, today);
        button_enabled(v->buttons[V_OPEN], eligible);
        if (!eligible && v->open) show_ballot(v, false);
    }
    if (!v->open) { free(table.rows); return; }
    v->displayed_round = instance->election.round;
    v->displayed_actor = reform_actor();
    const ElectionCountry *actor = election_country(&table, v->displayed_actor);
    char player[4];
    election_text(v->displayed_actor,player);
    BallotCountry *list = table.count ? calloc(table.count,sizeof(*list)) : NULL;
    if (table.count && !list) { free(table.rows); return; }
    unsigned n = 0;
    for (size_t i=0; i<table.count; ++i) {
        if (!table.rows[i].candidate) continue;
        BallotCountry *c = &list[n];
        c->tag = table.rows[i].tag;
        char tag[4];
        election_text(c->tag,tag);
        const uint64_t country = country_tag(tag);
        const GameString *name = country ? plugin.country_name(&country,false,false,false) : NULL;
        const size_t len = name && name->size ? (size_t)name->size : 3;
        c->name = malloc(len+1);
        if (!c->name) break;
        memcpy(c->name, name && name->size ? string_text(name) : tag, len);
        c->name[len] = 0;
        if (actor) c->opinion = instance->roll.opinions[(size_t)(actor-table.rows)*table.count+i];
        else if (v->displayed_actor && c->tag != v->displayed_actor) c->opinion = electoral_opinion(v->displayed_actor,c->tag);
        ++n;
    }
    if (n > 1) qsort(list,n,sizeof(*list),v->by_opinion ? vote_opinion_order : vote_name_order);
    size_list(&v->list,n > BALLOT_ROWS ? n - BALLOT_ROWS : 0,true);
    for (unsigned i=0; i<BALLOT_ROWS; ++i) {
        unsigned index = v->list.top+i;
        if (index >= n) {
            render_shield(&v->choices[i], NULL);
            native_text(v->names[i], "");
            native_text(v->opinions[i], "");
            set_number(&v->relation[i], NULL, 0, 0);
            continue;
        }
        const BallotCountry *c = &list[v->reverse ? n-1-index : index];
        char tag[4], value[32];
        render_shield(&v->choices[i],election_text(c->tag,tag));
        native_text(v->names[i],c->name);
        if (c->tag == v->displayed_actor) strcpy(value,"-");
        else snprintf(value,sizeof(value),"%+d",c->opinion);
        native_text(v->opinions[i],value);
        set_number(&v->relation[i], c->tag == v->displayed_actor ? NULL : instance, c->opinion, c->tag);
    }
    for (unsigned i=0; i<n; ++i) free(list[i].name);
    free(list);
    free(table.rows);
}

static void process_vote_input(void) {
    PoliticalInstance *instance = selected_instance();
    VoteView *v = &panel_view()->vote;
    VoteInput input = v->input;
    v->input = (VoteInput){0};
    if (!v->bound || !instance || !elective(instance)) return;
    if (!input.action) {
        bool moved = read_list(&v->tumen);
        if (v->open && read_list(&v->list)) moved = true;
        if (moved) render_ballot(instance);
        return;
    }
    if (input.action == V_OPEN || input.action == V_CLOSE) {
        bool show = input.action == V_OPEN && !v->open;
        if (show_ballot(v,show)) log_message(ANVIL_LOG_INFO,show ? "election ballot=open side=left" : "election ballot=closed");
    } else if (!v->open) return;
    else if (input.action == V_COUNTRY || input.action == V_OPINION) {
        bool opinion = input.action == V_OPINION;
        v->reverse = v->by_opinion == opinion ? !v->reverse : false;
        v->by_opinion = opinion;
        set_list_top(&v->list,0);
    } else if (input.action == V_PICK && input.actor == reform_actor() && input.target) {
        PsArguments args = { .country=input.target, .round=input.round };
        if (!electoral_action(instance,PS_CAST_VOTE,&args,input.actor)) {
            show_ballot(v,false);
            render_roles(instance, false);
            log_message(ANVIL_LOG_INFO,"election ballot=closed source=candidate_shield");
        }
    }
    render_ballot(instance);
}

static void refresh(bool opened) {
    PoliticalInstance *instance = selected_instance();
    uint32_t today = 0;
    if (!instance || !campaign_date(&today)) return;
    const KhanDefinition *definition = &instance->type->definition;
    const KhanHistory record = instance->starting_history;
    if (!elective(instance) && validate_holder(instance)) publish_campaign();
    AnvilGameTag holder = {0};
    memcpy(holder.text, &instance->holder, 3);
    Shogunate next = {0};
    const bool capital = uses_capital_members(instance);
    const bool list_ok = instance_members(instance, &holder, &next);
    if (!list_ok) next.emperor = holder;
    const Shogunate *old = &plugin.current;
    const bool holder_changed = strcmp(next.emperor.text, old->emperor.text) != 0;
    const bool changed = holder_changed || next.count != old->count ||
                         (next.count && memcmp(next.daimyo, old->daimyo, next.count * sizeof(*next.daimyo)));
    release_shogunate(&plugin.current);
    plugin.current = next;

    if (opened) {
        const char *title_key = ps_identifier(instance->spec->title) ? instance->spec->title :
            !instance->spec->title[0] && definition->entry.title_key[0] ? definition->entry.title_key : NULL;
        render_instance_text(panel_view()->title, title_key, instance_title(instance), instance);
        const char *leader_key = definition->leader_enabled && ps_identifier(definition->emperor) ? definition->emperor : NULL;
        render_instance_text(panel_view()->emperor_caption, leader_key,
                             definition->leader_enabled ? instance->type->emperor_title : "", instance);
        if (definition->script_type) {
            PoliticalPanel *panel = panel_view();
            if (!definition->authority_enabled) native_text(panel->khanate_authority, "");
            if (panel->close_button) control_visible(panel->close_button, definition->entry.close_button);
            if (panel->switch_button) {
                const InstanceChoices choices = type_choices(instance->type, reform_actor());
                control_visible(panel->switch_button, choices.count > 1);
                button_enabled(panel->switch_button, choices.enabled_count > 0);
            }
        }
        if (capital) {
            const int32_t limit = plugin.world->province_limit ? plugin.world->province_limit() : 0;
            const size_t provinces = count_khanate_provinces(&instance->khanate_history, instance->history_date, limit);
            char total[24];
            snprintf(total, sizeof(total), "%zu", provinces);
            native_text(panel_view()->khanate_provinces, total);
            set_number(&panel_view()->territory_count, instance, (int32_t)provinces, 0);
        }
    }
    if (panel_view()->khanate_authority && (capital || (definition->script_type && definition->authority_enabled))) {
        char authority[24];
        const unsigned slot = resource_find(definition->resources, definition->resource_count, PS_AUTHORITY);
        int32_t amount = instance->reform.authority * PS_RESOURCE_SCALE;
        if (slot < definition->resource_count) {
            const ResourceDefinition *def = &definition->resources[slot];
            amount = resource_balance(def, &instance->accounts[slot], 0);
            format_fixed_number(amount, def->precision, authority);
        } else snprintf(authority, sizeof(authority), "%d", instance->reform.authority);
        native_text(panel_view()->khanate_authority, authority);
        set_number(&panel_view()->authority_value, instance, amount, 0);
    }
    const LeaderReign *reign = current_reign(instance);
    if (panel_view()->history_hover.button && reign) {
        GameString title = {.capacity = 15};
        append_ruler_identity(&title, reign, false);
        native_text(panel_view()->emperor_label, string_text(&title));
        plugin.free_string(&title);
    } else if (!definition->leader_enabled) {
        native_text(panel_view()->emperor_label, "");
    } else if (!list_ok) {
        GameString title = {.capacity = 15};
        plugin.append_string(&title, holder.text, strlen(holder.text));
        plugin.append_string(&title, " - ", 3);
        append_ui_label(&title, "PS_MEMBERS_UNAVAILABLE", "membership unavailable");
        native_text(panel_view()->emperor_label, string_text(&title));
        plugin.free_string(&title);
    } else if (holder.text[0]) {
        native_text(panel_view()->emperor_label, holder.text);
    } else native_ui_label(panel_view()->emperor_label, "PS_LEADER_HISTORY_VACANT", "Office vacant");
    if (panel_view()->history_hover.button)
        control_visible(panel_view()->history_hover.button, definition->leader_enabled);
    render_roles(instance, opened);
    if (panel_view()->resources.list.bar || panel_view()->resources.summary.value)
        render_resources(instance, reform_actor(), opened);
    if (panel_view()->actions.list.bar) render_actions(instance, reform_actor(), opened);
    render_reform_view(opened);
    if (panel_view()->vote.bound) render_ballot(instance);
    if (changed || opened) {
        if (panel_view()->show_shields && panel_view()->holder_shield.tooltip.button)
            render_shield_slot(&panel_view()->holder_shield, holder.text, definition->leader_enabled);
        if (definition->member_source != MEMBERS_UNSET) render_member_heading(definition, next.count);
        else native_text(panel_view()->daimyo_label, "");
        set_number(&panel_view()->members_count, definition->member_source != MEMBERS_UNSET ? instance : NULL, next.count, 0);
        sync_scrollbar(opened || holder_changed);
        render_daimyo();
        if (capital && !elective(instance)) render_tumen(instance, &next);
        char members[DAIMYO_SLOTS * 4 + 1] = {0};
        for (unsigned i = 0; i < next.count && i < DAIMYO_SLOTS; ++i) {
            if (i) members[i * 4 - 1] = ',';
            memcpy(members + i * 4, next.daimyo[i].text, 3);
        }
        static const char *const sources[] = {
            [MEMBERS_UNSET] = "none", [MEMBERS_DIRECT_SUBJECTS] = "direct_subjects_of_leader",
            [MEMBERS_CAPITAL_PROVINCES] = "capital_provinces", [MEMBERS_EXPLICIT] = "explicit",
            [MEMBERS_QUERY] = "query", [MEMBERS_MIXED] = "mixed", [MEMBERS_CAPITAL_TERRITORY] = "capital_territory"
        };
        char message[1120];
        snprintf(message, sizeof(message),
                 "political panel instance=%s type=%s date=%u entry=%u configured=%s holder=%s "
                 "role=%s member_title=%s direct=%u subject_type=%s members=%u tags=%s member_source=%s history_date=%u",
                 instance->spec->id, instance->type->id, today, record.effective_date,
                 record.tag[0] ? record.tag : "none", holder.text[0] ? holder.text : "none",
                 instance->type->emperor_title, definition->member_title, next.direct_subjects,
                 definition->subject_type, next.count, members,
                 capital ? "capital_provinces" : sources[definition->member_source], instance->history_date);
        log_message(ANVIL_LOG_INFO, message);
        if (opened) log_original_empires();
    }
}

static void clear_panel_input(void) {
    VoteView *vote = &panel_view()->vote;
    vote->input = (VoteInput){0};
    if (vote->bound && vote->open) show_ballot(vote, false);
    plugin.pending_country = 0;
    plugin.pending_country_instance = NULL;
    panel_view()->reform_view.pending = REFORM_INPUT_NONE;
    panel_view()->reform_view.confirmed_for = 0;
    panel_view()->reform_view.confirmed_instance = NULL;
}

static void close_instance_panel(const char *source) {
    close_selector();
    if (!plugin.visible) return;
    const PoliticalInstanceSpec *spec = plugin.instances[plugin.selected].spec;
    window_visible(false);
    clear_panel_input();
    char message[260];
    snprintf(message, sizeof(message), "political panel instance=%s source=%s action=close", spec->id, source);
    log_message(ANVIL_LOG_INFO, message);
}

static const char *native_peer(void *in_game, bool close) {
    static const struct {
        size_t slot, visibility;
        const char *name;
    } peers[] = {
        {0x1280, 0x30, "hre"},
        {0x1298, 0x08, "papacy"},
        {0x12c8, 0x440, "china"},
    };
    const char *opened = NULL;
    for (size_t i = 0; i < sizeof(peers) / sizeof(*peers); ++i) {
        const unsigned char *view = pointer_at(in_game, peers[i].slot);
        if (!view || !view[peers[i].visibility]) continue;
        opened = peers[i].name;
        if (!close) return opened;
        plugin.native_panel_toggle[i](in_game);
        char message[130];
        snprintf(message, sizeof(message), "political native_peer=%s action=close", peers[i].name);
        log_message(ANVIL_LOG_INFO, message);
    }
    return opened;
}

static bool open_instance_panel(size_t target, void *in_game, const char *source) {
    if (target >= plugin.roster.count || !plugin.instances[target].enabled ||
        !instance_access(target, reform_actor()).view) return false;
    size_t previous = plugin.selected;
    char opening[340];
    snprintf(opening, sizeof(opening), "political panel attempt instance=%s source=%s",
             plugin.instances[target].spec->id, source);
    log_message(ANVIL_LOG_INFO, opening);
    plugin.selected = target;
    bool bound = bind_window();
    plugin.selected = previous;
    if (!bound) return false;
    close_instance_panel("another_instance");
    plugin.selected = target;
    native_peer(in_game, true);
    window_visible(true);
    clear_panel_input();
    release_shogunate(&plugin.current);
    plugin.top_row = plugin.max_top_row = 0;
    if (plugin.instances[target].type->definition.script_type)
        log_message(ANVIL_LOG_INFO, "political panel stage=render");
    refresh(true);
    enable_ballot_wheel();
    char message[260];
    snprintf(message, sizeof(message), "political panel instance=%s source=%s action=open",
             plugin.instances[target].spec->id, source);
    log_message(ANVIL_LOG_INFO, message);
    return true;
}

static bool update_authored_leader(PoliticalInstance *instance, unsigned today) {
    const LeaderSelection *rule = &instance->type->definition.succession;
    bool changed = false, eligible;
    if (instance->holder && !leader_eligible(instance, instance->holder, false, &eligible) && !eligible) {
        instance->holder = 0;
        changed = true;
    }
    const bool elected = rule->mode == LEADER_ELECTED;
    if (elected && today != instance->roll.date && !sample_electoral_roll(instance, today)) return changed;
    uint64_t ruler = 0;
    const bool reads_ruler = instance->election.round && ruler_id(instance->holder, &ruler);
    const bool succession = reads_ruler && instance->election.ruler && instance->election.ruler != ruler;
    const bool due = (rule->event == SELECT_VACANCY && !instance->holder && instance->election_checked_date != today) ||
        (rule->event == SELECT_RULER_CHANGE && succession) || (rule->event == SELECT_THRESHOLD && instance->threshold_succession_pending);
    if (due) {
        bool settled = false;
        if (rule->mode == LEADER_HIGHEST) settled = select_highest_leader(instance);
        else if (elected) {
            ElectionTable table = {0};
            if (electoral_table(instance, &table)) {
                settled = !settle_election(instance, &table, instance->election.round, "configured_event");
                free(table.rows);
            }
        }
        if (!settled) return changed;
        instance->threshold_succession_pending = false;
        instance->election_checked_date = today;
        changed = true;
    } else if (reads_ruler && instance->election.ruler != ruler) {
        instance->election.ruler = ruler;
        changed = true;
    }
    return changed;
}

static void update_campaign_holders(void) {
    bool changed = false;
    uint32_t today = 0;
    for (size_t i = 0; i < plugin.roster.count; ++i) {
        PoliticalInstance *instance = &plugin.instances[i];
        if (!instance->enabled) continue;
        if (prune_roles(instance)) {
            changed = true;
            if (instance->type->definition.succession.defined && elective(instance)) instance->roll.date = 0;
        }
        if (instance->type->definition.succession.defined) {
            if (today || campaign_date(&today)) changed |= update_authored_leader(instance, today);
            continue;
        }
        if (!elective(instance)) {
            if (validate_holder(instance)) changed = true;
            if (instance->election.round) {
                uint64_t current = 0;
                if (ruler_id(instance->holder, &current) && current != instance->election.ruler) {
                    instance->election.ruler = current;
                    changed = true;
                }
            }
            continue;
        }
        if (!today && !campaign_date(&today)) continue;
        if (!sample_electoral_roll(instance, today)) continue;
        uint64_t ruler = 0;
        const bool eliminated = instance->holder && !live_holder_tag(instance->holder);
        if (!eliminated && !ruler_id(instance->holder, &ruler)) continue;
        const bool succession = instance->election.ruler && ruler != instance->election.ruler;
        if (!eliminated && !succession && ruler == instance->election.ruler &&
            instance->election_checked_date == today && instance->holder) continue;
        if (!eliminated && !succession && ruler != instance->election.ruler) {
            instance->election.ruler = ruler;
            changed = true;
        }
        ElectionTable table = {0};
        if (!electoral_table(instance, &table)) continue;
        instance->election_checked_date = today;
        if (election_prune(&instance->election, &table)) changed = true;
        if (eliminated || succession || (!instance->holder && (table.winner || instance->election.round == 1)))
            settle_election(instance, &table, instance->election.round,
                            eliminated ? "holder_eliminated" : succession ? "ruler_change" : "vacancy");
        free(table.rows);
    }
    if (today || campaign_date(&today))
        for (size_t i = 0; i < plugin.roster.count; ++i)
            if (plugin.instances[i].enabled) changed |= sync_leader_reign(&plugin.instances[i], today);
    if (changed) publish_campaign();
}

static void handle_panel_request(PanelRequest request, void *in_game, unsigned actor, ULONGLONG now) {
    const size_t pending = plugin.pending_instance;
    const PoliticalInstance *selected = selected_instance();
    switch (request) {
    case PANEL_REQUEST_CLOSE: close_instance_panel("close_button"); return;
    case PANEL_REQUEST_ESCAPE: close_instance_panel("escape"); return;
    case PANEL_REQUEST_CANCEL_SELECTOR: close_selector(); return;
    case PANEL_REQUEST_CHOOSE_INSTANCE:
        if (active_selector()->open && pending < plugin.roster.count) {
            const PoliticalType *type = &plugin.types[active_selector()->type_index];
            const InstanceChoices choices = type_choices(type, actor);
            if (!choice_available(&choices, pending)) {
                update_selector(actor);
                return;
            }
            if (plugin.visible && pending == plugin.selected) close_selector();
            else if (open_instance_panel(pending, in_game, "instance_selector")) plugin.next_update = now + 750;
        }
        return;
    case PANEL_REQUEST_TYPE_ICON:
    case PANEL_REQUEST_SWITCH_INSTANCE: {
        const bool from_panel = request == PANEL_REQUEST_SWITCH_INSTANCE;
        if (from_panel && (!plugin.visible || !selected)) return;
        const size_t index = from_panel ? (size_t)(selected->type - plugin.types) : pending;
        if (index >= plugin.type_count) return;
        if (active_selector()->open && active_selector()->type_index == index) {
            close_selector();
            return;
        }
        const PoliticalType *type = &plugin.types[index];
        const InstanceChoices choices = type_choices(type, actor);
        const EntryRoute route = entry_route(&choices);
        if (route == ENTRY_HIDDEN || route == ENTRY_DISABLED) return;
        if (!from_panel && plugin.visible && selected && selected->type == type) {
            if (type->definition.entry.toggle) close_instance_panel("type_icon");
            else {
                close_selector();
                window_visible(true);
            }
        } else if (route == ENTRY_SELECTOR) open_selector(index, &choices);
        else if (!from_panel && open_instance_panel(choices.items[0], in_game, "type_icon")) plugin.next_update = now + 750;
        return;
    }
    case PANEL_REQUEST_NONE: return;
    default: break;
    }
    const bool shortcut = request == PANEL_REQUEST_SHORTCUT;
    const size_t target = shortcut ? plugin.selected : pending;
    if (target >= plugin.roster.count || !plugin.instances[target].enabled) return;
    const EntryButton *entry = &plugin.instances[target].type->definition.entry;
    if (request == PANEL_REQUEST_ICON || shortcut) {
        const EntryAccess access = instance_access(target, actor);
        if (!access.visible || !access.enabled) return;
    }
    const char *source = shortcut ? "shortcut" : request == PANEL_REQUEST_ICON ? "icon" :
                         request == PANEL_REQUEST_PROVINCE ? "province_badge" : "accession_message";
    if (!plugin.visible || plugin.selected != target) {
        if (open_instance_panel(target, in_game, source)) plugin.next_update = now + 750;
    } else if (shortcut || (request == PANEL_REQUEST_ICON && (!entry->defined || entry->toggle))) {
        close_instance_panel(source);
    }
}

static void update_entry_windows(unsigned actor) {
    for (size_t i = 0; i < plugin.roster.count; ++i)
        if (plugin.instances[i].enabled && !plugin.instances[i].type->definition.script_type) show_hud(i, actor);
    update_type_huds(actor);
    if (plugin.visible) {
        if (!instance_access(plugin.selected, actor).view) close_instance_panel("view_lost");
        else if (panel_view()->switch_button) {
            const InstanceChoices choices = type_choices(selected_instance()->type, actor);
            control_visible(panel_view()->switch_button, choices.count > 1);
            button_enabled(panel_view()->switch_button, choices.enabled_count > 0);
        }
    }
    update_selector(actor);
}

static bool sync_message_priority(void *in_game) {
    if (!in_game) return false;
    const bool visible = raise_native_messages(&plugin.messages, plugin.gui);
    if (visible == plugin.message_priority) return visible;
    plugin.message_priority = visible;
    if (plugin.visible || active_selector()->open)
        log_message(ANVIL_LOG_INFO, visible ? "political message_priority=above_panel" : "political message_priority=closed");
    return visible;
}

static void on_frame(const char *event, const void *payload, void *context) {
    (void)event;
    (void)payload;
    (void)context;
    if ((!plugin.active && !plugin.reset_windows) || !plugin.root || plugin.root != *plugin.root_slot) return;
    void *in_game = game_gui_context();
    if (plugin.reset_windows && in_game) {
        close_instance_panel("session");
        update_province_badge(0);
        for (size_t i = 0; i < plugin.roster.count; ++i)
            if (plugin.entries[i].visible) hud_visible(&plugin.entries[i], false);
        update_type_huds(0);
        plugin.reset_windows = false;
        plugin.message_priority = false;
    }
    if (!plugin.active) return;

    const ULONGLONG now = GetTickCount64();
    const bool message_was_visible = plugin.message_priority;
    const bool message_visible = sync_message_priority(in_game);
    if (!plugin.ready) {
        const WindowRetirement retirement = retire_dynamic_windows();
        if (retirement == RETIRE_PENDING) return;
        if (retirement == RETIRE_UNAVAILABLE) {
            log_message(ANVIL_LOG_WARNING, "political restore=native_window_retirement_failed");
            plugin.active = false;
            return;
        }
        AnvilGamePlayer player = { .size = sizeof(player) };
        if (plugin.world->player_tags(&player) != ANVIL_OK || !player.own.text[0]) return;
        uint32_t today;
        if (!campaign_date(&today)) return;
        if (!initialize_campaign(player.own.text, today)) {
            plugin.active = false;
            return;
        }
        plugin.next_update = now + 750;
    } else if (in_game) {
        // Wait until a regular frame after campaign readiness, outside game loading.
        if (now >= plugin.next_entry_check) {
            update_entry_windows(reform_actor());
            plugin.next_entry_check = now + 175;
        } else if (active_selector()->open && read_list(&active_selector()->list)) {
            draw_selector();
        }
        update_province_badge(viewed_province());
    }
    if (now >= resource_next_check || !resource_fact_day || selection_requests.count) {
        unsigned day;
        if (campaign_date(&day)) {
            pulse_decree_removals(day);
            sync_native_passives(day);
            refresh_resource_world(day);
            pulse_lifecycle(day);
            if (selection_requests.count) drain_selections(day);
            pulse_processes(day, false);
            for (size_t i = 0; i < plugin.roster.count; ++i) {
                PoliticalInstance *instance = &plugin.instances[i];
                if (!instance->enabled) continue;
                for (unsigned j = 0; j < instance->type->definition.role_count; ++j) settle_contest(instance, j, day);
                settle_contest(instance, PS_MAX_ROLES, day);
            }
        }
        resource_next_check = now + 175;
    }
    if (queued_actions.count) drain_actions();
    if (have_native_passives) {
        unsigned day;
        if (campaign_date(&day)) sync_native_passives(day);
    }

    DWORD foreground_pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &foreground_pid);
    const bool foreground = foreground_pid == GetCurrentProcessId() && accepts_shortcuts();
    const bool chord = foreground && (GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState(VK_SHIFT) & 0x8000);
    const bool open = chord && (GetAsyncKeyState('J') & 0x8000);
    const bool escape = !wheel_window && foreground && (GetAsyncKeyState(VK_ESCAPE) & 0x8000);
    const bool just_open = open && !plugin.open_held;
    const bool just_escape = escape && !plugin.escape_held;
    plugin.open_held = open;
    plugin.escape_held = escape;

    PanelRequest request = message_visible ? PANEL_REQUEST_NONE : plugin.pending_panel;
    if (!message_visible) plugin.pending_panel = PANEL_REQUEST_NONE;
    if (!message_visible && !message_was_visible && request == PANEL_REQUEST_NONE) {
        if (active_selector()->open && just_escape) request = PANEL_REQUEST_CANCEL_SELECTOR;
        else if (plugin.visible && just_escape && panel_view()->vote.open)
            panel_view()->vote.input = (VoteInput){ .action = V_CLOSE };
        else if (plugin.visible && just_escape && panel_view()->reform_view.confirmed_for)
            panel_view()->reform_view.pending = REFORM_INPUT_CANCEL;
        else if (plugin.visible && just_escape) {
            const KhanDefinition *definition = &selected_instance()->type->definition;
            if ((!definition->script_type && !definition->entry.defined) || definition->entry.close_on_escape)
                request = PANEL_REQUEST_ESCAPE;
        }
        else if (just_open) request = PANEL_REQUEST_SHORTCUT;
    }
    if (request != PANEL_REQUEST_NONE && in_game) {
        handle_panel_request(request, in_game, reform_actor(), now);
    } else if (in_game && plugin.visible && !message_visible) {
        const char *peer = native_peer(in_game, false);
        if (peer) {
            close_instance_panel(peer);
        } else if (!active_selector()->open) {
            process_vote_input();
            if (process_reform_input()) render_reform_view(false);
            if (read_list(&panel_view()->roles.list)) render_roles(selected_instance(), false);
            if (read_list(&panel_view()->resources.list)) render_resources(selected_instance(), reform_actor(), false);
            if (read_list(&panel_view()->actions.list)) render_actions(selected_instance(), reform_actor(), false);
            if (!open_pending_diplomacy()) read_scrollbar();
        }
    }
    if (now >= plugin.next_update) {
        update_campaign_holders();
        if (in_game && plugin.visible) refresh(false);
        plugin.next_update = now + 750;
    }
    if (in_game) {
        deliver_accession();
        if (!native_message_visible(&plugin.messages, plugin.gui)) deliver_invitation();
        sync_message_priority(in_game);
    }
}

static void on_resource_month(const char *event, const void *payload, void *context) {
    (void)event;
    (void)context;
    const AnvilGameDateChange *change = payload;
    if (!political_campaign_active() || !change || change->size < sizeof(*change)) return;
    const AnvilGameDate *date = &change->date;
    refresh_resource_world((uint32_t)date->year * 10000 + date->month * 100 + date->day);
}

static void on_action_day(const char *event, const void *payload, void *context) {
    (void)event;
    (void)context;
    const AnvilGameDateChange *change = payload;
    if (!political_campaign_active() || !change || change->size < sizeof(*change)) return;
    const AnvilGameDate *date = &change->date, *previous = &change->previous;
    const unsigned day = (unsigned)date->year * 10000 + date->month * 100 + date->day;
    const unsigned before = (unsigned)previous->year * 10000 + previous->month * 100 + previous->day;
    const unsigned player = reform_actor();
    pulse_processes(day, true);
    for (size_t i = 0; i < plugin.roster.count; ++i) {
        PoliticalInstance *instance = &plugin.instances[i];
        if (!instance->enabled) continue;
        const KhanDefinition *d = &instance->type->definition;
        for (unsigned j = 0; j < instance->actions.count; ++j) {
            const PoliticalActionRun *run = &instance->actions.runs[j];
            if (run->expires > before && run->expires <= day) {
                char line[352];
                snprintf(line, sizeof(line), "political action expiry instance=%s id=%s date=%u", instance->spec->id,
                         d->actions[run->slot].id, run->expires);
                log_message(ANVIL_LOG_INFO, line);
                plugin.next_update = 0;
            }
        }
        if (!player || !instance->holder || instance->holder == player) continue;
        int32_t best_weight = 0;
        unsigned best = d->action_count;
        for (unsigned j = 0; j < d->action_count; ++j) {
            const PoliticalAction *action = &d->actions[j];
            const NativeScope scope = {.root = instance->holder, .instance = instance->spec->id};
            const int32_t weight = action->ai_rule.program ?
                native_eu4_weight(&action->ai_rule, &scope, live_holder_tag(scope.root), 0) : action->ai_weight;
            if (weight <= 0 || weight < best_weight) continue;
            if (weight == best_weight && strcmp(d->actions[j].id, d->actions[best].id) >= 0) continue;
            ActionPlan plan;
            if (prepare_action(instance, j, instance->holder, 0, day, &plan)) continue;
            best_weight = weight;
            best = j;
        }
        if (best < d->action_count)
            enqueue_action((ActionRequest){.instance=i, .slot=best, .actor=instance->holder, .source=ACTION_AI});
    }
}

static void on_session(const char *event, const void *payload, void *context) {
    (void)event;
    (void)context;
    const AnvilGameSession *session = payload;
    if (have_native_passives) replace_native_passives((NativePassives){0}, NULL);
    native_passives_day = 0;
    native_passives_dirty = true;
    while (plugin.first_notice) discard_first_notice();
    resource_fact_day = 0;
    resource_next_check = 0;
    last_player_war_block = 0;
    queued_actions.count = 0;
    selection_requests.count = 0;
    memset(country_facts, 0, sizeof(country_facts));
    plugin.active = plugin.scripts_loaded && session->change == ANVIL_GAME_SESSION_STARTED &&
                    !session->ironman && !session->multiplayer;
    plugin.ready = false;
    publish_native_save(NULL, NULL);
    release_shogunate(&plugin.current);
    plugin.retained.count = 0;
    plugin.enabled_count = 0;
    for (size_t i = 0; i < plugin.type_count; ++i) plugin.types[i].entry_logged = false;
    for (size_t i = 0; i < plugin.roster.count; ++i) {
        release_roll(&plugin.instances[i].roll);
        plugin.instances[i].holder = 0;
        plugin.instances[i].history_date = 0;
        plugin.instances[i].enabled = false;
        plugin.instances[i].has_members = false;
        plugin.instances[i].condition_error = false;
        plugin.instances[i].members = (MemberTags){0};
    }
    plugin.top_row = plugin.max_top_row = 0;
    clear_panel_input();
    plugin.pending_panel = PANEL_REQUEST_NONE;
    plugin.pending_instance = 0;
    plugin.next_entry_check = plugin.next_update = 0;
    plugin.open_held = plugin.escape_held = false;
    plugin.reset_windows = true;
    if (plugin.active) log_message(ANVIL_LOG_INFO, "great_khan session started; HUD waits for campaign readiness");
}

typedef struct ModScript {
    char *bytes;
    size_t size;
    const char *source;
} ModScript;

static void log_file_error(const char *path, const char *source, const ScriptError *error) {
    if (error->path[0] && strcmp(path, error->path)) {
        path = error->path;
        source = plugin.files.source(path);
    }
    char message[1024];
    snprintf(message, sizeof(message), "great_khan %.259s:%u:%u source=\"%.500s\": %.191s",
             path, error->line, error->column, source ? source : "active mod search path", error->reason);
    log_message(ANVIL_LOG_ERROR, message);
}

static bool read_mod_script(const char *path, ModScript *script, ScriptError *error) {
    enum { MAX_SCRIPT_SIZE = 1024 * 1024 };
    script->source = plugin.files.source(path);
    void *file = script->source ? plugin.files.open(path) : NULL;
    if (!file) {
        strcpy(error->reason, script->source ? "unable to open mounted file" : "file missing from enabled mods");
        return false;
    }
    const int64_t size = plugin.files.size(file);
    char *bytes = size >= 0 && size <= MAX_SCRIPT_SIZE ? malloc((size_t)size + 1) : NULL;
    const bool complete = bytes && plugin.files.read(file, bytes, (uint64_t)size) == size;
    const bool closed = plugin.files.close(file) != 0;
    if (!complete || !closed) {
        free(bytes);
        strcpy(error->reason, size > MAX_SCRIPT_SIZE ? "script exceeds 1 MiB" : "unable to read mounted file");
        return false;
    }
    bytes[(size_t)size] = '\0';
    script->bytes = bytes;
    script->size = (size_t)size;
    return true;
}

static int fragment_order(const void *a, const void *b);

static void history_text(char *out, size_t capacity, const ScriptItem *item) {
    if (!item || item->block || !item->word[0]) return;
    const char *text = item->word;
    char utf8[4 * GK_SCRIPT_WORD_SIZE];
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0)) {
        wchar_t wide[GK_SCRIPT_WORD_SIZE];
        if (!MultiByteToWideChar(1252, 0, text, -1, wide, GK_SCRIPT_WORD_SIZE) ||
            !WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, sizeof(utf8), NULL, NULL)) return;
        text = utf8;
    }
    size_t length = strlen(text);
    if (length >= capacity) {
        length = capacity - 1;
        while (length && ((unsigned char)text[length] & 0xc0) == 0x80) --length;
    }
    memcpy(out, text, length);
    out[length] = 0;
}

static void read_historical_monarch(const ScriptTree *tree, LeaderReign *reign) {
    const ScriptItem *root = tree->items;
    const ScriptItem *monarch = script_field(tree, root, "monarch");
    unsigned best = 0;
    for (unsigned i = root->first; i; i = tree->items[i].next) {
        const ScriptItem *day = &tree->items[i];
        const unsigned date = day->block ? date_key(day->key) : 0;
        if (!date || date < best || date > reign->start) continue;
        const ScriptItem *found = script_field(tree, day, "monarch");
        if (found && found->block) {
            monarch = found;
            best = date;
        }
    }
    if (monarch && monarch->block) {
        history_text(reign->name, sizeof(reign->name), script_field(tree, monarch, "name"));
        history_text(reign->dynasty, sizeof(reign->dynasty), script_field(tree, monarch, "dynasty"));
    }
}

static void historical_ruler(LeaderReign *reign) {
    if (!plugin.files.enumerate || !plugin.files.free_list) return;
    const char *directory = "history/countries";
    char **names = plugin.files.enumerate(directory);
    if (!names) return;
    for (size_t i = 0; names[i]; ++i) {
        const size_t length = strlen(names[i]);
        if (length <= 4 || strcmp(names[i] + length - 4, ".txt") ||
            khanate_history_key(names[i], true) != reign->country) continue;
        char path[2 * PS_CONTENT_PATH_SIZE];
        const int width = snprintf(path, sizeof(path), "%s/%s", directory, names[i]);
        if (width < 0 || width >= (int)sizeof(path)) continue;
        ModScript file = {0};
        ScriptError error = {0};
        if (!read_mod_script(path, &file, &error)) continue;
        ScriptTree tree;
        if (script_tree_read(&tree, file.bytes, file.size, path, &error)) {
            read_historical_monarch(&tree, reign);
            script_tree_free(&tree);
        }
        free(file.bytes);
        if (reign->name[0]) break;
    }
    plugin.files.free_list(names);
}

static bool load_history_flags(const char *directory, bool countries, const char *keyword, bool mounted_history,
                               KhanateHistoryFiles *output) {
    char **names = plugin.files.enumerate(directory);
    ScriptError error = { .line = 1, .column = 1 };
    if (!names) {
        strcpy(error.reason, "cannot enumerate mounted history files");
        log_file_error(directory, NULL, &error);
        return false;
    }
    size_t count = 0;
    while (names[count] && count < GK_MAX_TIMELINE_ENTRIES) ++count;
    if (!names[count] && count) output->items = calloc(count, sizeof(*output->items));
    bool ok = !names[count] && (!count || output->items);
    if (!ok) {
        strcpy(error.reason, "too many history files or failed to allocate history");
        log_file_error(directory, NULL, &error);
    }
    if (ok && count > 1) qsort(names, count, sizeof(*names), fragment_order);
    for (size_t i = 0; ok && i < count; ++i) {
        const char *name = names[i];
        if (i && !strcmp(names[i - 1], name)) continue;
        const size_t length = strlen(name);
        if (length <= 4 || strcmp(name + length - 4, ".txt")) continue;
        char path[2 * PS_CONTENT_PATH_SIZE];
        const int width = snprintf(path, sizeof(path), "%s/%s", directory, name);
        const uint32_t key = khanate_history_key(name, countries);
        if (width < 0 || width >= (int)sizeof(path) || !key) {
            if (mounted_history) continue;
            strcpy(error.reason, countries ? "expected a country-tag history filename" : "expected a province-ID history filename");
            log_file_error(directory, NULL, &error);
            ok = false;
            break;
        }
        KhanFlagTimeline timeline = {0};
        ModScript file = {0};
        ok = read_mod_script(path, &file, &error);
        if (ok && mounted_history) {
            if (strstr(file.bytes, keyword)) ok = read_mounted_flag_history(file.bytes, file.size, path, keyword, &timeline, &error);
        } else if (ok) ok = read_flag_history(file.bytes, file.size, keyword, &timeline, &error);
        free(file.bytes);
        if (!ok) {
            log_file_error(path, file.source, &error);
            break;
        }
        if (!timeline.count) continue;
        for (size_t prior = 0; prior < output->count; ++prior) {
            if (output->items[prior].key != key) continue;
            strcpy(error.reason, countries ? "country has multiple marked history files" : "province has multiple marked history files");
            log_file_error(path, file.source, &error);
            ok = false;
            break;
        }
        if (ok) output->items[output->count++] = (KhanateFlag){ .key = key, .timeline = timeline };
        else free(timeline.entries);
    }
    plugin.files.free_list(names);
    if (!ok) release_history_flags(output);
    return ok;
}

static bool load_khanate_flags(const char *book, bool countries, KhanateHistoryFiles *output) {
    char directory[PS_CONTENT_PATH_SIZE];
    snprintf(directory, sizeof(directory), "history/%s/%s", book, countries ? "countries" : "provinces");
    return load_history_flags(directory, countries, countries ? KHAN_TUMEN_FLAG : KHAN_PROVINCE_FLAG, false, output);
}

static bool load_type_history_flags(PoliticalType *type) {
    const KhanDefinition *d = &type->definition;
    if (d->territory_history_key[0] &&
        !load_history_flags("history/provinces", false, d->territory_history_key, true, &type->territory_history))
        return false;
    for (unsigned i = 0; i < d->role_count; ++i) {
        if (!d->roles[i].history_key[0]) continue;
        if (load_history_flags("history/countries", true, d->roles[i].history_key, true, &type->role_history[i])) continue;
        release_history_flags(&type->territory_history);
        for (unsigned j = 0; j < i; ++j) release_history_flags(&type->role_history[j]);
        return false;
    }
    return true;
}

static void load_base_roster(void) {
    ModScript file = {0};
    ScriptError error = { .line = 1, .column = 1 };
    if (!read_mod_script(PS_ROSTER_PATH, &file, &error)) {
        if (file.source) {
            log_file_error(PS_ROSTER_PATH, file.source, &error);
            plugin.roster_loaded = true;
        } else if (plugin.files.source(GK_DEFINITION_PATH)) {
            plugin.roster.items[0] = (PoliticalInstanceSpec){
                .id = PS_LEGACY_INSTANCE, .type = PS_LEGACY_TYPE,
                .history = PS_LEGACY_TYPE, .starting_authority = -1,
            };
            plugin.roster.count = 1;
            plugin.roster_loaded = true;
        }
        return;
    }
    plugin.roster_loaded = true;
    const bool parsed = read_roster(file.bytes, file.size, &plugin.roster, &error);
    free(file.bytes);
    if (!parsed) {
        plugin.roster.count = 0;
        log_file_error(PS_ROSTER_PATH, file.source, &error);
        return;
    }
    char message[1050];
    snprintf(message, sizeof(message), "political registry path=%s source=\"%.900s\" instances=%zu",
             PS_ROSTER_PATH, file.source, plugin.roster.count);
    log_message(ANVIL_LOG_INFO, message);
}

static int fragment_order(const void *a, const void *b) {
    return strcmp(*(char *const *)a, *(char *const *)b);
}

typedef bool (*ReadMountedScript)(const ModScript *file, const char *path, void *context, ScriptError *error);

static void scan_scripts(const char *directory, ReadMountedScript read, void *context) {
    if (!plugin.files.enumerate || !plugin.files.free_list) return;
    char **names = plugin.files.enumerate(directory);
    if (!names) return;
    size_t count = 0;
    while (names[count]) ++count;
    qsort(names, count, sizeof(*names), fragment_order);
    for (size_t i = 0; i < count; ++i) {
        const char *name = names[i];
        const char *ext = strrchr(name, '.');
        if (!ext || _stricmp(ext, ".txt") || strpbrk(name, "/\\")) continue;
        if (i && !strcmp(names[i - 1], name)) continue;
        char path[PS_FILE_PATH_SIZE];
        if (snprintf(path, sizeof(path), "%s/%s", directory, name) >= (int)sizeof(path)) {
            log_message(ANVIL_LOG_ERROR, "political content path too long");
            continue;
        }
        ModScript file = {0};
        ScriptError error = { .line = 1, .column = 1 };
        if (!read_mod_script(path, &file, &error) || !read(&file, path, context, &error))
            log_file_error(path, file.source, &error);
        free(file.bytes);
    }
    plugin.files.free_list(names);
}

static bool load_native_modifier_fragment(const ModScript *file, const char *path, void *context, ScriptError *error) {
    return collect_native_modifiers(context, file->bytes, file->size, path, error);
}

static AnvilResult modifier_id_at_startup(const char *name, int32_t *id) {
    // The world service is only callable during a campaign. Here vanilla is still initializing its databases.
    return startup_modifier_id(*plugin.modifier_defs_begin, *plugin.modifier_defs_end, name, id);
}

static void *before_static_modifiers(void) {
    if (!plugin.modifiers_loaded) {
        plugin.modifiers_loaded = true;
        scan_scripts(PS_NATIVE_MODIFIER_DIRECTORY, load_native_modifier_fragment, &plugin.modifiers);
        for (unsigned i = 0; i < plugin.modifiers.count; ++i) {
            NativeModifier *definition = &plugin.modifiers.items[i];
            ScriptError error = {0};
            if (!register_native_modifier(&plugin.modifier_api, definition, &error)) {
                log_file_error(definition->path, NULL, &error);
                continue;
            }
            char line[300];
            snprintf(line, sizeof(line), "political native modifier key=%s scope=%s id=%d",
                     definition->key, definition->scope == NATIVE_MODIFIER_COUNTRY ? "country" : "province", definition->id);
            log_message(ANVIL_LOG_INFO, line);
        }
    }
    return plugin.original_static_modifier_db();
}

static const char *check_growth_modifiers(const KhanDefinition *definition) {
    for (unsigned i = 0; i < definition->resource_count; ++i) {
        const ResourceDefinition *resource = &definition->resources[i];
        if (!resource->growth_modifier[0]) continue;
        const NativeModifier *modifier = find_native_modifier(&plugin.modifiers, resource->growth_modifier);
        if (!modifier || modifier->id < 0) return "growth_modifier requires a registered common/political_modifiers key";
        if (growth_modifier_is_local(resource) != (modifier->scope == NATIVE_MODIFIER_PROVINCE))
            return "growth_modifier scope does not match its declared source";
    }
    return NULL;
}

static bool load_roster_fragment(const ModScript *file, const char *path, void *context, ScriptError *error) {
    PoliticalRoster *roster = context;
    const size_t previous = roster->count;
    if (!append_roster(file->bytes, file->size, roster, error)) return false;
    for (size_t i = previous; i < roster->count; ++i) strcpy(roster->items[i].path, path);
    return true;
}

static bool load_history_fragment(const ModScript *file, const char *path, void *context, ScriptError *error) {
    return append_content_history(file->bytes, file->size, path, context, error);
}

static void load_instance_roster(void) {
    if (plugin.roster_loaded) return;
    load_base_roster();
    scan_scripts(PS_ROSTER_DIRECTORY, load_roster_fragment, &plugin.roster);
    scan_scripts(PS_HISTORY_DIRECTORY, load_history_fragment, &plugin.roster);
    plugin.roster_loaded |= plugin.roster.count != 0;
}

static void load_political_aliases(PsAliases *output) {
    load_instance_roster();
    ModScript file = {0};
    ScriptError error = { .line = 1, .column = 1 };
    if (!read_mod_script(PS_ALIASES_PATH, &file, &error)) {
        if (file.source) log_file_error(PS_ALIASES_PATH, file.source, &error);
        return;
    }
    const bool parsed = read_ps_aliases(file.bytes, file.size, output, &error);
    free(file.bytes);
    if (!parsed) {
        output->count = 0;
        log_file_error(PS_ALIASES_PATH, file.source, &error);
        return;
    }
    char message[1250];
    snprintf(message, sizeof(message), "political script aliases=%s source=\"%.980s\" entries=%zu",
             PS_ALIASES_PATH, file.source, output->count);
    log_message(ANVIL_LOG_INFO, message);
}

static bool subject_type_exists(const char *id) {
    const void *registry = *plugin.subject_types_db;
    if (!registry) return false;
    GameString key = { .capacity = 15 };
    plugin.assign_string(&key, id, strlen(id));
    const bool exists = plugin.lookup_subject_type(registry, &key) != NULL;
    plugin.free_string(&key);
    return exists;
}

static void load_reform_definition(PoliticalType *type) {
    char path[PS_CONTENT_PATH_SIZE];
    snprintf(path, sizeof(path), "common/political_systems/reforms/%s.txt", type->id);
    ModScript file = {0};
    ScriptError error = { .line = 1, .column = 1 };
    if (!read_mod_script(path, &file, &error)) {
        if (file.source) log_file_error(path, file.source, &error);
        return;
    }
    const bool parsed = read_type_reform(file.bytes, file.size, type->id, &type->reform, &error);
    free(file.bytes);
    if (!parsed) {
        log_file_error(path, file.source, &error);
        return;
    }
    strcpy(type->reform.path, path);
    char message[1500];
    snprintf(message, sizeof(message), "political script reform=%s source=\"%.900s\" id=%s "
             "initial_authority=%d cost=%d add_prestige=%d",
             path, file.source, type->reform.id, type->definition.starting_authority,
             type->reform.cost, type->reform.prestige);
    log_message(ANVIL_LOG_INFO, message);
}

static void localize_key(char out[GK_SCRIPT_WORD_SIZE], const char *key) {
    if (!key[0]) return;
    GameString label = { .capacity = 15 };
    plugin.localize(&label, key, NULL, 0);
    const char *translated = string_text(&label);
    if (translated && label.size && label.size < GK_SCRIPT_WORD_SIZE)
        memcpy(out, translated, label.size + 1);
    plugin.free_string(&label);
}

static void resolve_political_titles(PoliticalType *type) {
    KhanDefinition *definition = &type->definition;
    const char *leader = definition->emperor;
    const char *key = !leader[0] ? "PS_LEADER_TITLE" : ps_identifier(leader) ? leader : NULL;
    if (key) localize_key(type->emperor_title, key);
    if (!key || !type->emperor_title[0] || !strcmp(type->emperor_title, key))
        format_title(type->emperor_title, leader[0] ? leader : "leader");
    if (definition->entry.title_key[0]) localize_key(definition->title, definition->entry.title_key);
    else if (ps_identifier(definition->title)) localize_key(definition->title, definition->title);
    if (!definition->title[0]) format_title(definition->title, type->id);
}

static bool load_type_fragment(const ModScript *file, const char *path, void *context, ScriptError *error) {
    return collect_content_types(context, file->bytes, file->size, path, error);
}

static bool load_layout_fragment(const ModScript *file, const char *path, void *context, ScriptError *error) {
    return collect_layouts(context, file->bytes, file->size, path, error);
}

static bool load_text_fragment(const ModScript *file, const char *path, void *context, ScriptError *error) {
    return collect_text_templates(context, file->bytes, file->size, path, error);
}

static bool mounted_file(const char *path) { return plugin.files.source(path) != NULL; }

static bool read_mounted_tree(const char *path, ScriptTree *tree, ScriptError *error) {
    ModScript source = {0};
    if (!read_mod_script(path, &source, error)) return false;
    const bool read = script_tree_read(tree, source.bytes, source.size, path, error);
    if (read) tree->owned_text = source.bytes;
    else free(source.bytes);
    return read;
}

static void load_layouts(void) {
    scan_scripts(PS_LAYOUT_DIRECTORY, load_layout_fragment, &plugin.layouts);
    for (size_t i = 0; i < plugin.layouts.count; ++i) {
        PoliticalLayout *layout = &plugin.layouts.items[i];
        ScriptTree tree = {0};
        ScriptError error = { .line = 1, .column = 1 };
        layout->valid = read_mounted_tree(layout->gui_file, &tree, &error) &&
                        check_layout_gui(layout, &tree, &error);
        if (!layout->valid) log_file_error(layout->gui_file, plugin.files.source(layout->gui_file), &error);
        script_tree_free(&tree);
    }
    for (size_t i = 0; i < plugin.layouts.hud_count; ++i) {
        HudRegion *region = &plugin.layouts.hud[i];
        ScriptTree tree = {0};
        ScriptError error = { .line = 1, .column = 1 };
        region->valid = read_mounted_tree(region->gui_file, &tree, &error) &&
                        check_hud_gui(region, &tree, &error);
        if (!region->valid) log_file_error(region->gui_file, plugin.files.source(region->gui_file), &error);
        script_tree_free(&tree);
        if (!region->valid) continue;
        error = (ScriptError){ .line = 1, .column = 1 };
        region->valid = read_mounted_tree(region->gfx_file, &tree, &error) &&
                        check_hud_gfx(region, &tree, mounted_file, &error);
        if (!region->valid) log_file_error(region->gfx_file, plugin.files.source(region->gfx_file), &error);
        script_tree_free(&tree);
    }
    if (plugin.layouts.hud_count) {
        ScriptTree tree = {0};
        ScriptError error = { .line = 1, .column = 1 };
        plugin.selector_gui_valid = read_mounted_tree(PS_SELECTOR_GUI, &tree, &error) &&
                                    check_gui_window_tooltips(&tree, PS_SELECTOR_WINDOW, &error);
        if (!plugin.selector_gui_valid)
            log_file_error(PS_SELECTOR_GUI, plugin.files.source(PS_SELECTOR_GUI), &error);
        script_tree_free(&tree);
    }
}

static const char *resolve_type_layout(PoliticalType *type) {
    const KhanDefinition *definition = &type->definition;
    const EntryButton *entry = &definition->entry;
    if (definition->abstract || !definition->script_type) return NULL;
    bool has_actions_ui = false;
    for (unsigned i = 0; i < definition->action_count; ++i) has_actions_ui |= definition->actions[i].shown;
    if (has_actions_ui && !entry->layout[0]) return "actions_region requires interface.layout";
    if (entry->layout[0]) {
        const int mapped = layout_index(&plugin.layouts, entry->layout);
        if (mapped < 0 || !plugin.layouts.items[mapped].valid) return "missing or invalid interface.layout";
        const PoliticalLayout *layout = &plugin.layouts.items[mapped];
        if ((entry->close_button && !layout->close[0]) ||
            (definition->leader_enabled && !layout->leader[0]) ||
            (definition->authority_enabled && !layout->authority[0] && !layout->resources.rows) ||
            (definition->member_source != MEMBERS_UNSET && !layout->members.rows) || (has_actions_ui && !layout->actions.rows))
            return "layout is missing a control required by an enabled module";
        type->layout = layout;
    }
    if (!entry->defined) return NULL;
    const int region = hud_index(&plugin.layouts, entry->region);
    if (region < 0 || !plugin.layouts.hud[region].valid) return "missing or invalid entry_button.region";
    const int icon = hud_icon_index(&plugin.layouts.hud[region], entry->icon);
    if (icon < 0) return "entry_button.icon is not bound by the HUD region's .gui and .gfx files";
    type->hud = region;
    type->sprite = icon;
    return NULL;
}

static void report_type_error(const ScriptError *error) {
    log_file_error(error->path, plugin.files.source(error->path), error);
}

static bool release_native_clause(NativeClause *clause, NativeClauseKind kind, ScriptError *error) {
    (void)kind;
    (void)error;
    release_native_eu4(clause);
    return true;
}

static bool compile_content_clause(NativeClause *clause, NativeClauseKind kind, ScriptError *error) {
    if (clause->text && (kind == NATIVE_COUNTRY_MODIFIER || kind == NATIVE_PROVINCE_MODIFIER) &&
        !native_passives_available(kind)) {
        *error = (ScriptError){.line = clause->line, .column = clause->column};
        snprintf(error->path, sizeof(error->path), "%s", clause->path);
        strcpy(error->reason, "native modifier aggregation unavailable on this build");
        return false;
    }
    return compile_native_eu4(clause, kind, error);
}

static const PoliticalType *load_type(const char *id, ContentTypes *content) {
    for (size_t i = 0; i < plugin.type_count; ++i)
        if (!strcmp(id, plugin.types[i].id)) return &plugin.types[i];
    if (plugin.type_count == PS_MAX_INSTANCES) {
        log_message(ANVIL_LOG_ERROR, "political type registry is full");
        return NULL;
    }
    PoliticalType *type = &plugin.types[plugin.type_count++];
    *type = (PoliticalType){ .hud = -1, .sprite = -1 };
    strcpy(type->id, id);
    char path[PS_CONTENT_PATH_SIZE];
    snprintf(path, sizeof(path), "common/political_systems/%s.txt", id);

    ModScript definition_file = {0};
    ScriptError error = { .line = 1, .column = 1 };
    KhanDefinition *definition = &type->definition;
    if (has_content_type(content, id)) {
        const KhanDefinition *resolved = resolve_content_type(content, id, &error);
        if (!resolved) {
            log_file_error(path, NULL, &error);
            return type;
        }
        *definition = *resolved;
        const char *reason = resolve_type_layout(type);
        if (reason) {
            error = (ScriptError){ .line = definition->source_line, .column = 1 };
            snprintf(error.reason, sizeof(error.reason), "%.100s (%.76s)", reason, id);
            log_file_error(definition->source_path, plugin.files.source(definition->source_path), &error);
            return type;
        }
        if (!visit_native_clauses(definition, compile_content_clause, &error)) {
            report_type_error(&error);
            visit_native_clauses(definition, release_native_clause, NULL);
            return type;
        }
        for (unsigned i = 0; i < definition->action_count; ++i)
            for (unsigned j = 0; j < ACTION_MODIFIER_COUNT; ++j)
                type->native_passives |= definition->actions[i].modifiers[j].program != NULL;
        have_native_passives |= type->native_passives;
    } else {
        if (!read_mod_script(path, &definition_file, &error)) {
            log_file_error(path, definition_file.source, &error);
            return type;
        }
        const bool parsed = read_type_definition(definition_file.bytes, definition_file.size, id, definition, &error);
        free(definition_file.bytes);
        if (!parsed) {
            log_file_error(path, definition_file.source, &error);
            return type;
        }
    }
    if (definition->member_source == MEMBERS_DIRECT_SUBJECTS && definition->subject_type[0] &&
        !subject_type_exists(definition->subject_type)) {
        error.line = definition->subject_type_line;
        error.column = definition->subject_type_column;
        strcpy(error.path, definition->subject_type_path);
        snprintf(error.reason, sizeof(error.reason), "unknown members.subject_type '%s'", definition->subject_type);
        log_file_error(path, definition_file.source, &error);
        return type;
    }
    const char *modifier_problem = check_growth_modifiers(definition);
    if (modifier_problem) {
        error = (ScriptError){ .line = definition->source_line, .column = 1 };
        snprintf(error.reason, sizeof(error.reason), "%.120s (%.64s)", modifier_problem, id);
        log_file_error(definition->source_path, NULL, &error);
        return type;
    }
    if (!definition->abstract && !load_type_history_flags(type)) return type;
    type->usable = true;
    resolve_political_titles(type);
    char message[1570];
    snprintf(message, sizeof(message), "political script definition=%s source=\"%.960s\" emperor=%s subject_type=%s",
             path, definition_file.source ? definition_file.source : "mounted types",
             definition->emperor, definition->subject_type);
    log_message(ANVIL_LOG_INFO, message);
    if (!definition->script_type && definition->authority_enabled) load_reform_definition(type);
    return type;
}

static void load_concrete_type(const char *id, void *context) {
    const PoliticalType *type = load_type(id, context);
    if (type && type->usable) plugin.scripts_loaded = true;
}

static bool load_instance_history(PoliticalInstance *instance) {
    const PoliticalInstanceSpec *spec = instance->spec;
    const KhanDefinition *definition = &instance->type->definition;
    if (spec->date) {
        const char *reason = NULL;
        if (!definition->script_type || definition->abstract || !definition->history_allowed)
            reason = "history requires a concrete type with allow_history=yes";
        else if (spec->leader[0] && !definition->leader_enabled) reason = "history names a leader but leader is disabled";
        else if (spec->members_written && !records_members(definition->member_source))
            reason = "history members require an explicit membership source";
        else if (spec->territory_written && !definition->territory_enabled)
            reason = "history territory requires the territory module";
        else if (spec->starting_authority >= 0 && !definition->authority_enabled)
            reason = "history authority requires an enabled authority account";
        for (unsigned i = 0; !reason && i < spec->balance_count; ++i) {
            const ResourceInitial *start = &spec->balances[i];
            reason = history_balance_problem(definition, start->id, start->balance);
        }
        if (reason) {
            instance_history_error(spec, NULL, reason);
            return false;
        }
        for (size_t i = 0; i < spec->change_count; ++i) {
            const PoliticalHistoryChange *change = &spec->changes[i];
            switch (change->kind) {
            case HISTORY_LEADER:
                if (change->key && !definition->leader_enabled) reason = "history names a leader but leader is disabled";
                break;
            case HISTORY_CLEAR_MEMBERS:
            case HISTORY_MEMBER:
                if (!records_members(definition->member_source)) reason = "history members require explicit or mixed membership";
                break;
            case HISTORY_CLEAR_TERRITORY:
            case HISTORY_PROVINCE:
                if (!definition->territory_enabled) reason = "history provinces require the territory module";
                break;
            case HISTORY_BALANCE:
                reason = history_balance_problem(definition, change->id, change->amount);
                break;
            case HISTORY_ROLE:
                if (role_index(instance, change->id) == definition->role_count) reason = "history refers to an unknown role";
                break;
            }
            if (reason) {
                instance_history_error(spec, change, reason);
                return false;
            }
        }
        localize_key(instance->title, spec->title);
        char message[610];
        snprintf(message, sizeof(message), "political history path=%s:%u instance=%s type=%s date=%u dated_changes=%zu",
                 spec->path, spec->line, spec->id, spec->type, spec->date, spec->change_count);
        log_message(ANVIL_LOG_INFO, message);
        return true;
    }
    if (definition->script_type) {
        log_message(ANVIL_LOG_ERROR, "political type requires named dated history");
        return false;
    }
    strcpy(instance->title, spec->title);
    char path[PS_CONTENT_PATH_SIZE];
    const bool capital = uses_capital_members(instance);
    if (capital && strcmp(instance->spec->history, GK_KHANATE_HISTORY)) {
        log_message(ANVIL_LOG_ERROR, "khanate capital history requires history = GKhanate");
        return false;
    }
    snprintf(path, sizeof(path), "history/%s/%s.txt", capital ? "diplomacy" : "political_systems", instance->spec->history);
    ModScript history_file = {0};
    ScriptError error = { .line = 1, .column = 1 };
    if (!read_mod_script(path, &history_file, &error)) {
        log_file_error(path, history_file.source, &error);
        return false;
    }
    const bool read = read_holder_history(history_file.bytes, history_file.size, capital ? KHAN_HOLDER_FLAG : NULL,
                                         &instance->timeline, &error);
    free(history_file.bytes);
    if (!read) {
        log_file_error(path, history_file.source, &error);
        return false;
    }
    if (capital) {
        KhanateHistory *history = &instance->khanate_history;
        if (!load_khanate_flags(instance->spec->history, false, &history->provinces) ||
            !load_khanate_flags(instance->spec->history, true, &history->countries)) {
            release_khanate_history(history);
            free(instance->timeline.entries);
            instance->timeline = (KhanTimeline){0};
            return false;
        }
        char data_message[320];
        snprintf(data_message, sizeof(data_message),
                 "khanate history instance=%s provinces=%zu tumen_files=%zu",
                 instance->spec->id, history->provinces.count, history->countries.count);
        log_message(ANVIL_LOG_INFO, data_message);
    }
    char message[1490];
    snprintf(message, sizeof(message), "political script history=%s source=\"%.1020s\" instance=%s entries=%zu",
             path, history_file.source, instance->spec->id, instance->timeline.count);
    log_message(ANVIL_LOG_INFO, message);
    return true;
}

static void on_ready(const char *event, const void *payload, void *context) {
    (void)event;
    (void)payload;
    (void)context;
    if (plugin.scripts_loaded) return;
    free_text_templates(&plugin.text_templates);
    scan_scripts(PS_TEXT_DIRECTORY, load_text_fragment, &plugin.text_templates);
    ScriptTree ballot = {0};
    ScriptError error = {0};
    if (plugin.nomination_hover_hook && mounted_file("interface/great_khan_history.gui") &&
        read_mounted_tree("interface/great_khan_history.gui", &ballot, &error)) {
        plugin.ballot_number_areas = read_number_areas(&ballot, "gk_ballot_view", "gk_opinion_", BALLOT_ROWS,
                                                       &plugin.ballot_area, plugin.ballot_opinions);
        script_tree_free(&ballot);
    }
    load_instance_roster();
    ContentTypes content = {0};
    scan_scripts(PS_TYPE_DIRECTORY, load_type_fragment, &content);
    load_layouts();
    plugin.type_count = 0;
    for (size_t i = 0; i < plugin.roster.count; ++i) {
        PoliticalInstance *instance = &plugin.instances[i];
        instance->spec = &plugin.roster.items[i];
        instance->type = load_type(instance->spec->type, &content);
        instance->configured = instance->type && instance->type->usable && load_instance_history(instance);
        if (instance->configured) plugin.scripts_loaded = true;
    }
    validate_unused_types(&content, report_type_error);
    visit_concrete_types(&content, load_concrete_type, &content);
    free_content_types(&content);
    for (size_t i = 0; i < plugin.roster.count; ++i) {
        if (!plugin.instances[i].configured || plugin.instances[i].type->definition.script_type) continue;
        const EntryButton *entry = &plugin.instances[i].type->definition.entry;
        const int order = entry->has_order ? entry->order : (int)i;
        unsigned slot = 0;
        for (size_t j = 0; j < plugin.roster.count; ++j) {
            if (!plugin.instances[j].configured || plugin.instances[j].type->definition.script_type) continue;
            const EntryButton *other = &plugin.instances[j].type->definition.entry;
            const int other_order = other->has_order ? other->order : (int)j;
            if (other_order < order || (other_order == order && j < i)) ++slot;
        }
        plugin.instances[i].hud_slot = slot;
    }
}

ANVIL_PLUGIN_EXPORT AnvilResult anvil_plugin_load(const AnvilHost *host, AnvilPlugin *self) {
    plugin.host = host;
    plugin.self = self;
    plugin.world = host->get_service(self, ANVIL_GAME_WORLD_SERVICE, ANVIL_GAME_WORLD_SERVICE_VERSION);
    plugin.game = host->get_service(self, ANVIL_GAME_SERVICE, ANVIL_GAME_SERVICE_VERSION);
    if (!plugin.world || !ANVIL_GAME_WORLD_HAS(plugin.world, player_tags) || !plugin.game) return ANVIL_ERROR_UNSUPPORTED;

    plugin.use_shields = _wgetenv(L"GREAT_KHAN_V055_TEXT") == NULL;
    uint64_t root_rva, db_rva, subject_types_rva, click_rva, modifiers_begin, modifiers_end;
    if (host->symbol_value(self, SYM_GAME_ROOT, "slot", &root_rva) != ANVIL_OK ||
        host->symbol_value(self, SYM_COUNTRIES_DB, "slot", &db_rva) != ANVIL_OK ||
        host->symbol_value(self, SYM_SUBJECT_TYPES_DB, "slot", &subject_types_rva) != ANVIL_OK ||
        host->symbol_value(self, SYM_CLICK_LISTENER, "table", &click_rva) != ANVIL_OK ||
        host->symbol_value(self, SYM_MODIFIER_DEFINITION_RANGE, "begin", &modifiers_begin) != ANVIL_OK ||
        host->symbol_value(self, SYM_MODIFIER_DEFINITION_RANGE, "end", &modifiers_end) != ANVIL_OK ||
        root_rva >= host->game->image_size || db_rva >= host->game->image_size ||
        subject_types_rva >= host->game->image_size || click_rva >= host->game->image_size ||
        modifiers_begin >= host->game->image_size || modifiers_end >= host->game->image_size)
        return ANVIL_ERROR_UNSUPPORTED;
    plugin.root_slot = (void *const *)(host->game->image_base + root_rva);
    plugin.countries_db = (void *const *)(host->game->image_base + db_rva);
    plugin.subject_types_db = (void *const *)(host->game->image_base + subject_types_rva);
    plugin.click_vtable = (const void *)(host->game->image_base + click_rva);
    plugin.modifier_defs_begin = (const void *const *)(host->game->image_base + modifiers_begin);
    plugin.modifier_defs_end = (const void *const *)(host->game->image_base + modifiers_end);
    plugin.create_window = host->address_of(self, SYM_CREATE_WINDOW);
    if (host->address_of(self, SYM_WINDOW_ICON_LOOKUP)) {
        plugin.window_icon_method = host->address_of(self, SYM_FIND_ICON);
        plugin.find_window_icon = host->address_of(self, SYM_FIND_EXISTING_WINDOW_ICON);
        plugin.set_icon_sprite = host->address_of(self, SYM_SET_ICON_SPRITE);
        plugin.icon_sprite_name = host->address_of(self, SYM_ICON_SPRITE_NAME);
    }
    plugin.shield_position = host->address_of(self, SYM_SHIELD_POSITION);
    plugin.shield_move = host->address_of(self, SYM_SHIELD_MOVE);
    plugin.label_position = host->address_of(self, SYM_LABEL_POSITION);
    plugin.label_move = host->address_of(self, SYM_LABEL_MOVE);
    plugin.set_ui_tooltip = host->address_of(self, SYM_SET_UI_TOOLTIP);
    plugin.retire_window = host->address_of(self, SYM_RETIRE_FIXED_WINDOW);
    plugin.set_text = host->address_of(self, SYM_SET_TEXT);
    plugin.assign_string = host->address_of(self, SYM_ASSIGN_STRING);
    plugin.append_string = host->address_of(self, SYM_APPEND_STRING);
    plugin.free_string = host->address_of(self, SYM_FREE_STRING);
    plugin.localize = host->address_of(self, SYM_LOCALIZE);
    uint64_t localisation_slot;
    plugin.lookup_localisation_raw = host->address_of(self, SYM_LOCALISATION_RAW_LOOKUP);
    if (plugin.lookup_localisation_raw &&
        host->symbol_value(self, SYM_LOCALISATION_DATABASE_SITE, "slot", &localisation_slot) == ANVIL_OK &&
        localisation_slot < host->game->image_size) {
        plugin.localisation_db = (const void *const *const *)(host->game->image_base + localisation_slot);
        plugin.text_engine = (TextEngine){.localize = plugin.localize, .assign = plugin.assign_string,
            .release = plugin.free_string, .lookup = raw_localisation, .problem = localisation_problem};
    } else log_message(ANVIL_LOG_WARNING, "political localisation parameters unavailable on this build");
    plugin.country_name = host->address_of(self, SYM_COUNTRY_NAME);
    plugin.describe_daimyo = host->address_of(self, SYM_DESCRIBE_DAIMYO);
    plugin.valid_tag = host->address_of(self, SYM_VALID_TAG);
    plugin.lookup_tag = host->address_of(self, SYM_LOOKUP_COUNTRY_TAG);
    plugin.religion_bound = host->address_of(self, SYM_COUNTRY_RELIGION_SITE) &&
                            host->address_of(self, SYM_RELIGION_KEY_SITE);
    plugin.capital_bound = host->address_of(self, SYM_COUNTRY_CAPITAL_SITE) != NULL;
    uint64_t discovery_root;
    if (!host->address_of(self, SYM_PROVINCE_DISCOVERY_ARRAY) || !host->address_of(self, SYM_PROVINCE_DISCOVERY_FLAGS) ||
        host->symbol_value(self, SYM_PROVINCE_DISCOVERY_ARRAY, "root", &discovery_root) != ANVIL_OK ||
        discovery_root != root_rva) return ANVIL_ERROR_NOT_FOUND;
    plugin.province_discovered = native_province_discovered;
    plugin.government_stack = host->address_of(self, SYM_COUNTRY_GOVERNMENT_STACK);
    plugin.government_kind = host->address_of(self, SYM_GOVERNMENT_CATEGORY);
    plugin.lookup_subject_type = host->address_of(self, SYM_LOOKUP_SUBJECT_TYPE);
    plugin.add_prestige = host->address_of(self, SYM_COUNTRY_ADD_PRESTIGE);
    plugin.get_prestige = host->address_of(self, SYM_COUNTRY_GET_PRESTIGE);
    plugin.files = (ModFiles){
        .source = host->address_of(self, SYM_MOD_FILE_SOURCE),
        .open = host->address_of(self, SYM_MOD_FILE_OPEN),
        .size = host->address_of(self, SYM_MOD_FILE_SIZE),
        .read = host->address_of(self, SYM_MOD_FILE_READ),
        .close = host->address_of(self, SYM_MOD_FILE_CLOSE),
        .enumerate = host->address_of(self, SYM_MOD_ENUMERATE),
        .free_list = host->address_of(self, SYM_MOD_FREE_LIST),
    };
    if (!ANVIL_GAME_WORLD_HAS(plugin.world, province_modifier) ||
        !plugin.world->country_modifier || !plugin.world->province_modifier)
        return ANVIL_ERROR_UNSUPPORTED;
    plugin.modifier_api = (ModifierRegistration){
        .lookup = modifier_id_at_startup, .assign = plugin.assign_string, .release = plugin.free_string,
        .intern = host->address_of(self, SYM_INTERN_MODIFIER_KEY),
        .construct = host->address_of(self, SYM_CONSTRUCT_MODIFIER),
        .append = host->address_of(self, SYM_REGISTER_MODIFIER),
    };
    if (!plugin.modifier_api.intern || !plugin.modifier_api.construct || !plugin.modifier_api.append)
        return ANVIL_ERROR_UNSUPPORTED;
    plugin.find_button = host->address_of(self, SYM_FIND_BUTTON);
    plugin.find_scrollbar = host->address_of(self, SYM_FIND_SCROLLBAR);
    plugin.open_diplomacy = host->address_of(self, SYM_OPEN_DIPLOMACY);
    plugin.native_panel_toggle[0] = host->address_of(self, SYM_TOGGLE_HRE_PANEL);
    plugin.native_panel_toggle[1] = host->address_of(self, SYM_TOGGLE_PAPACY_PANEL);
    plugin.native_panel_toggle[2] = host->address_of(self, SYM_TOGGLE_CHINA_PANEL);
    if (!bind_accession_messages(&plugin.messages,host,self)) return ANVIL_ERROR_UNSUPPORTED;
    if (!plugin.create_window || !plugin.set_text || !plugin.assign_string || !plugin.free_string ||
        !plugin.append_string || !plugin.localize || !plugin.country_name || !plugin.describe_daimyo ||
        !plugin.valid_tag || !plugin.lookup_tag || !plugin.find_button || !plugin.find_scrollbar ||
        !plugin.lookup_subject_type || !plugin.open_diplomacy || !plugin.files.source ||
        !plugin.files.open || !plugin.files.size || !plugin.files.read || !plugin.files.close ||
        !plugin.files.enumerate || !plugin.files.free_list ||
        !plugin.add_prestige || !plugin.get_prestige || !plugin.native_panel_toggle[0] ||
        !plugin.native_panel_toggle[1] || !plugin.native_panel_toggle[2])
        return ANVIL_ERROR_UNSUPPORTED;
    AnvilChange *change;
    AnvilResult result = host->hook_function(self, SYM_CHINA_GUI_INIT, (void *)capture_gui,
                                             (void **)&plugin.original_china_init, &change);
    if (result != ANVIL_OK) return result;
    result = host->redirect_call(self, SYM_STOCK_DIPLOMACY_HISTORY_PATH, (void *)stock_diplomacy_path,
                                 (void **)&plugin.original_diplomacy_path, NULL);
    if (result != ANVIL_OK) return result;
    result = host->redirect_call(self, SYM_PROVINCE_CARD_BIND, (void *)capture_province_card,
                                 (void **)&plugin.original_province_card_bind, NULL);
    if (result != ANVIL_OK) return result;
    result = host->redirect_call(self, SYM_STATIC_MODIFIER_DB_START, (void *)before_static_modifiers,
                                 (void **)&plugin.original_static_modifier_db, NULL);
    if (result != ANVIL_OK) return result;
    result = host->subscribe(self, ANVIL_GAME_EVENT_READY, on_ready, NULL);
    if (result != ANVIL_OK) return result;
    result = host->subscribe(self, ANVIL_GAME_EVENT_SESSION, on_session, NULL);
    if (result != ANVIL_OK) return result;
    result = host->subscribe(self, ANVIL_GAME_EVENT_FRAME, on_frame, NULL);
    if (result != ANVIL_OK) return result;
    result = host->subscribe(self, ANVIL_GAME_EVENT_MONTH, on_resource_month, NULL);
    if (result != ANVIL_OK) return result;
    result = host->subscribe(self, ANVIL_GAME_EVENT_DAY, on_action_day, NULL);
    if (result != ANVIL_OK) return result;
    if (!ANVIL_GAME_WORLD_HAS(plugin.world, war_side) || !plugin.world->war_side ||
        !plugin.world->war_ids || !plugin.world->war ||
        !bind_native_defense(&plugin.defense, host, self)) return ANVIL_ERROR_UNSUPPORTED;
    result = host->subscribe(self, ANVIL_GAME_EVENT_WAR, on_war_started, NULL);
    if (result != ANVIL_OK) return result;
    result = register_native_save(host, self);
    if (result != ANVIL_OK) return result;
    const OperatorState operators = {
        .load_aliases = load_political_aliases, .instance_known = instance_known, .resolve_type = resolve_type_instance,
        .query = query_political_state,
        .execute = execute_political_action, .log = log_message, .election_query = query_election,
        .member_query = query_member, .structure_query = query_structure, .resource_query = query_resource, .action_query = query_action,
        .lifecycle_query = query_instance_mode,
        .war_query = query_direct_war,
        .describe = describe_operation,
    };
    result = register_operators(host, self, operators);
    if (result == ANVIL_ERROR_INTERNAL) return result;
    if (result != ANVIL_OK)
        log_message(ANVIL_LOG_WARNING, "great_khan scripts unavailable; leaving the history window active");
    result = bind_custom_message_buttons(&plugin.messages, host, self);
    log_message(result == ANVIL_OK ? ANVIL_LOG_INFO : ANVIL_LOG_WARNING,
                result == ANVIL_OK ? "political native custom messages installed"
                                   : "political native custom messages unavailable");
    result = install_native_eu4(host, self);
    log_message(result == ANVIL_OK ? ANVIL_LOG_INFO : ANVIL_LOG_WARNING,
                result == ANVIL_OK ? "political native clause compiler installed"
                                   : "political native clause compiler unavailable");
    result = install_native_passives(host, self);
    refresh_country_modifiers = host->address_of(self, SYM_NATIVE_UPDATE_COUNTRY_MODIFIERS);
    refresh_province_modifiers = host->address_of(self, SYM_NATIVE_UPDATE_PROVINCE_MODIFIERS);
    log_message(result == ANVIL_OK ? ANVIL_LOG_INFO : ANVIL_LOG_WARNING,
                result == ANVIL_OK ? "political native country and province modifiers installed"
                                   : "political native modifier collectors unavailable or incomplete");
    result = host->mid_hook(self, SYM_NOMINATION_TOOLTIP_HIT, route_nomination_tooltip, NULL, NULL);
    plugin.nomination_hover_hook = result == ANVIL_OK;
    log_message(plugin.nomination_hover_hook ? ANVIL_LOG_INFO : ANVIL_LOG_WARNING,
                plugin.nomination_hover_hook ? "political nomination_tooltip native dispatch registered" :
                "political nomination_tooltip native dispatch unavailable; using the panel providers");
    return ANVIL_OK;
}
