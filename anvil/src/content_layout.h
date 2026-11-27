#pragma once
#include "content_types.h"

#define PS_SELECTOR_GUI "interface/ps_instance_selector.gui"
#define PS_SELECTOR_WINDOW "ps_instance_selector"

enum { PS_MAX_LAYOUTS = 32, PS_MAX_HUD_REGIONS = 8, PS_MAX_HUD_SLOTS = 8, PS_MAX_HUD_ICONS = 32 };
enum { PS_MAX_LAYOUT_HINTS = 16 };

typedef struct UiRectangle { int x, y, width, height; } UiRectangle;
typedef struct UiPoint { int x, y; } UiPoint;

typedef struct MemberLayout {
    char row[GK_SCRIPT_WORD_SIZE], caption[GK_SCRIPT_WORD_SIZE], scrollbar[GK_SCRIPT_WORD_SIZE];
    char tooltip[GK_SCRIPT_WORD_SIZE], label[GK_SCRIPT_WORD_SIZE];
    char group_heading[GK_SCRIPT_WORD_SIZE], group_hover[GK_SCRIPT_WORD_SIZE];
    char nomination_score[GK_SCRIPT_WORD_SIZE], nomination_hover[GK_SCRIPT_WORD_SIZE];
    char support_shield[GK_SCRIPT_WORD_SIZE];
    char frame[GK_SCRIPT_WORD_SIZE];
    unsigned rows, columns;
    UiRectangle wheel;
    UiRectangle nomination_areas[PS_MAX_ROLES];
    UiPoint shield_offset[32], label_offset[PS_MAX_ROLES];
} MemberLayout;

typedef struct LayoutHint {
    char control[GK_SCRIPT_WORD_SIZE], key[GK_SCRIPT_WORD_SIZE];
    UiRectangle area;
} LayoutHint;

typedef struct ResourceLayout {
    char row[GK_SCRIPT_WORD_SIZE], scrollbar[GK_SCRIPT_WORD_SIZE];
    char value[GK_SCRIPT_WORD_SIZE], hover[GK_SCRIPT_WORD_SIZE];
    UiRectangle wheel, labels[PS_MAX_RESOURCES], values[PS_MAX_RESOURCES];
    unsigned rows;
} ResourceLayout;

typedef struct ResourceSummaryLayout {
    char value[GK_SCRIPT_WORD_SIZE], growth[GK_SCRIPT_WORD_SIZE], hover[GK_SCRIPT_WORD_SIZE];
    UiRectangle value_area, growth_area;
} ResourceSummaryLayout;

typedef struct PoliticalLayout {
    char id[GK_SCRIPT_WORD_SIZE], path[PS_FILE_PATH_SIZE], gui_file[GK_SCRIPT_WORD_SIZE];
    char window[GK_SCRIPT_WORD_SIZE], background[GK_SCRIPT_WORD_SIZE], background_sprite[GK_SCRIPT_WORD_SIZE];
    char title[GK_SCRIPT_WORD_SIZE], close[GK_SCRIPT_WORD_SIZE], instances[GK_SCRIPT_WORD_SIZE], hover[GK_SCRIPT_WORD_SIZE];
    char ballot[GK_SCRIPT_WORD_SIZE];
    char leader[GK_SCRIPT_WORD_SIZE], leader_caption[GK_SCRIPT_WORD_SIZE], leader_label[GK_SCRIPT_WORD_SIZE];
    char leader_tooltip[GK_SCRIPT_WORD_SIZE], leader_history[GK_SCRIPT_WORD_SIZE], authority[GK_SCRIPT_WORD_SIZE];
    char leader_frame[GK_SCRIPT_WORD_SIZE];
    UiPoint leader_offset;
    MemberLayout members, roles, actions;
    char action_read_only[GK_SCRIPT_WORD_SIZE], action_seal[GK_SCRIPT_WORD_SIZE];
    char action_check[GK_SCRIPT_WORD_SIZE], action_status[GK_SCRIPT_WORD_SIZE];
    ResourceLayout resources;
    ResourceSummaryLayout resource_summary;
    UiRectangle area;
    UiRectangle member_count_area, authority_area;
    LayoutHint hints[PS_MAX_LAYOUT_HINTS];
    unsigned line, hint_count;
    bool valid, background_is_icon;
} PoliticalLayout;

typedef struct HudIcon { char sprite[GK_SCRIPT_WORD_SIZE], control[GK_SCRIPT_WORD_SIZE]; } HudIcon;

typedef struct HudRegion {
    char id[GK_SCRIPT_WORD_SIZE], path[PS_FILE_PATH_SIZE], gui_file[GK_SCRIPT_WORD_SIZE], gfx_file[GK_SCRIPT_WORD_SIZE];
    char container[GK_SCRIPT_WORD_SIZE], button_template[GK_SCRIPT_WORD_SIZE], page_window[GK_SCRIPT_WORD_SIZE];
    unsigned capacity, line, icon_count;
    HudIcon icons[PS_MAX_HUD_ICONS];
    bool valid;
} HudRegion;

typedef struct ContentLayouts {
    PoliticalLayout items[PS_MAX_LAYOUTS];
    HudRegion hud[PS_MAX_HUD_REGIONS];
    size_t count, hud_count;
} ContentLayouts;

bool collect_layouts(ContentLayouts *content, const char *text, size_t size, const char *path, ScriptError *error);
bool check_layout_gui(PoliticalLayout *layout, const ScriptTree *gui, ScriptError *error);
bool check_hud_gui(HudRegion *region, const ScriptTree *gui, ScriptError *error);
bool check_gui_window_tooltips(const ScriptTree *gui, const char *name, ScriptError *error);
bool read_number_areas(const ScriptTree *gui, const char *window, const char *prefix,
                       unsigned count, UiRectangle *area, UiRectangle *controls);
bool check_hud_gfx(const HudRegion *region, const ScriptTree *gfx, bool (*mounted)(const char *), ScriptError *error);
int layout_index(const ContentLayouts *content, const char *id);
int hud_index(const ContentLayouts *content, const char *id);
int hud_icon_index(const HudRegion *region, const char *sprite);

typedef struct InstanceChoices {
    size_t items[PS_MAX_INSTANCES];
    unsigned count, enabled_count;
    uint32_t enabled;
} InstanceChoices;

typedef enum EntryRoute { ENTRY_HIDDEN, ENTRY_DISABLED, ENTRY_DIRECT, ENTRY_SELECTOR } EntryRoute;
void add_instance_choice(InstanceChoices *choices, size_t index, EntryAccess access);
EntryRoute entry_route(const InstanceChoices *choices);
bool choice_available(const InstanceChoices *choices, size_t index);
unsigned hud_page_count(unsigned count, unsigned capacity);
