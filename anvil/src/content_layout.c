#include "content_layout.h"

#include <stdio.h>
#include <string.h>

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define F(key) script_field(tree, item, key)

static bool field_name(const ScriptTree *tree, const ScriptItem *item, const char *key, char *out,
                       bool required, ScriptError *error) {
    const ScriptItem *field = F(key);
    if (!field && !required) return true;
    if (!field || field->block || !ps_identifier(field->word)) {
        char detail[176];
        snprintf(detail, sizeof(detail), "expected a GUI name for %.100s", key);
        return script_problem(tree, field ? field : item, error, detail);
    }
    strcpy(out, field->word);
    return true;
}

static bool interface_file(const ScriptTree *tree, const ScriptItem *item, const char *key, const char *extension,
                           char *out, ScriptError *error) {
    const ScriptItem *field = F(key);
    if (!field || field->block || strncmp(field->word, "interface/", 10) ||
        strlen(field->word) <= 10 + strlen(extension) || strstr(field->word, "..") ||
        strpbrk(field->word, "\\:") || strcmp(field->word + strlen(field->word) - strlen(extension), extension))
        return script_problem(tree, field ? field : item, error, "expected an interface file path and the correct extension");
    strcpy(out, field->word);
    return true;
}

static bool read_auto_frame(const ScriptTree *tree, const ScriptItem *item, char *out, ScriptError *error) {
    const ScriptItem *mode = F("auto_place");
    const bool automatic = script_word(mode, "yes");
    if (mode && !automatic && !script_word(mode, "no"))
        return script_problem(tree, mode, error, "auto_place expects yes or no");
    char frame[GK_SCRIPT_WORD_SIZE] = {0};
    if (!field_name(tree, item, "frame", frame, automatic, error)) return false;
    if (automatic) strcpy(out, frame);
    return true;
}

static bool read_slots(const ScriptTree *tree, const ScriptItem *item, PoliticalLayout *layout, ScriptError *error) {
    static const char *const slots[] = { "leader_region", "members_region", "system_resources_region",
        "primary_resource_region", "special_members_region", "actions_region" };
    if (!item) return true;
    if (!script_keys(tree, item, slots, COUNT(slots), error)) return false;
    const ScriptItem *leader = F("leader_region"), *members = F("members_region"), *resources = F("system_resources_region");
    if (leader) {
        static const char *const keys[] = { "control", "kind", "caption", "label", "tooltip", "auto_place", "frame" };
        if (!script_keys(tree, leader, keys, COUNT(keys), error)) return false;
        if (!script_word(script_field(tree, leader, "kind"), "slot"))
            return script_problem(tree, leader, error, "leader_region requires kind=slot");
        if (!field_name(tree, leader, "control", layout->leader, true, error) ||
            !field_name(tree, leader, "caption", layout->leader_caption, false, error) ||
            !field_name(tree, leader, "label", layout->leader_label, false, error) ||
            !field_name(tree, leader, "tooltip", layout->leader_tooltip, false, error) ||
            !read_auto_frame(tree, leader, layout->leader_frame, error)) return false;
    }
    if (members) {
        static const char *const keys[] = {
            "control", "kind", "caption", "scrollbar", "rows", "columns", "tooltip", "auto_place", "frame"
        };
        if (!script_keys(tree, members, keys, COUNT(keys), error)) return false;
        MemberLayout *list = &layout->members;
        const ScriptItem *kind = script_field(tree, members, "kind");
        const bool is_list = script_word(kind, "list");
        if (!is_list && !script_word(kind, "grid"))
            return script_problem(tree, members, error, "members_region requires kind=grid or kind=list");
        int rows = 0, columns = 1;
        if (!script_number(script_field(tree, members, "rows"), 1, 32, &rows) ||
            (!is_list && !script_number(script_field(tree, members, "columns"), 1, 16, &columns)) ||
            (is_list && script_field(tree, members, "columns")) || rows * columns > 32)
            return script_problem(tree, members, error, "member layout dimensions must fit 1..32 slots");
        list->rows = (unsigned)rows;
        list->columns = (unsigned)columns;
        if (!field_name(tree, members, "control", list->row, true, error) ||
            !field_name(tree, members, "caption", list->caption, true, error) ||
            !field_name(tree, members, "scrollbar", list->scrollbar, true, error) ||
            !field_name(tree, members, "tooltip", list->tooltip, false, error) ||
            !read_auto_frame(tree, members, list->frame, error)) return false;
    }
    if (resources) {
        static const char *const keys[] = { "control", "kind", "scrollbar", "rows", "value_control", "tooltip_control" };
        if (!script_keys(tree, resources, keys, COUNT(keys), error)) return false;
        const ScriptItem *kind = script_field(tree, resources, "kind");
        if (script_word(kind, "slot")) {
            if (script_field(tree, resources, "rows") || script_field(tree, resources, "scrollbar") ||
                script_field(tree, resources, "value_control") || script_field(tree, resources, "tooltip_control"))
                return script_problem(tree, resources, error, "a resource slot receives only control and kind");
            if (!field_name(tree, resources, "control", layout->authority, true, error)) return false;
        } else {
            int rows;
            if (!script_word(kind, "list") || !script_number(script_field(tree, resources, "rows"), 1, PS_MAX_RESOURCES, &rows))
                return script_problem(tree, resources, error, "system_resources_region requires kind=slot or a list with 1..8 rows");
            layout->resources.rows = (unsigned)rows;
            if (!field_name(tree, resources, "control", layout->resources.row, true, error) ||
                !field_name(tree, resources, "scrollbar", layout->resources.scrollbar, true, error) ||
                !field_name(tree, resources, "value_control", layout->resources.value, false, error) ||
                !field_name(tree, resources, "tooltip_control", layout->resources.hover, false, error)) return false;
            if (layout->resources.hover[0] && !layout->resources.value[0])
                return script_problem(tree, resources, error, "tooltip_control requires value_control");
        }
    }
    const ScriptItem *summary = F("primary_resource_region");
    if (summary) {
        static const char *const keys[] = {"value_control", "growth_control", "tooltip_control"};
        ResourceSummaryLayout *display = &layout->resource_summary;
        if (!script_keys(tree, summary, keys, COUNT(keys), error) ||
            !field_name(tree, summary, "value_control", display->value, true, error) ||
            !field_name(tree, summary, "growth_control", display->growth, true, error) ||
            !field_name(tree, summary, "tooltip_control", display->hover, false, error)) return false;
    }
    const ScriptItem *roles = F("special_members_region");
    if (roles) {
        static const char *const keys[] = {
            "control", "kind", "caption", "scrollbar", "rows", "label", "tooltip", "auto_place", "frame",
            "nomination_score", "nomination_tooltip_control", "support_shield", "columns", "group_heading", "group_tooltip_control"
        };
        int rows, columns = 1;
        if (!script_keys(tree, roles, keys, COUNT(keys), error)) return false;
        const ScriptItem *kind = script_field(tree, roles, "kind");
        const ScriptItem *width = script_field(tree, roles, "columns");
        const bool grid = script_word(kind, "grid");
        if ((!grid && !script_word(kind, "list")) ||
            (grid ? !script_number(width, 2, PS_MAX_ROLES, &columns) : width != NULL) ||
            !script_number(script_field(tree, roles, "rows"), 1, PS_MAX_ROLES, &rows) ||
            rows * columns > PS_MAX_ROLES)
            return script_problem(tree, roles, error, "special_members_region requires a list or grid with at most 8 visible seats");
        MemberLayout *list = &layout->roles;
        list->rows = (unsigned)rows;
        list->columns = (unsigned)columns;
        if (!field_name(tree, roles, "control", list->row, true, error) ||
            !field_name(tree, roles, "label", list->label, false, error) ||
            !field_name(tree, roles, "group_heading", list->group_heading, false, error) ||
            !field_name(tree, roles, "group_tooltip_control", list->group_hover, false, error) ||
            !field_name(tree, roles, "caption", list->caption, true, error) ||
            !field_name(tree, roles, "scrollbar", list->scrollbar, true, error) ||
            !field_name(tree, roles, "tooltip", list->tooltip, false, error) ||
            !field_name(tree, roles, "nomination_score", list->nomination_score, false, error) ||
            !field_name(tree, roles, "nomination_tooltip_control", list->nomination_hover, false, error) ||
            !field_name(tree, roles, "support_shield", list->support_shield, false, error) ||
            !read_auto_frame(tree, roles, list->frame, error)) return false;
        if ((list->label[0] != 0) == (list->group_heading[0] != 0) ||
            (list->group_heading[0] != 0) != (list->group_hover[0] != 0))
            return script_problem(tree, roles, error, "provide a seat label or a group heading with its tooltip control");
        if (list->nomination_hover[0] && !list->nomination_score[0])
            return script_problem(tree, roles, error, "nomination_tooltip_control requires nomination_score");
    }
    const ScriptItem *actions = F("actions_region");
    if (actions) {
        static const char *const keys[] = { "control", "kind", "scrollbar", "rows", "label",
            "read_only_control", "seal_control", "check_control", "status_control" };
        int rows;
        if (!script_keys(tree, actions, keys, COUNT(keys), error)) return false;
        if (!script_word(script_field(tree, actions, "kind"), "list") ||
            !script_number(script_field(tree, actions, "rows"), 1, PS_MAX_ACTIONS, &rows))
            return script_problem(tree, actions, error, "actions_region requires a vertical list with 1..16 rows");
        layout->actions.rows = (unsigned)rows;
        layout->actions.columns = 1;
        if (!field_name(tree, actions, "control", layout->actions.row, true, error) ||
            !field_name(tree, actions, "label", layout->actions.label, true, error) ||
            !field_name(tree, actions, "scrollbar", layout->actions.scrollbar, true, error) ||
            !field_name(tree, actions, "read_only_control", layout->action_read_only, false, error) ||
            !field_name(tree, actions, "seal_control", layout->action_seal, false, error) ||
            !field_name(tree, actions, "check_control", layout->action_check, false, error) ||
            !field_name(tree, actions, "status_control", layout->action_status, false, error)) return false;
        const unsigned state_controls = (layout->action_read_only[0] != 0) + (layout->action_seal[0] != 0) +
                                        (layout->action_check[0] != 0);
        if (state_controls && state_controls != 3)
            return script_problem(tree, actions, error, "action states need read-only, seal and check controls together");
    }
    return true;
}

static bool read_layout(const ScriptTree *tree, const ScriptItem *item, PoliticalLayout *layout, ScriptError *error) {
    static const char *const keys[] = {
        "gui_file", "window", "background_control", "hover_control", "close_control", "title_control",
        "instance_control", "vote_control", "leader_history_control", "regions", "tooltips"
    };
    if (!script_keys(tree, item, keys, COUNT(keys), error)) return false;
    *layout = (PoliticalLayout){ .line = item->line };
    strcpy(layout->id, item->key);
    strcpy(layout->path, tree->path);
    const ScriptItem *hints = F("tooltips");
    if (hints) {
        if (!hints->block) return script_problem(tree, hints, error, "tooltips requires a control-to-localisation map");
        for (unsigned i = hints->first; i; i = tree->items[i].next) {
            const ScriptItem *field = &tree->items[i];
            if (layout->hint_count == PS_MAX_LAYOUT_HINTS || field->block ||
                !ps_identifier(field->key) || !ps_identifier(field->word))
                return script_problem(tree, field, error, "expected a text control name and localisation key");
            for (unsigned j = 0; j < layout->hint_count; ++j)
                if (!strcmp(layout->hints[j].control, field->key))
                    return script_problem(tree, field, error, "duplicate tooltip control");
            LayoutHint *hint = &layout->hints[layout->hint_count++];
            strcpy(hint->control, field->key);
            strcpy(hint->key, field->word);
        }
    }
    return interface_file(tree, item, "gui_file", ".gui", layout->gui_file, error) &&
           field_name(tree, item, "window", layout->window, true, error) &&
           field_name(tree, item, "background_control", layout->background, true, error) &&
           field_name(tree, item, "hover_control", layout->hover, false, error) &&
           field_name(tree, item, "close_control", layout->close, false, error) &&
           field_name(tree, item, "title_control", layout->title, false, error) &&
           field_name(tree, item, "instance_control", layout->instances, false, error) &&
           field_name(tree, item, "vote_control", layout->ballot, false, error) &&
           field_name(tree, item, "leader_history_control", layout->leader_history, false, error) &&
           read_slots(tree, F("regions"), layout, error);
}

static bool read_hud(const ScriptTree *tree, const ScriptItem *item, HudRegion *region, ScriptError *error) {
    static const char *const keys[] = {
        "gui_file", "gfx_file", "container", "button_template", "capacity", "page_window", "overflow"
    };
    if (!script_keys(tree, item, keys, COUNT(keys), error)) return false;
    *region = (HudRegion){ .line = item->line };
    strcpy(region->id, item->key);
    strcpy(region->path, tree->path);
    int capacity;
    if (!script_number(F("capacity"), 1, PS_MAX_HUD_SLOTS, &capacity))
        return script_problem(tree, item, error, "HUD capacity requires 1..8 physical slots");
    region->capacity = (unsigned)capacity;
    if (F("overflow") && !script_word(F("overflow"), "paginate"))
        return script_problem(tree, F("overflow"), error, "HUD overflow supports paginate");
    return interface_file(tree, item, "gui_file", ".gui", region->gui_file, error) &&
           interface_file(tree, item, "gfx_file", ".gfx", region->gfx_file, error) &&
           field_name(tree, item, "container", region->container, true, error) &&
           field_name(tree, item, "button_template", region->button_template, true, error) &&
           field_name(tree, item, "page_window", region->page_window, true, error);
}

int layout_index(const ContentLayouts *content, const char *id) {
    for (size_t i = 0; i < content->count; ++i)
        if (!strcmp(content->items[i].id, id)) return (int)i;
    return -1;
}

int hud_index(const ContentLayouts *content, const char *id) {
    for (size_t i = 0; i < content->hud_count; ++i)
        if (!strcmp(content->hud[i].id, id)) return (int)i;
    return -1;
}

bool collect_layouts(ContentLayouts *content, const char *text, size_t size, const char *path, ScriptError *error) {
    ScriptTree parsed;
    if (!script_tree_read(&parsed, text, size, path, error)) return false;
    const ScriptTree *tree = &parsed;
    const size_t first_layout = content->count, first_hud = content->hud_count;
    bool hud_seen = false;
    for (unsigned i = tree->items[0].first; i; i = tree->items[i].next) {
        const ScriptItem *item = &tree->items[i];
        if (!strcmp(item->key, "hud_regions") && item->block && !hud_seen) {
            hud_seen = true;
            for (unsigned j = item->first; j; j = tree->items[j].next) {
                const ScriptItem *region = &tree->items[j];
                if (content->hud_count == PS_MAX_HUD_REGIONS || !ps_identifier(region->key) ||
                    hud_index(content, region->key) >= 0) {
                    script_problem(tree, region, error, "duplicate or invalid HUD region, or capacity exceeded");
                    goto failed;
                }
                if (!read_hud(tree, region, &content->hud[content->hud_count], error)) goto failed;
                ++content->hud_count;
            }
        } else {
            if (content->count == PS_MAX_LAYOUTS || !ps_identifier(item->key) ||
                !strcmp(item->key, "hud_regions") || layout_index(content, item->key) >= 0) {
                script_problem(tree, item, error, "duplicate or invalid layout, or capacity exceeded");
                goto failed;
            }
            if (!read_layout(tree, item, &content->items[content->count], error)) goto failed;
            ++content->count;
        }
    }
    script_tree_free(&parsed);
    return true;
failed:
    content->count = first_layout;
    content->hud_count = first_hud;
    script_tree_free(&parsed);
    return false;
}

static const ScriptItem *gui_field(const ScriptTree *tree, const ScriptItem *parent, const char *key) {
    if (parent)
        for (unsigned i = parent->first; i; i = tree->items[i].next)
            if (!_stricmp(tree->items[i].key, key)) return &tree->items[i];
    return NULL;
}

static const ScriptItem *gui_control(const ScriptTree *tree, const ScriptItem *window, const char *kind, const char *name) {
    if (window && name[0])
        for (unsigned i = window->first; i; i = tree->items[i].next) {
            const ScriptItem *item = &tree->items[i];
            if (!_stricmp(item->key, kind) && script_word(gui_field(tree, item, "name"), name)) return item;
        }
    return NULL;
}

static bool missing_control(const ScriptTree *tree, const ScriptItem *window, const char *name, ScriptError *error) {
    char detail[178];
    snprintf(detail, sizeof(detail), "missing or incompatible GUI control %.118s", name);
    return script_problem(tree, window, error, detail);
}

static bool check_tooltip_templates(const ScriptTree *tree, const ScriptItem *root, const ScriptItem *control,
                                    ScriptError *error) {
    for (unsigned i = control->first; i; i = tree->items[i].next) {
        const ScriptItem *item = &tree->items[i];
        if (!_stricmp(item->key, "tooltip")) {
            // The engine creates a text box from this name. tooltipText carries the localisation key.
            if (!item->block && (!item->word[0] || script_word(item, "NOTOOLTIP") ||
                gui_control(tree, root, "instantTextBoxType", item->word) ||
                gui_control(tree, root, "textBoxType", item->word))) continue;
            char detail[175];
            snprintf(detail, sizeof(detail),
                     "tooltip refers to an undeclared text box '%.74s'; put localisation in tooltipText", item->word);
            return script_problem(tree, item, error, detail);
        }
        if (item->block && !check_tooltip_templates(tree, root, item, error)) return false;
    }
    return true;
}

bool check_gui_window_tooltips(const ScriptTree *gui, const char *name, ScriptError *error) {
    const ScriptItem *root = gui_field(gui, gui->items, "guiTypes");
    const ScriptItem *window = gui_control(gui, root, "windowType", name);
    return window ? check_tooltip_templates(gui, root, window, error) : missing_control(gui, root, name, error);
}

static bool pair(const ScriptTree *tree, const ScriptItem *item, const char *key, int *x, int *y) {
    const ScriptItem *block = gui_field(tree, item, key);
    return block && script_number(gui_field(tree, block, "x"), -8192, 8192, x) &&
           script_number(gui_field(tree, block, "y"), -8192, 8192, y);
}

static bool centered_window(const ScriptTree *tree, const ScriptItem *window, UiRectangle *area) {
    const ScriptItem *orientation = gui_field(tree, window, "orientation");
    const ScriptItem *movable = gui_field(tree, window, "moveable");
    return (!movable || script_word(movable, "0") || script_word(movable, "no")) &&
           orientation && !_stricmp(orientation->word, "CENTER") &&
           pair(tree, window, "position", &area->x, &area->y) &&
           pair(tree, window, "size", &area->width, &area->height) && area->width > 0 && area->height > 0;
}

static bool frame_rectangle(const ScriptTree *gui, const ScriptItem *root, const UiRectangle *panel,
                            const char *name, UiRectangle *box, ScriptError *error) {
    const ScriptItem *frame = gui_control(gui, root, "windowType", name);
    if (!frame) return missing_control(gui, root, name, error);
    if (!pair(gui, frame, "position", &box->x, &box->y) ||
        !pair(gui, frame, "size", &box->width, &box->height) ||
        box->x < 0 || box->y < 0 || box->width <= 0 || box->height <= 0 ||
        box->x + box->width > panel->width || box->y + box->height > panel->height)
        return script_problem(gui, frame, error, "shield frame must fit inside the panel");
    return true;
}

static bool point_and_size(const ScriptTree *gui, const ScriptItem *control, bool label, UiRectangle *box) {
    const ScriptItem *orientation = gui_field(gui, control, "orientation");
    if (!control || gui_field(gui, control, "parent") ||
        (orientation && _stricmp(orientation->word, "UPPER_LEFT")) ||
        !pair(gui, control, "position", &box->x, &box->y)) return false;
    if (label) return script_number(gui_field(gui, control, "maxWidth"), 1, 8192, &box->width) &&
                      script_number(gui_field(gui, control, "maxHeight"), 1, 8192, &box->height);
    return pair(gui, control, "size", &box->width, &box->height) && box->width > 0 && box->height > 0;
}

static bool contains_rectangle(UiRectangle area, UiRectangle box) {
    return box.x >= area.x && box.y >= area.y &&
           box.x + box.width <= area.x + area.width && box.y + box.height <= area.y + area.height;
}

static void include_rectangle(UiRectangle *area, UiRectangle box) {
    const int x = box.x < area->x ? box.x : area->x;
    const int y = box.y < area->y ? box.y : area->y;
    const int right = box.x + box.width > area->x + area->width ? box.x + box.width : area->x + area->width;
    const int bottom = box.y + box.height > area->y + area->height ? box.y + box.height : area->y + area->height;
    *area = (UiRectangle){x, y, right - x, bottom - y};
}

static bool place_shield(const ScriptTree *gui, const ScriptItem *shield, const ScriptItem *label,
                         UiRectangle cell, UiPoint *shield_offset, UiPoint *label_offset, ScriptError *error) {
    enum { LABEL_GAP = 6 };
    UiRectangle flag, text;
    if (!point_and_size(gui, shield, false, &flag))
        return script_problem(gui, shield, error, "auto-placed shield requires position, size and upper-left anchoring");
    int width = flag.width;
    if (label) {
        if (!point_and_size(gui, label, true, &text))
            return script_problem(gui, label, error, "auto-placed role label requires position, maxWidth and maxHeight");
        width += LABEL_GAP + text.width;
    }
    if (width > cell.width || flag.height > cell.height || (label && text.height > cell.height))
        return script_problem(gui, shield, error, "shield and its label must fit in one frame cell");
    const int x = cell.x + (cell.width - width) / 2;
    *shield_offset = (UiPoint){ x - flag.x, cell.y + (cell.height - flag.height) / 2 - flag.y };
    if (label)
        *label_offset = (UiPoint){
            x + flag.width + LABEL_GAP - text.x, cell.y + (cell.height - text.height) / 2 - text.y
        };
    return true;
}

static bool check_nomination_gui(MemberLayout *list, const ScriptTree *gui, const ScriptItem *window,
                                 const ScriptItem *shield, const ScriptItem *background, const ScriptItem *hover,
                                 unsigned index, UiPoint offset, UiRectangle area, ScriptError *error) {
    const bool automatic = list->frame[0] != 0;
    char name[GK_SCRIPT_WORD_SIZE + 12];
    const ScriptItem *support = NULL;
    UiRectangle badge = {0};
    if (list->support_shield[0]) {
        snprintf(name, sizeof(name), "%s_%02u", list->support_shield, index);
        support = gui_control(gui, window, "guiButtonType", name);
        if (!point_and_size(gui, support, false, &badge) || support == shield || support == background || support == hover)
            return missing_control(gui, window, name, error);
        const UiRectangle placed = {badge.x + offset.x, badge.y + offset.y, badge.width, badge.height};
        if (!contains_rectangle(area, placed))
            return script_problem(gui, support, error, automatic ? "support shield must fit inside its visible list area" :
                                  "manual office control must fit inside the panel");
        if (!automatic) include_rectangle(&list->wheel, badge);
    }
    if (!list->nomination_score[0]) return true;
    snprintf(name, sizeof(name), "%s_%02u", list->nomination_score, index);
    const ScriptItem *score = gui_control(gui, window, "instantTextBoxType", name);
    const bool separate = list->nomination_hover[0] != 0;
    UiRectangle text, hit, flag;
    if (!point_and_size(gui, score, true, &text) ||
        script_word(gui_field(gui, score, "alwaystransparent"), "yes") != separate)
        return missing_control(gui, window, name, error);
    const ScriptItem *surface = score;
    hit = text;
    if (separate) {
        snprintf(name, sizeof(name), "%s_%02u", list->nomination_hover, index);
        surface = gui_control(gui, window, "guiButtonType", name);
        if (!point_and_size(gui, surface, false, &hit) || surface == shield || surface == support ||
            surface == background || surface == hover)
            return missing_control(gui, window, name, error);
    }
    if (!contains_rectangle(hit, text))
        return script_problem(gui, score, error, "nomination tooltip must cover the score text");
    const UiRectangle placed = {hit.x + offset.x, hit.y + offset.y, hit.width, hit.height};
    if (automatic && (!point_and_size(gui, shield, false, &flag) ||
        (hit.x < flag.x + flag.width && hit.y < flag.y + flag.height) ||
        (support && badge.x < hit.x + hit.width && hit.x < badge.x + badge.width &&
         badge.y < hit.y + hit.height && hit.y < badge.y + badge.height)))
        return script_problem(gui, surface, error, "nomination tooltip must cover the number beside the shield inside the visible row");
    if (!contains_rectangle(area, placed))
        return script_problem(gui, surface, error, automatic ? "nomination tooltip must cover the number beside the shield inside the visible row" :
                              "manual office control must fit inside the panel");
    list->nomination_areas[index] = placed;
    if (!automatic) include_rectangle(&list->wheel, placed);
    return true;
}

static bool check_list_gui(MemberLayout *list, const ScriptTree *gui, const ScriptItem *window,
                           const ScriptItem *background, const ScriptItem *hover, const UiRectangle *panel,
                           bool offices, ScriptError *error) {
    if (!list->rows) return true;
    const ScriptItem *scroll = gui_control(gui, window, "scrollbarType", list->scrollbar);
    int sx, sy, sw, sh;
    if (!scroll || !pair(gui, scroll, "position", &sx, &sy) || !pair(gui, scroll, "size", &sw, &sh) ||
        sh <= 0 || sw <= 0 || !script_word(gui_field(gui, scroll, "horizontal"), "0"))
        return missing_control(gui, window, list->scrollbar, error);
    if (list->frame[0]) {
        const ScriptItem *root = gui_field(gui, gui->items, "guiTypes");
        UiRectangle frame;
        if (!frame_rectangle(gui, root, panel, list->frame, &frame, error)) return false;
        const int right = frame.x + frame.width;
        const int bottom = frame.y + frame.height;
        if (sx <= frame.x + 4 || sx + sw > right || sy < frame.y || sy + sh > bottom)
            return script_problem(gui, scroll, error, "auto-placement needs a vertical scrollbar on the right inside the frame");
        const int grid_width = sx - frame.x - 4;
        const int cols = (int)list->columns, rows = (int)list->rows;
        for (int i = 0; i < rows * cols; ++i) {
            char name[GK_SCRIPT_WORD_SIZE + 12];
            snprintf(name, sizeof(name), "%s_%02d", list->row, i);
            const ScriptItem *shield = gui_control(gui, window, "guiButtonType", name);
            if (!shield || shield == background || shield == hover) return missing_control(gui, window, name, error);
            const ScriptItem *label = NULL;
            if (list->label[0]) {
                snprintf(name, sizeof(name), "%s_%02d", list->label, i);
                label = gui_control(gui, window, "instantTextBoxType", name);
                if (!label) return missing_control(gui, window, name, error);
            }
            const int col = i % cols, row = i / cols;
            const int x = col * grid_width / cols, y = row * frame.height / rows;
            const UiRectangle cell = { frame.x + x, frame.y + y,
                (col + 1) * grid_width / cols - x, (row + 1) * frame.height / rows - y };
            if (!place_shield(gui, shield, label, cell, &list->shield_offset[i],
                              label ? &list->label_offset[i] : NULL, error) ||
                !check_nomination_gui(list, gui, window, shield, background, hover, (unsigned)i,
                                      list->shield_offset[i], cell, error)) return false;
        }
        list->wheel = frame;
        return true;
    }
    const UiRectangle area = {0, 0, offices ? panel->width : sx, panel->height};
    if (offices) {
        list->wheel = (UiRectangle){sx, sy, sw, sh};
        if (!contains_rectangle(area, list->wheel))
            return script_problem(gui, scroll, error, "manual office control must fit inside the panel");
    }
    int min_x = sx, min_y = sy;
    for (unsigned i = 0; i < list->rows * list->columns; ++i) {
        char name[GK_SCRIPT_WORD_SIZE + 12];
        snprintf(name, sizeof(name), "%s_%02u", list->row, i);
        const ScriptItem *shield = gui_control(gui, window, "guiButtonType", name);
        int x, y;
        if (!shield || !pair(gui, shield, "position", &x, &y)) return missing_control(gui, window, name, error);
        if (shield == hover || shield == background)
            return script_problem(gui, shield, error, "list shields cannot also serve as the panel background or hover_control");
        if (x < min_x) min_x = x;
        if (y < min_y) min_y = y;
        if (offices) {
            UiRectangle box;
            if (point_and_size(gui, shield, false, &box)) {
                if (!contains_rectangle(area, box))
                    return script_problem(gui, shield, error, "manual office control must fit inside the panel");
                include_rectangle(&list->wheel, box);
            } else {
                if (list->group_heading[0] || list->nomination_score[0])
                    return missing_control(gui, window, name, error);
                include_rectangle(&list->wheel, (UiRectangle){x, y, 1, 1});
            }
        } else if (x >= sx || y >= sy + sh) {
            return script_problem(gui, shield, error, "list extends beyond the vertical rail");
        }
        if (!check_nomination_gui(list, gui, window, shield, background, hover, i, (UiPoint){0}, area, error))
            return false;
        if (!list->label[0]) continue;
        snprintf(name, sizeof(name), "%s_%02u", list->label, i);
        const ScriptItem *label = gui_control(gui, window, "instantTextBoxType", name);
        if (!label || !pair(gui, label, "position", &x, &y)) return missing_control(gui, window, name, error);
        if (offices) {
            UiRectangle box;
            if (point_and_size(gui, label, true, &box)) {
                if (!contains_rectangle(area, box))
                    return script_problem(gui, label, error, "manual office control must fit inside the panel");
                include_rectangle(&list->wheel, box);
            } else include_rectangle(&list->wheel, (UiRectangle){x, y, 1, 1});
        }
        if (x < min_x) min_x = x;
        if (y < min_y) min_y = y;
    }
    if (!offices) list->wheel = (UiRectangle){min_x - 4, min_y - 4, sx + sw - min_x + 4, sy + sh - min_y + 4};
    return true;
}

static bool check_resource_gui(ResourceLayout *list, const ScriptTree *gui, const ScriptItem *window, ScriptError *error) {
    if (!list->rows) return true;
    const ScriptItem *scroll = gui_control(gui, window, "scrollbarType", list->scrollbar);
    int x, y, width, height;
    if (!scroll || !pair(gui, scroll, "position", &x, &y) || !pair(gui, scroll, "size", &width, &height) ||
        width <= 0 || height <= 0 || !script_word(gui_field(gui, scroll, "horizontal"), "0"))
        return missing_control(gui, window, list->scrollbar, error);
    int left = x;
    for (unsigned i = 0; i < list->rows; ++i) {
        char name[GK_SCRIPT_WORD_SIZE + 12];
        snprintf(name, sizeof(name), "%s_%02u", list->row, i);
        const ScriptItem *label = gui_control(gui, window, "instantTextBoxType", name);
        UiRectangle *r = &list->labels[i];
        if (!label || !pair(gui, label, "position", &r->x, &r->y) ||
            !script_number(gui_field(gui, label, "maxWidth"), 1, 8192, &r->width) ||
            !script_number(gui_field(gui, label, "maxHeight"), 1, 8192, &r->height) ||
            !script_word(gui_field(gui, label, "alwaystransparent"), "yes"))
            return missing_control(gui, window, name, error);
        if (r->x + r->width > x || r->y < y || r->y + r->height > y + height)
            return script_problem(gui, label, error, "resource row must fit beside its vertical scrollbar");
        if (list->value[0]) {
            snprintf(name, sizeof(name), "%s_%02u", list->value, i);
            const ScriptItem *value = gui_control(gui, window, "instantTextBoxType", name);
            const bool separate = list->hover[0] != 0;
            UiRectangle number;
            if (!value || !pair(gui, value, "position", &number.x, &number.y) ||
                !script_number(gui_field(gui, value, "maxWidth"), 1, 8192, &number.width) ||
                !script_number(gui_field(gui, value, "maxHeight"), 1, 8192, &number.height) ||
                script_word(gui_field(gui, value, "alwaystransparent"), "yes") != separate)
                return missing_control(gui, window, name, error);
            int hx = number.x, hy = number.y;
            if (separate) {
                snprintf(name, sizeof(name), "%s_%02u", list->hover, i);
                const ScriptItem *hover = gui_control(gui, window, "guiButtonType", name);
                if (!hover || !pair(gui, hover, "position", &hx, &hy) ||
                    script_word(gui_field(gui, hover, "alwaystransparent"), "yes"))
                    return missing_control(gui, window, name, error);
            }
            if (number.x < hx || number.y < hy || number.x + number.width > x ||
                (!separate && number.y < y) || number.y + number.height > y + height || hx < r->x + r->width)
                return script_problem(gui, value, error, "resource number and tooltip control must align beside the name");
            list->values[i] = number;
        }
        if (r->x < left) left = r->x;
    }
    list->wheel = (UiRectangle){left, y, x + width - left, height};
    return true;
}

static bool check_action_state_gui(const PoliticalLayout *layout, const ScriptTree *gui,
                                   const ScriptItem *window, ScriptError *error) {
    if (!layout->action_read_only[0]) return true;
    for (unsigned i = 0; i < layout->actions.rows; ++i) {
        char name[GK_SCRIPT_WORD_SIZE + 12];
        snprintf(name, sizeof(name), "%s_%02u", layout->actions.row, i);
        const ScriptItem *active = gui_control(gui, window, "guiButtonType", name);
        snprintf(name, sizeof(name), "%s_%02u", layout->action_read_only, i);
        const ScriptItem *passive = gui_control(gui, window, "guiButtonType", name);
        int ax, ay, px, py;
        if (!passive || passive == active || !script_word(gui_field(gui, passive, "no_clicksound"), "yes") ||
            script_word(gui_field(gui, passive, "alwaystransparent"), "yes") ||
            !pair(gui, active, "position", &ax, &ay) || !pair(gui, passive, "position", &px, &py) || ax != px || ay != py)
            return missing_control(gui, window, name, error);
        const char *const graphics[] = {layout->action_seal, layout->action_check};
        for (unsigned j = 0; j < COUNT(graphics); ++j) {
            snprintf(name, sizeof(name), "%s_%02u", graphics[j], i);
            const ScriptItem *graphic = gui_control(gui, window, "guiButtonType", name);
            if (!graphic || graphic == active || graphic == passive ||
                !script_word(gui_field(gui, graphic, "alwaystransparent"), "yes") ||
                !script_word(gui_field(gui, graphic, "no_clicksound"), "yes")) return missing_control(gui, window, name, error);
        }
    }
    return true;
}

static bool check_text_surface(const ScriptTree *gui, const ScriptItem *window,
                               const char *surface, const char *label, UiRectangle *area, ScriptError *error) {
    const ScriptItem *text = gui_control(gui, window, "instantTextBoxType", label);
    const bool separate = surface[0] != 0;
    UiRectangle h, t;
    if (!point_and_size(gui, text, true, &t) ||
        script_word(gui_field(gui, text, "alwaystransparent"), "yes") != separate)
        return missing_control(gui, window, label, error);
    if (area) *area = t;
    if (!separate) return true;
    const ScriptItem *hit = gui_control(gui, window, "guiButtonType", surface);
    if (!point_and_size(gui, hit, false, &h) ||
        !script_word(gui_field(gui, hit, "no_clicksound"), "yes") ||
        script_word(gui_field(gui, hit, "alwaystransparent"), "yes"))
        return missing_control(gui, window, surface, error);
    if (t.x < h.x || t.y < h.y || t.x + t.width > h.x + h.width || t.y + t.height > h.y + h.height)
        return script_problem(gui, text, error, "text must fit inside its read-only tooltip surface");
    return true;
}

static bool check_role_groups(PoliticalLayout *layout, const ScriptTree *gui, const ScriptItem *window,
                              ScriptError *error) {
    MemberLayout *roles = &layout->roles;
    if (!roles->group_heading[0]) return true;
    const ScriptItem *scroll = gui_control(gui, window, "scrollbarType", roles->scrollbar);
    int sx, sy;
    if (!pair(gui, scroll, "position", &sx, &sy)) return false;
    int previous_bottom = sy;
    for (unsigned row = 0; row < roles->rows; ++row) {
        char title[GK_SCRIPT_WORD_SIZE + 12], surface[GK_SCRIPT_WORD_SIZE + 12];
        snprintf(title, sizeof(title), "%s_%02u", roles->group_heading, row);
        snprintf(surface, sizeof(surface), "%s_%02u", roles->group_hover, row);
        if (!check_text_surface(gui, window, surface, title, NULL, error)) return false;
        const ScriptItem *control = gui_control(gui, window, "guiButtonType", surface);
        UiRectangle hit;
        if (!point_and_size(gui, control, false, &hit))
            return missing_control(gui, window, surface, error);
        if (!roles->frame[0]) {
            const UiRectangle area = {0, 0, layout->area.width, layout->area.height};
            if (!contains_rectangle(area, hit))
                return script_problem(gui, control, error, "manual office control must fit inside the panel");
            include_rectangle(&roles->wheel, hit);
            continue;
        }
        if (hit.x < 0 || hit.x + hit.width > sx ||
            hit.y < previous_bottom || hit.y + hit.height > layout->area.height)
            return script_problem(gui, control, error, "office group heading must sit above its row and beside the vertical rail");
        previous_bottom = hit.y + hit.height;
        for (unsigned col = 0; col < roles->columns; ++col) {
            const unsigned slot = row * roles->columns + col;
            char name[GK_SCRIPT_WORD_SIZE + 12];
            snprintf(name, sizeof(name), "%s_%02u", roles->row, slot);
            const ScriptItem *shield = gui_control(gui, window, "guiButtonType", name);
            UiRectangle box;
            if (!point_and_size(gui, shield, false, &box) || box.y + roles->shield_offset[slot].y < hit.y + hit.height)
                return script_problem(gui, shield, error, "office shield overlaps its group heading");
            const int bottom = box.y + roles->shield_offset[slot].y + box.height;
            if (bottom > layout->area.height)
                return script_problem(gui, shield, error, "office shield extends below the panel");
            if (bottom > previous_bottom) previous_bottom = bottom;
        }
        if (hit.x < roles->wheel.x) {
            roles->wheel.width += roles->wheel.x - hit.x;
            roles->wheel.x = hit.x;
        }
    }
    return true;
}

bool check_layout_gui(PoliticalLayout *layout, const ScriptTree *gui, ScriptError *error) {
    const ScriptItem *root = gui_field(gui, gui->items, "guiTypes");
    const ScriptItem *window = gui_control(gui, root, "windowType", layout->window);
    if (!window) return missing_control(gui, NULL, layout->window, error);
    if (!check_tooltip_templates(gui, root, window, error)) return false;
    const ScriptItem *background = gui_control(gui, window, "iconType", layout->background);
    layout->background_is_icon = background != NULL;
    const ScriptItem *sprite = gui_field(gui, background, "spriteType");
    if (!background && script_word(gui_field(gui, window, "backGround"), layout->background)) {
        background = gui_control(gui, window, "guiButtonType", layout->background);
        sprite = gui_field(gui, background, "quadTextureSprite");
    }
    if (!sprite || sprite->block || !ps_identifier(sprite->word))
        return script_problem(gui, window, error, "background_control needs an iconType or the window's backGround with a sprite");
    strcpy(layout->background_sprite, sprite->word);
    if ((layout->hint_count || (layout->resources.rows && !layout->resources.hover[0])) && !layout->hover[0])
        return script_problem(gui, window, error, "tooltips requires a separate hover_control");
    const ScriptItem *hover = gui_control(gui, window, "guiButtonType", layout->hover);
    if (layout->hover[0] && (!hover || hover == background ||
        script_word(gui_field(gui, hover, "alwaystransparent"), "yes")))
        return script_problem(gui, window, error, "hover_control needs a separate native button that receives pointer input");
    const char *const labels[] = {
        layout->title, layout->leader_caption, layout->leader_label, layout->members.caption, layout->authority, layout->roles.caption
    };
    for (size_t i = 0; i < COUNT(labels); ++i)
        if (*labels[i] && !gui_control(gui, window, "instantTextBoxType", labels[i]))
            return missing_control(gui, window, labels[i], error);
    const char *const buttons[] = {layout->close, layout->leader, layout->instances, layout->ballot};
    for (size_t i = 0; i < COUNT(buttons); ++i) {
        if (!*buttons[i]) continue;
        const ScriptItem *button = gui_control(gui, window, "guiButtonType", buttons[i]);
        if (!button) return missing_control(gui, window, buttons[i], error);
        if (button == hover || button == background)
            return script_problem(gui, button, error, "interactive controls cannot also serve as the panel background or hover_control");
    }
    if ((layout->members.rows || layout->roles.rows || layout->resources.rows || layout->actions.rows ||
         layout->hint_count || layout->leader_frame[0]) &&
        !centered_window(gui, window, &layout->area))
        return script_problem(gui, window, error, "wheel and text hover require a fixed CENTER window with position and size");
    point_and_size(gui, gui_control(gui, window, "instantTextBoxType", layout->members.caption), true, &layout->member_count_area);
    point_and_size(gui, gui_control(gui, window, "instantTextBoxType", layout->authority), true, &layout->authority_area);
    for (unsigned i = 0; i < layout->hint_count; ++i) {
        LayoutHint *hint = &layout->hints[i];
        UiRectangle *area = &hint->area;
        const ScriptItem *label = gui_control(gui, window, "instantTextBoxType", hint->control);
        if (!label || !pair(gui, label, "position", &area->x, &area->y) ||
            !script_number(gui_field(gui, label, "maxWidth"), 1, 8192, &area->width) ||
            !script_number(gui_field(gui, label, "maxHeight"), 1, 8192, &area->height) ||
            !script_word(gui_field(gui, label, "alwaystransparent"), "yes"))
            return missing_control(gui, window, hint->control, error);
    }
    if (layout->leader_frame[0]) {
        UiRectangle box;
        const ScriptItem *shield = gui_control(gui, window, "guiButtonType", layout->leader);
        if (!frame_rectangle(gui, root, &layout->area, layout->leader_frame, &box, error) ||
            !place_shield(gui, shield, NULL, box, &layout->leader_offset, NULL, error)) return false;
    }
    if (layout->leader_history[0] &&
        !check_text_surface(gui, window, layout->leader_history, layout->leader_label, NULL, error)) return false;
    ResourceSummaryLayout *summary = &layout->resource_summary;
    if (summary->value[0] &&
        (!check_text_surface(gui, window, summary->hover, summary->value, &summary->value_area, error) ||
         !check_text_surface(gui, window, summary->hover, summary->growth, &summary->growth_area, error))) return false;
    return check_list_gui(&layout->members, gui, window, background, hover, &layout->area, false, error) &&
           check_list_gui(&layout->roles, gui, window, background, hover, &layout->area, true, error) &&
           check_role_groups(layout, gui, window, error) &&
           check_list_gui(&layout->actions, gui, window, background, hover, &layout->area, false, error) &&
           check_action_state_gui(layout, gui, window, error) &&
           check_resource_gui(&layout->resources, gui, window, error);
}

bool read_number_areas(const ScriptTree *gui, const char *window_name, const char *prefix,
                       unsigned count, UiRectangle *area, UiRectangle *controls) {
    const ScriptItem *root = gui_field(gui, gui->items, "guiTypes");
    const ScriptItem *window = gui_control(gui, root, "windowType", window_name);
    if (!window || !centered_window(gui, window, area)) return false;
    for (unsigned i = 0; i < count; ++i) {
        char name[GK_SCRIPT_WORD_SIZE];
        snprintf(name, sizeof(name), "%s%u", prefix, i);
        if (!point_and_size(gui, gui_control(gui, window, "instantTextBoxType", name), true, &controls[i])) return false;
    }
    return true;
}

int hud_icon_index(const HudRegion *region, const char *sprite) {
    for (unsigned i = 0; i < region->icon_count; ++i)
        if (!strcmp(region->icons[i].sprite, sprite)) return (int)i;
    return -1;
}

bool check_hud_gui(HudRegion *region, const ScriptTree *gui, ScriptError *error) {
    const ScriptItem *root = gui_field(gui, gui->items, "guiTypes");
    region->icon_count = 0;
    for (unsigned slot = 0; slot < region->capacity; ++slot) {
        char name[GK_SCRIPT_WORD_SIZE + 12];
        snprintf(name, sizeof(name), "%s_%02u", region->container, slot);
        const ScriptItem *window = gui_control(gui, root, "windowType", name);
        if (!window) return missing_control(gui, root, name, error);
        if (!check_tooltip_templates(gui, root, window, error)) return false;
        if (slot) {
            for (unsigned i = 0; i < region->icon_count; ++i) {
                const HudIcon *icon = &region->icons[i];
                const ScriptItem *button = gui_control(gui, window, "guiButtonType", icon->control);
                if (!button || !script_word(gui_field(gui, button, "quadTextureSprite"), icon->sprite))
                    return missing_control(gui, window, icon->control, error);
            }
            continue;
        }
        const size_t prefix = strlen(region->button_template);
        for (unsigned i = window->first; i; i = gui->items[i].next) {
            const ScriptItem *button = &gui->items[i];
            if (_stricmp(button->key, "guiButtonType")) continue;
            const ScriptItem *control = gui_field(gui, button, "name");
            if (!control || strncmp(control->word, region->button_template, prefix) ||
                (control->word[prefix] && control->word[prefix] != '_')) continue;
            const ScriptItem *sprite = gui_field(gui, button, "quadTextureSprite");
            if (!sprite || !ps_identifier(sprite->word) || hud_icon_index(region, sprite->word) >= 0 ||
                region->icon_count == PS_MAX_HUD_ICONS)
                return script_problem(gui, button, error, "missing or duplicate HUD sprite, or sprite capacity exceeded");
            HudIcon *icon = &region->icons[region->icon_count++];
            strcpy(icon->sprite, sprite->word);
            strcpy(icon->control, control->word);
        }
        if (!region->icon_count) return missing_control(gui, window, region->button_template, error);
    }
    const ScriptItem *page = gui_control(gui, root, "windowType", region->page_window);
    if (!page) return missing_control(gui, root, region->page_window, error);
    if (!gui_control(gui, page, "guiButtonType", "ps_page_next")) return missing_control(gui, page, "ps_page_next", error);
    return check_tooltip_templates(gui, root, page, error);
}

bool check_hud_gfx(const HudRegion *region, const ScriptTree *gfx, bool (*mounted)(const char *), ScriptError *error) {
    const ScriptItem *root = gui_field(gfx, gfx->items, "spriteTypes");
    for (unsigned i = 0; i < region->icon_count; ++i) {
        const HudIcon *icon = &region->icons[i];
        const ScriptItem *sprite = gui_control(gfx, root, "spriteType", icon->sprite);
        const ScriptItem *texture = gui_field(gfx, sprite, "texturefile");
        if (!texture || !texture->word[0] || !mounted(texture->word)) {
            char detail[170];
            snprintf(detail, sizeof(detail), "missing sprite or mounted texture for %.106s", icon->sprite);
            return script_problem(gfx, sprite ? sprite : root, error, detail);
        }
    }
    return true;
}

void add_instance_choice(InstanceChoices *choices, size_t index, EntryAccess access) {
    if (!access.visible || choices->count == PS_MAX_INSTANCES) return;
    if (access.enabled) {
        choices->enabled |= UINT32_C(1) << choices->count;
        ++choices->enabled_count;
    }
    choices->items[choices->count++] = index;
}

EntryRoute entry_route(const InstanceChoices *choices) {
    if (!choices->count) return ENTRY_HIDDEN;
    if (!choices->enabled_count) return ENTRY_DISABLED;
    return choices->count == 1 ? ENTRY_DIRECT : ENTRY_SELECTOR;
}

bool choice_available(const InstanceChoices *choices, size_t index) {
    for (unsigned i = 0; i < choices->count; ++i)
        if (choices->items[i] == index) return (choices->enabled & (UINT32_C(1) << i)) != 0;
    return false;
}

unsigned hud_page_count(unsigned count, unsigned capacity) {
    return capacity ? (count + capacity - 1) / capacity : 0;
}
