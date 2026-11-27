#include "accession.h"
#include "symbols.gen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct { unsigned char prefix[0x38]; GameString name; } type;
static const uint64_t player = 0x10001, winner = 0x20002;
static char instance;
static CrownDestination saved_destination;
static unsigned tokens_seen, posted, followed;
static bool inviting, formatted;

static void *manager(void) { return &type; }
static void *lookup(void *owner, const GameString *key) {
    assert(owner == &type && !strcmp(key->inline_text, inviting ? "PS_INVITATION" : "GK_KHAN_CROWN"));
    return &type;
}
static void assign(GameString *text, const char *bytes, size_t size) {
    assert(size < sizeof(text->inline_text));
    memcpy(text->inline_text,bytes,size+1);
    text->size=size; text->capacity=15;
}
static void token(CrownTokens *bag, CrownToken *part) {
    static const char *const keys[]={"REALM","COUNTRY","KING"};
    static const char *const values[]={"GKhanate","Khorchin","Bolai"};
    static const char *const invitation_keys[]={"REALM","ISSUER","DEADLINE"};
    static const char *const invitation_values[]={"Banner Leagues","France","1444.11.13"};
    if (tokens_seen < 3) {
        assert(!strcmp(part->key.inline_text,(inviting ? invitation_keys : keys)[tokens_seen]));
        assert(!strcmp(part->value.inline_text,(inviting ? invitation_values : values)[tokens_seen]));
    } else {
        assert(formatted && tokens_seen < 5);
        assert(!strcmp(part->key.inline_text, tokens_seen == 3 ? "PF_BODY" : "PF_LOG"));
        assert(!strcmp(part->value.inline_text, tokens_seen == 3 ? "Typed body" : "Typed log"));
    }
    assert(part->from_tag==0x2d2d2d && part->to_tag==0x2d2d2d);
    ++tokens_seen; ++bag->count;
}
static CrownMessage *compose(CrownMessage *out, void *message_type, CrownTokens *bag,
                             uint64_t recipient, void *a, void *b, uint64_t khan,
                             void *c, GameString *label, void *d) {
    assert(message_type==&type && recipient==player && khan==winner && bag->count==(formatted ? 5u : 3u));
    assert(!a && !b && !c && !d && !label->size);
    out->data[0]=73;
    return out;
}
static void submit(void *owner, CrownMessage *message, void *context, const CrownDestination *go) {
    assert(owner==&type && message->data[0]==73 && context==&instance && !go->adjustment);
    saved_destination=*go; ++posted;
}
static void follow(void *context) { assert(context==&instance); ++followed; }

typedef struct TestMessageType {
    unsigned char prefix[0x38];
    GameString name;
    unsigned mode, padding;
    bool two_buttons;
} TestMessageType;

typedef struct TestToken {
    CrownToken token;
    void *prev, *next;
} TestToken;

static TestMessageType native_notice_type;
static TestToken native_tokens[9];
static void (*fill_notice)(void *);
static unsigned notice_tokens, native_fills, native_writes;
static bool expect_log_only, expect_two_buttons;
static GameString button_labels[3];
static void *button_table[0x290 / sizeof(void *) + 1];
static void *buttons[3] = {button_table, button_table, button_table};
static void *dialog_surface[1];

static void *lookup_notice(void *owner, const GameString *key) {
    assert(owner == &type);
    assert(!strcmp(key->inline_text, expect_two_buttons ? "PS_NOTE_CHOICE" : "PS_NOTE_SINGLE"));
    return &native_notice_type;
}

static void store_notice_token(CrownTokens *bag, CrownToken *part) {
    assert(notice_tokens == bag->count && notice_tokens < sizeof(native_tokens) / sizeof(*native_tokens));
    TestToken *node = &native_tokens[notice_tokens++];
    *node = (TestToken){.token = *part, .prev = bag->last};
    if (bag->last) ((TestToken *)bag->last)->next = node;
    else bag->first = node;
    bag->last = node;
    ++bag->count;
}

static CrownMessage *compose_notice(CrownMessage *message, void *definition, CrownTokens *bag,
                                   uint64_t recipient, void *a, void *b, uint64_t sender,
                                   void *c, GameString *label, void *d) {
    assert(definition == &native_notice_type && recipient == player && sender == winner && bag->count == 9);
    assert(!a && !b && !c && !d && !label->size);
    memcpy((unsigned char *)message + 0x8, &definition, sizeof(definition));
    memcpy((unsigned char *)message + 0x10, &bag->first, sizeof(bag->first));
    assert(!strcmp(native_tokens[0].token.key.inline_text, "PF_TITLE"));
    assert(!strcmp(native_tokens[0].token.value.inline_text, "Court dispatch"));
    assert(!strcmp(native_tokens[2].token.value.inline_text, "First line"));
    assert(!strcmp(native_tokens[3].token.value.inline_text, "Second line"));
    assert(!strcmp(native_tokens[6].token.value.inline_text, "Game log"));
    return message;
}

static void *find_notice_button(void *window, const char *name) {
    assert(window);
    if (!strcmp(name, "CenterOK")) return &buttons[0];
    if (!strcmp(name, "AgreeButton")) return &buttons[1];
    assert(!strcmp(name, "DeclineButton"));
    return &buttons[2];
}

static void write_notice_button(void *button, GameString *label, bool localize) {
    assert(!localize && native_fills > 0);
    button_labels[(void **)button - buttons] = *label;
    ++native_writes;
}

static void stock_fill(void *popup) {
    assert(popup);
    ++native_fills;
}

static AnvilResult redirect(AnvilPlugin *self, AnvilSymbol site, void *callback, void **original, AnvilChange **change) {
    assert(site == SYM_DEFAULT_POPUP_FILL_CALL && !self && !change);
    fill_notice = callback;
    *original = stock_fill;
    return ANVIL_OK;
}

static void submit_notice(void *owner, CrownMessage *message, void *context, const CrownDestination *go) {
    assert(owner == &type && (go->open != NULL) == expect_two_buttons);
    assert(((unsigned char *)message)[0x8c] == expect_log_only);
    if (go->open) go->open(context);
    if (expect_log_only) return;
    unsigned char popup[0x250] = {0}, window[0x48] = {0};
    void *table[0x68 / sizeof(void *) + 1] = {[0x68 / sizeof(void *)] = find_notice_button};
    void *ref = window;
    memcpy(popup + 8, &ref, sizeof(ref));
    memcpy(popup + 0x1c0, message, sizeof(*message));
    ref = table;
    memcpy(window, &ref, sizeof(ref));
    ref = dialog_surface;
    memcpy(window + 0x30, &ref, sizeof(ref));
    fill_notice(popup);
    memset(window + 0x30, 0, sizeof(ref));
    fill_notice(popup); // Does not write through a different final window type.
    ref = dialog_surface;
    memcpy(window + 0x30, &ref, sizeof(ref));
    ref = &type;
    memcpy(popup + 0x1c8, &ref, sizeof(ref));
    fill_notice(popup); // Invokes vanilla and never edits a stock definition.
}

static void test_custom_notice(void) {
    AccessionMessages api = {.manager = manager, .definition = lookup_notice, .add_token = store_notice_token,
        .compose = compose_notice, .post = submit_notice, .assign = assign, .dialog_vtable = dialog_surface};
    const AnvilHost host = {.redirect_call = redirect};
    assert(bind_custom_message_buttons(&api, &host, NULL) == ANVIL_OK);
    button_table[0x290 / sizeof(void *)] = write_notice_button;
    NativeNoticeText notice = {.title = "Court dispatch", .subtitle = "Council",
        .body = {"First line", "Second line", "", ""}, .log = "Game log", .close = "Understood",
        .secondary = "File a copy", .popup = true};
    expect_two_buttons = true;
    native_notice_type.two_buttons = true;
    assign(&native_notice_type.name, "PS_NOTE_CHOICE", 14);
    assert(post_custom_message(&api, player, winner, &notice));
    assert(native_fills == 3 && native_writes == 2);
    assert(!strcmp(button_labels[1].inline_text, "Understood") && !strcmp(button_labels[2].inline_text, "File a copy"));
    expect_two_buttons = false;
    native_notice_type.two_buttons = false;
    assign(&native_notice_type.name, "PS_NOTE_SINGLE", 14);
    notice.secondary = "";
    notice_tokens = 0;
    assert(post_custom_message(&api, player, winner, &notice));
    assert(native_fills == 6 && native_writes == 3 && !strcmp(button_labels[0].inline_text, "Understood"));
    notice_tokens = 0;
    native_notice_type.mode = 3;
    assert(!post_custom_message(&api, player, winner, &notice) && !notice_tokens);
    native_notice_type.mode = 0;
    notice.popup = false;
    expect_log_only = true;
    assert(post_custom_message(&api, player, winner, &notice) && native_fills == 6);
    puts("PASS native custom notice tokens, option labels, disabled popup, original fill and type guard (mock).");
}

int main(void) {
    AccessionMessages api={.manager=manager, .definition=lookup, .add_token=token, .compose=compose, .post=submit, .assign=assign};
    assign(&type.name,"UNKNOWN",7);
    assert(!post_accession_message(&api,player,winner,"GKhanate","Khorchin","Bolai",&instance,follow,(NoticeText){0}));
    assert(!tokens_seen && !posted);
    assign(&type.name,"GK_KHAN_CROWN",13);
    assert(post_accession_message(&api,player,winner,"GKhanate","Khorchin","Bolai",&instance,follow,(NoticeText){0}));
    assert(tokens_seen==3 && posted==1);
    saved_destination.open(&instance);
    assert(followed==1);
    inviting = true;
    tokens_seen = 0;
    assign(&type.name,"PS_INVITATION",13);
    assert(post_invitation_message(&api,player,winner,"Banner Leagues","France","1444.11.13",&instance,follow,(NoticeText){0}));
    assert(tokens_seen==3 && posted==2);
    formatted = true;
    tokens_seen = 0;
    assert(post_invitation_message(&api,player,winner,"Banner Leagues","France","1444.11.13",&instance,follow,
                                   (NoticeText){"Typed body", "Typed log"}));
    assert(tokens_seen==5 && posted==3);
    unsigned char gui[0x128] = {0}, ordinary[0xc6] = {0}, dialog[0xc6] = {0};
    api.dialog_vtable = &type;
    const void *surfaces[] = {ordinary, dialog};
    const void *registry[] = {surfaces, surfaces + 2, surfaces + 2};
    memcpy(gui + 0x110, registry, sizeof(registry));
    memcpy(dialog, &api.dialog_vtable, sizeof(api.dialog_vtable));
    ordinary[0xc5] = 1;
    assert(!native_message_visible(&api, gui));
    dialog[0xc5] = 1;
    assert(native_message_visible(&api, gui));
    dialog[0xc4] = 1;
    assert(!native_message_visible(&api, gui));
    puts("PASS native accession and invitation use the recipient and issuer shields, editable substitutions and Go To.");
    test_custom_notice();
    return 0;
}
