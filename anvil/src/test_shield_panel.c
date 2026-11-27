#include "plugin.c"

#include <assert.h>

typedef struct ListenerNode {
    void *listener, *unused;
    struct ListenerNode *next;
} ListenerNode;

typedef struct FakeButton {
    void **vtable;
    unsigned char padding[0xc0];
    struct {
        void **vtable;
        ListenerNode *first;
    } signal;
    ListenerNode listener;
    const ButtonTooltip *tooltip;
    int frame;
    bool shown, enabled;
} FakeButton;

typedef struct FakeTextBox {
    void **vtable;
    const ButtonTooltip *tooltip;
    bool shown;
    char text[64];
} FakeTextBox;

typedef struct FakeScrollbar {
    void **vtable;
    unsigned char padding[0xc0];
    struct Slider {
        void **vtable;
        ScrollRange range;
        float value;
        unsigned writes;
    } slider;
    bool shown;
} FakeScrollbar;

_Static_assert(offsetof(FakeButton, signal) == 0xc8, "button signal offset");
_Static_assert(offsetof(FakeScrollbar, slider) == 0xc8, "scrollbar value offset");

static uint64_t pack_tag(const char text[4], uint16_t index) {
    uint64_t tag = (uint64_t)index << 32 | UINT64_C(1) << 56;
    memcpy(&tag, text, 3);
    return tag;
}

static uint64_t *find_tag(const void *db, uint64_t *out, const char *text, bool ignore_case) {
    (void)db;
    assert(ignore_case);
    *out = 0;
    if (!strcmp(text, "ASK")) *out = pack_tag("ASK", 7);
    if (!strcmp(text, "AMA")) *out = pack_tag("AMA", 6);
    if (!strcmp(text, "REB")) *out = pack_tag("REB", 0);
    return out;
}

static bool valid_tag(const void *tag) {
    return ((const unsigned char *)tag)[7] != 0;
}

static void *pointer(void *memory, size_t offset, void *value) {
    memcpy((unsigned char *)memory + offset, &value, sizeof(value));
    return memory;
}

static void show_button(FakeButton *button) { button->shown = true; }
static void hide_button(FakeButton *button) { button->shown = false; }
static void enable_button(FakeButton *button, bool enabled) { button->enabled = enabled; }
static void set_button_frame(FakeButton *button, int frame) { button->frame = frame; }
static void refresh_button(FakeButton *button) { (void)button; }
static void set_button_tooltip(FakeButton *button, const ButtonTooltip *tooltip) { button->tooltip = tooltip; }

static void show_textbox(FakeTextBox *box) { box->shown = true; }
static void hide_textbox(FakeTextBox *box) { box->shown = false; }
static UiPoint *textbox_position(void *label, UiPoint *position) {
    (void)label;
    *position = (UiPoint){0};
    return position;
}
static void textbox_tooltip(FakeTextBox *box, const ButtonTooltip *tooltip) { box->tooltip = tooltip; }
static void capture_textbox(void *control, GameString *text, bool format) {
    assert(!format);
    FakeTextBox *box = control;
    snprintf(box->text, sizeof(box->text), "%s", string_text(text));
}

static void set_button_scope(FakeButton *button, const uint64_t *scope) {
    (void)button;
    assert(*scope == pack_tag("ASK", 7) || *scope == pack_tag("AMA", 6) || *scope == pack_tag("REB", 0));
}

static void subscribe_button(void *signal, void *listener) {
    FakeButton *button = (FakeButton *)((unsigned char *)signal - offsetof(FakeButton, signal));
    assert(button->signal.first == NULL);
    button->listener.listener = listener;
    button->signal.first = &button->listener;
}

static void unsubscribe_button(void *signal, void *listener) {
    FakeButton *button = (FakeButton *)((unsigned char *)signal - offsetof(FakeButton, signal));
    assert(button->signal.first && button->signal.first->listener == listener);
    button->signal.first = button->signal.first->next;
}

static void show_bar(FakeScrollbar *bar) { bar->shown = true; }
static void hide_bar(FakeScrollbar *bar) { bar->shown = false; }

static void set_range(struct Slider *slider, const ScrollRange *range) { slider->range = *range; }
static float slider_value(struct Slider *slider) { return slider->value; }

static void set_value(struct Slider *slider, float value, int direction, bool notify) {
    assert(direction == 0 && !notify);
    assert(value >= 0.0f && value <= (float)slider->range.maximum / SCROLL_SCALE);
    const float step = (float)slider->range.step / SCROLL_SCALE;
    slider->value = (unsigned)(value / step + 0.5f) * step;
    ++slider->writes;
}

static void fake_log(AnvilPlugin *self, AnvilLogLevel level, const char *message) {
    (void)self;
    (void)level;
    (void)message;
}

static uint64_t diplomacy_target;
static bool window_hidden;
static void open_diplomacy(void *in_game, uint64_t tag) {
    assert(in_game == pointer_at(plugin.root, 0x1e00));
    assert(plugin.visible && !window_hidden);
    diplomacy_target = tag;
}

static void hide_window(void *surface) { (void)surface; window_hidden = true; }
static void show_window(void *surface) { (void)surface; window_hidden = false; }

static void assign_inline(GameString *text, const char *source, size_t size) {
    assert(size < sizeof(text->inline_text));
    memcpy(text->inline_text,source,size);
    text->inline_text[size] = 0;
    text->size = size;
    text->capacity = 15;
}

static GameString *vote_localisation(GameString *text, const char *key, const void *context, unsigned flags) {
    (void)context; (void)flags;
    const bool vacant = !strcmp(key, "PS_OFFICE_VACANT");
    if (!strcmp(key, "PS_ROLE_HEADING")) {
        assign_inline(text, "Offices", 7);
        return text;
    }
    const char *history = !strcmp(key, "PS_LEADER_HISTORY_CURRENT") ? "Current: " :
        !strcmp(key, "PS_LEADER_HISTORY_PAST") ? "Past:" : NULL;
    if (history) {
        assign_inline(text, history, strlen(history));
        return text;
    }
    if (!strcmp(key, "PS_RESOURCE_SYSTEM_TOOLTIP") || !strcmp(key, "PS_RESOURCE_COUNTRY_TOOLTIP")) {
        const char *kind = !strcmp(key, "PS_RESOURCE_SYSTEM_TOOLTIP") ? "Shared" : "Your country";
        assign_inline(text, kind, strlen(kind));
        return text;
    }
    if (!strcmp(key, "test_resource_name")) {
        assign_inline(text, "Fortune", 7);
        return text;
    }
    if (!strcmp(key, "test_resource_tip")) {
        assign_inline(text, "Custom detail", 13);
        return text;
    }
    if (!strcmp(key, "PS_RESOURCE_NO_ACCOUNT")) {
        assign_inline(text, "No account", 10);
        return text;
    }
    const char *label = vacant ? "Vacant office" : !strcmp(key, "GK_VOTE_PENDING") ? "No candidate" :
        !strcmp(key, "PS_NOMINATION_TIP") ? "Nomination" : !strcmp(key, "test_penalty") ? "Test condition" : "";
    assign_inline(text,label,strlen(label));
    return text;
}

static void capture_label(void *control, GameString *text, bool format) {
    assert(!format);
    strcpy(control,string_text(text));
}
static void release_text(GameString *text) {
    if (text->capacity > 15) free(pointer_at(text, 0));
}

static void append_text(GameString *text, const char *suffix, size_t length) {
    const size_t old_size = (size_t)text->size, total = old_size + length;
    char *combined = malloc(total + 1);
    assert(combined);
    memcpy(combined, string_text(text), old_size);
    memcpy(combined + old_size, suffix, length);
    combined[total] = '\0';
    release_text(text);
    if (total <= 15) {
        assign_inline(text, combined, total);
        free(combined);
    } else {
        memcpy(text->inline_text, &combined, sizeof(combined));
        text->size = text->capacity = total;
    }
}

static const GameString *find_country_name(const uint64_t *tag, bool adjective, bool lower_case, bool debug) {
    assert(!adjective && !lower_case && !debug);
    static const GameString amago = { .inline_text = "Amago", .size = 5, .capacity = 15 };
    if (*tag == pack_tag("AMA", 6)) return &amago;
    assert(*tag == pack_tag("ASK", 7));
    static const GameString name = { .inline_text = "Ashikaga", .size = 8, .capacity = 15 };
    return &name;
}

static bool describe_subject(const ButtonTooltip *tooltip, const void *button, GameString *text) {
    assert(tooltip->tag == pack_tag("AMA", 6) && button == tooltip->button);
    append_text(text, "Amago", 5);
    return true;
}

static AnvilResult election_viewer(AnvilGamePlayer *player) {
    memcpy(player->own.text, "ASK", 4);
    return ANVIL_OK;
}

static const unsigned char *tooltip_viewport(const void *context) {
    return pointer_at(context, 8);
}

static char *read_fixture(const char *folder, const char *relative, size_t *length) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", folder, relative);
    FILE *file = fopen(path, "rb");
    assert(file);
    assert(!fseek(file, 0, SEEK_END));
    const long size = ftell(file);
    assert(size > 0 && !fseek(file, 0, SEEK_SET));
    char *bytes = malloc((size_t)size + 1);
    assert(bytes && fread(bytes, 1, (size_t)size, file) == (size_t)size);
    fclose(file);
    bytes[size] = 0;
    *length = (size_t)size;
    return bytes;
}

static struct { char key[128], text[512]; } rendered_keys[64];
static unsigned rendered_key_count, render_warnings;

static void read_fixture_yml(const char *folder, const char *file) {
    size_t length;
    char *bytes = read_fixture(folder, file, &length);
    for (char *line = strtok(bytes, "\n"); line; line = strtok(NULL, "\n")) {
        char *key = line + strspn(line, " \t"), *colon = strchr(key, ':');
        char *quote = colon ? strchr(colon, '"') : NULL;
        if (!quote) continue;
        assert(rendered_key_count < 64 && colon - key < 128);
        const unsigned i = rendered_key_count++;
        memcpy(rendered_keys[i].key, key, (size_t)(colon - key));
        rendered_keys[i].key[colon - key] = 0;
        char *out = rendered_keys[i].text;
        for (const char *p = quote + 1; *p && *p != '"'; ++p) {
            assert(out - rendered_keys[i].text < 511);
            if (*p == '\\' && p[1] == 'n') { *out++ = '\n'; ++p; }
            else *out++ = *p;
        }
        *out = 0;
    }
    free(bytes);
}

static const char *fixture_translation(const char *key) {
    if (!strcmp(key, "pf_format_probe")) return "$VAL|G$$VAL|R$ $VAL|Y$ $VAL|2Y$ $VAL|Y0$ $VAL|%-$ $VAL|+=$";
    for (unsigned i = rendered_key_count; i; --i)
        if (!strcmp(rendered_keys[i - 1].key, key)) return rendered_keys[i - 1].text;
    return NULL;
}

static void fixture_assign(GameString *out, const char *source, size_t count) {
    release_text(out);
    *out = (GameString){.capacity = 15};
    append_text(out, source, count);
}

static GameString *fixture_localize(GameString *out, const char *key, const void *params, unsigned count) {
    const char *raw = fixture_translation(key);
    if (!raw) return vote_localisation(out, key, NULL, 0);
    const char *tail = raw;
    for (const char *open = strchr(tail, '$'); open; open = strchr(tail, '$')) {
        append_text(out, tail, (size_t)(open - tail));
        const char *close = strchr(open + 1, '$');
        assert(close);
        const char *format = memchr(open + 1, '|', (size_t)(close - open - 1));
        const size_t size = (size_t)((format ? format : close) - open - 1);
        bool found = false;
        for (unsigned i = 0; i < count; ++i) {
            const unsigned char *param = (const unsigned char *)params + i * 0x68;
            const char *name = pointer_at(param, 0);
            if (strlen(name) != size || memcmp(name, open + 1, size)) continue;
            uint32_t kind;
            memcpy(&kind, param + 0x28, sizeof(kind));
            if (kind == 1) {
                const GameString *arg = (const void *)(param + 0x30);
                append_text(out, string_text(arg), arg->size);
            } else {
                assert(kind == 5 || kind == 7);
                int32_t value;
                memcpy(&value, param + (kind == 5 ? 0x60 : 0x50), sizeof(value));
                const double number = kind == 5 ? value : (double)value / PS_RESOURCE_SCALE;
                const int precision = format && memchr(format, '0', (size_t)(close - format)) ? 0 : 2;
                char printed[48];
                snprintf(printed, sizeof(printed), "%.*f", precision, number);
                append_text(out, printed, strlen(printed));
            }
            found = true;
            break;
        }
        assert(found);
        tail = close + 1;
    }
    append_text(out, tail, strlen(tail));
    return out;
}

static void fixture_problem(const char *path, unsigned line, const char *key, const char *param, const char *reason) {
    assert(path && key && param && reason);
    (void)line;
    ++render_warnings;
}

static void verify_example(const char *sample, const char *update, const char *edited) {
    size_t length;
    ScriptError error = {0};
    PoliticalRoster roster = {0};
    ContentTypes content = {0};
    char *bytes = read_fixture(sample, "common/political_systems/types/pf087_localisation_types.txt", &length);
    assert(collect_content_types(&content, bytes, length, "example_types.txt", &error));
    free(bytes);
    bytes = read_fixture(sample, "history/political_systems/pf087_localisation_history.txt", &length);
    assert(append_content_history(bytes, length, "example_history.txt", &roster, &error) && roster.count == 2);
    free(bytes);
    plugin.roster = roster;
    read_fixture_yml(sample, "localisation/pf087_example_l_english.yml");
    read_fixture_yml(update, "localisation/replace/pf087_notice_bridge_l_english.yml");
    plugin.localize = fixture_localize;
    plugin.text_engine = (TextEngine){fixture_localize, fixture_assign, release_text, fixture_translation, fixture_problem};
    const char *bindings = "common/political_systems/localisation/pf087_bindings.txt";
    bytes = read_fixture(sample, bindings, &length);
    assert(collect_text_templates(&plugin.text_templates, bytes, length, bindings, &error));
    free(bytes);
    for (unsigned i = 0; i < roster.count; ++i) {
        const PoliticalInstanceSpec *spec = &roster.items[i];
        const KhanDefinition *rule = resolve_content_type(&content, spec->type, &error);
        if (!rule) fprintf(stderr, "%s:%u %s\n", error.path, error.line, error.reason);
        assert(rule && rule->resource_count == 1);
        plugin.types[i].definition = *rule;
        PoliticalInstance *instance = &plugin.instances[i];
        *instance = (PoliticalInstance){.spec = spec, .type = &plugin.types[i], .enabled = true};
        strcpy(instance->title, fixture_translation(spec->title));
        const ResourceDefinition *resource = &instance->type->definition.resources[0];
        struct ResourceDisplayRow row = {0};
        char value[24];
        display_resource(&row, instance, 0, 0, value);
        const bool north = !strcmp(spec->id, "pf087_north");
        assert(!strcmp(value, north ? "18.75" : "42.50"));
        assert(row.change == (north ? 1250 : 5500));
        GameString tip = {.capacity = 15};
        assert(describe_resource_row(&row, instance, true, &tip));
        assert(strstr(string_text(&tip), instance->title) && strstr(string_text(&tip), north ? "1.25" : "5.50"));
        assert(strstr(string_text(&tip), "Base per month") && strstr(string_text(&tip), "Combined multiplier"));
        assert((strstr(string_text(&tip), "Adjustment at the resource limit") != NULL) == north);
        release_text(&tip);
        PsArguments args = {.fields = PS_FIELD(PS_RESOURCE_FIELD) | PS_FIELD(PS_AMOUNT_FIELD), .fixed_amount = resource->initial};
        strcpy(args.instance, spec->id);
        strcpy(args.resource, resource->id);
        tip = (GameString){.capacity = 15};
        assert(describe_operation(PS_COMPARE_RESOURCE, &args, 0, "PS_SCRIPT_DESC_ps_compare_resource", &tip));
        assert(strstr(string_text(&tip), instance->title) && strstr(string_text(&tip), "Requested comparison"));
        release_text(&tip);
        args.fixed_amount = north ? 19500 : 48250;
        tip = (GameString){.capacity = 15};
        assert(describe_operation(PS_SET_RESOURCE, &args, 0, "PS_SCRIPT_DESC_ps_set_resource", &tip));
        assert(strstr(string_text(&tip), "Requested balance") && strstr(string_text(&tip), north ? "19.50" : "48.25"));
        release_text(&tip);
        const TextField fields[] = {{"value", text_fixed(args.fixed_amount)}, {"breakdown", text_value("Threshold crossed")}};
        GameString body = {.capacity = 15}, log = {.capacity = 15};
        const NoticeText notice = prepare_notice("PS_RES_LIMIT", resource->thresholds[0].message,
            (PoliticalText){.instance = instance, .resource = resource, .fields = fields, .count = 2}, &body, &log);
        assert(notice.body && notice.log && strstr(notice.body, instance->title) && strstr(notice.body, "Threshold crossed"));
        assert(strstr(notice.body, north ? "19.50" : "48.25") && strstr(notice.log, north ? "19.50" : "48.25"));
        release_text(&body);
        release_text(&log);
    }
    free_text_templates(&plugin.text_templates);
    read_fixture_yml(edited, "localisation/pf087_example_l_english.yml");
    bytes = read_fixture(edited, bindings, &length);
    assert(collect_text_templates(&plugin.text_templates, bytes, length, bindings, &error));
    free(bytes);
    for (unsigned i = 0; i < roster.count; ++i) {
        const PoliticalInstance *instance = &plugin.instances[i];
        struct ResourceDisplayRow row = {0};
        char value[24];
        display_resource(&row, instance, 0, 0, value);
        GameString tip = {.capacity = 15};
        assert(describe_resource_row(&row, instance, true, &tip));
        assert(strstr(string_text(&tip), value) && strstr(string_text(&tip), i == 0 ? "1.25" : "5.50"));
        release_text(&tip);
    }
    const char bad[] = "bad = { parameters = { P = value P = maximum } }";
    assert(!collect_text_templates(&plugin.text_templates, bad, sizeof(bad)-1, "bad.txt", &error));
    GameString unavailable = {.capacity = 15};
    assert(!append_political_text(&unavailable, "pf087_shared_text", (PoliticalText){0}, NULL));
    assert(render_warnings == 1 && !unavailable.size);
    release_text(&unavailable);
    const TextField format_value = {"value", text_fixed(4250)};
    unavailable = (GameString){.capacity = 15};
    assert(append_political_text(&unavailable, "pf_format_probe", (PoliticalText){.fields = &format_value, .count = 1}, NULL));
    assert(strstr(string_text(&unavailable), "4.254.25"));
    assert(render_warnings == 1);
    release_text(&unavailable);
    free_text_templates(&plugin.text_templates);
    free_content_types(&content);
    for (unsigned i = 0; i < roster.count; ++i) free(roster.items[i].changes);
    puts("PASS same actual sample template and two parsed types bind native typed values in resource hover, trigger/effect descriptions and notice. Changed name/source using only sample content.");
}

int main(int argc, char **argv) {
    unsigned char country[0x1348] = {0}, countries_db[0x120] = {0};
    unsigned char root[0x1e08] = {0}, ui[0x1118] = {0}, gui[0x280] = {0}, window[0x38] = {0};
    const int32_t provinces = 1;
    memcpy(country + 0x1344, &provinces, sizeof(provinces));
    void *countries[8] = { [6] = country, [7] = country };
    void *db = pointer(countries_db, 0x118, countries);
    void *root_ptr = pointer(root, 0x1e00, ui);
    pointer(ui, 0x58, gui);
    pointer(ui, 0x1110, gui);
    void *window_vtable[0x98 / 8] = { [0x88 / 8] = show_window, [0x90 / 8] = hide_window };
    pointer(window, 0x30, window_vtable);
    const AnvilHost host = { .log = fake_log };
    plugin = (Plugin){ .host = &host, .root_slot = &root_ptr, .countries_db = &db,
        .root = root, .gui = gui, .active = true, .ready = true, .visible = true,
        .lookup_tag = find_tag, .valid_tag = valid_tag, .open_diplomacy = open_diplomacy,
        .append_string = append_text, .country_name = find_country_name, .describe_daimyo = describe_subject,
        .localize = vote_localisation, .free_string = release_text,
        .click_vtable = window_vtable };
    plugin.roster.count = 2;
    plugin.instances[0] = (PoliticalInstance){ .spec = &plugin.roster.items[0], .type = &plugin.types[0], .enabled = true };
    plugin.instances[1] = (PoliticalInstance){ .spec = &plugin.roster.items[1], .type = &plugin.types[0], .enabled = true };
    strcpy(plugin.types[0].emperor_title, "Shogun");
    panel_view()->window = window;

    void *button_methods[0x308 / 8] = {
        [0x88 / 8] = show_button, [0x90 / 8] = hide_button, [0x170 / 8] = enable_button,
        [0x1b0 / 8] = set_button_frame, [0x218 / 8] = set_button_scope, [0x300 / 8] = refresh_button,
        [0x220 / 8] = set_button_tooltip,
    };
    void *signal_methods[] = { NULL, subscribe_button, unsubscribe_button };
    FakeButton button = { .vtable = button_methods, .signal = { .vtable = signal_methods }, .shown = true };
    Shield *holder = &panel_view()->holder_shield;
    holder->instance = plugin.instances[0].spec;
    holder->tooltip.button = &button;
    plugin.instances[0].holder = election_tag("ASK");
    plugin.instances[0].leaders = (LeaderHistory){.count = 2, .items = {
        {.country = election_tag("AMA"), .name = "Past Ruler", .dynasty = "Past Clan", .start = 14300101, .end = 14440101},
        {.country = election_tag("ASK"), .name = "Current Ruler", .dynasty = "Current Clan", .start = 14440101},
    }};
    render_shield(holder, NULL);
    assert(!button.shown && !button.tooltip && !holder->tooltip.tag);

    render_shield(holder, "ASK");
    render_shield(holder, "ASK");
    assert(button.shown && button.enabled && button.frame == 7);
    assert(button.signal.first->listener == &holder->listener && !button.signal.first->next);
    assert(button.tooltip == &holder->tooltip);
    GameString message = { .capacity = 15 };
    assert(!button.tooltip->methods->describe(button.tooltip, window, &message));
    assert(button.tooltip->methods->describe(button.tooltip, &button, &message));
    assert(strstr(string_text(&message), "Ashikaga\nCurrent: Current Ruler Current Clan"));
    assert(strstr(string_text(&message), "AMA - Past Ruler Past Clan, 1430.01.01 - 1444.01.01"));
    holder->listener.on_click(holder->listener.owner);
    assert(plugin.pending_country == pack_tag("ASK", 7));
    assert(!open_pending_diplomacy() && !diplomacy_target && plugin.visible);
    pointer(gui, 0x20, window);
    holder->listener.on_click(holder->listener.owner);
    plugin.selected = 1;
    assert(!open_pending_diplomacy() && !diplomacy_target);
    assert(!button.tooltip->methods->describe(button.tooltip, &button, &message));
    release_text(&message);
    plugin.selected = 0;
    holder->listener.on_click(holder->listener.owner);
    assert(open_pending_diplomacy() && diplomacy_target == pack_tag("ASK", 7) && !plugin.pending_country);
    plugin.localize = vote_localisation;
    plugin.free_string = release_text;
    render_shield_slot(holder, NULL, true);
    assert(button.shown && !button.enabled && button.frame == 0 && !holder->tooltip.tag);
    assert(!button.signal.first);
    message = (GameString){ .capacity = 15 };
    assert(button.tooltip->methods->describe(button.tooltip, &button, &message));
    assert(!strcmp(message.inline_text, "Vacant office"));
    holder->listener.on_click(holder->listener.owner);
    assert(!plugin.pending_country && !window_hidden);
    render_shield_slot(holder, "ASK", true);
    assert(button.shown && button.enabled && button.frame == 7);
    holder->listener.on_click(holder->listener.owner);
    assert(open_pending_diplomacy() && diplomacy_target == pack_tag("ASK", 7));
    render_shield(holder, NULL);
    assert(!button.shown && !button.tooltip && !button.signal.first);
    puts("PASS defined empty seat shows the native REB frame and a read-only hint, clears the old target, and rebinds on reappointment.");
    struct ResourceDisplayRow *metric = &panel_view()->resources.rows[0];
    const PoliticalLayout number_layout = { .resources.rows = 1 };
    plugin.types[0].layout = &number_layout;
    attach_tooltip(&button, &metric->hover, &resource_tooltip);
    button_enabled(&button, false);
    metric->ownership = "PS_RESOURCE_SYSTEM_TOOLTIP";
    message = (GameString){ .capacity = 15 };
    assert(button.tooltip->methods->describe(button.tooltip, &button, &message) && !strcmp(message.inline_text, "Shared"));
    metric->ownership = "PS_RESOURCE_COUNTRY_TOOLTIP";
    message = (GameString){ .capacity = 15 };
    assert(button.tooltip->methods->describe(button.tooltip, &button, &message) && !strcmp(message.inline_text, "Your country"));
    metric->ownership = NULL;
    assert(!button.tooltip->methods->describe(button.tooltip, &button, &message));
    plugin.types[0].layout = NULL;
    puts("PASS per-number native tooltip binding updates with recycled rows and clears unused rows.");
    FakeButton active_action = {.vtable = button_methods, .signal.vtable = signal_methods};
    FakeButton passive_action = {.vtable = button_methods, .signal.vtable = signal_methods};
    FakeButton seal = {.vtable = button_methods}, check = {.vtable = button_methods};
    struct ActionDisplayRow action = {.hover.button = &active_action, .read_only.button = &passive_action,
        .seal = &seal, .check = &check, .visible = true, .reason = "PS_ACTION_PREREQUISITE"};
    show_action_row(&action);
    assert(!active_action.shown && !active_action.signal.first && passive_action.shown && !passive_action.signal.first);
    assert(!seal.shown && !check.shown);
    action.reason = NULL;
    show_action_row(&action);
    assert(active_action.shown && active_action.signal.first && !passive_action.shown && seal.shown && !check.shown);
    action.reason = "PS_ACTION_ALREADY_DONE";
    action.enacted = true;
    show_action_row(&action);
    assert(!active_action.shown && !active_action.signal.first && passive_action.shown && !seal.shown && check.shown);
    action.visible = false;
    show_action_row(&action);
    assert(!passive_action.shown && !check.shown);
    puts("PASS lock hides the active control, unlock installs the only action listener, completion removes it and shows the check.");

    FakeButton member_button = { .vtable = button_methods, .signal = { .vtable = signal_methods } };
    Shield *member = &panel_view()->daimyo_shields[0];
    member->instance = plugin.instances[0].spec;
    member->tooltip.button = &member_button;
    render_shield(member, "AMA");
    const ButtonTooltip *target = member_button.tooltip;
    assert(target == &member->tooltip && member_button.shown);
    message = (GameString){ .capacity = 15 };
    assert(target->methods->describe(target, &member_button, &message));
    assert(!strcmp(message.inline_text, "Amago"));
    member->listener.on_click(member->listener.owner);
    assert(open_pending_diplomacy() && diplomacy_target == pack_tag("AMA", 6) && plugin.visible);

    countries[6] = NULL;
    assert(!target->methods->describe(target, &member_button, &message));
    render_shield(member, "AMA");
    assert(!member_button.shown && !member_button.tooltip && !target->tag);
    member->listener.on_click(member->listener.owner);
    assert(!plugin.pending_country && !window_hidden);

    void *bar_methods[0x98 / 8] = { [0x88 / 8] = show_bar, [0x90 / 8] = hide_bar };
    void *slider_methods[0x40 / 8] = { [0x20 / 8] = slider_value, [0x28 / 8] = set_range, [0x38 / 8] = set_value };
    FakeScrollbar bar = { .vtable = bar_methods, .slider = { .vtable = slider_methods } };
    panel_view()->scrollbar = &bar;
    panel_view()->member_rows = DAIMYO_ROWS;
    for (uint32_t count = 31; count <= 33; ++count) {
        plugin.current.count = count;
        sync_scrollbar(false);
        assert(bar.shown == (count == 33));
    }
    assert(bar.slider.range.maximum == 1000 && bar.slider.range.step == 1000 && bar.slider.range.stops == 2);
    assert(plugin.max_top_row == 1);
    plugin.top_row = 1;
    plugin.current.count = 31;
    sync_scrollbar(false);
    assert(!bar.shown && !plugin.top_row && !plugin.max_top_row);

    plugin.selected = 1;
    panel_view()->scrollbar = &bar;
    panel_view()->member_rows = KHANATE_MEMBER_ROWS;
    for (uint32_t count = 24; count <= 33; ++count) {
        plugin.current.count = count;
        sync_scrollbar(false);
        const uint32_t expected = (count - 24 + DAIMYO_COLUMNS - 1) / DAIMYO_COLUMNS;
        assert(plugin.max_top_row == expected && bar.shown == (expected != 0));
    }
    assert(bar.slider.range.stops == 3 && plugin.max_top_row == 2);
    AnvilGameTag roster[33] = { [0] = { .text = "ASK" }, [24] = { .text = "AMA" }, [32] = { .text = "ASK" } };
    FakeButton member_slots[KHANATE_MEMBER_ROWS * DAIMYO_COLUMNS] = {0};
    countries[6] = country;
    plugin.current.daimyo = roster;
    panel_view()->show_shields = true;
    for (size_t i = 0; i < sizeof(member_slots) / sizeof(member_slots[0]); ++i) {
        member_slots[i].vtable = button_methods;
        member_slots[i].signal.vtable = signal_methods;
        panel_view()->daimyo_shields[i].tooltip.button = &member_slots[i];
    }
    plugin.top_row = 2;
    render_daimyo();
    assert(member_slots[8].shown && member_slots[8].tooltip->tag == pack_tag("AMA", 6));
    assert(member_slots[16].shown && member_slots[16].tooltip->tag == pack_tag("ASK", 7));
    assert(!member_slots[17].shown);
    plugin.current.count = 25;
    sync_scrollbar(false);
    assert(plugin.top_row == 1 && bar.slider.value == 1.0f);
    render_daimyo();
    assert(!member_slots[8].shown && !member_slots[8].tooltip);
    assert(member_slots[16].shown && member_slots[16].tooltip->tag == pack_tag("AMA", 6));
    panel_view()->member_rows = 2;
    panel_view()->member_columns = 5;
    plugin.current.count = 33;
    sync_scrollbar(true);
    assert(bar.shown && plugin.max_top_row == 5);
    panel_view()->member_wheel = -WHEEL_DELTA;
    read_scrollbar();
    assert(plugin.top_row == 1 && bar.slider.value == 1.0f);
    bar.slider.value = 5;
    read_scrollbar();
    assert(plugin.top_row == 5 && member_slots[7].shown && member_slots[7].tooltip->tag == pack_tag("ASK", 7));
    panel_view()->member_wheel = 20*WHEEL_DELTA;
    read_scrollbar();
    assert(plugin.top_row == 0 && bar.slider.value == 0);
    plugin.current.count = 8;
    sync_scrollbar(false);
    render_daimyo();
    assert(!bar.shown && !member_slots[8].shown && !member_slots[8].tooltip);
    puts("PASS authored member grid with vertical rail and wheel, clamp, shrink and cleared stale shield.");
    plugin.selected = 0;
    sync_scrollbar(true);
    assert(!bar.shown && !plugin.top_row && !plugin.max_top_row);
    ListScroll vote_list = { .bar = &bar };
    size_list(&vote_list,17-BALLOT_ROWS,true);
    vote_list.wheel = -WHEEL_DELTA;
    assert(read_list(&vote_list) && vote_list.top == 1 && bar.slider.value == 1.0f);
    assert(!read_list(&vote_list) && vote_list.top == 1);
    bar.slider.value = 4;
    vote_list.wheel = -WHEEL_DELTA;
    assert(read_list(&vote_list) && vote_list.top == 5 && bar.slider.value == 5.0f);
    vote_list.wheel = -15*WHEEL_DELTA;
    assert(read_list(&vote_list) && vote_list.top + BALLOT_ROWS == 17 && bar.slider.value == 10.0f);
    const unsigned at_end = bar.slider.writes;
    vote_list.wheel = -WHEEL_DELTA;
    assert(!read_list(&vote_list) && bar.slider.writes == at_end && bar.slider.value == 10.0f);
    bar.slider.value = 3;
    assert(read_list(&vote_list) && vote_list.top == 3);
    size_list(&vote_list,1,true);
    assert(vote_list.top == 1);
    size_list(&vote_list,0,true);
    assert(!vote_list.top && bar.shown);
    size_list(&vote_list,0,false);
    assert(!bar.shown);
    plugin.assign_string = assign_inline;
    plugin.localize = vote_localisation;
    plugin.set_text = capture_label;
    plugin.free_string = release_text;
    char vote_status[16] = {0};
    FakeButton support = { .vtable = button_methods, .signal = { .vtable = signal_methods } };
    PoliticalPanel *panel = panel_view();
    panel->show_shields = true;
    panel->supports[0].tooltip.button = &support;
    panel->support_labels[0] = vote_status;
    ElectionCountry voter = { .tag = election_tag("HMI"), .weight = 1 };
    render_elector_support(panel,0,&voter);
    assert(!support.shown && !strcmp(vote_status,"No candidate"));
    voter.support = election_tag("ASK");
    render_elector_support(panel,0,&voter);
    assert(support.shown && support.tooltip->tag == pack_tag("ASK",7) && !*vote_status);
    render_elector_support(panel,0,NULL);
    assert(!support.shown && !support.tooltip && !*vote_status);
    panel->show_shields = false;
    render_elector_support(panel,0,&voter);
    assert(!strcmp(vote_status,"ASK"));
    panel->vote.bound = true;
    panel->vote.window = window;
    plugin.types[0].definition.election.defined = true;
    assert(!panel->vote.open);
    for (int i=0; i<3; ++i) {
        panel->vote.input.action = V_OPEN;
        process_vote_input();
        assert(panel->vote.open == (i%2 == 0) && plugin.visible);
    }
    panel->vote.input.action = V_CLOSE;
    process_vote_input();
    assert(!panel->vote.open && plugin.visible);
    puts("PASS elector shield, no-candidate state, restored automatic support and clearing an empty slot.");
    puts("PASS vote toggle open, close, reopen and cancel while keeping the main window open.");
    puts("PASS native scroll units: one notch, drag followed by wheel, stable floor and ceiling, and list shrink synchronize the rows with the thumb.");

    const PoliticalLayout grouped = {.roles = {.rows = 4, .columns = 2, .group_heading = "office_group_title"}};
    PoliticalInstance *instance = &plugin.instances[0];
    KhanDefinition *definition = &plugin.types[0].definition;
    plugin.types[0].layout = &grouped;
    definition->role_count = 2;
    definition->roles[0] = (PoliticalRole){.id = "kanrei", .max_holders = 3};
    definition->roles[1] = (PoliticalRole){.id = "samurai_dokoro", .max_holders = 4};
    instance->roles[0] = (RoleHolders){.count = 2, .countries = {election_tag("ASK"), election_tag("AMA")}};
    instance->roles[1] = (RoleHolders){.count = 1, .countries = {election_tag("AMA")}};
    FakeButton seats[8] = {0}, group_hits[4] = {0};
    char section[16], headings[4][16] = {0};
    panel->roles.caption = section;
    panel->roles.list = (ListScroll){.bar = &bar};
    for (unsigned i = 0; i < 8; ++i) {
        seats[i] = (FakeButton){.vtable = button_methods, .signal = {.vtable = signal_methods}};
        panel->roles.shields[i].tooltip.button = &seats[i];
        panel->roles.shields[i].instance = instance->spec;
    }
    for (unsigned i = 0; i < 4; ++i) {
        group_hits[i].vtable = button_methods;
        panel->roles.groups[i].label = headings[i];
        panel->roles.groups[i].hover.button = &group_hits[i];
    }
    render_roles(instance, true);
    assert(!strcmp(section, "Offices") && !strcmp(headings[0], "kanrei") && !headings[1][0]);
    assert(!strcmp(headings[2], "samurai_dokoro") && !headings[3][0]);
    assert(group_hits[0].shown && group_hits[2].shown && !group_hits[1].shown && !group_hits[3].shown);
    assert(seats[0].enabled && seats[1].enabled && seats[2].shown && seats[2].frame == 0);
    assert(!seats[2].enabled && !seats[2].signal.first && !seats[3].shown && !seats[3].tooltip);
    assert(seats[4].enabled && seats[4].tooltip->tag == pack_tag("AMA", 6));
    assert(seats[7].shown && seats[7].frame == 0 && !seats[7].enabled && !seats[7].signal.first);

    definition->role_count = 3;
    definition->roles[2] = (PoliticalRole){.id = "governor", .max_holders = 3};
    render_roles(instance, false);
    assert(bar.shown && panel->roles.list.last == 2);
    panel->roles.list.wheel = -WHEEL_DELTA;
    assert(read_list(&panel->roles.list) && panel->roles.list.top == 1);
    render_roles(instance, false);
    assert(!strcmp(headings[0], "kanrei") && !strcmp(headings[1], "samurai_dokoro"));
    assert(!headings[2][0] && !strcmp(headings[3], "governor"));
    assert(seats[0].shown && seats[0].frame == 0 && !seats[0].enabled && !seats[0].signal.first);
    assert(!seats[1].shown && !seats[1].signal.first && !seats[1].tooltip);
    puts("PASS role headings and independent two-column groups, genuine REB vacancies, unused column hidden, and wheel rebind.");

    PoliticalLayout election_layout = grouped;
    strcpy(election_layout.roles.nomination_score, "score");
    strcpy(election_layout.roles.support_shield, "support");
    plugin.types[0].layout = &election_layout;
    AnvilGameWorldService world = {.size = sizeof(world), .player_tags = election_viewer};
    plugin.world = &world;
    definition->role_count = 1;
    definition->roles[0] = (PoliticalRole){.id = "tumen", .max_holders = 1};
    strcpy(definition->succession.elector_role, "tumen");
    definition->succession.nomination = (NominationRules){.defined = true, .minimum = -2000, .maximum = 2000,
        .count = 1, .terms = {{.id = "penalty", .tooltip = "test_penalty", .add = -1000}}};
    instance->roles[0] = (RoleHolders){.count = 1, .countries = {election_tag("AMA")}};
    ElectionCountry roster_rows[] = {
        {.tag = election_tag("ASK")}, {.tag = election_tag("AMA"), .tumen = true},
    };
    int16_t nomination[] = {-1000, -1000, -1000, -1000};
    instance->roll = (ElectionSnapshot){.count = 2, .date = 14441111, .members = roster_rows, .nomination = nomination};
    assert(!election_prefer(&instance->election, roster_rows[1].tag, roster_rows[0].tag, 2));
    FakeButton score_hit = {.vtable = button_methods, .signal.vtable = signal_methods};
    FakeButton badge = {.vtable = button_methods, .signal.vtable = signal_methods};
    char score_label[32];
    bind_nomination_display(&panel->roles.scores[0], score_label, &score_hit, (UiPoint){0});
    assert(score_hit.tooltip == &panel->roles.scores[0].hover);
    panel->roles.supports[0].tooltip.button = &badge;
    panel->roles.supports[0].instance = instance->spec;
    render_roles(instance, true);
    assert(score_hit.shown && score_hit.enabled && !score_hit.signal.first);
    assert(badge.shown && badge.tooltip->tag == pack_tag("ASK",7));
    message = (GameString){.capacity = 15};
    assert(score_hit.tooltip->methods->describe(score_hit.tooltip, &score_hit, &message));
    assert(strstr(string_text(&message), "Vote weight: 2") && strstr(string_text(&message), "Currently supports: Ashikaga"));
    assert(strstr(string_text(&message), "-1000  Test condition"));
    assert(strstr(string_text(&message), "Amago: -1000\nAshikaga: -1000"));
    release_text(&message);
    assert(!election_prefer(&instance->election, roster_rows[1].tag, roster_rows[1].tag, 2));
    render_roles(instance, false);
    assert(badge.shown && badge.tooltip->tag == pack_tag("AMA",6));

    election_layout.area = (UiRectangle){-305, -359, 756, 719};
    election_layout.roles.nomination_areas[0] = (UiRectangle){103, 357, 35, 20};
    const UiPoint viewport_size = {1969, 1230};
    const float viewport_scale[2] = {1969.0f / 2560, 1230.0f / 1600};
    unsigned char viewport[0x4e0] = {0}, dispatcher[0x28] = {0};
    memcpy(viewport + 0x490, &viewport_size, sizeof(viewport_size));
    memcpy(viewport + 0x4d8, viewport_scale, sizeof(viewport_scale));
    void *viewport_methods[0x80 / 8] = { [0x78 / 8] = tooltip_viewport };
    void *viewport_context[] = {viewport_methods, viewport};
    pointer(dispatcher, 0x20, viewport_context);
    UiPoint cursor = {1040, 811};
    AnvilCpuContext cpu = { .rsi = (uintptr_t)dispatcher, .r12 = (uintptr_t)&cursor, .r14 = (uintptr_t)window + 0x30 };
    route_nomination_tooltip(&cpu, NULL);
    assert(cpu.r14 == (uintptr_t)&seats[0]);
    message = (GameString){.capacity = 15};
    assert(seats[0].tooltip->methods->describe(seats[0].tooltip, &seats[0], &message));
    assert(strstr(string_text(&message), "Amago > Ashikaga    -1000"));
    assert(strstr(string_text(&message), "Currently supports: Amago"));
    assert(strstr(string_text(&message), "-1000  Test condition"));
    assert(panel->roles.scores[0].tooltip_logged && !nomination_route.shield);
    release_text(&message);
    message = (GameString){.capacity = 15};
    assert(seats[0].tooltip->methods->describe(seats[0].tooltip, &seats[0], &message));
    assert(!strstr(string_text(&message), "Currently supports:"));
    release_text(&message);
    cpu.r14 = (uintptr_t)&seats[1];
    route_nomination_tooltip(&cpu, NULL);
    assert(cpu.r14 == (uintptr_t)&seats[1] && !nomination_route.shield);
    // Two manually overlapped score rectangles. The actual native hit owns its score.
    Shield saved_seat = panel->roles.shields[1];
    NominationDisplay saved_score = panel->roles.scores[1];
    panel->roles.shields[1] = (Shield){.tooltip = {.button = &seats[1], .tag = pack_tag("ASK", 6)}, .instance = instance->spec};
    panel->roles.scores[1] = panel->roles.scores[0];
    panel->roles.scores[1].elector = election_tag("ASK");
    election_layout.roles.nomination_areas[1] = election_layout.roles.nomination_areas[0];
    route_nomination_tooltip(&cpu, NULL);
    assert(cpu.r14 == (uintptr_t)&seats[1] && nomination_route.score == &panel->roles.scores[1]);
    panel->roles.shields[1] = saved_seat;
    panel->roles.scores[1] = saved_score;
    election_layout.roles.nomination_areas[1] = (UiRectangle){0};
    cpu.r14 = 0;
    plugin.message_priority = true;
    route_nomination_tooltip(&cpu, NULL);
    assert(!cpu.r14);
    plugin.message_priority = false;
    active_selector()->open = true;
    route_nomination_tooltip(&cpu, NULL);
    assert(!cpu.r14);
    active_selector()->open = false;
    cursor.x = 960;
    route_nomination_tooltip(&cpu, NULL);
    assert(!cpu.r14);
    cursor.x = 1040;
    instance->roles[0].count = 0;
    render_roles(instance, false);
    route_nomination_tooltip(&cpu, NULL);
    assert(!cpu.r14);
    assert(!badge.shown && !badge.signal.first && !badge.tooltip && !score_hit.shown && !score_label[0]);
    puts("PASS leader shield reads its instance's reigns. Nomination surface has no click. Support, reasons, ranking and recycled rows match the election.");
    puts("PASS native tooltip dispatch routes the score at 130% UI scale, consumes it only for the matching shield, and leaves messages, selectors and other hits alone.");

    void *textbox_methods[0x228 / 8] = {
        [0x88 / 8] = show_textbox, [0x90 / 8] = hide_textbox,
        [0x98 / 8] = textbox_position, [0x220 / 8] = textbox_tooltip,
    };
    FakeTextBox score = {.vtable = textbox_methods};
    plugin.label_position = NULL;
    plugin.set_ui_tooltip = NULL;
    plugin.button_class = textbox_methods;
    plugin.set_text = capture_textbox;
    NominationDisplay direct_score = {0};
    bind_nomination_display(&direct_score, &score, &score, (UiPoint){0});
    assert(score.tooltip == &direct_score.hover);
    ElectionTable table = {0};
    assert(electoral_table(instance, &table));
    const ElectionCountry *elector = election_country(&table, election_tag("AMA"));
    render_nomination(instance, &table, elector, election_tag("ASK"), true, &direct_score);
    assert(score.shown && strstr(score.text, "-1000"));
    message = (GameString){.capacity = 15};
    assert(score.tooltip->methods->describe(score.tooltip, &score, &message));
    assert(strstr(string_text(&message), "Currently supports: Amago"));
    release_text(&message);
    render_nomination(NULL, NULL, NULL, 0, false, &direct_score);
    assert(!score.shown && !score.text[0]);
    message = (GameString){.capacity = 15};
    assert(!score.tooltip->methods->describe(score.tooltip, &score, &message));

    void *unknown_methods[0x228 / 8] = {0};
    FakeTextBox alternate_score = {.vtable = unknown_methods, .shown = true};
    NominationDisplay *fallback_score = &panel->roles.scores[0];
    *fallback_score = (NominationDisplay){0};
    bind_nomination_display(fallback_score, &alternate_score, &alternate_score, (UiPoint){0});
    render_nomination(instance, &table, elector, election_tag("ASK"), true, fallback_score);
    assert(!alternate_score.tooltip && strstr(alternate_score.text, "-1000"));
    election_layout.area = (UiRectangle){-200, -150, 400, 300};
    election_layout.roles.nomination_areas[0] = (UiRectangle){100, 50, 35, 20};
    const RECT client = {0, 0, 800, 600};
    message = (GameString){.capacity = 15};
    assert(describe_panel_numbers(instance, &election_layout, &client, (POINT){305, 205}, &message));
    assert(strstr(string_text(&message), "Currently supports: Amago"));
    release_text(&message);
    render_nomination(NULL, NULL, NULL, 0, false, fallback_score);
    message = (GameString){.capacity = 15};
    assert(!describe_panel_numbers(instance, &election_layout, &client, (POINT){305, 205}, &message));
    free(table.rows);

    FakeTextBox balance = {.vtable = textbox_methods}, growth = {.vtable = textbox_methods};
    struct ResourceDisplayRow *summary = &panel->resources.summary;
    summary->value = &balance;
    panel->resources.growth = &growth;
    assert(bind_text_tooltip(&balance, &summary->hover, &resource_tooltip));
    assert(bind_text_tooltip(&growth, &panel->resources.growth_hover, &resource_tooltip));
    definition->resource_count = 1;
    definition->resources[0] = (ResourceDefinition){.id = "fortune", .name = "test_resource_name",
        .tooltip = "test_resource_tip", .owner = RESOURCE_INSTANCE, .maximum = 100 * PS_RESOURCE_SCALE};
    render_primary_resource(panel, instance, 0, election_tag("ASK"));
    message = (GameString){.capacity = 15};
    assert(balance.tooltip->methods->describe(balance.tooltip, &balance, &message));
    assert(strstr(string_text(&message), "Fortune\nShared\nCustom detail\nStarting balance: 0"));
    release_text(&message);
    definition->resources[0].owner = RESOURCE_LEADER;
    render_primary_resource(panel, instance, 0, 0);
    message = (GameString){.capacity = 15};
    assert(!strcmp(balance.text, "--"));
    assert(growth.tooltip->methods->describe(growth.tooltip, &growth, &message));
    assert(!strcmp(string_text(&message), "Fortune\nNo account\nCustom detail"));
    release_text(&message);
    election_layout.resource_summary.value_area = (UiRectangle){250, 50, 50, 20};
    balance.vtable = unknown_methods;
    summary->hover = (ButtonTooltip){0};
    assert(!bind_text_tooltip(&balance, &summary->hover, &resource_tooltip));
    render_primary_resource(panel, instance, 0, 0);
    message = (GameString){.capacity = 15};
    assert(describe_panel_numbers(instance, &election_layout, &client, (POINT){455, 205}, &message));
    assert(!strcmp(string_text(&message), "Fortune\nNo account\nCustom detail"));
    release_text(&message);
    FakeButton number_carrier = {.vtable = button_methods};
    panel->label_hover = (ButtonTooltip){.methods = &label_tooltip, .button = &number_carrier};
    election_layout.resource_summary.growth_area = (UiRectangle){250, 80, 50, 20};
    cpu.r14 = (uintptr_t)window + 0x30;
    route_panel_number(&cpu, instance, &election_layout, panel, 255, 85);
    assert(cpu.r14 == (uintptr_t)&number_carrier && number_route.monthly);
    message = (GameString){.capacity = 15};
    assert(describe_layout_label(&panel->label_hover, &number_carrier, &message));
    assert(!strcmp(string_text(&message), "Fortune\nNo account\nCustom detail"));
    release_text(&message);
    puts("PASS native textboxes own score and resource tooltips. Resource descriptions use the configured tooltip independently of title and account.");
    puts("PASS shared stock setter works without position or symbol matches, and incompatible text controls preserve numbers and coordinate-based tooltip data.");
    if (argc == 4) verify_example(argv[1], argv[2], argv[3]);
    return 0;
}
