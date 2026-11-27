#pragma once
#include <anvil/anvil_plugin.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "script_types.h"

typedef struct GameString {
    char inline_text[16];
    uint64_t size, capacity;
} GameString;

typedef struct CrownTokens {
    void *first, *last;
    uint32_t count, flags;
} CrownTokens;

typedef struct CrownToken {
    GameString key, value;
    uint64_t from_tag, to_tag, extra;
} CrownToken;

typedef struct CrownMessage { uint64_t data[18]; } CrownMessage;
typedef struct CrownDestination {
    void (*open)(void *);
    int32_t adjustment, reserved;
} CrownDestination;

typedef struct NoticeText { const char *body, *log; } NoticeText;
typedef struct NativeNoticeText {
    const char *title, *subtitle, *body[PS_MESSAGE_BODY_LINES], *log, *close, *secondary;
    bool popup;
} NativeNoticeText;

typedef struct AccessionMessages {
    void *(*manager)(void);
    void *(*definition)(void *, const GameString *);
    void (*add_token)(CrownTokens *, CrownToken *);
    CrownMessage *(*compose)(CrownMessage *, void *, CrownTokens *, uint64_t, void *, void *,
                             uint64_t, void *, GameString *, void *);
    void (*post)(void *, CrownMessage *, void *, const CrownDestination *);
    void (*assign)(GameString *, const char *, size_t);
    void (*append_scene_child)(void *, void *);
    const void *dialog_vtable;
    bool custom_buttons;
} AccessionMessages;

bool bind_accession_messages(AccessionMessages *api, const AnvilHost *host, AnvilPlugin *self);
AnvilResult bind_custom_message_buttons(AccessionMessages *api, const AnvilHost *host, AnvilPlugin *self);
bool native_message_visible(const AccessionMessages *api, const void *gui);
bool raise_native_messages(const AccessionMessages *api, const void *gui);
bool fixed_window_registry_contains(const void *gui, const void *window, bool *registered);
bool post_accession_message(const AccessionMessages *api, uint64_t player, uint64_t winner,
                            const char *realm, const char *country, const char *ruler,
                            void *instance, void (*open)(void *), NoticeText text);
bool post_resource_message(const AccessionMessages *api, uint64_t player, const char *realm,
                           const char *resource, const char *value, void *instance, void (*open)(void *), NoticeText text);
bool post_leader_message(const AccessionMessages *api, uint64_t player, uint64_t winner, const char *realm,
                         const char *country, const char *ruler, void *instance, void (*open)(void *), NoticeText text);
bool post_invitation_message(const AccessionMessages *api, uint64_t recipient, uint64_t issuer,
                             const char *realm, const char *country, const char *deadline,
                             void *instance, void (*open)(void *), NoticeText text);
bool post_custom_message(const AccessionMessages *api, uint64_t recipient, uint64_t sender,
                         const NativeNoticeText *text);
