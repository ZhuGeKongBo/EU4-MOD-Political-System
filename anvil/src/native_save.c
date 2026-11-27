#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "native_save.h"
#include "symbols.gen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct NativeWriteToken {
    int32_t kind;
    char text[513];
} NativeWriteToken;

typedef struct WriteScope {
    const void *world;
    NativeSaveFields fields;
    bool written;
} WriteScope;

typedef struct ReadScope {
    const void *world;
    KhanLoadedSave save;
} ReadScope;

static struct {
    const AnvilHost *host;
    AnvilPlugin *self;
    bool (*save_world)(void *, void *, bool, bool, bool, bool);
    bool (*load_world)(void *, void *);
    void (*write_members)(void *, void *);
    void (*read_member)(void *, unsigned char *, int);
    void (*indent)(void *);
    void (*literal)(void *, const char *);
    void (*token)(void *, const NativeWriteToken *);
    void (*string)(void *, const char *, bool, bool);
    void (*end_line)(void *);
} native;

static SRWLOCK state_lock = SRWLOCK_INIT;
static const void *published_world;
static NativeSaveFields published_fields;
static ReadScope staged;
static _Thread_local WriteScope *writing;
static _Thread_local ReadScope *reading;

static void copy_fields(NativeSaveFields *to, const NativeSaveFields *from) {
    to->count = from ? from->count : 0;
    to->incomplete = from && from->incomplete;
    if (to->count > PS_MAX_SAVE_RECORDS) {
        to->count = 0;
        to->incomplete = true;
    }
    if (to->count) memcpy(to->entries, from->entries, to->count * sizeof(*to->entries));
}

void publish_native_save(const void *world, const NativeSaveFields *fields) {
    AcquireSRWLockExclusive(&state_lock);
    published_world = world;
    copy_fields(&published_fields, fields);
    ReleaseSRWLockExclusive(&state_lock);
}

void take_native_load(const void *world, KhanLoadedSave *out) {
    AcquireSRWLockExclusive(&state_lock);
    out->loaded = world && world == staged.world && staged.save.loaded;
    copy_fields(&out->fields, out->loaded ? &staged.save.fields : NULL);
    staged.world = NULL;
    ReleaseSRWLockExclusive(&state_lock);
}

static bool save_world(void *world, void *file, bool mode, bool rename, bool flags, bool last) {
    WriteScope *scope = malloc(sizeof(*scope));
    if (!scope) {
        native.host->log(native.self, ANVIL_LOG_ERROR, "political save refused: could not allocate write state");
        return false;
    }
    scope->world = world;
    scope->written = false;
    AcquireSRWLockShared(&state_lock);
    copy_fields(&scope->fields, world == published_world ? &published_fields : NULL);
    ReleaseSRWLockShared(&state_lock);
    if (scope->fields.incomplete) {
        native.host->log(native.self, ANVIL_LOG_ERROR, "political save refused: input records exceeded supported bounds");
        free(scope);
        return false;
    }

    WriteScope *previous = writing;
    writing = scope->fields.count ? scope : NULL;
    const bool result = native.save_world(world, file, mode, rename, flags, last);
    writing = previous;
    if (scope->fields.count) {
        char message[120];
        snprintf(message, sizeof(message), "political save result=%s instances=%zu",
                 result && scope->written ? "written" : "unavailable",
                 scope->written ? scope->fields.count : 0);
        native.host->log(native.self, result && scope->written ? ANVIL_LOG_INFO : ANVIL_LOG_WARNING, message);
    }
    free(scope);
    return result;
}

static const char *stream_name(const void *writer) {
    const unsigned char *stream;
    memcpy(&stream, (const unsigned char *)writer + 0x10, sizeof(stream));
    if (!stream) return NULL;

    // CFile +0x18 is the native string tested by the stock gamestate writer.
    const char *name = (const char *)stream + 0x18;
    uint64_t capacity;
    memcpy(&capacity, name + 0x18, sizeof(capacity));
    if (capacity >= 16) memcpy(&name, stream + 0x18, sizeof(name));
    return name;
}

static void write_members(void *world, void *writer) {
    native.write_members(world, writer);
    WriteScope *scope = writing;
    if (!scope || scope->world != world || scope->written) return;
    const char *name = stream_name(writer);
    if (!name || !strcmp(name, "meta") || !strcmp(name, "ai")) return;

    static const NativeWriteToken equal = { .kind = 1, .text = "=" };
    for (size_t i = 0; i < scope->fields.count; ++i) {
        const NativeSaveField *field = &scope->fields.entries[i];
        native.indent(writer);
        native.literal(writer, field->legacy ? GK_SAVE_KEY : PS_SAVE_KEY);
        native.token(writer, &equal);
        native.string(writer, field->value, false, false);
        native.end_line(writer);
    }
    scope->written = true;
}

static bool load_world(void *world, void *file) {
    ReadScope *scope = malloc(sizeof(*scope));
    if (!scope) {
        native.host->log(native.self, ANVIL_LOG_ERROR, "political load refused: could not allocate read state");
        return false;
    }
    scope->world = world;
    scope->save.loaded = true;
    copy_fields(&scope->save.fields, NULL);
    ReadScope *previous = reading;
    reading = scope;
    const bool result = native.load_world(world, file);
    reading = previous;
    AcquireSRWLockExclusive(&state_lock);
    staged.world = result ? world : NULL;
    staged.save.loaded = result;
    copy_fields(&staged.save.fields, result ? &scope->save.fields : NULL);
    ReleaseSRWLockExclusive(&state_lock);
    free(scope);
    return result;
}

static void read_member(void *world, unsigned char *reader, int token) {
    ReadScope *scope = reading;
    const char *key = (const char *)reader + 0x34;
    const bool legacy = !memcmp(key, GK_SAVE_KEY, sizeof(GK_SAVE_KEY));
    if (!scope || scope->world != world || (!legacy && memcmp(key, PS_SAVE_KEY, sizeof(PS_SAVE_KEY)))) {
        native.read_member(world, reader, token);
        return;
    }

    // The game has already read the member's scalar before dispatching to this reader.
    const char *value = (const char *)reader + 0x444;
    const char *end = memchr(value, 0, GK_SAVE_VALUE_SIZE);
    NativeSaveFields *fields = &scope->save.fields;
    // The custom values use a fixed printable alphabet so a preserved field can be
    // written through the engine's text writer without changing quoting or escaping.
    if (fields->count == PS_MAX_SAVE_RECORDS || !end ||
        strspn(value, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_|-") != (size_t)(end - value)) {
        fields->incomplete = true;
        return;
    }
    NativeSaveField *field = &fields->entries[fields->count++];
    field->legacy = legacy;
    memcpy(field->value, value, (size_t)(end - value) + 1);
}

AnvilResult register_native_save(const AnvilHost *host, AnvilPlugin *self) {
    native.host = host;
    native.self = self;
    native.indent = host->address_of(self, SYM_SAVE_WRITE_INDENT);
    native.literal = host->address_of(self, SYM_SAVE_WRITE_LITERAL);
    native.token = host->address_of(self, SYM_SAVE_WRITE_TOKEN);
    native.string = host->address_of(self, SYM_SAVE_WRITE_STRING);
    native.end_line = host->address_of(self, SYM_SAVE_WRITE_END_LINE);
    if (!native.indent || !native.literal || !native.token || !native.string || !native.end_line)
        return ANVIL_ERROR_UNSUPPORTED;

    AnvilChange *change;
    AnvilResult result = host->hook_function(self, SYM_SAVE_WRITE_MEMBERS, (void *)write_members,
                                             (void **)&native.write_members, &change);
    if (result != ANVIL_OK) return result;
    result = host->hook_function(self, SYM_SAVE_READ_MEMBER, (void *)read_member,
                                 (void **)&native.read_member, &change);
    if (result != ANVIL_OK) return result;
    result = host->hook_function(self, SYM_SAVE_LOAD_WORLD, (void *)load_world,
                                 (void **)&native.load_world, &change);
    if (result != ANVIL_OK) return result;
    return host->hook_function(self, SYM_SAVE_WRITE_WORLD, (void *)save_world,
                               (void **)&native.save_world, &change);
}
