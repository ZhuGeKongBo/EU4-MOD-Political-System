#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <assert.h>
#include <stdbool.h>

static bool fake_key_down(int key);
static DWORD WINAPI fake_window_thread(HWND window, LPDWORD process);

#define GetAsyncKeyState(key) (fake_key_down(key) ? 0x8000 : 0)
#define GetWindowThreadProcessId fake_window_thread
#include "plugin.c"
#undef GetAsyncKeyState
#undef GetWindowThreadProcessId

static bool press_open, press_escape, player_available;
static const char *viewing_country = "ASK";
static bool scotland_available = true;
static bool date_available;
static const wchar_t *definition_path, *timeline_path, *reform_path;
static bool mod_enabled, subject_type_registered;
static const char *expected_holder, *expected_filter;
static unsigned hud_windows, panels, subscriptions;
static char presented_authority[16], presented_status[64], presented_heading[96];
static unsigned effects;
static int32_t prestige;
static void *native_country;
static int32_t shown_province;
static bool province_view_allowed = true;

typedef struct FakeSurface {
    void **vtable;
    bool visible;
} FakeSurface;

typedef struct FakeWindow {
    void **vtable;
    unsigned char padding[0x28];
    FakeSurface surface;
} FakeWindow;

typedef struct FakeListenerNode {
    ClickListener *listener;
    void *unused, *next;
} FakeListenerNode;

typedef struct FakeButton {
    void **vtable;
    unsigned char padding[0xc0];
    struct { void **vtable; FakeListenerNode *first; } signal;
    FakeListenerNode subscription;
    const ButtonTooltip *tooltip;
    bool enabled, shown;
} FakeButton;

_Static_assert(offsetof(FakeWindow, surface) == 0x30, "native window surface");
_Static_assert(offsetof(FakeButton, signal) == 0xc8, "native button signal");

static FakeWindow entry_window, panel_window, badge_window;
static FakeButton icon, close_button, badge_button;
static FakeButton review_button, confirm_button, cancel_button;
static FakeWindow content_huds[2], other_panel;
static FakeButton content_buttons[2];
static FakeSurface scrollbar;

static bool fake_key_down(int key) {
    return (press_escape && key == VK_ESCAPE) || (press_open && (key == VK_CONTROL || key == VK_SHIFT || key == 'J'));
}

static DWORD WINAPI fake_window_thread(HWND window, LPDWORD process) {
    (void)window;
    *process = GetCurrentProcessId();
    return GetCurrentThreadId();
}

static AnvilResult fake_player(AnvilGamePlayer *out) {
    if (!player_available) return ANVIL_ERROR_NOT_FOUND;
    memcpy(out->own.text, viewing_country, 4);
    return ANVIL_OK;
}

static AnvilResult fake_province_view(int32_t *province) {
    if (!province_view_allowed) return ANVIL_ERROR_WRONG_PHASE;
    if (!shown_province) return ANVIL_ERROR_NOT_FOUND;
    *province = shown_province;
    return ANVIL_OK;
}

static AnvilResult fake_country(const char *tag, AnvilGameCountry *out) {
    if (strcmp(tag, "ASK") && strcmp(tag, "TIM")) return ANVIL_ERROR_NOT_FOUND;
    out->province_count = 1;
    return ANVIL_OK;
}

static int fake_date(AnvilGameDate *out) {
    if (!date_available) return false;
    out->year = 1444;
    out->month = 11;
    out->day = 11;
    return true;
}

static AnvilResult no_subjects(const char *tag, AnvilGameTag *out, uint32_t capacity, uint32_t *total) {
    assert(!strcmp(tag, expected_holder) && !out && capacity == 0);
    *total = 0;
    return ANVIL_OK;
}

static const char *mounted_source(const char *path) {
    if (!strcmp(path, PS_ROSTER_PATH)) return NULL;
    assert(!strcmp(path, GK_DEFINITION_PATH) || !strcmp(path, GK_HISTORY_PATH) || !strcmp(path, GK_REFORM_PATH));
    return mod_enabled ? "playset/test-mod" : NULL;
}

static void *open_script(const char *path) {
    assert(mod_enabled);
    const wchar_t *file = !strcmp(path, GK_DEFINITION_PATH) ? definition_path
                         : (!strcmp(path, GK_HISTORY_PATH) ? timeline_path : reform_path);
    return _wfopen(file, L"rb");
}

static int64_t script_size(void *handle) {
    FILE *file = handle;
    assert(!fseek(file, 0, SEEK_END));
    const long size = ftell(file);
    rewind(file);
    return size;
}

static int64_t read_script(void *handle, void *bytes, uint64_t size) {
    return (int64_t)fread(bytes, 1, (size_t)size, handle);
}

static int close_script(void *handle) { return fclose(handle) == 0; }

static const void *lookup_subject_type(const void *registry, const GameString *key) {
    assert(!strcmp(string_text(key), expected_filter));
    return subject_type_registered ? registry : NULL;
}

static void fake_log(AnvilPlugin *self, AnvilLogLevel level, const char *message) {
    (void)self;
    if (level >= ANVIL_LOG_WARNING && strstr(message, "political restore=")) fprintf(stderr, "%s\n", message);
}

static void *fake_create_window(void *manager, const GameString *name) {
    assert(manager == plugin.gui);
    if (!strcmp(string_text(name), "author_region_long_hud_00")) return &content_huds[0];
    if (!strcmp(string_text(name), "author_region_long_hud_01")) return &content_huds[1];
    if (!strcmp(name->inline_text, "gk_hud_slot_00")) {
        assert(name->size == strlen("gk_hud_slot_00"));
        ++hud_windows;
        return &entry_window;
    }
    assert(name->size == 15 && !strcmp(name->inline_text, "gk_history_view"));
    ++panels;
    return &panel_window;
}

static void show(FakeSurface *surface) { surface->visible = true; }
static void hide(FakeSurface *surface) { surface->visible = false; }
static void enable(FakeButton *button, bool enabled) { button->enabled = enabled; }
static void show_button(FakeButton *button) { button->shown = true; }
static void hide_button(FakeButton *button) { button->shown = false; }
static void set_tooltip(FakeButton *button, const ButtonTooltip *tooltip) { button->tooltip = tooltip; }

static void subscribe(void *signal, ClickListener *listener) {
    FakeButton *button = (FakeButton *)((unsigned char *)signal - offsetof(FakeButton, signal));
    assert(!button->signal.first);
    button->subscription.listener = listener;
    button->signal.first = &button->subscription;
    ++subscriptions;
}

static void unsubscribe(void *signal, void *listener) {
    FakeButton *button = (FakeButton *)((unsigned char *)signal - offsetof(FakeButton, signal));
    assert(button->signal.first && button->signal.first->listener == listener);
    button->signal.first = button->signal.first->next;
}

static void click(FakeButton *button) {
    assert(button->enabled && button->signal.first);
    ClickListener *listener = button->signal.first->listener;
    listener->on_click(listener->owner);
}

static void *find_button(void *window, const char *name) {
    for (unsigned i = 0; i < 2; ++i)
        if (window == &content_huds[i] && !strcmp(name, "author_control")) return &content_buttons[i];
    if (window == &entry_window && !strcmp(name, "gk_icon_cavalry")) return &icon;
    if (window == &badge_window && !strcmp(name, "gk_prov_member")) return &badge_button;
    if (window == &badge_window && !strcmp(name, "hre_button")) return &icon;
    if (window == &panel_window && !strcmp(name, "gk_close_button")) return &close_button;
    if (window == &panel_window && !strcmp(name, "gk_reform_review")) return &review_button;
    if (window == &panel_window && !strcmp(name, "gk_reform_confirm")) return &confirm_button;
    if (window == &panel_window && !strcmp(name, "gk_reform_cancel")) return &cancel_button;
    return NULL;
}

static void *find_text(void *window, const char *name) {
    assert(window == &panel_window && !strncmp(name, "gk_", 3));
    if (!strcmp(name, "gk_authority_value")) return presented_authority;
    if (!strcmp(name, "gk_reform_status")) return presented_status;
    return window;
}

static void *find_scrollbar(void *window, const GameString *name) {
    assert(window == &panel_window && !strcmp(pointer_at(name, 0), "gk_daimyo_scroll"));
    return &scrollbar;
}

static void set_text(void *control, GameString *text, bool unused) {
    assert(text->capacity >= 15 && !unused);
    if (control == presented_authority) snprintf(presented_authority, sizeof(presented_authority), "%s", string_text(text));
    else if (control == presented_status) snprintf(presented_status, sizeof(presented_status), "%s", string_text(text));
    else if (control == presented_heading) snprintf(presented_heading, sizeof(presented_heading), "%s", string_text(text));
    else assert(control == &panel_window);
}

static uint64_t *lookup_tag(const void *db, uint64_t *out, const char *text, bool ignore_case) {
    assert(db && ignore_case);
    *out = 0;
    if (!strcmp(text, expected_holder) || !strcmp(text, "MNG") || !strcmp(text, "FRA") ||
        !strcmp(text, "CAS") || !strcmp(text, "POR") || !strcmp(text, "NAV") || !strcmp(text, "SAV") || !strcmp(text, "ENG") ||
        (scotland_available && !strcmp(text, "SCO"))) {
        *out = UINT64_C(1) << 32;
        memcpy(out, text, 3);
    }
    return out;
}

static bool valid_tag(const void *tag) {
    uint64_t packed;
    memcpy(&packed, tag, sizeof(packed));
    return packed != 0;
}

static void award_prestige(void *country, const int32_t *delta, const char *file, int line) {
    assert(country == native_country && !strcmp(file, GK_REFORM_PATH) && line > 0);
    const ReformState *reform = &plugin.instances[0].reform;
    assert(!strcmp(reform->passed_id, "gk_council_reform") && reform->authority == 30);
    prestige += *delta;
    ++effects;
}

static int32_t *read_prestige(const void *country, int32_t *out) {
    assert(country == native_country);
    *out = prestige;
    return out;
}

static void assign(GameString *text, const char *value, size_t length) {
    char *result = text->inline_text;
    text->size = length;
    text->capacity = 15;
    if (length > 15) {
        result = malloc(length + 1);
        assert(result);
        memcpy(text->inline_text, &result, sizeof(result));
        text->capacity = length;
    }
    memcpy(result, value, length);
    result[length] = 0;
}

static void free_string(GameString *text) {
    if (text->capacity > 15) free(pointer_at(text, 0));
}

static void append_text(GameString *text, const char *value, size_t length) {
    char combined[512];
    const size_t old_size = (size_t)text->size;
    assert(old_size + length < sizeof(combined));
    memcpy(combined, string_text(text), old_size);
    memcpy(combined + old_size, value, length);
    free_string(text);
    assign(text, combined, old_size + length);
}

static GameString *localize(GameString *text, const char *key, const void *params, unsigned count) {
    assert(!params && !count);
    if (!strcmp(key, "PROV718")) {
        assign(text, "Kobdo", 5);
        return text;
    }
    if (!strcmp(key, "GK_PROVINCE_PART_OF")) {
        assign(text, "is part of", 10);
        return text;
    }
    const bool closing = !strcmp(key, "GK_HUD_TOOLTIP_CLOSE");
    const bool subject = !strcmp(key, !strcmp(expected_filter, "vassal") ? "vassal_title" : "daimyo_vassal_title");
    const bool reform = !strcmp(key, "GK_COUNCIL_REFORM");
    const char *translated = subject ? "Subject" : (reform ? "Establish the Council" :
                             closing ? "Close" : !strcmp(key, "GK_HUD_TOOLTIP_OPEN") ? "Open" : key);
    assign(text, translated, strlen(translated));
    return text;
}

static GameString *political_title_localize(GameString *text, const char *key, const void *params, unsigned count) {
    const char *translated = NULL;
    if (!strcmp(key, "mongol_khan_title")) translated = "合罕";
    else if (!strcmp(key, "mongol_noyan_title")) translated = "那颜";
    else if (!strcmp(key, "mongol_title")) translated = "亦克蒙古勒兀鲁思";
    else if (!strcmp(key, "PS_MEMBERS_TITLE")) translated = "成员";
    else if (!strcmp(key, "PS_REFORM_PASSED")) translated = "已通过";
    if (!translated) return localize(text, key, params, count);
    assign(text, translated, strlen(translated));
    return text;
}

typedef struct FakeBackdrop {
    void **vtable;
    GameString sprite;
    unsigned changes;
} FakeBackdrop;

typedef struct FakeBackdropWindow {
    FakeWindow window;
    unsigned char padding[SYMVAL_WINDOW_ICON_LOOKUP_CONTROLS - sizeof(FakeWindow)];
    FakeBackdrop *icons[3];
    const char *names[3];
} FakeBackdropWindow;

_Static_assert(offsetof(FakeBackdropWindow, names) == SYMVAL_WINDOW_ICON_LOOKUP_NAMES,
               "native window icon lookup vectors");

static FakeBackdropWindow backdrop_windows[4];
static unsigned backdrop_created;

static void *create_backdrop_window(void *manager, const GameString *name) {
    assert(manager == plugin.gui && !strcmp(string_text(name), "celestial_window"));
    assert(backdrop_created < sizeof(backdrop_windows) / sizeof(*backdrop_windows));
    return &backdrop_windows[backdrop_created++];
}

static void *find_backdrop_icon(const char *name, int length, const void *names, const void *icons) {
    assert(length == (int)strlen(name));
    const char *const *names_vector = names;
    FakeBackdrop *const *icons_vector = icons;
    return names_vector[0] && !strcmp(names_vector[0], name) ? icons_vector[0] : NULL;
}

static void *find_backdrop_with_placeholder(void *window, const char *name) {
    FakeBackdropWindow *native = window;
    assert(native->icons[0] && "must never reuse a shared placeholder to replace its sprite");
    return find_backdrop_icon(name, (int)strlen(name), native->names, native->icons);
}

static const GameString *backdrop_sprite(const void *icon) {
    return &((const FakeBackdrop *)icon)->sprite;
}

static void replace_backdrop_sprite(void *icon, const GameString *name) {
    FakeBackdrop *backdrop = icon;
    if (strcmp(string_text(name), "GFX_shogunate_bg")) return;
    free_string(&backdrop->sprite);
    assign(&backdrop->sprite, string_text(name), name->size);
    ++backdrop->changes;
}

static void test_inherited_background(void **surface_vtable) {
    static const AnvilHost host = { .log = fake_log };
    const char *source =
        "celestial_style = { abstract = yes members = no interface = { layout = celestial_layout "
        "  background = GFX_eoc_bg_dom close_button = no entry_button = { region = bar icon = GFX_entry } } } "
        "shogun = { copy_from = celestial_style interface = { background = GFX_shogunate_bg } } "
        "sibling = { copy_from = celestial_style } "
        "missing = { copy_from = celestial_style interface = { background = GFX_missing } } "
        "unavailable = { copy_from = shogun } "
        "cleared = { copy_from = shogun interface = { background = clear } }";
    ContentTypes types = {0};
    ScriptError error = {0};
    assert(collect_content_types(&types, source, strlen(source), "background-test.txt", &error));
    assert(!resolve_content_type(&types, "cleared", &error)->entry.background[0]);

    memset(&plugin, 0, sizeof(plugin));
    plugin.host = &host;
    plugin.create_window = create_backdrop_window;
    plugin.assign_string = assign;
    plugin.free_string = free_string;
    plugin.window_icon_method = find_backdrop_with_placeholder;
    plugin.find_window_icon = find_backdrop_icon;
    plugin.icon_sprite_name = backdrop_sprite;
    plugin.set_icon_sprite = replace_backdrop_sprite;

    void *icon_vtable[0x2b8 / 8 + 1] = { [0x2b0 / 8] = replace_backdrop_sprite, [0x2b8 / 8] = backdrop_sprite };
    void *window_vtable[0xc8 / 8 + 1] = { [0x98 / 8] = find_text, [0xc8 / 8] = find_backdrop_with_placeholder };
    plugin.shield_class = icon_vtable;
    plugin.shield_signal_class = icon_vtable;
    plugin.layouts.count = plugin.layouts.hud_count = 1;
    plugin.layouts.items[0] = (PoliticalLayout){ .id = "celestial_layout", .valid = true,
        .window = "celestial_window", .background = "panel_background",
        .background_sprite = "GFX_eoc_bg_dom", .background_is_icon = true };
    plugin.layouts.hud[0] = (HudRegion){ .id = "bar", .valid = true, .icon_count = 1, .icons[0].sprite = "GFX_entry" };

    const char *ids[] = { "shogun", "sibling", "missing", "unavailable" };
    FakeBackdrop backdrops[sizeof(ids) / sizeof(*ids)] = {0};
    for (unsigned i = 0; i < sizeof(ids) / sizeof(*ids); ++i) {
        FakeBackdropWindow *native = &backdrop_windows[i];
        native->window = (FakeWindow){ .vtable = window_vtable, .surface.vtable = surface_vtable };
        native->names[0] = "panel_background";
        native->icons[0] = i < 3 ? &backdrops[i] : NULL;
        backdrops[i].vtable = icon_vtable;
        assign(&backdrops[i].sprite, "GFX_eoc_bg_dom", strlen("GFX_eoc_bg_dom"));

        PoliticalType *type = &plugin.types[i];
        const KhanDefinition *loaded = resolve_content_type(&types, ids[i], &error);
        assert(loaded);
        type->definition = *loaded;
        assert(!resolve_type_layout(type) && type->hud == 0 && type->sprite == 0);
        assert(type->layout == &plugin.layouts.items[0]);
        PoliticalInstanceSpec *spec = &plugin.roster.items[i];
        strcpy(spec->id, ids[i]);
        plugin.instances[i] = (PoliticalInstance){ .type = type, .spec = spec };
        plugin.selected = i;
        assert(bind_scripted_panel(&plugin.instances[i]));
    }
    assert(backdrop_created == 4 && backdrops[0].changes == 1 &&
           !strcmp(string_text(backdrop_sprite(&backdrops[0])), "GFX_shogunate_bg"));
    for (unsigned i = 1; i < sizeof(ids) / sizeof(*ids); ++i) {
        assert(!backdrops[i].changes && !strcmp(string_text(backdrop_sprite(&backdrops[i])), "GFX_eoc_bg_dom"));
    }
    assert(!strcmp(plugin.layouts.items[0].background_sprite, "GFX_eoc_bg_dom"));
    for (unsigned i = 0; i < sizeof(ids) / sizeof(*ids); ++i) free_string(&backdrops[i].sprite);
    free_content_types(&types);
    puts("PASS inherited backdrop overrides apply to one native icon, preserve other windows and keep missing assets open (mock host).");
}

static void test_political_titles(void) {
    static PoliticalType fixture;
    KhanDefinition *definition = &fixture.definition;
    void *old_control = panel_view()->daimyo_label;
    panel_view()->daimyo_label = presented_heading;
    plugin.localize = political_title_localize;

    strcpy(definition->emperor, "mongol_khan_title");
    strcpy(definition->member_title, "mongol_noyan_title");
    strcpy(definition->title, "mongol_title");
    resolve_political_titles(&fixture);
    assert(!strcmp(fixture.emperor_title, "合罕"));
    assert(!strcmp(definition->title, "亦克蒙古勒兀鲁思"));
    render_member_heading(definition, 17);
    assert(!strcmp(presented_heading, "那颜 (17)"));
    render_member_heading(definition, 3);
    assert(!strcmp(presented_heading, "那颜 (3)"));

    strcpy(definition->emperor, "mongol_khan");
    strcpy(definition->member_title, "Noyan");
    resolve_political_titles(&fixture);
    assert(!strcmp(fixture.emperor_title, "Mongol Khan"));
    render_member_heading(definition, 1);
    assert(!strcmp(presented_heading, "Noyan (1)"));
    strcpy(definition->emperor, "合罕");
    strcpy(definition->member_title, "那颜");
    resolve_political_titles(&fixture);
    assert(!strcmp(fixture.emperor_title, "合罕"));
    render_member_heading(definition, 0);
    assert(!strcmp(presented_heading, "那颜 (0)"));
    definition->member_title[0] = '\0';
    render_member_heading(definition, 2);
    assert(!strcmp(presented_heading, "成员 (2)"));
    native_ui_label(presented_status, reform_status_key[REFORM_PASSED], reform_status_text[REFORM_PASSED]);
    assert(!strcmp(presented_status, "已通过"));

    plugin.localize = localize;
    panel_view()->daimyo_label = old_control;
    puts("PASS translated leader, members and system titles; literal titles, membership count refresh and reform status.");
}

static void expect_tooltip(const char *expected) {
    GameString result = { .capacity = 15 };
    assert(icon.tooltip->methods->describe(icon.tooltip, &icon, &result));
    assert(!strcmp(result.capacity > 15 ? pointer_at(&result, 0) : result.inline_text, expected));
    free_string(&result);
}

static void stock_province_bind(void *controls, void *window) {
    assert(controls && window == &badge_window);
}

static void test_province_badge(void) {
    PoliticalType type = { .definition.member_source = MEMBERS_CAPITAL_PROVINCES, .definition.title = "GKhanate" };
    PoliticalInstanceSpec spec = { .id = "gkhanate_main" };
    KhanFlagTimeline timeline = {0};
    ScriptError error = {0};
    static const char history[] = "1444.1.1 = { is_part_of_khanate = yes }";
    assert(read_flag_history(history, sizeof(history) - 1, KHAN_PROVINCE_FLAG, &timeline, &error));
    KhanateFlag row = { 718, timeline };
    plugin.roster.count = 2;
    plugin.instances[1] = (PoliticalInstance){
        .type = &type, .spec = &spec, .enabled = true, .history_date = 14441111,
        .khanate_history.provinces = { &row, 1 },
    };

    unsigned char view[0x9b1] = {0};
    capture_province_card(view + 0x9b0, &badge_window);
    assert(!badge_button.shown && plugin.province_badge.card == &badge_window);

    const void *marked_province = &row, *other_province = &spec;
    memcpy(view + 0x958, &marked_province, sizeof(marked_province));
    shown_province = 718;
    update_province_badge(viewed_province());
    assert(badge_button.shown && badge_button.tooltip && plugin.province_badge.memberships == 2);
    province_view_allowed = false;
    GameString text = { .capacity = 15 };
    assert(badge_button.tooltip->methods->describe(badge_button.tooltip, &badge_button, &text));
    assert(!strcmp(string_text(&text), "Kobdo is part of GKhanate"));
    free_string(&text);
    click(&badge_button);
    assert(plugin.pending_panel == PANEL_REQUEST_PROVINCE && plugin.pending_instance == 1);
    plugin.pending_panel = PANEL_REQUEST_NONE;

    shown_province = 719;
    memcpy(view + 0x958, &other_province, sizeof(other_province));
    assert(!badge_button.tooltip->methods->describe(badge_button.tooltip, &badge_button, &text));
    province_view_allowed = true;
    update_province_badge(viewed_province());
    assert(!badge_button.shown && !plugin.province_badge.memberships);
    shown_province = 718;
    memcpy(view + 0x958, &marked_province, sizeof(marked_province));
    update_province_badge(viewed_province());
    assert(badge_button.shown);
    shown_province = 0;
    update_province_badge(viewed_province());
    assert(!badge_button.shown && plugin.province_badge.province == 0);
    plugin.roster.count = 1;
    plugin.instances[1] = (PoliticalInstance){0};
    free(timeline.entries);
}

static void test_content_hud(void) {
    static const char definition[] =
        "fixture = { emperor = shogun members = { source = direct_subjects_of_leader subject_type = daimyo } "
        "interface = { entry_button = { region = author_region_long_hud control = author_control tooltip = GK_HUD_TOOLTIP_OPEN "
        "visible = { OR = { tag = ASK tag = OIR } } enabled = { tag = OIR } } } }";
    PoliticalType type = {0};
    ScriptError error;
    assert(read_type_definition(definition, sizeof(definition)-1, "fixture", &type.definition, &error));
    const PoliticalType *original = plugin.instances[0].type;
    const HudEntry legacy = plugin.entries[0];
    date_available = false; /* Isolate input routing from campaign data in this host fixture. */
    plugin.roster.count = 2;
    strcpy(plugin.roster.items[1].id, "fixture_2");
    plugin.instances[0].type = &type;
    plugin.instances[1] = (PoliticalInstance){ .configured = true, .enabled = true, .type = &type,
                                               .spec = &plugin.roster.items[1], .hud_slot = 1 };
    for (size_t i = 0; i < 2; ++i) {
        plugin.entries[i] = (HudEntry){0};
        content_huds[i].surface.vtable = entry_window.surface.vtable;
        content_buttons[i].vtable = icon.vtable;
        content_buttons[i].signal.vtable = icon.signal.vtable;
    }
    other_panel.surface.vtable = panel_window.surface.vtable;
    plugin.panels[1].window = &other_panel;
    viewing_country = "HAB";
    plugin.next_entry_check = 0;
    on_frame(NULL, NULL, NULL);
    assert(!plugin.entries[0].window && !plugin.entries[1].window);
    viewing_country = "ASK";
    plugin.next_entry_check = 0;
    on_frame(NULL, NULL, NULL);
    assert(content_huds[0].surface.visible && !content_buttons[0].enabled);
    GameString tip = { .capacity = 15 };
    assert(describe_hud(content_buttons[0].tooltip, &content_buttons[0], &tip) && !strcmp(string_text(&tip), "Open"));
    free_string(&tip);
    queue_panel_toggle(&plugin.entries[0]);
    assert(plugin.pending_panel == PANEL_REQUEST_NONE);
    viewing_country = "OIR";
    plugin.next_entry_check = 0;
    on_frame(NULL, NULL, NULL);
    click(&content_buttons[0]);
    viewing_country = "ASK"; /* Changed after click, before processing the request. */
    on_frame(NULL, NULL, NULL);
    assert(!plugin.visible);
    viewing_country = "OIR";
    plugin.next_entry_check = 0;
    on_frame(NULL, NULL, NULL);
    click(&content_buttons[0]);
    on_frame(NULL, NULL, NULL);
    assert(plugin.visible && plugin.selected == 0 && panel_window.surface.visible);
    click(&content_buttons[1]);
    on_frame(NULL, NULL, NULL);
    assert(plugin.visible && plugin.selected == 1 && !panel_window.surface.visible && other_panel.surface.visible);
    type.definition.entry.toggle = false;
    click(&content_buttons[1]);
    on_frame(NULL, NULL, NULL);
    assert(plugin.visible);
    type.definition.entry.toggle = true;
    click(&content_buttons[1]);
    on_frame(NULL, NULL, NULL);
    assert(!plugin.visible);
    click(&content_buttons[0]);
    on_frame(NULL, NULL, NULL);
    press_escape = true;
    on_frame(NULL, NULL, NULL);
    assert(!plugin.visible);
    press_escape = false;
    viewing_country = "HAB";
    plugin.next_entry_check = 0;
    on_frame(NULL, NULL, NULL);
    assert(!content_huds[0].surface.visible && !content_huds[1].surface.visible);
    plugin.roster.count = 1;
    plugin.entries[0] = legacy;
    plugin.instances[0].type = original;
    plugin.instances[1] = (PoliticalInstance){0};
    plugin.panels[1] = (PoliticalPanel){0};
    viewing_country = "ASK";
    date_available = true;
    puts("PASS authored HUD routing: hidden, disabled with hover, click recheck, two instance windows, switch, toggle override, reopen and Esc (mock host).");
}

static uint64_t test_ruler_id;
static bool ruler_service_ready;
static int32_t test_province_limit(void) { return 3; }
static AnvilResult election_province(int32_t id, AnvilGameProvince *out) {
    if (id < 1 || id > 2) return ANVIL_ERROR_NOT_FOUND;
    memcpy(out->owner.text, id == 1 ? expected_holder : "MNG", 4);
    return ANVIL_OK;
}
static AnvilResult election_member(const char *tag, AnvilGameCountry *out) {
    out->capital = !strcmp(tag, expected_holder) ? 1 : !strcmp(tag, "MNG") ? 2 : 0;
    out->province_count = out->capital != 0;
    return out->capital ? ANVIL_OK : ANVIL_ERROR_NOT_FOUND;
}
static AnvilResult election_monarch(const char *tag, uint32_t kind, AnvilGameMonarch *out) {
    assert(kind == ANVIL_GAME_MONARCH_RULER);
    if (!ruler_service_ready) return ANVIL_ERROR_WRONG_PHASE;
    out->id = !strcmp(tag, expected_holder) ? test_ruler_id : 333;
    return ANVIL_OK;
}

static void test_closed_panel_succession(AnvilGameWorldService *world) {
    PoliticalType type = { .id = "khanate", .definition = { .member_source = MEMBERS_CAPITAL_PROVINCES,
        .election = { .defined = true, .weight = 1, .cooldown_days=60, .all_member_voters = true, .incumbent_ties = true } } };
    const unsigned old = election_tag(expected_holder), next = election_tag("MNG");
    KhanFlagChange yes = { .date = 14441111, .enabled = true };
    KhanateFlag provinces[] = {{1,{&yes,1}}, {2,{&yes,1}}};
    PoliticalInstance *instance = &plugin.instances[0];
    instance->type = &type;
    instance->holder = old;
    instance->election = (ElectionState){ .round = 1, .ruler = 111 };
    instance->khanate_history.provinces = (KhanateHistoryFiles){provinces,2};
    instance->history_date = 14441111;
    plugin.visible = false;
    world->province_limit = test_province_limit;
    world->province = election_province;
    world->country = election_member;
    world->monarch = election_monarch;
    PsArguments vote = { .round = 1, .country = next };
    test_ruler_id = 111;
    ruler_service_ready = true;
    assert(sample_electoral_roll(instance,14441111));
    assert(!electoral_action(instance, PS_CAST_VOTE, &vote, old));
    assert(!electoral_action(instance, PS_CAST_VOTE, &vote, next));
    assert(instance->election.cooldown_count==2 && !plugin.first_notice);
    test_ruler_id = 222;
    ruler_service_ready = false;
    update_campaign_holders();
    assert(instance->election.round == 1 && instance->holder == old);
    ruler_service_ready = true;
    vote.country = old;
    assert(!strcmp(electoral_action(instance, PS_CAST_VOTE, &vote, old), "succession is pending"));
    update_campaign_holders();
    assert(instance->election.round == 2 && instance->holder == next && instance->election.ruler == 333);
    assert(instance->election.cooldown_count==0 && plugin.first_notice && plugin.first_notice==plugin.last_notice);
    assert(plugin.first_notice->winner==next);
    vote.round=2;
    assert(!electoral_action(instance, PS_CAST_VOTE, &vote, old));
    vote.country=next;
    assert(!strcmp(electoral_action(instance,PS_CAST_VOTE,&vote,old),"vote change is on cooldown"));
    assert(!strcmp(electoral_action(instance,PS_ABSTAIN,&vote,old),"an elector must choose a candidate"));
    update_campaign_holders();
    assert(instance->election.round == 2 && instance->election.last_votes == 2);
    assert(plugin.first_notice==plugin.last_notice);
    discard_first_notice();
    mark_accession(instance,next,333);
    assert(!plugin.first_notice && instance->election.cooldown_count==1);
    mark_accession(instance,next,334);
    assert(plugin.first_notice && instance->election.cooldown_count==0);
    discard_first_notice();
    puts("PASS one notice and instant unlock for an actual accession, same-country successor, repeat settlement suppression, next vote locks and forced choice.");
    type.definition.election.hereditary = true;
    instance->holder = old;
    update_campaign_holders();
    assert(instance->holder == old && instance->election.round == 2 && instance->election.ruler == 222);
    puts("PASS closed-panel succession, unavailable ruler service, refusal of late votes, one transfer and hereditary mode.");
}

static bool viewing_flags[3];

static AnvilResult native_flag_id(const char *name, int32_t *id) {
    const char *const names[] = { "show", "click", "no_view" };
    for (int32_t i = 0; i < 3; ++i)
        if (!strcmp(names[i], name)) {
            *id = i + 1;
            return ANVIL_OK;
        }
    return ANVIL_ERROR_NOT_FOUND;
}

static AnvilResult native_viewer_flags(const char *tag, AnvilGameFlag *out, uint32_t capacity, uint32_t *count) {
    assert(!strcmp(tag, viewing_country));
    *count = 0;
    for (int i = 0; i < 3; ++i) {
        if (!viewing_flags[i]) continue;
        if (*count < capacity) out[*count].id = i + 1;
        ++*count;
    }
    return ANVIL_OK;
}

static void store_instance(KhanLoadedSave *save, unsigned index) {
    const PoliticalInstance *instance = &plugin.instances[index];
    PoliticalSaveRecord record = {.data.reform = instance->reform, .history_date = instance->history_date};
    strcpy(record.type, instance->type->id);
    strcpy(record.instance, instance->spec->id);
    election_text(instance->holder, record.data.leader);
    assert(encode_political_state(&record, save->fields.entries[save->fields.count++].value));
    if (instance->has_members)
        assert(encode_explicit_members(record.instance, &instance->members, save->fields.entries[save->fields.count++].value));
}

static void set_mock_range(void *slider, const ScrollRange *range) { (void)slider; assert(range->stops == 2); }

static void set_mock_fraction(void *slider, float fraction, int direction, bool notify) {
    (void)slider;
    assert(!fraction && !direction && !notify);
}

static void ignore_mock_label(void *control, GameString *value, bool format) {
    (void)control;
    (void)value;
    assert(!format);
}

static FakeWindow test_strip, test_pager;
static FakeButton test_graphics[2], test_page_button;

static void *create_test_hud(void *gui, const GameString *name) {
    assert(gui == plugin.gui);
    if (!strcmp(string_text(name), "test_strip_00")) return &test_strip;
    if (!strcmp(string_text(name), "test_strip_page")) return &test_pager;
    return NULL;
}

static void *find_test_hud_button(void *window, const char *name) {
    if (window == &test_strip && !strcmp(name, "test_cavalry")) return &test_graphics[0];
    if (window == &test_strip && !strcmp(name, "test_infantry")) return &test_graphics[1];
    if (window == &test_pager && !strcmp(name, "ps_page_next")) return &test_page_button;
    return NULL;
}

static GameString *test_hud_loc(GameString *text, const char *key, const void *context, unsigned flags) {
    (void)context;
    (void)flags;
    assign(text, key, strlen(key));
    return text;
}

static void test_contract_hud(void *in_game, unsigned actor, void **surface, void **button) {
    test_strip = test_pager = (FakeWindow){.surface.vtable = surface};
    for (unsigned i = 0; i < 2; ++i) {
        test_graphics[i] = (FakeButton){ .vtable = button, .signal.vtable = plugin.button_signal_class };
        plugin.types[i].hud = 0;
        plugin.types[i].sprite = (int)i;
    }
    test_page_button = (FakeButton){ .vtable = button, .signal.vtable = plugin.button_signal_class };
    plugin.create_window = create_test_hud;
    plugin.find_button = find_test_hud_button;
    plugin.localize = test_hud_loc;
    plugin.layouts.hud_count = 1;
    plugin.layouts.hud[0] = (HudRegion) {
        .container = "test_strip", .page_window = "test_strip_page", .valid = true, .capacity = 1, .icon_count = 2,
        .icons = { {.control = "test_cavalry"}, {.control = "test_infantry"} }
    };
    viewing_flags[2] = viewing_flags[1] = false;
    update_type_huds(actor);
    assert(test_strip.surface.visible && test_pager.surface.visible);
    assert(test_graphics[0].shown && !test_graphics[0].enabled && !test_graphics[1].shown);
    GameString tooltip = { .capacity = 15 };
    const ButtonTooltip *provider = test_graphics[0].tooltip;
    assert(provider && provider->methods->describe(provider, &test_graphics[0], &tooltip));
    assert(!strcmp(string_text(&tooltip), "PS_TOOLTIP_ENTRY_DISABLED"));
    free_string(&tooltip);
    ClickListener *old_cavalry = test_graphics[0].signal.first->listener;
    click(&test_page_button);
    update_type_huds(actor);
    assert(!test_graphics[0].shown && !test_graphics[0].tooltip &&
           test_graphics[1].shown && test_graphics[1].enabled && test_pager.surface.visible);
    plugin.pending_panel = PANEL_REQUEST_NONE;
    old_cavalry->on_click(old_cavalry->owner);
    assert(plugin.pending_panel == PANEL_REQUEST_NONE);
    click(&test_graphics[1]);
    assert(plugin.pending_panel == PANEL_REQUEST_TYPE_ICON && plugin.pending_instance == 1);
    handle_panel_request(plugin.pending_panel, in_game, actor, 0);
    assert(plugin.visible && plugin.selected == 2);
    click(&test_page_button);
    update_type_huds(actor);
    assert(test_graphics[0].shown && !test_graphics[1].shown &&
           test_graphics[0].signal.first->listener == old_cavalry);
    viewing_flags[0] = false;
    update_type_huds(actor);
    assert(!test_pager.surface.visible && test_graphics[1].shown && plugin.visible);
    close_instance_panel("test");
    update_type_huds(0);
    assert(!test_strip.surface.visible && !test_pager.surface.visible);
    puts("PASS native HUD slot binding, disabled hover, changing sprite by page, stale sprite click rejection, and hidden empty strip (mock host).");
}

static FakeWindow scripted_window;
static FakeButton scripted_hover;
static unsigned hover_lookups;
static void *incompatible_text_methods[0x228 / 8];
static void *incompatible_balance = incompatible_text_methods;
static void *incompatible_growth = incompatible_text_methods;

static void *find_fixture_text(void *window, const char *name) {
    assert(window == &scripted_window);
    if (!strcmp(name, "fixture_balance")) return &incompatible_balance;
    if (!strcmp(name, "fixture_growth")) return &incompatible_growth;
    return NULL;
}

static void *create_test_scripted_window(void *gui, const GameString *name) {
    assert(gui == plugin.gui && !strcmp(string_text(name), "fixture_panel"));
    scripted_window.surface.visible = true;
    return &scripted_window;
}

static void *find_test_hover(void *window, const char *name) {
    assert(window == &scripted_window && !strcmp(name, "fixture_hover"));
    ++hover_lookups;
    return &scripted_hover;
}

static void test_scripted_background(void **surface, void **button) {
    static void *window_methods[0xA0 / 8] = { [0x98 / 8] = find_fixture_text };
    scripted_window = (FakeWindow){ .vtable = window_methods, .surface.vtable = surface };
    scripted_hover = (FakeButton){ .vtable = button, .signal.vtable = plugin.button_signal_class, .enabled = true };
    scripted_hover.subscription.listener = (void *)((unsigned char *)plugin.gui + 0x278);
    scripted_hover.signal.first = &scripted_hover.subscription;
    plugin.create_window = create_test_scripted_window;
    plugin.find_button = find_test_hover;
    plugin.shield_class = button;
    plugin.shield_signal_class = plugin.button_signal_class;

    PoliticalLayout layout = { .window = "fixture_panel", .background = "fixture_background" };
    plugin.selected = 0;
    plugin.types[0].layout = &layout;
    PoliticalPanel *panel = panel_view();
    *panel = (PoliticalPanel){0};
    assert(bind_scripted_panel(&plugin.instances[0]));
    assert(panel->window == &scripted_window && !scripted_window.surface.visible && !hover_lookups);

    *panel = (PoliticalPanel){0};
    strcpy(layout.hover, "fixture_hover");
    assert(bind_scripted_panel(&plugin.instances[0]));
    assert(panel->window == &scripted_window && !scripted_window.surface.visible && hover_lookups == 1);
    assert(scripted_hover.tooltip == &panel->label_hover && !scripted_hover.enabled && !scripted_hover.signal.first);

    *panel = (PoliticalPanel){0};
    strcpy(layout.resource_summary.value, "fixture_balance");
    strcpy(layout.resource_summary.growth, "fixture_growth");
    const bool scripted = plugin.types[0].definition.script_type;
    plugin.types[0].definition.script_type = true;
    assert(bind_window());
    assert(panel->window == &scripted_window && !panel->bad_template);
    assert(panel->resources.summary.value == &incompatible_balance && panel->resources.growth == &incompatible_growth);
    assert(!panel->resources.summary.hover.button && !panel->resources.growth_hover.button);
    window_visible(true);
    assert(scripted_window.surface.visible);
    window_visible(false);
    plugin.types[0].definition.script_type = scripted;
    *panel = (PoliticalPanel){0};
    plugin.types[0].layout = NULL;
    puts("PASS scripted panel binds only the separate tooltip surface, strips the default click and leaves the surface disabled (mock host).");
    puts("PASS a scripted panel opens when both native resource tooltip setters are incompatible.");
}

static void test_structure_changes(void) {
    PoliticalInstance *west = &plugin.instances[0], *south = &plugin.instances[1];
    KhanDefinition *type = &plugin.types[0].definition;
    type->member_source = MEMBERS_MIXED;
    type->member_query = (EntryCondition){ .count = 1, .nodes = {{ .test = ENTRY_TAG, .end = 1 }} };
    type->member_query.nodes[0].args.country = election_tag("SAV");
    type->territory_enabled = true;
    type->role_count = 1;
    type->roles[0] = (PoliticalRole){ .id = "quartermaster", .max_holders = 1,
                                     .require_member = true, .can_hold_with_leader = true };
    west->territory = (ProvinceList){ .count = 1, .ids = {2} };
    const unsigned savoy = election_tag("SAV"), france = election_tag("FRA"), castile = election_tag("CAS");
    PsArguments appointment = { .country = savoy, .value = true, .role = "quartermaster" };
    assert(!set_role(west, &appointment, france) && !set_role(south, &appointment, castile));
    assert(!set_role(west, &appointment, france) && west->roles[0].count == 1);
    appointment.country = france;
    assert(!strcmp(set_role(west, &appointment, france), "role has no open seats"));
    assert(set_role(south, &appointment, france));
    assert(!set_member(west, savoy, false) && !west->roles[0].count && south->roles[0].count == 1);
    bool member;
    assert(!instance_has_member(west, savoy, &member) && !member && west->excluded.count == 1);
    assert(!instance_has_member(south, savoy, &member) && member);

    static KhanLoadedSave load = {.loaded = true};
    for (unsigned i = 0; i < 3; ++i) {
        store_instance(&load, i);
        assert(store_structure(&load.fields, &plugin.instances[i]));
    }
    assert(apply_campaign_load(&load, "ASK", 14441111));
    assert(!instance_has_member(west, savoy, &member) && !member && west->territory.ids[0] == 2);
    assert(south->roles[0].count == 1 && south->roles[0].countries[0] == savoy);
    const PsArguments province = {.instance = "west", .province = 2};
    assert(!query_structure(PS_HAS_PROVINCE, &province, france, &member) && member);
    PsArguments count = {.instance = "west", .amount = 1, .comparison = PS_EQ, .role = "quartermaster"};
    assert(!query_structure(PS_MEMBER_COUNT, &count, 0, &member) && member);
    assert(!query_structure(PS_PROVINCE_COUNT, &count, 0, &member) && member);
    assert(!query_structure(PS_ROLE_COUNT, &count, 0, &member) && !member);
    strcpy(count.instance, "south");
    assert(!query_structure(PS_ROLE_COUNT, &count, 0, &member) && member);
    assert(!set_territory_province(west, 1, true) && west->territory.count == 2 && !south->territory.count);
    assert(!set_territory_province(west, 1, true) && west->territory.count == 2);
    assert(set_territory_province(west, 3, true) && west->territory.count == 2);
    assert(!set_territory_province(west, 1, false) && west->territory.count == 1);
    assert(!set_member(west, savoy, true) && !west->excluded.count && !west->roles[0].count);
    assert(!instance_has_member(west, savoy, &member) && member);
    puts("PASS mixed exclusions, full role, appointing permission, independent appointments, role and territory save restore (mock host).");
}

static void test_contract_restore_and_views(AnvilGameWorldService *world, void *in_game, void **surface, void **button) {
    const char history[] =
        "west = { type = demo date = 1444.11.11 name = shared leader = FRA members = { FRA SAV } }\n"
        "south = { type = demo date = 1444.11.11 name = shared leader = CAS members = { CAS POR } }\n"
        "neighbor = { type = other date = 1444.11.10 leader = POR members = { POR } }";
    const char definitions[] =
        "demo = { leader = yes existence = { max_instances = 2 } interface = { layout = demo"
        " view_trigger = { NOT = { has_country_flag = no_view } } entry_button = { region = hud icon = GFX_a"
        " visible = { has_country_flag = show } enabled = { has_country_flag = click } } } }\n"
        "other = { leader = yes interface = { layout = demo entry_button = { region = hud icon = GFX_b toggle = no } } }";
    ScriptError error;
    ContentTypes catalog = {0};
    assert(collect_content_types(&catalog, definitions, strlen(definitions), "fixture/types.txt", &error));
    plugin.roster.count = 0;
    assert(append_content_history(history, strlen(history), "fixture/history.txt", &plugin.roster, &error));
    plugin.type_count = 2;
    for (unsigned i = 0; i < 2; ++i) {
        PoliticalType *type = &plugin.types[i];
        const char *id = i ? "other" : "demo";
        const KhanDefinition *resolved = resolve_content_type(&catalog, id, &error);
        assert(resolved);
        *type = (PoliticalType){.definition = *resolved, .hud = -1, .usable = true};
        strcpy(type->id, id);
    }
    free_content_types(&catalog);
    plugin.ready = false;
    plugin.visible = false;
    plugin.active = true;
    world->flag_id = native_flag_id;
    world->country_flags = native_viewer_flags;
    FakeWindow windows[3] = {0};
    for (unsigned i = 0; i < 3; ++i) {
        plugin.instances[i] = (PoliticalInstance){
            .spec = &plugin.roster.items[i], .type = &plugin.types[i == 2], .configured = true
        };
        windows[i].surface.vtable = surface;
        plugin.panels[i] = (PoliticalPanel){ .window = &windows[i] };
    }
    static KhanLoadedSave load;
    assert(apply_campaign_load(&load, "ASK", 14441110) && plugin.enabled_count == 1);
    assert(!plugin.instances[0].enabled && !plugin.instances[1].enabled && plugin.instances[2].enabled);
    assert(apply_campaign_load(&load, "ASK", 14441111) && plugin.enabled_count == 3);
    assert(plugin.instances[0].holder == election_tag("FRA") && plugin.instances[1].holder == election_tag("CAS"));
    char resolved_id[GK_SCRIPT_WORD_SIZE];
    assert(strstr(resolve_type_instance("demo", resolved_id), "multiple"));
    assert(!resolve_type_instance("other", resolved_id) && !strcmp(resolved_id, "neighbor"));
    assert(strstr(resolve_type_instance("unspecified", resolved_id), "unknown"));
    for (unsigned i = 0; i < 3; ++i) store_instance(&load, i);
    load.loaded = true;
    strcpy(plugin.roster.items[0].leader, "CAS");
    plugin.roster.items[0].members.count = 0;
    assert(apply_campaign_load(&load, "ASK", 14450101) && plugin.enabled_count == 3);
    assert(plugin.instances[0].holder == election_tag("FRA") && plugin.instances[0].members.count == 2 &&
           plugin.instances[0].members.tags[1] == election_tag("SAV"));
    assert(plugin.instances[1].holder == election_tag("CAS") && plugin.instances[1].members.tags[1] == election_tag("POR"));
    static const KhanLoadedSave unextended = {.loaded = true};
    assert(apply_campaign_load(&unextended, "ASK", 14450101) && !plugin.enabled_count);
    assert(!plugin.instances[0].enabled && !plugin.instances[0].holder && !plugin.instances[0].members.count);
    assert(apply_campaign_load(&load, "ASK", 14450101) && plugin.enabled_count == 3);
    puts("PASS applied bookmark dates, independent named history seeds and saved membership restore after editing history, empty old save and no session leak (mock host).");

    // Model the native list created over the other type, over south, and with no panel open.
    const size_t selector_slots[] = {3, 2, 0};
    FakeWindow native_selectors[3] = {0};
    // With two visible instances the vertical rail is hidden, but its native slider still receives an empty range.
    void *slider[0x40 / 8] = { [0x28/8] = set_mock_range, [0x38/8] = set_mock_fraction };
    struct {
        FakeSurface head;
        unsigned char pad[0xc8 - sizeof(FakeSurface)];
        void **vtable;
    } rail = { .head.vtable = surface, .vtable = slider };
    FakeButton rows[3][SELECTOR_ROWS] = {0};
    for (unsigned view = 0; view < 3; ++view) {
        native_selectors[view].surface.vtable = surface;
        InstanceSelector *selector = &plugin.selectors[selector_slots[view]];
        *selector = (InstanceSelector){ .window = &native_selectors[view], .list.bar = &rail };
        for (unsigned i = 0; i < SELECTOR_ROWS; ++i) {
            rows[view][i].vtable = button;
            selector->rows[i].selector_slot = selector_slots[view];
            attach_tooltip(&rows[view][i], &selector->rows[i].tooltip, &selector_tooltip);
        }
    }
    void (*old_text)(void *, GameString *, bool) = plugin.set_text;
    plugin.set_text = ignore_mock_label;
    const unsigned actor = election_tag(viewing_country);
    plugin.pending_instance = 1;
    handle_panel_request(PANEL_REQUEST_TYPE_ICON, in_game, actor, 0);
    assert(plugin.visible && plugin.selected == 2 && windows[2].surface.visible);
    assert(entry_route(&((InstanceChoices){0})) == ENTRY_HIDDEN);
    assert(!instance_access(0, actor).visible);
    viewing_flags[0] = true;
    assert(instance_access(0, actor).visible && !instance_access(0, actor).enabled);
    viewing_flags[1] = true;
    plugin.pending_instance = 0;
    handle_panel_request(PANEL_REQUEST_TYPE_ICON, in_game, actor, 0);
    assert(active_selector()->open && native_selectors[0].surface.visible && windows[2].surface.visible &&
           rows[0][0].enabled && rows[0][1].enabled);
    handle_panel_request(PANEL_REQUEST_CANCEL_SELECTOR, in_game, actor, 0);
    assert(!active_selector()->open && windows[2].surface.visible);
    handle_panel_request(PANEL_REQUEST_TYPE_ICON, in_game, actor, 0);
    plugin.pending_instance = 1;
    viewing_flags[1] = false;
    handle_panel_request(PANEL_REQUEST_CHOOSE_INSTANCE, in_game, actor, 0);
    assert(active_selector()->open && !rows[0][1].enabled && windows[2].surface.visible);
    viewing_flags[1] = true;
    handle_panel_request(PANEL_REQUEST_CHOOSE_INSTANCE, in_game, actor, 0);
    assert(!active_selector()->open && plugin.selected == 1 && !windows[2].surface.visible && windows[1].surface.visible);
    handle_panel_request(PANEL_REQUEST_SWITCH_INSTANCE, in_game, actor, 0);
    assert(active_selector()->open && native_selectors[1].surface.visible && !native_selectors[0].surface.visible &&
           windows[1].surface.visible && plugin.selector_slot == 2);
    plugin.pending_panel = PANEL_REQUEST_NONE;
    queue_selector_choice(&plugin.selectors[3].rows[0]);
    assert(plugin.pending_panel == PANEL_REQUEST_NONE);
    queue_selector_choice(&plugin.selectors[2].rows[0]);
    assert(plugin.pending_panel == PANEL_REQUEST_CHOOSE_INSTANCE && plugin.pending_instance == 0);
    plugin.pending_panel = PANEL_REQUEST_NONE;
    handle_panel_request(PANEL_REQUEST_CANCEL_SELECTOR, in_game, actor, 0);
    assert(!native_selectors[1].surface.visible && windows[1].surface.visible);
    viewing_flags[1] = false;
    update_entry_windows(actor);
    assert(plugin.visible && windows[1].surface.visible);
    handle_panel_request(PANEL_REQUEST_ESCAPE, in_game, actor, 0);
    assert(!plugin.visible && !windows[1].surface.visible);
    viewing_flags[1] = true;
    plugin.pending_instance = 0;
    handle_panel_request(PANEL_REQUEST_TYPE_ICON, in_game, actor, 0);
    assert(active_selector()->open && native_selectors[2].surface.visible && !native_selectors[1].surface.visible);
    handle_panel_request(PANEL_REQUEST_CHOOSE_INSTANCE, in_game, actor, 0);
    assert(plugin.visible && plugin.selected == 0 && windows[0].surface.visible);
    viewing_flags[2] = true;
    update_entry_windows(actor);
    assert(!plugin.visible && !windows[0].surface.visible);
    plugin.pending_instance = 1;
    handle_panel_request(PANEL_REQUEST_TYPE_ICON, in_game, actor, 0);
    handle_panel_request(PANEL_REQUEST_TYPE_ICON, in_game, actor, 0);
    assert(plugin.visible && plugin.selected == 2 && windows[2].surface.visible);
    handle_panel_request(PANEL_REQUEST_CLOSE, in_game, actor, 0);
    assert(!plugin.visible && !windows[2].surface.visible);
    test_contract_hud(in_game, actor, surface, button);
    test_scripted_background(surface, button);
    test_structure_changes();
    plugin.set_text = old_text;
    release_shogunate(&plugin.current);
    puts("PASS runtime native flags, selector isolated by its parent panel, stale rows, cancel retains parent, same-name routing, view loss and closing (mock host).");
}

static int ledger_date(AnvilGameDate *date) {
    date->year = 1444;
    date->month = 12;
    date->day = 1;
    return true;
}

static AnvilResult ledger_country(const char *tag, AnvilGameCountry *out) {
    out->values = ANVIL_GAME_VALUE_MANPOWER;
    out->province_count = 1;
    out->manpower = !strcmp(tag, "SAV") ? 6000 : 3000;
    out->capital = !strcmp(tag, "SAV") ? 205 : 183;
    return ANVIL_OK;
}

static AnvilResult ledger_flags(const char *tag, AnvilGameFlag *flags, uint32_t capacity, uint32_t *count) {
    *count = !strcmp(tag, "FRA");
    if (*count && capacity) flags[0].id = 1;
    return ANVIL_OK;
}

static void prove_balance(const char *instance, const char *id, const char *country, const char *value) {
    PsArguments args = {0};
    const PsOperation op = PS_COMPARE_RESOURCE;
    assert(!ps_read_argument(&args, op, PS_INSTANCE_FIELD, instance));
    assert(!ps_read_argument(&args, op, PS_RESOURCE_FIELD, id));
    assert(!ps_read_argument(&args, op, PS_COMPARISON_FIELD, "eq"));
    assert(!ps_read_argument(&args, op, PS_AMOUNT_FIELD, value));
    if (country) assert(!ps_read_argument(&args, op, PS_COUNTRY_FIELD, country));
    bool ok = false;
    assert(!query_resource(op, &args, election_tag("FRA"), &ok) && ok);
}

static void test_resource_ledger(AnvilGameWorldService *world, AnvilGameService *game) {
    const char definitions[] =
        "ledger = { leader = yes territory = yes existence = { max_instances = 2 } authority = { initial = 10 monthly_growth = { value = 1"
        " extra = { add = 2 trigger = { political_leader = { has_country_flag = show } } } } }"
        " resources = { cohesion = { initial = 40.25 monthly_growth = {"
        " strong = { add = -0.25 trigger = { political_calculated_value = { id = stronger compare = gt value = 0 } } } }"
        " thresholds = { review = { direction = falling at = 40 } } }"
        " funds = { owner = country on_owner_loss = retain initial = 2.5 monthly_growth = { value = 0.25 } }"
        " temporary = { owner = country on_owner_loss = remove initial = -1 min = -5 }"
        " reserve = { initial = 20 monthly_growth = { value = 2"
        " charter_bonus = { modifier = 0.5 trigger = { political_has_reform = { id = charter } } } } }"
        " stipend = { owner = country on_owner_loss = retain initial = 5 monthly_growth = { value = 1"
        " office_bonus = { add = 1 trigger = { political_has_role = { role = satrap } } } } } }"
        " special_members = { satrap = { acquisition = appointed require_member = yes } }"
        " reforms = { charter = { actor_rule = leader } }"
        " calculated_values = { strength = { scope = country value = { native_value = manpower } }"
        " stronger = { scope = instance value = { count = { members = yes value_id = strength compare = gt reference = leader } } } } }";
    const char history[] =
        "west = { type = ledger date = 1444.11.11 leader = FRA members = { FRA SAV } territory = { 205 } }\n"
        "south = { type = ledger date = 1444.11.11 leader = CAS members = { CAS POR } resources = { cohesion = 70 } }";
    ContentTypes catalog = {0};
    ScriptError error;
    assert(collect_content_types(&catalog, definitions, strlen(definitions), "fixture/ledger.txt", &error));
    const KhanDefinition *definition = resolve_content_type(&catalog, "ledger", &error);
    if (!definition) fprintf(stderr, "%s\n", error.reason);
    assert(definition);
    plugin.types[0].definition = *definition;
    strcpy(plugin.types[0].id, "ledger");
    free_content_types(&catalog);
    plugin.roster.count = 0;
    assert(append_content_history(history, strlen(history), "fixture/history.txt", &plugin.roster, &error));
    for (unsigned i = 0; i < 2; ++i) plugin.instances[i] = (PoliticalInstance){
        .spec = &plugin.roster.items[i], .type = &plugin.types[0], .configured = true
    };
    plugin.active = true;
    plugin.visible = false;
    game->date = ledger_date;
    world->country = ledger_country;
    world->country_flags = NULL;
    static KhanLoadedSave save;
    assert(apply_campaign_load(&save, "FRA", 14441111));
    prove_balance("west", "authority", NULL, "10");
    prove_balance("west", "cohesion", NULL, "40.25");
    const unsigned fra = election_tag("FRA"), sav = election_tag("SAV");
    PoliticalInstance *west = &plugin.instances[0];
    PsArguments funds = { .resource = "funds", .country = sav, .fixed_amount = 7250 };
    assert(!change_resource(west, PS_SET_RESOURCE, &funds, fra));
    PsArguments temporary = { .resource = "temporary", .country = sav, .fixed_amount = 1500 };
    assert(!change_resource(west, PS_SET_RESOURCE, &temporary, fra));
    assert(!set_member(west, sav, false));
    assert(!set_member(west, sav, true));
    const PsArguments charter = { .instance = "west", .reform = "charter", .value = true };
    assert(!execute_political_action(PS_SET_REFORM, &charter, fra));
    const PsArguments satrap = { .country = sav, .role = "satrap", .value = true };
    assert(!set_role(west, &satrap, fra));
    prove_balance("west", "funds", "SAV", "7.25");
    prove_balance("west", "temporary", "SAV", "-1");
    resource_fact_day = 0;
    // France sees the support flag; the other leader does not.
    world->country_flags = ledger_flags;
    refresh_resource_world(14441201);
    prove_balance("west", "authority", NULL, "13");
    prove_balance("south", "authority", NULL, "11");
    prove_balance("west", "funds", "SAV", "7.50");
    prove_balance("west", "funds", "FRA", "2.75");
    prove_balance("west", "reserve", NULL, "23");
    prove_balance("south", "reserve", NULL, "22");
    prove_balance("west", "stipend", "SAV", "7");
    prove_balance("west", "stipend", "FRA", "6");
    prove_balance("west", "cohesion", NULL, "40.00");
    assert(plugin.first_notice && !plugin.first_notice->next);
    refresh_resource_world(14441201);
    assert(plugin.first_notice && !plugin.first_notice->next);
    PsArguments cohesion = { .resource = "cohesion", .fixed_amount = 250 };
    assert(!change_resource(west, PS_CHANGE_RESOURCE, &cohesion, fra));
    cohesion.fixed_amount = -250;
    assert(!change_resource(west, PS_CHANGE_RESOURCE, &cohesion, fra));
    assert(plugin.first_notice->next && !plugin.first_notice->next->next);
    while (plugin.first_notice) discard_first_notice();
    store_instance(&save, 0);
    store_instance(&save, 1);
    assert(store_structure(&save.fields, west) && store_resources(&save.fields, west) && store_actions(&save.fields, west));
    assert(store_resources(&save.fields, &plugin.instances[1]));
    save.loaded = true;
    assert(apply_campaign_load(&save, "FRA", 14441201));
    refresh_resource_world(14441201);
    assert(!plugin.first_notice);
    prove_balance("west", "authority", NULL, "13");
    prove_balance("west", "cohesion", NULL, "40.00");
    prove_balance("west", "funds", "SAV", "7.50");
    prove_balance("south", "cohesion", NULL, "70");
    prove_balance("west", "reserve", NULL, "23");
    prove_balance("west", "stipend", "SAV", "7");
    assert(passed_action(west, "charter"));
    puts("PASS two instances get 3 vs 2 resource growth from the same template and only the office holder receives the named stipend (mock host).");
    puts("PASS fractional account owners, retention and removal, native strength, conditional monthly resource growth, one-shot thresholds, rearm and save reload.");
}

static unsigned action_test_date;
static int action_date(AnvilGameDate *out) {
    out->year = (int32_t)(action_test_date / 10000);
    out->month = action_test_date / 100 % 100;
    out->day = action_test_date % 100;
    return true;
}

static int32_t action_awarded;
static void award_action_prestige(void *country, const int32_t *delta, const char *file, int line) {
    assert(country == native_country && !strcmp(file, "fixture/actions.txt") && line == 1);
    action_awarded += *delta;
}

static GameString *action_tooltip_text(GameString *text, const char *key, const void *args, unsigned count) {
    assert(!args && !count);
    const char *value = !strcmp(key, "PS_ACTION_EXPIRES_ON") ? " until " : key;
    assign(text, value, strlen(value));
    return text;
}

static void test_action_pipeline(AnvilGameService *game) {
    const char source[] =
        "laws = { leader = yes existence = { max_instances = 2 } authority = { initial = 60 }"
        " resources = { credits = {owner = country initial = 20 on_owner_loss = retain} }"
        " reforms = { charter = {actor_rule = leader cost = { resource = authority payer = instance amount = 20 }"
        " effect = { add_prestige = 5 } } chamber = { actor_rule = leader requires = { charter } }"
        " ai_reform = {actor_rule = leader actor_trigger = {tag = CAS}} }"
        " decisions = { petition = { actor_rule = member repeatable = no cost = {resource = credits payer = actor amount = 5} } }"
        " decrees = { service = {actor_rule = leader term = fixed duration_days = 30 cooldown_days = 45"
        " cost = {resource = credits payer = actor amount = 10} } }"
        " ai = { ai_reform = { weight = { value = 1 } } } }";
    const char history[] =
        "west = { type = laws date = 1444.11.11 leader = FRA members = { FRA SAV } }\n"
        "south = { type = laws date = 1444.11.11 leader = CAS members = { CAS POR } }";
    ContentTypes catalog = {0};
    ScriptError error;
    assert(collect_content_types(&catalog, source, strlen(source), "fixture/actions.txt", &error));
    const KhanDefinition *definition = resolve_content_type(&catalog, "laws", &error);
    if (!definition) fprintf(stderr, "%s\n", error.reason);
    assert(definition);
    plugin.types[0].definition = *definition;
    strcpy(plugin.types[0].id, "laws");
    free_content_types(&catalog);
    plugin.roster.count = 0;
    assert(append_content_history(history, strlen(history), "fixture/laws_history.txt", &plugin.roster, &error));
    for (unsigned i = 0; i < 2; ++i) plugin.instances[i] = (PoliticalInstance){
        .spec = &plugin.roster.items[i], .type = &plugin.types[0], .configured = true
    };
    plugin.add_prestige = award_action_prestige;
    game->date = action_date;
    action_test_date = 14441111;
    static KhanLoadedSave save;
    assert(apply_campaign_load(&save, "FRA", action_test_date));
    const unsigned fra = election_tag("FRA"), sav = election_tag("SAV");
    PoliticalInstance *west = &plugin.instances[0];
    PsArguments grant = { .instance = "south", .reform = "charter", .value = true };
    assert(!execute_political_action(PS_SET_REFORM, &grant, fra));
    assert(!execute_political_action(PS_SET_REFORM, &grant, fra) && !action_awarded);
    prove_balance("south", "authority", NULL, "60");
    assert(passed_action(&plugin.instances[1], "charter") && !passed_action(west, "charter"));
    strcpy(grant.reform, "chamber");
    assert(!execute_political_action(PS_SET_REFORM, &grant, fra));
    grant.value = false;
    strcpy(grant.reform, "charter");
    assert(execute_political_action(PS_SET_REFORM, &grant, fra));
    strcpy(grant.reform, "chamber");
    assert(!execute_political_action(PS_SET_REFORM, &grant, fra));
    strcpy(grant.reform, "charter");
    assert(!execute_political_action(PS_SET_REFORM, &grant, fra));
    assert(!passed_action(&plugin.instances[1], "charter"));
    puts("PASS direct reform grant and revocation, duplicate grant, prerequisite coherence, no cost or prestige, and instance isolation (mock host).");
    ActionPlan plan;
    assert(!strcmp(prepare_action(west, 0, sav, 0, action_test_date, &plan), "PS_ACTION_LEADER_ONLY"));
    assert(!strcmp(prepare_action(west, 1, fra, 0, action_test_date, &plan), "PS_ACTION_PREREQUISITE"));
    GameString reason = { .capacity = 15 };
    plugin.localize = action_tooltip_text;
    struct ActionDisplayRow row = {.entry.slot = 1, .reason = "PS_ACTION_PREREQUISITE", .actor = fra};
    append_action_reason(&reason, west, &row, action_test_date);
    assert(strstr(string_text(&reason), "\n- charter"));
    free_string(&reason);
    PsArguments charter = { .instance = "west", .action = "charter", .actor = fra };
    assert(execute_political_action(PS_RUN_ACTION, &charter, sav));
    assert(!execute_political_action(PS_RUN_ACTION, &charter, fra));
    assert(!execute_political_action(PS_RUN_ACTION, &charter, fra) && queued_actions.count == 1);
    assert(!passed_action(west, "charter"));
    drain_actions();
    assert(passed_action(west, "charter") && action_awarded == 5000);
    assert(!execute_political_action(PS_RUN_ACTION, &charter, fra));
    assert(!enqueue_action((ActionRequest){.instance=0, .slot=1, .actor=fra, .source=ACTION_UI}));
    assert(!enqueue_action((ActionRequest){.instance=0, .slot=3, .actor=fra, .source=ACTION_UI}));
    assert(!enqueue_action((ActionRequest){.instance=0, .slot=4, .actor=fra, .source=ACTION_UI}));
    viewing_country = "FRA";
    drain_actions();
    assert(passed_action(west, "chamber") && action_awarded == 5000);
    prove_balance("west", "authority", NULL, "40");
    prove_balance("west", "credits", "FRA", "5");
    prove_balance("west", "credits", "SAV", "20");
    prove_balance("south", "authority", NULL, "60");
    assert(!strcmp(prepare_action(west, 4, fra, 0, action_test_date, &plan), "PS_ACTION_IN_FORCE"));
    row = (struct ActionDisplayRow){ .entry.slot = 4, .reason = "PS_ACTION_IN_FORCE", .actor = fra };
    reason = (GameString){ .capacity = 15 };
    append_action_reason(&reason, west, &row, action_test_date);
    assert(strstr(string_text(&reason), "service until 1444.12.11"));
    free_string(&reason);
    plugin.localize = localize;
    PsArguments active = { .instance = "west", .action = "service" };
    bool receiving;
    assert(!query_action(PS_ACTION_ACTIVE, &active, fra, &receiving) && receiving);
    assert(!query_action(PS_ACTION_ACTIVE, &active, sav, &receiving) && !receiving);

    action_test_date = 14441211;
    AnvilGameDateChange edge = { .size = sizeof(edge), .date = {.year = 1444, .month = 12, .day = 11},
                               .previous = {.year = 1444, .month = 12, .day = 10} };
    on_action_day(NULL, &edge, NULL);
    assert(queued_actions.count == 1 && queued_actions.items[0].source == ACTION_AI);
    drain_actions();
    assert(passed_action(&plugin.instances[1], "ai_reform") && !passed_action(west, "ai_reform"));
    assert(!query_action(PS_ACTION_ACTIVE, &active, fra, &receiving) && !receiving);
    strcpy(active.action, "chamber");
    assert(!query_action(PS_ACTION_ACTIVE, &active, fra, &receiving) && receiving);
    assert(!strcmp(prepare_action(west, 4, fra, 0, action_test_date, &plan), "PS_ACTION_COOLDOWN"));
    store_instance(&save, 0);
    store_instance(&save, 1);
    assert(store_resources(&save.fields, west) && store_actions(&save.fields, west));
    assert(store_resources(&save.fields, &plugin.instances[1]) && store_actions(&save.fields, &plugin.instances[1]));
    save.loaded = true;
    assert(apply_campaign_load(&save, "FRA", action_test_date));
    assert(passed_action(west, "charter") && passed_action(west, "chamber") && action_awarded == 5000);
    assert(!strcmp(prepare_action(west, 4, fra, 0, action_test_date, &plan), "PS_ACTION_COOLDOWN"));
    prove_balance("west", "credits", "FRA", "5");
    puts("PASS common UI, script and opt-in AI submitter, costs and effects once, unlock, exclusive decree, cooldown, independent expiry and save restore.");
}

static AnvilResult selection_monarch(const char *tag, uint32_t kind, AnvilGameMonarch *out) {
    assert(kind == ANVIL_GAME_MONARCH_RULER);
    out->id = election_tag(tag);
    return ANVIL_OK;
}

static void test_selection_pipeline(AnvilGameService *game, AnvilGameWorldService *world) {
    const char definitions[] =
        "chairs = { leader = yes authority = {initial = 50} resources = { credits = {owner = country initial = 20 on_owner_loss = retain} }"
        " special_members = { chair = { acquisition = competition contest_period_days = 30 score = invested tie_break = stable_id"
        " investment = {resource = credits payer = actor amount = 5} permissions = {submit_petition = yes} } }"
        " decisions = { petition = { actor_rule = permission actor_permission = submit_petition repeatable = no"
        " cost = {resource = authority payer = instance amount = 5} } } }\n"
        "ranked = { leader = {require_member = yes succession = {mode = highest_value event = threshold value = strength"
        " tie_break = incumbent_then_stable_id threshold_resource = cohesion threshold_id = review} }"
        " resources = {cohesion = {initial = 40.25 thresholds = {review = {at = 40 direction = falling}}} }"
        " calculated_values = { strength = {scope = country value = {native_value = manpower}} } }\n"
        "elected = { leader = {require_member = yes succession = {mode = elected event = explicit electors_role = voter"
        " vote_cooldown_days = 30 cooldown_on_turnover = reset tie_break = incumbent_then_stable_id} }"
        " special_members = {voter = {acquisition = appointed max_holders = 2}} }";
    const char history[] =
        "seat = { type = chairs date = 1444.11.11 leader = FRA members = { FRA SAV } }\n"
        "rank = { type = ranked date = 1444.11.11 leader = FRA members = { FRA SAV } }\n"
        "ballot = { type = elected date = 1444.11.11 leader = FRA members = { FRA SAV } }";
    ContentTypes catalog = {0};
    ScriptError error;
    assert(collect_content_types(&catalog, definitions, strlen(definitions), "fixture/selection.txt", &error));
    const char *types[] = {"chairs", "ranked", "elected"};
    for (unsigned i = 0; i < 3; ++i) {
        const KhanDefinition *d = resolve_content_type(&catalog, types[i], &error);
        if (!d) fprintf(stderr, "%s\n", error.reason);
        assert(d);
        plugin.types[i].definition = *d;
        strcpy(plugin.types[i].id, types[i]);
    }
    free_content_types(&catalog);
    plugin.roster.count = 0;
    assert(append_content_history(history, strlen(history), "fixture/selection_history.txt", &plugin.roster, &error));
    for (unsigned i = 0; i < 3; ++i) {
        release_roll(&plugin.instances[i].roll);
        plugin.instances[i] = (PoliticalInstance){ .spec = &plugin.roster.items[i], .type = &plugin.types[i], .configured = true };
    }
    game->date = action_date;
    world->monarch = selection_monarch;
    action_test_date = 14441111;
    static KhanLoadedSave save;
    assert(apply_campaign_load(&save, "FRA", action_test_date));
    PoliticalInstance *seat = &plugin.instances[0], *rank = &plugin.instances[1], *ballot = &plugin.instances[2];
    const unsigned fra = election_tag("FRA"), sav = election_tag("SAV");
    assert(better_candidate(sav, 5, fra, 5, sav, true));
    assert(!better_candidate(sav, 5, fra, 5, sav, false));
    assert(prepare_action(seat, 0, fra, 0, action_test_date, &(ActionPlan){0}));
    assert(!submit_selection(0, 0, fra, 0, PS_START_CONTEST));
    assert(!submit_selection(0, 0, fra, 0, PS_INVEST_CONTEST));
    assert(!submit_selection(0, 0, sav, 0, PS_INVEST_CONTEST));
    assert(!submit_selection(0, 0, sav, 0, PS_INVEST_CONTEST));
    drain_selections(action_test_date);
    prove_balance("seat", "credits", "FRA", "15");
    prove_balance("seat", "credits", "SAV", "10");
    assert(!seat->roles[0].count && seat->contests[0].due == 14441211);

    // Save the outstanding bids and deadline before the winner is decided.
    store_instance(&save, 0);
    assert(store_resources(&save.fields, seat) && store_selections(&save.fields, seat));
    for (unsigned i = 1; i < 3; ++i) store_instance(&save, i);
    save.loaded = true;
    action_test_date = 14441201;
    assert(apply_campaign_load(&save, "FRA", action_test_date));
    assert(!seat->contests[0].settled);
    settle_contest(seat, 0, 14441210);
    assert(!seat->contests[0].settled);
    action_test_date = 14441211;
    settle_contest(seat, 0, action_test_date);
    settle_contest(seat, 0, action_test_date);
    assert(seat->contests[0].settled && seat->contests[0].winner == sav && seat->roles[0].countries[0] == sav);
    assert(!has_political_permission(seat, fra, "submit_petition") && has_political_permission(seat, sav, "submit_petition"));
    PsArguments petition = { .instance = "seat", .action = "petition" };
    assert(!execute_political_action(PS_RUN_ACTION, &petition, fra));
    assert(!execute_political_action(PS_RUN_ACTION, &petition, sav));
    drain_actions();
    prove_balance("seat", "authority", NULL, "45");

    const ResourceDefinition *cohesion = &rank->type->definition.resources[0];
    resource_crossing(rank, cohesion, 0, 40250, 40000);
    assert(rank->threshold_succession_pending && update_authored_leader(rank, action_test_date));
    assert(rank->holder == sav && !rank->threshold_succession_pending);
    assert(!update_authored_leader(rank, action_test_date));

    PsArguments voter = { .role = "voter", .country = fra, .value = true };
    assert(!set_role(ballot, &voter, fra));
    voter.country = sav;
    assert(!set_role(ballot, &voter, fra));
    assert(sample_electoral_roll(ballot, action_test_date));
    PsArguments votes = { .country = sav, .round = 1 };
    assert(!electoral_action(ballot, PS_CAST_VOTE, &votes, fra));
    assert(!electoral_action(ballot, PS_CAST_VOTE, &votes, sav));
    assert(election_locked_until(&ballot->election, fra, action_test_date));
    assert(!electoral_action(ballot, PS_RESOLVE_ELECTION, &votes, fra));
    assert(ballot->holder == sav && ballot->election.round == 2 && !ballot->election.cooldown_count);
    while (plugin.first_notice) discard_first_notice();
    puts("PASS mid-contest save and reload, exact investments, once-only settlement, granted action payment, threshold ranking and named electoral seats.");
}

static void expect_process(const char *action, unsigned target, ProcessStatus status, unsigned round) {
    PsArguments query = { .instance = "west", .target = target, .status = status, .round = round };
    strcpy(query.action, action);
    bool found;
    assert(!query_action(PS_PROCESS_STATUS, &query, 0, &found) && found);
}

static void test_process_pipeline(AnvilGameService *game) {
    static const char source[] =
        "union = { leader = yes members = { source = explicit } existence = { max_instances = 2 } authority = { initial = 20 }"
        " diplomatic_actions = { invite = { actor_rule = leader target = country response = request timeout_days = 30"
        " cost = { resource = authority payer = instance amount = 5 } ui = { region = actions_region }"
        " effect = { political_add_member = { instance = INSTANCE country = TARGET } } }"
        " auto = { actor_rule = leader target = country response = request timeout_days = 30"
        " cost = { resource = authority payer = instance amount = 5 }"
        " effect = { political_add_member = { instance = INSTANCE country = TARGET } } } }"
        " ai = { auto = { accept = { value = 1 } } }"
        " incidents = { diet = { actor_rule = leader participants = members resolution = leader_choice duration_days = 30"
        " options = { approve = { effect = { add_prestige = 3 } } reject = { effect = {} } }"
        " ui = { region = actions_region } } } }";
    static const char history[] =
        "west = { type = union date = 1444.11.11 leader = FRA members = { FRA SAV } }\n"
        "south = { type = union date = 1444.11.11 leader = CAS members = { CAS POR } }";
    ContentTypes catalog = {0};
    ScriptError error;
    assert(collect_content_types(&catalog, source, strlen(source), "fixture/actions.txt", &error));
    const KhanDefinition *d = resolve_content_type(&catalog, "union", &error);
    if (!d) fprintf(stderr, "%s\n", error.reason);
    assert(d);
    plugin.types[0].definition = *d;
    strcpy(plugin.types[0].id, "union");
    free_content_types(&catalog);
    plugin.roster.count = 0;
    assert(append_content_history(history, strlen(history), "fixture/process_history.txt", &plugin.roster, &error));
    for (unsigned i = 0; i < 2; ++i) {
        release_roll(&plugin.instances[i].roll);
        plugin.instances[i] = (PoliticalInstance){ .spec = &plugin.roster.items[i], .type = &plugin.types[0], .configured = true };
    }
    plugin.visible = false;
    game->date = action_date;
    action_test_date = 14441111;
    viewing_country = "FRA";
    static KhanLoadedSave save;
    assert(apply_campaign_load(&save, "FRA", action_test_date));
    const unsigned fra = election_tag("FRA"), eng = election_tag("ENG"), mng = election_tag("MNG"), sco = election_tag("SCO");
    const unsigned cas = election_tag("CAS"), por = election_tag("POR"), sav = election_tag("SAV");
    const unsigned targets[] = {eng, por, cas, sco, mng};
    PoliticalInstance *west = &plugin.instances[0], *south = &plugin.instances[1];
    PsArguments args = { .instance = "west", .action = "invite" };
    for (unsigned i = 0; i < sizeof(targets) / sizeof(*targets); ++i) {
        args.target = targets[i];
        if (i == 4) strcpy(args.action, "auto");
        assert(!execute_political_action(PS_RUN_ACTION, &args, fra));
    }
    args.target = 0;
    strcpy(args.action, "diet");
    assert(!execute_political_action(PS_RUN_ACTION, &args, fra));
    drain_actions();
    bool member;
    assert(!instance_has_member(west, eng, &member) && !member);
    expect_process("invite", eng, PROCESS_PENDING, 1);
    expect_process("diet", 0, PROCESS_PENDING, 1);
    prove_balance("west", "authority", NULL, "20");
    store_instance(&save, 0);
    store_instance(&save, 1);
    assert(store_resources(&save.fields, west) && store_resources(&save.fields, south) && store_processes(&save.fields, west));
    save.loaded = true;
    assert(apply_campaign_load(&save, "FRA", action_test_date));
    expect_process("invite", eng, PROCESS_PENDING, 1);
    assert(west->processes.items[0].deadline == 14441211);
    ActionRowEntry rows[32];
    const unsigned received = list_action_rows(west, eng, rows);
    assert(received == 3 && rows[0].step == ACTION_VIEW && rows[1].step == ACTION_ACCEPT && rows[2].step == ACTION_DECLINE);
    args.round = 1;
    strcpy(args.action, "invite");
    args.value = true;
    assert(!execute_political_action(PS_RESPOND_REQUEST, &args, eng));
    drain_actions();
    assert(!execute_political_action(PS_RESPOND_REQUEST, &args, eng));
    args.value = false;
    assert(!execute_political_action(PS_RESPOND_REQUEST, &args, por));
    drain_actions();
    assert(!instance_has_member(west, eng, &member) && member);
    assert(!instance_has_member(south, eng, &member) && !member);
    expect_process("invite", eng, PROCESS_ACCEPTED, 1);
    expect_process("invite", por, PROCESS_REJECTED, 1);
    prove_balance("west", "authority", NULL, "15");
    const unsigned authority = resource_find(west->type->definition.resources, west->type->definition.resource_count, "authority");
    assert(store_balance(&west->accounts[authority], 0, 0));
    action_test_date = 14441112;
    scotland_available = false;
    pulse_processes(action_test_date, true);
    assert(queued_actions.count == 1 && queued_actions.items[0].source == ACTION_AI);
    drain_actions();
    expect_process("invite", sco, PROCESS_INVALID, 1);
    expect_process("auto", mng, PROCESS_PENDING, 1);
    // Direct script reply and AI use exactly the same budget validation.
    args.value = true;
    strcpy(args.action, "auto");
    assert(!execute_political_action(PS_RESPOND_REQUEST, &args, mng));
    drain_actions();
    expect_process("auto", mng, PROCESS_PENDING, 1);
    assert(!instance_has_member(west, mng, &member) && !member);
    assert(store_balance(&west->accounts[authority], 0, 15000));
    action_test_date = 14441113;
    pulse_processes(action_test_date, true);
    drain_actions();
    expect_process("auto", mng, PROCESS_ACCEPTED, 1);
    prove_balance("west", "authority", NULL, "10");
    prove_balance("south", "authority", NULL, "20");
    strcpy(args.action, "diet");
    strcpy(args.option, "approve");
    const int32_t prestige_before = action_awarded;
    assert(!execute_political_action(PS_RESOLVE_INCIDENT, &args, sav));
    drain_actions();
    expect_process("diet", 0, PROCESS_PENDING, 1);
    assert(!execute_political_action(PS_RESOLVE_INCIDENT, &args, fra));
    drain_actions();
    assert(!execute_political_action(PS_RESOLVE_INCIDENT, &args, fra));
    drain_actions();
    expect_process("diet", 0, PROCESS_SETTLED, 1);
    assert(action_awarded == prestige_before + 3000);
    action_test_date = 14441211;
    pulse_processes(action_test_date, false);
    strcpy(args.action, "invite");
    assert(!execute_political_action(PS_RESPOND_REQUEST, &args, cas));
    drain_actions();
    expect_process("invite", cas, PROCESS_EXPIRED, 1);
    args.target = por;
    assert(!execute_political_action(PS_RUN_ACTION, &args, fra));
    drain_actions();
    expect_process("invite", por, PROCESS_PENDING, 2);
    assert(!execute_political_action(PS_RESPOND_REQUEST, &args, por));
    drain_actions();
    expect_process("invite", por, PROCESS_PENDING, 2);
    args.round = 2;
    assert(!execute_political_action(PS_WITHDRAW_REQUEST, &args, fra));
    drain_actions();
    expect_process("invite", por, PROCESS_WITHDRAWN, 2);
    scotland_available = true;
    save.fields.count = 0;
    store_instance(&save, 0);
    store_instance(&save, 1);
    assert(store_resources(&save.fields, west) && store_resources(&save.fields, south) && store_processes(&save.fields, west));
    assert(apply_campaign_load(&save, "FRA", action_test_date));
    expect_process("diet", 0, PROCESS_SETTLED, 1);
    assert(action_awarded == prestige_before + 3000);
    prove_balance("west", "authority", NULL, "10");
    puts("PASS pending invitation reload, recipient rows, isolation, reject, invalidation, expiry and duplicate, shared AI budget, one dated incident resolution.");
}

static void test_instance_lifecycle(AnvilGameService *game) {
    static const char source[] =
        "union = { leader = yes members = { source = explicit } authority = { initial = 20 }"
        " existence = { max_instances = 3 allow_dynamic_creation = yes allow_dissolution = yes create_trigger = { tag = FRA } }"
        " rules = { internal_war = block_direct }"
        " military_obligations = { defensive = yes conflict = vanilla_first defensive_trigger = { has_country_flag = banner } }"
        " special_members = { quartermaster = { acquisition = appointed require_member = yes max_holders = 1 } }"
        " diplomatic_actions = { invite = { actor_rule = leader target = country response = request timeout_days = 2"
        " effect = { political_add_member = { instance = INSTANCE country = TARGET } } } } }";
    static const char history[] =
        "west = { type = union date = 1444.11.11 leader = FRA members = { FRA SAV } }"
        "south = { type = union date = 1444.11.11 leader = CAS members = { CAS POR } }";
    ContentTypes catalog = {0};
    ScriptError error = {0};
    assert(collect_content_types(&catalog, source, strlen(source), "fixture/lifecycle.txt", &error));
    const KhanDefinition *d = resolve_content_type(&catalog, "union", &error);
    assert(d && d->block_direct_war && d->defensive_trigger.count);
    plugin.types[0].definition = *d;
    plugin.types[0].usable = true;
    strcpy(plugin.types[0].id, "union");
    plugin.type_count = 1;
    free_content_types(&catalog);
    plugin.roster.count = 0;
    assert(append_content_history(history, strlen(history), "fixture/lifecycle_history.txt", &plugin.roster, &error));
    plugin.panels[2] = (PoliticalPanel){0};
    for (unsigned i = 0; i < 2; ++i) {
        release_roll(&plugin.instances[i].roll);
        plugin.instances[i] = (PoliticalInstance){.spec = &plugin.roster.items[i], .type = &plugin.types[0], .configured = true};
    }
    plugin.visible = false;
    game->date = action_date;
    action_test_date = 14441111;
    static KhanLoadedSave original, save;
    assert(apply_campaign_load(&original, "FRA", action_test_date));
    char selected[GK_SCRIPT_WORD_SIZE] = {0};
    const char *ambiguous = resolve_type_instance("union", selected);
    assert(ambiguous && strstr(ambiguous, "multiple active instances") && !selected[0]);
    plugin.instances[1].enabled = false;
    assert(!resolve_type_instance("union", selected) && !strcmp(selected, "west"));
    plugin.instances[1].enabled = true;
    const unsigned fra = election_tag("FRA"), sav = election_tag("SAV"), cas = election_tag("CAS");
    const unsigned eng = election_tag("ENG"), por = election_tag("POR"), sco = election_tag("SCO");
    bool allowed;
    assert(!query_direct_war(fra, sav, &allowed) && !allowed);
    assert(!query_direct_war(fra, por, &allowed) && allowed);
    const PsArguments create = { .instance = "third", .type = "union", .country = eng };
    assert(execute_political_action(PS_CREATE_INSTANCE, &create, sav));
    assert(!execute_political_action(PS_CREATE_INSTANCE, &create, fra));
    assert(execute_political_action(PS_CREATE_INSTANCE, &create, fra));
    PoliticalInstance *third = &plugin.instances[2];
    assert(third->holder == eng && third->members.count == 1 && plugin.enabled_count == 3);
    assert(!execute_political_action(PS_SET_MEMBER, &(PsArguments){.instance = "third", .country = sco, .value = true}, eng));
    assert(!execute_political_action(PS_SET_ROLE, &(PsArguments){.instance = "third", .role = "quartermaster", .country = sco, .value = true}, eng));
    assert(!execute_political_action(PS_RUN_ACTION, &(PsArguments){.instance = "third", .action = "invite", .target = sav}, eng));
    drain_actions();
    const PsArguments lifecycle = {.instance = "third"};
    assert(execute_political_action(PS_SUSPEND_INSTANCE, &lifecycle, fra));
    assert(!execute_political_action(PS_SUSPEND_INSTANCE, &lifecycle, eng));
    assert(!query_direct_war(eng, sco, &allowed) && allowed);
    assert(instance_mode(third) == POLITICAL_SUSPENDED && plugin.enabled_count == 2);
    assert(execute_political_action(PS_SET_MEMBER, &(PsArguments){.instance = "third", .country = cas, .value = true}, eng));
    action_test_date = 14441114;
    pulse_processes(action_test_date, true);
    assert(third->processes.items[0].status == PROCESS_PENDING);
    for (unsigned i = 0; i < 3; ++i) {
        store_instance(&save, i);
        assert(store_resources(&save.fields, &plugin.instances[i]) && store_structure(&save.fields, &plugin.instances[i]));
    }
    assert(store_processes(&save.fields, third));
    assert(encode_lifecycle(third->spec, &third->life, save.fields.entries[save.fields.count++].value));
    save.loaded = true;
    assert(apply_campaign_load(&save, "FRA", action_test_date));
    assert(third->holder == eng && instance_mode(third) == POLITICAL_SUSPENDED && third->roles[0].count == 1);
    assert(!execute_political_action(PS_RESUME_INSTANCE, &lifecycle, eng));
    assert(third->processes.items[0].status == PROCESS_EXPIRED);
    assert(!query_direct_war(eng, sco, &allowed) && !allowed);
    assert(!execute_political_action(PS_DISSOLVE_INSTANCE, &lifecycle, eng));
    assert(instance_mode(third) == POLITICAL_DISSOLVED && !third->roles[0].count && !third->processes.count);
    assert(!query_direct_war(eng, sco, &allowed) && allowed);
    assert(!query_direct_war(fra, sav, &allowed) && !allowed);
    save.fields.count = 0;
    store_instance(&save, 0);
    store_instance(&save, 1);
    assert(encode_lifecycle(third->spec, &third->life, save.fields.entries[save.fields.count++].value));
    assert(apply_campaign_load(&save, "FRA", action_test_date));
    assert(execute_political_action(PS_CREATE_INSTANCE, &create, fra) && instance_mode(third) == POLITICAL_DISSOLVED);
    assert(apply_campaign_load(&original, "FRA", action_test_date));
    assert(plugin.roster.count == 2 && !reserved_instance_id("third"));
    puts("PASS third instance, rights, suspension, independent reload, expiry on resume, removal, reserved ID and isolated declaration rules.");
}

static unsigned scene_moves;
static void append_test_scene_child(void *child, void *parent) {
    assert(pointer_at(child, 0x110) == parent);
    memcpy((unsigned char *)parent + 0x108, &child, sizeof(child));
    ++scene_moves;
}

static void test_message_priority(void **surface_vtable) {
    unsigned char gui[0x128] = {0}, message[0xa08] = {0};
    unsigned char node[0x118] = {0}, parent[0x110] = {0};
    void *scene_methods[0xc8 / sizeof(void *) + 1] = {0};
    scene_methods[0xc8 / sizeof(void *)] = (void *)append_test_scene_child;
    const void *dialog_vtable = message;
    const void *surfaces[] = {message};
    const void *registry[] = {surfaces, surfaces + 1, surfaces + 1};
    memcpy(gui + 0x110, registry, sizeof(registry));
    memcpy(message, &dialog_vtable, sizeof(dialog_vtable));
    void *node_ref = node, *parent_ref = parent, *scene_ref = scene_methods;
    memcpy(message + 0xa00, &node_ref, sizeof(node_ref));
    memcpy(node, &scene_ref, sizeof(scene_ref));
    memcpy(node + 0x110, &parent_ref, sizeof(parent_ref));
    FakeWindow panel = {.surface.vtable = surface_vtable}, ballot = {.surface.vtable = surface_vtable};
    FakeWindow chooser = {.surface.vtable = surface_vtable};
    plugin.gui = gui;
    plugin.messages.dialog_vtable = dialog_vtable;
    plugin.messages.append_scene_child = append_test_scene_child;
    plugin.visible = true;
    plugin.selected = 0;
    plugin.panels[0] = (PoliticalPanel){ .window = &panel, .vote = {.window = &ballot, .open = true, .list.top = 2} };
    *active_selector() = (InstanceSelector){ .window = &chooser, .open = true, .list.top = 3 };
    panel.surface.visible = ballot.surface.visible = chooser.surface.visible = true;
    message[0xc5] = 1;
    assert(sync_message_priority(gui) && plugin.visible && active_selector()->open);
    assert(panel.surface.visible && ballot.surface.visible && chooser.surface.visible);
    assert(pointer_at(parent, 0x108) == node && scene_moves == 1);
    assert(sync_message_priority(gui) && scene_moves == 1);
    message[0xc5] = 0;
    assert(!sync_message_priority(gui) && panel.surface.visible && ballot.surface.visible && chooser.surface.visible);
    assert(plugin.selected == 0 && plugin.panels[0].vote.list.top == 2 && active_selector()->list.top == 3);
    puts("PASS native message scene node is raised without hiding or resetting the political panel or its subwindows.");
}

static const char *notice_lookup(const char *key) {
    if (!strcmp(key, "notice_title")) return "Council dispatch";
    if (!strcmp(key, "notice_body")) return "$SENDER$ to $RECIPIENT$ from $INSTANCE$";
    return NULL;
}

static GameString *notice_translate(GameString *text, const char *key, const void *parameters, unsigned count) {
    if (!strcmp(key, "notice_title")) {
        assert(!count);
        assign(text, "Council dispatch", strlen("Council dispatch"));
        return text;
    }
    assert(!strcmp(key, "notice_body") && count == 3);
    const unsigned char *values = parameters;
    const GameString *sender = (const void *)(values + 0x30);
    const GameString *recipient = (const void *)(values + 0x68 + 0x30);
    const GameString *instance = (const void *)(values + 0xd0 + 0x30);
    assert(!strcmp(string_text(sender), "FRA") && !strcmp(string_text(recipient), "FRA"));
    assign(text, string_text(instance), instance->size);
    return text;
}

static void test_scripted_message_targets(void) {
    const TextEngine previous = plugin.text_engine;
    const AccessionMessages old_messages = plugin.messages;
    plugin.text_engine = (TextEngine){.lookup = notice_lookup, .localize = notice_translate, .assign = assign, .release = free_string};
    plugin.messages.custom_buttons = true;
    viewing_country = "FRA";
    player_available = true;
    const unsigned fra = election_tag("FRA"), cas = election_tag("CAS");
    PoliticalInstance *first = &plugin.instances[0], *second = &plugin.instances[1];
    for (unsigned i = 0; i < 2; ++i) {
        KhanDefinition *type = &plugin.types[i].definition;
        type->messages[0] = (PoliticalMessageDefinition){.id = "dispatch", .title = "notice_title", .body = {"notice_body"},
            .path = "fixture/messages.txt", .line = 1, .popup = true};
        type->message_count = 1;
    }
    assert(first->enabled && second->enabled && plugin.first_notice == NULL);
    PsArguments request = {.actor = fra, .target = cas, .message = "dispatch"};
    strcpy(request.instance, first->spec->id);
    assert(!execute_political_action(PS_SEND_MESSAGE, &request, fra) && !plugin.first_notice);
    request.target = 0;
    assert(!execute_political_action(PS_SEND_MESSAGE, &request, fra));
    strcpy(request.instance, second->spec->id);
    assert(!execute_political_action(PS_SEND_MESSAGE, &request, fra));
    assert(plugin.custom_notice_count == 2 && plugin.first_notice->next == plugin.last_notice);
    assert(!strcmp(string_text(&plugin.first_notice->custom->body[0]), first->spec->id));
    assert(!strcmp(string_text(&plugin.last_notice->custom->body[0]), second->spec->id));
    deliver_custom_notice(plugin.first_notice, cas);
    assert(plugin.custom_notice_count == 1 && plugin.first_notice == plugin.last_notice);
    discard_first_notice();
    strcpy(request.message, "unknown");
    assert(execute_political_action(PS_SEND_MESSAGE, &request, fra));
    strcpy(request.message, "dispatch");
    strcpy(plugin.types[1].definition.messages[0].title, "missing_title");
    assert(execute_political_action(PS_SEND_MESSAGE, &request, fra) && !plugin.first_notice);
    plugin.messages = old_messages;
    plugin.text_engine = previous;
    puts("PASS only the addressed local player is queued, two instances render separately, and invalid text leaves no notification (mock).");
}

static int32_t bookmark_province_limit(void) { return 120; }

static AnvilResult bookmark_province(int32_t id, AnvilGameProvince *out) {
    const char *owner = id == 2 ? "FRA" : id == 3 ? "SAV" : id == 4 ? "FRA" : NULL;
    if (owner) memcpy(out->owner.text, owner, 4);
    return ANVIL_OK;
}

static int32_t french_capital = 2;
static AnvilResult bookmark_country(const char *tag, AnvilGameCountry *out) {
    out->capital = !strcmp(tag, "FRA") ? french_capital : 3;
    out->province_count = 1;
    return ANVIL_OK;
}

static void test_bookmark_history(AnvilGameWorldService *world) {
    static const char types[] =
        "timeline = { leader = yes special_members = { quartermaster = { acquisition = appointed } } }"
        "assembly = { leader = yes members = { source = capital_territory }"
        " territory = { source = province_history history_key = hre }"
        " special_members = { elector = { history_key = elector acquisition = appointed } } }"
        "curia = { leader = yes members = { source = query source_trigger = { religion = catholic } } }";
    static const char history[] =
        "south = { type = timeline founded = 1444.11.11"
        " 1460.1.1 = { leader = POR remove_members = { NAV } roles = { quartermaster = { POR = no CAS = yes } } }"
        " 1444.11.11 = { leader = CAS members = { CAS POR } roles = { quartermaster = { POR = yes } } }"
        " 1450.1.1 = { add_members = { NAV } } }\n"
        "electoral = { type = assembly 1444.11.11 = { leader = FRA }"
        " 1460.1.1 = { remove_provinces = { 2 } add_provinces = { 3 } roles = { elector = { FRA = no SAV = yes } } } }\n"
        "faith = { type = curia 1444.11.11 = { leader = FRA } }";
    ScriptError error;
    ContentTypes catalog = {0};
    assert(collect_content_types(&catalog, types, strlen(types), "fixture/types.txt", &error));
    plugin.roster.count = 0;
    assert(append_content_history(history, strlen(history), "fixture/timeline.txt", &plugin.roster, &error));
    static const char duplicate[] = "bad = { type = timeline 1444.1.1 = { leader = CAS } 1444.01.01 = { leader = POR } }";
    assert(!append_content_history(duplicate, strlen(duplicate), "fixture/duplicate.txt", &plugin.roster, &error) &&
           plugin.roster.count == 3 && strstr(error.reason, "duplicate historical date"));
    const char *const ids[] = {"timeline", "assembly", "curia"};
    for (unsigned i = 0; i < 3; ++i) {
        const KhanDefinition *type = resolve_content_type(&catalog, ids[i], &error);
        assert(type);
        plugin.types[i] = (PoliticalType){ .definition = *type };
        strcpy(plugin.types[i].id, ids[i]);
        release_roll(&plugin.instances[i].roll);
        plugin.instances[i] = (PoliticalInstance){
            .type = &plugin.types[i], .spec = &plugin.roster.items[i], .configured = true
        };
    }
    free_content_types(&catalog);
    const unsigned fra = election_tag("FRA"), sav = election_tag("SAV"), cas = election_tag("CAS");
    const unsigned por = election_tag("POR"), nav = election_tag("NAV");
    PoliticalType *assembly = &plugin.types[1];
    static KhanFlagChange enabled[] = {{.enabled = true}};
    static KhanateFlag provinces[46];
    for (unsigned i = 0; i < 46; ++i)
        provinces[i] = (KhanateFlag){ .key = i ? i + 10 : 2, .timeline = {enabled, 1} };
    assembly->territory_history = (KhanateHistoryFiles){ provinces, 46 };
    const char flags[] = "# elector = no\n government = monarchy\n elector = yes\n"
                         "1450.1.1 = { elector = no monarch = { name = \"Arthur\" } }";
    KhanateFlag elector = { .key = fra };
    assert(read_mounted_flag_history(flags, strlen(flags), "fixture/country.txt", "elector", &elector.timeline, &error));
    assembly->role_history[0] = (KhanateHistoryFiles){ &elector, 1 };
    world->province_limit = bookmark_province_limit;
    world->province = bookmark_province;
    world->country = bookmark_country;
    plugin.religion_bound = true;
    unsigned char religion[SYMVAL_RELIGION_KEY_SITE_OFFSET + sizeof(GameString)] = {0};
    void *faith = religion;
    GameString key = {.inline_text = "catholic", .size = 8, .capacity = 15};
    memcpy(religion + SYMVAL_RELIGION_KEY_SITE_OFFSET, &key, sizeof(key));
    memcpy((unsigned char *)native_country + SYMVAL_COUNTRY_RELIGION_SITE_OFFSET, &faith, sizeof(faith));

    static const KhanLoadedSave fresh = {0};
    PoliticalInstance *south = &plugin.instances[0], *electoral = &plugin.instances[1], *curia = &plugin.instances[2];
    plugin.visible = false;
    assert(apply_campaign_load(&fresh, "FRA", 14441110) && !plugin.enabled_count);
    assert(apply_campaign_load(&fresh, "FRA", 14441111) && plugin.enabled_count == 3);
    assert(south->holder == cas && south->members.count == 2 && south->roles[0].countries[0] == por);
    assert(electoral->territory.count == 46 && electoral->roles[0].count == 1 && electoral->roles[0].countries[0] == fra);
    bool member;
    assert(!instance_has_member(electoral, fra, &member) && member);
    assert(!instance_has_member(electoral, sav, &member) && !member);
    french_capital = 4;
    assert(!instance_has_member(electoral, fra, &member) && !member);
    french_capital = 2;
    plugin.capital_bound = true;
    memcpy((unsigned char *)native_country + SYMVAL_COUNTRY_CAPITAL_SITE_OFFSET, &french_capital, sizeof(french_capital));
    assert(!instance_has_member(electoral, fra, &member) && member);
    plugin.capital_bound = false;
    assert(!instance_has_member(curia, fra, &member) && member);
    strcpy(key.inline_text, "sunni");
    key.size = 5;
    memcpy(religion + SYMVAL_RELIGION_KEY_SITE_OFFSET, &key, sizeof(key));
    assert(!instance_has_member(curia, fra, &member) && !member);
    puts("PASS native state religion and current capital select membership without explicit country lists (mock world).");

    static KhanLoadedSave saved = {.loaded = true};
    for (unsigned i = 0; i < 3; ++i) {
        store_instance(&saved, i);
        assert(store_structure(&saved.fields, &plugin.instances[i]));
    }
    assert(apply_campaign_load(&fresh, "FRA", 14500101));
    assert(south->holder == cas && south->members.count == 3 && has_tag(south->members.tags, 3, nav));
    assert(electoral->roles[0].count == 0);
    assert(apply_campaign_load(&fresh, "FRA", 14600101));
    assert(south->holder == por && south->members.count == 2 && south->roles[0].countries[0] == cas);
    assert(electoral->territory.count == 46 && !has_tag(electoral->territory.ids, 46, 2));
    assert(electoral->roles[0].count == 1 && electoral->roles[0].countries[0] == sav);
    assert(apply_campaign_load(&saved, "FRA", 14600101));
    assert(south->holder == cas && south->members.count == 2 && south->roles[0].countries[0] == por);
    assert(electoral->history_date == 14441111 && electoral->territory.count == 46 &&
           has_tag(electoral->territory.ids, 46, 2) && electoral->roles[0].countries[0] == fra);
    puts("PASS founding gate, unordered dates, dated members and appointments, mounted yes/no marks, and saved 46-province territory overriding later history.");
    free(elector.timeline.entries);
    assembly->role_history[0] = (KhanateHistoryFiles){0};
    for (size_t i = 0; i < plugin.roster.count; ++i) free(plugin.roster.items[i].changes);
}

int wmain(int argc, wchar_t **argv) {
    assert(argc == 4 || argc == 5);
    const bool edited = argc == 5;
    definition_path = argv[1];
    timeline_path = argv[2];
    reform_path = argv[3];
    expected_holder = edited ? "TIM" : "ASK";
    expected_filter = edited ? "vassal" : "daimyo_vassal";
    AnvilHost host = { .log = fake_log };
    AnvilGameWorldService world = {
        .size = sizeof(world), .player_tags = fake_player, .country = fake_country,
        .country_subjects = no_subjects, .province_view = fake_province_view,
    };
    AnvilGameService game = { .date = fake_date };
    unsigned char root[0x1e08] = {0}, context[0x12d0] = {0}, gui[0x280] = {0};
    unsigned char db[0x120] = {0}, country[SYMVAL_COUNTRY_CAPITAL_SITE_OFFSET + sizeof(int32_t)] = {0};
    native_country = country;
    const int32_t province_count = 1;
    memcpy(country + 0x1344, &province_count, sizeof(province_count));
    void *countries[2] = { NULL, country }, *table = countries, *db_ptr = db;
    memcpy(db + 0x118, &table, sizeof(table));
    void *root_ptr = root, *context_ptr = context, *gui_ptr = gui, *registry = &game;
    memcpy(root + 0x1e00, &context_ptr, sizeof(context_ptr));
    void *surface_vtable[0x98 / 8] = { [0x88 / 8] = show, [0x90 / 8] = hide };
    void *panel_vtable[0x130 / 8] = { [0x98 / 8] = find_text, [0x128 / 8] = find_scrollbar };
    void *button_vtable[0x228 / 8] = {
        [0x88 / 8] = show_button, [0x90 / 8] = hide_button,
        [0x170 / 8] = enable, [0x220 / 8] = set_tooltip,
    };
    void *signal_vtable[] = { NULL, subscribe, unsubscribe };
    entry_window.surface.vtable = surface_vtable;
    panel_window = (FakeWindow){ .vtable = panel_vtable, .surface.vtable = surface_vtable };
    scrollbar.vtable = surface_vtable;
    icon = close_button = badge_button = review_button = confirm_button = cancel_button =
        (FakeButton){ .vtable = button_vtable, .signal.vtable = signal_vtable };
    plugin = (Plugin){
        .host = &host, .world = &world, .game = &game, .root_slot = &root_ptr, .root = root, .gui = gui,
        .create_window = fake_create_window, .find_button = find_button, .find_scrollbar = find_scrollbar,
        .original_province_card_bind = stock_province_bind,
        .button_class = button_vtable, .button_signal_class = signal_vtable, .click_vtable = panel_vtable,
        .assign_string = assign, .append_string = append_text, .free_string = free_string,
        .set_text = set_text, .localize = localize,
        .files = { mounted_source, open_script, script_size, read_script, close_script },
        .subject_types_db = &registry, .lookup_subject_type = lookup_subject_type,
        .countries_db = &db_ptr, .lookup_tag = lookup_tag, .valid_tag = valid_tag,
        .add_prestige = award_prestige, .get_prestige = read_prestige,
    };
    AnvilGameSession session = { .change = ANVIL_GAME_SESSION_STARTED };

    on_ready(NULL, NULL, NULL);
    assert(!plugin.scripts_loaded);
    on_session(NULL, &session, NULL);
    assert(!plugin.active);
    mod_enabled = true;
    on_ready(NULL, NULL, NULL);
    assert(!plugin.scripts_loaded);
    subject_type_registered = true;
    on_ready(NULL, NULL, NULL);
    PoliticalInstance *instance = &plugin.instances[0];
    assert(plugin.scripts_loaded && !strcmp(instance->type->emperor_title, edited ? "Mirza" : "Shogun") &&
           instance->timeline.count == 1);

    on_frame(NULL, NULL, NULL);
    on_session(NULL, &session, NULL);
    press_open = true;
    on_frame(NULL, NULL, NULL);
    assert(!plugin.ready && hud_windows == 0 && panels == 0);

    press_open = false;
    player_available = true;
    on_frame(NULL, NULL, NULL);
    assert(!plugin.ready && hud_windows == 0);
    date_available = true;
    on_frame(NULL, NULL, NULL);
    unsigned holder = 0;
    memcpy(&holder, expected_holder, 3);
    assert(plugin.ready && selected_instance() == instance && instance->holder == holder && hud_windows == 0);
    world.country = NULL;
    PsQueryState query;
    PsArguments setter = { .instance = PS_LEGACY_INSTANCE };
    assert(query_political_state(&setter, &query) == PS_QUERY_READY && query.leader == holder);
    memcpy(&setter.country, "MNG", 3);
    assert(!execute_political_action(PS_SET_LEADER, &setter, holder));
    assert(query_political_state(&setter, &query) == PS_QUERY_READY && query.leader == setter.country);
    memcpy(&setter.country, "QQQ", 3);
    assert(execute_political_action(PS_SET_LEADER, &setter, holder));
    assert(query_political_state(&setter, &query) == PS_QUERY_READY && query.leader != setter.country);
    setter.country = holder;
    assert(!execute_political_action(PS_SET_LEADER, &setter, holder));
    world.country = fake_country;
    on_frame(NULL, NULL, NULL);
    assert(hud_windows == 0);

    memcpy(context + 0x58, &gui_ptr, sizeof(gui_ptr));
    on_frame(NULL, NULL, NULL);
    on_frame(NULL, NULL, NULL);
    assert(hud_windows == 1 && panels == 0 && subscriptions == 1 && entry_window.surface.visible);
    expect_tooltip("Open Great Khan");

    click(&icon);
    assert(!panel_window.surface.visible);
    on_frame(NULL, NULL, NULL);
    assert(panels == 1 && subscriptions == 5 && panel_window.surface.visible);
    expect_tooltip("Close Great Khan");
    assert(!strcmp(presented_authority, "50") && !confirm_button.shown && !cancel_button.shown);
    if (edited) {
        assert(!review_button.enabled && !strcmp(presented_status, "Leader only"));
    } else {
        assert(review_button.enabled && review_button.shown);
        click(&review_button);
        on_frame(NULL, NULL, NULL);
        assert(confirm_button.shown && cancel_button.shown && prestige == 0);
        click(&cancel_button);
        on_frame(NULL, NULL, NULL);
        assert(!confirm_button.shown && !strcmp(presented_authority, "50") && effects == 0);
        click(&review_button);
        on_frame(NULL, NULL, NULL);
        click(&confirm_button);
        click(&confirm_button);
        on_frame(NULL, NULL, NULL);
        assert(!confirm_button.shown && !review_button.enabled);
        assert(effects == 1 && prestige == 5000 && !strcmp(presented_authority, "30") &&
               !strcmp(presented_status, "Passed"));
        click(&confirm_button);
        on_frame(NULL, NULL, NULL);
        assert(effects == 1 && prestige == 5000 && instance->reform.authority == 30);
    }
    click(&close_button);
    on_frame(NULL, NULL, NULL);
    assert(!panel_window.surface.visible && entry_window.surface.visible);
    click(&icon);
    on_frame(NULL, NULL, NULL);
    assert(panels == 1 && panel_window.surface.visible);
    assert(!strcmp(presented_authority, edited ? "50" : "30"));
    click(&icon);
    on_frame(NULL, NULL, NULL);
    assert(!panel_window.surface.visible);
    press_open = true;
    on_frame(NULL, NULL, NULL);
    on_frame(NULL, NULL, NULL);
    assert(panel_window.surface.visible && subscriptions == 5);

    session.change = ANVIL_GAME_SESSION_ENDED;
    on_session(NULL, &session, NULL);
    on_frame(NULL, NULL, NULL);
    assert(!plugin.ready && !instance->holder && !entry_window.surface.visible && !panel_window.surface.visible);
    click(&icon);
    assert(plugin.pending_panel == PANEL_REQUEST_NONE);
    press_open = false;
    session.change = ANVIL_GAME_SESSION_STARTED;
    on_session(NULL, &session, NULL);
    on_frame(NULL, NULL, NULL);
    assert(!entry_window.surface.visible);
    on_frame(NULL, NULL, NULL);
    assert(entry_window.surface.visible && hud_windows == 1 && subscriptions == 5);
    assert(instance->reform.authority == 50 && !instance->reform.passed_id[0]);
    test_content_hud();
    test_province_badge();
    test_closed_panel_succession(&world);
    free(instance->timeline.entries);
    test_contract_restore_and_views(&world, context, surface_vtable, button_vtable);
    test_resource_ledger(&world, &game);
    test_action_pipeline(&game);
    test_selection_pipeline(&game, &world);
    test_process_pipeline(&game);
    test_instance_lifecycle(&game);
    test_bookmark_history(&world);
    test_scripted_message_targets();
    test_message_priority(surface_vtable);
    test_political_titles();
    test_inherited_background(surface_vtable);
    printf("Campaign mock passed: embedded province button show, hover, click and hide. Player ASK, leader %s, %s.\n", expected_holder,
           edited ? "reform is blocked for non-leaders" : "cancel, duplicate confirmation, native reward and panel refresh");
    return 0;
}
