#include "accession.h"
#include "symbols.gen.h"
#include <string.h>

_Static_assert(sizeof(GameString)==0x20 && sizeof(CrownToken)==0x58 && sizeof(CrownTokens)==0x18,
               "EU4 1.37.5 message substitution layout");
_Static_assert(sizeof(CrownMessage)==0x90 && sizeof(CrownDestination)==0x10,
               "EU4 1.37.5 election message layout");

#define SINGLE_NOTICE "PS_NOTE_SINGLE"
#define CHOICE_NOTICE "PS_NOTE_CHOICE"

bool bind_accession_messages(AccessionMessages *api, const AnvilHost *host, AnvilPlugin *self) {
    api->manager = host->address_of(self,SYM_CROWN_MESSAGE_MANAGER);
    api->definition = host->address_of(self,SYM_CROWN_MESSAGE_TYPE);
    api->add_token = host->address_of(self,SYM_CROWN_MESSAGE_TOKEN);
    api->compose = host->address_of(self,SYM_CROWN_MESSAGE_COMPOSE);
    api->post = host->address_of(self,SYM_CROWN_MESSAGE_POST);
    api->assign = host->address_of(self,SYM_ASSIGN_STRING);
    api->append_scene_child = host->address_of(self,SYM_GUI_APPEND_SCENE_CHILD);
    const unsigned char *site = host->address_of(self, SYM_MESSAGE_DIALOG_SURFACE);
    if (site) {
        int32_t displacement;
        memcpy(&displacement, site + 3, sizeof(displacement));
        api->dialog_vtable = site + 7 + displacement;
    }
    return api->manager && api->definition && api->add_token && api->compose && api->post && api->assign &&
           api->dialog_vtable && api->append_scene_child;
}

static void *message_pointer(const void *object, size_t offset) {
    void *pointer;
    memcpy(&pointer, (const unsigned char *)object + offset, sizeof(pointer));
    return pointer;
}

static bool surface_registry(const void *gui, uintptr_t registry[3]) {
    if (!gui) return false;
    // Both the stock message and named windows register their surface with GUI vslot 0x98.
    memcpy(registry, (const unsigned char *)gui + 0x110, 3 * sizeof(*registry));
    const uintptr_t begin = registry[0], end = registry[1], limit = registry[2];
    return (begin || (!end && !limit)) && end >= begin && limit >= end &&
           !(begin % sizeof(void *)) && !((end - begin) % sizeof(void *)) && end - begin <= 16384 * sizeof(void *);
}

bool fixed_window_registry_contains(const void *gui, const void *window, bool *registered) {
    uintptr_t registry[3];
    if (!surface_registry(gui, registry)) return false;
    *registered = false;
    const uintptr_t surface = (uintptr_t)window + 0x30;
    for (uintptr_t slot = registry[0]; slot < registry[1]; slot += sizeof(void *)) {
        if ((uintptr_t)message_pointer((const void *)slot, 0) != surface) continue;
        *registered = true;
        break;
    }
    return true;
}

static bool walk_messages(const AccessionMessages *api, const void *gui, bool raise) {
    uintptr_t registry[3];
    if (!api->dialog_vtable || !surface_registry(gui, registry)) return false;
    bool visible = false;
    for (uintptr_t slot = registry[0]; slot < registry[1]; slot += sizeof(void *)) {
        const unsigned char *surface;
        memcpy(&surface, (const void *)slot, sizeof(surface));
        if (!surface || message_pointer(surface, 0) != api->dialog_vtable || surface[0xc4] || !surface[0xc5]) continue;
        if (!raise) return true;
        visible = true;
        if (!api->append_scene_child) continue;
        // CEU3Dialog's surface begins 0x30 into the CFixedWindow. Its native Show
        // at 0x10A1EEB updates this same scene node (window + 0xA30).
        void *node = message_pointer(surface, 0xA00);
        void *vtable = node ? message_pointer(node, 0) : NULL;
        if (!vtable || message_pointer(vtable, 0xC8) != (void *)api->append_scene_child) continue;
        void *parent = message_pointer(node, 0x110);
        if (parent && message_pointer(parent, 0x108) != node) api->append_scene_child(node, parent);
    }
    return visible;
}

bool native_message_visible(const AccessionMessages *api, const void *gui) {
    return walk_messages(api, gui, false);
}

bool raise_native_messages(const AccessionMessages *api, const void *gui) {
    return walk_messages(api, gui, true);
}

static void add_text(const AccessionMessages *api, CrownTokens *tokens, const char *key, const char *value) {
    CrownToken token = { .key.capacity=15, .value.capacity=15, .from_tag=0x2d2d2d, .to_tag=0x2d2d2d };
    api->assign(&token.key,key,strlen(key));
    api->assign(&token.value,value,strlen(value));
    api->add_token(tokens,&token); /* The game moves both strings into its message. */
}

static void add_notice_text(const AccessionMessages *api, CrownTokens *tokens, NoticeText text) {
    if (text.body) add_text(api, tokens, "PF_BODY", text.body);
    if (text.log) add_text(api, tokens, "PF_LOG", text.log);
}

static const char *message_string(const GameString *text) {
    return text->capacity < 16 ? text->inline_text : message_pointer(text, 0);
}

static bool named_message(const void *definition, const char *key) {
    if (!definition) return false;
    const GameString *name = (const void *)((const unsigned char *)definition + 0x38);
    if (name->size != strlen(key)) return false;
    const char *registered = message_string(name);
    return registered && !memcmp(registered, key, name->size);
}

static void *message_type(const AccessionMessages *api, void *manager, const char *type) {
    GameString key = { .size = strlen(type), .capacity = 15 };
    if (key.size > 15 || !manager) return NULL;
    memcpy(key.inline_text, type, key.size);
    void *definition = api->definition(manager, &key);
    /* EU4 returns a fallback definition for an unknown name. */
    return named_message(definition, type) ? definition : NULL;
}

static const AccessionMessages *custom_message_api;
static void (*original_popup_fill)(void *);

static const GameString *message_token(const void *popup, const char *key) {
    // CMessageDialog + 0x1c0 owns the copied message. CMessage + 0x10
    // owns a list whose nodes are created by 0x1AF0B0.
    const void *node = message_pointer(popup, 0x1d0);
    for (unsigned i = 0; node && i < 16; ++i, node = message_pointer(node, 0x60)) {
        const CrownToken *token = node;
        if (!strcmp(message_string(&token->key), key)) return &token->value;
    }
    return NULL;
}

static void set_notice_button(void *window, const char *name, const GameString *text) {
    if (!text || !text->size) return;
    // Same named button lookup and consuming SetText used by 0x8C5ACC.
    void *table = message_pointer(window, 0);
    void *(*find)(void *, const char *) = message_pointer(table, 0x68);
    void *button = find(window, name);
    if (!button) return;
    table = message_pointer(button, 0);
    void (*set_text)(void *, GameString *, bool) = message_pointer(table, 0x290);
    GameString label = {.capacity = 15};
    custom_message_api->assign(&label, message_string(text), text->size);
    set_text(button, &label, false);
}

static void fill_custom_popup(void *popup) {
    original_popup_fill(popup);
    // This call site is exclusive to the stock DefaultPopup fill. Check the
    // copied definition and final dialog type before touching any controls.
    const void *definition = message_pointer(popup, 0x1c8);
    const bool choice = named_message(definition, CHOICE_NOTICE);
    if (!choice && !named_message(definition, SINGLE_NOTICE)) return;
    void *window = message_pointer(popup, 8);
    if (!window || message_pointer(window, 0x30) != custom_message_api->dialog_vtable) return;
    set_notice_button(window, choice ? "AgreeButton" : "CenterOK", message_token(popup, "PF_CLOSE"));
    if (choice) set_notice_button(window, "DeclineButton", message_token(popup, "PF_SECOND"));
}

AnvilResult bind_custom_message_buttons(AccessionMessages *api, const AnvilHost *host, AnvilPlugin *self) {
    const AnvilResult result = host->redirect_call(self, SYM_DEFAULT_POPUP_FILL_CALL, (void *)fill_custom_popup,
                                                  (void **)&original_popup_fill, NULL);
    if (result == ANVIL_OK) {
        custom_message_api = api;
        api->custom_buttons = true;
    }
    return result;
}

static void submit_message(const AccessionMessages *api, void *manager, void *definition, CrownTokens *tokens,
                           uint64_t player, uint64_t subject, void *instance, void (*open)(void *), bool log_only) {
    GameString empty = { .capacity=15 };
    CrownMessage message = {0};
    const CrownDestination destination = { .open=open };
    api->compose(&message,definition,tokens,player,NULL,NULL,subject,NULL,&empty,NULL);
    // 0x8C0852 bypasses the popup branch for this flag and still checks the message log.
    if (log_only) ((unsigned char *)&message)[0x8c] = 1;
    /* This is the stock HRE submission path. It takes ownership of the message
       contents, including the substitutions, and copies the Go To destination. */
    api->post(manager,&message,instance,&destination);
}

static bool post_successor(const AccessionMessages *api, const char *key, uint64_t player, uint64_t winner,
                           const char *realm, const char *country, const char *ruler, void *instance, void (*open)(void *), NoticeText text) {
    void *manager = api->manager();
    void *definition = message_type(api, manager, key);
    if (!definition) return false;
    CrownTokens tokens = {0};
    add_text(api,&tokens,"REALM",realm);
    add_text(api,&tokens,"COUNTRY",country);
    add_text(api,&tokens,"KING",ruler);
    add_notice_text(api, &tokens, text);
    submit_message(api, manager, definition, &tokens, player, winner, instance, open, false);
    return true;
}

bool post_accession_message(const AccessionMessages *api, uint64_t player, uint64_t winner,
                            const char *realm, const char *country, const char *ruler,
                            void *instance, void (*open)(void *), NoticeText text) {
    return post_successor(api, "GK_KHAN_CROWN", player, winner, realm, country, ruler, instance, open, text);
}

bool post_leader_message(const AccessionMessages *api, uint64_t player, uint64_t winner,
                         const char *realm, const char *country, const char *ruler, void *instance, void (*open)(void *), NoticeText text) {
    return post_successor(api, "PS_NEW_LEADER", player, winner, realm, country, ruler, instance, open, text);
}

bool post_resource_message(const AccessionMessages *api, uint64_t player, const char *realm,
                           const char *resource, const char *value, void *instance, void (*open)(void *), NoticeText text) {
    void *manager = api->manager();
    void *definition = message_type(api, manager, "PS_RES_LIMIT");
    if (!definition) return false;
    CrownTokens tokens = {0};
    add_text(api, &tokens, "REALM", realm);
    add_text(api, &tokens, "RESOURCE", resource);
    add_text(api, &tokens, "VALUE", value);
    add_notice_text(api, &tokens, text);
    submit_message(api, manager, definition, &tokens, player, player, instance, open, false);
    return true;
}

bool post_invitation_message(const AccessionMessages *api, uint64_t recipient, uint64_t issuer,
                             const char *realm, const char *country, const char *deadline,
                             void *instance, void (*open)(void *), NoticeText text) {
    void *manager = api->manager();
    void *definition = message_type(api, manager, "PS_INVITATION");
    if (!definition) return false;
    CrownTokens tokens = {0};
    add_text(api, &tokens, "REALM", realm);
    add_text(api, &tokens, "ISSUER", country);
    add_text(api, &tokens, "DEADLINE", deadline);
    add_notice_text(api, &tokens, text);
    submit_message(api, manager, definition, &tokens, recipient, issuer, instance, open, false);
    return true;
}

static void dismiss_notice(void *ignored) { (void)ignored; }

bool post_custom_message(const AccessionMessages *api, uint64_t recipient, uint64_t sender,
                         const NativeNoticeText *text) {
    if (!api->custom_buttons) return false;
    const bool choice = text->popup && text->secondary && *text->secondary;
    void *manager = api->manager();
    const unsigned char *definition = message_type(api, manager, choice ? CHOICE_NOTICE : SINGLE_NOTICE);
    if (!definition) return false;
    // Require the stock DefaultPopup and its matching one/two-button mode.
    uint32_t mode;
    memcpy(&mode, definition + 0x58, sizeof(mode));
    if (mode || (definition[0x60] != 0) != choice) return false;
    CrownTokens tokens = {0};
    add_text(api, &tokens, "PF_TITLE", text->title);
    add_text(api, &tokens, "PF_SUBTITLE", text->subtitle);
    static const char *const body_keys[PS_MESSAGE_BODY_LINES] = {"PF_LINE1", "PF_LINE2", "PF_LINE3", "PF_LINE4"};
    for (unsigned i = 0; i < PS_MESSAGE_BODY_LINES; ++i)
        add_text(api, &tokens, body_keys[i], text->body[i]);
    add_text(api, &tokens, "PF_LOG", text->log);
    add_text(api, &tokens, "PF_CLOSE", text->close);
    add_text(api, &tokens, "PF_SECOND", text->secondary);
    // The native left button takes a Go To callback and the native dialog
    // closes itself. This informational alternative needs no action.
    submit_message(api, manager, (void *)definition, &tokens, recipient, sender, choice ? manager : NULL,
                   choice ? dismiss_notice : NULL, !text->popup);
    return true;
}
