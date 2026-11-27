#include "native_modifiers.h"
#include "ps_script.h"

#include <string.h>

typedef struct NativeModifierRecord {
    GameString script_key;
    unsigned char definition[0x40];
} NativeModifierRecord;

_Static_assert(sizeof(NativeModifierRecord) == 0x60, "EU4 modifier definition layout");

AnvilResult startup_modifier_id(const void *begin, const void *end, const char *key, int32_t *id) {
    const uintptr_t first = (uintptr_t)begin, limit = (uintptr_t)end;
    if (!first || limit <= first || (limit - first) % sizeof(NativeModifierRecord)) return ANVIL_ERROR_WRONG_PHASE;
    const size_t count = (limit - first) / sizeof(NativeModifierRecord);
    if (count > INT32_MAX) return ANVIL_ERROR_INTERNAL;
    const NativeModifierRecord *records = begin;
    const size_t length = strlen(key);
    for (size_t i = 0; i < count; ++i) {
        const GameString *string = &records[i].script_key;
        if (length != string->size || string->capacity < length) continue;
        const char *text = string->inline_text;
        if (string->capacity >= sizeof(string->inline_text)) memcpy(&text, string->inline_text, sizeof(text));
        if (text && !_strnicmp(text, key, length)) {
            *id = (int32_t)i;
            return ANVIL_OK;
        }
    }
    return ANVIL_ERROR_NOT_FOUND;
}

static bool native_key(const char *key) {
    const char *alphabet = "abcdefghijklmnopqrstuvwxyz_";
    return key[0] && strchr(alphabet, key[0]) &&
           strspn(key, "abcdefghijklmnopqrstuvwxyz_0123456789") == strlen(key);
}

const NativeModifier *find_native_modifier(const NativeModifierCatalog *catalog, const char *key) {
    for (unsigned i = 0; i < catalog->count; ++i)
        if (strcmp(catalog->items[i].key, key) == 0) return &catalog->items[i];
    return NULL;
}

bool collect_native_modifiers(NativeModifierCatalog *catalog, const char *text, size_t size,
                              const char *path, ScriptError *error) {
    ScriptTree tree;
    if (!script_tree_read(&tree, text, size, path, error)) return false;
    const unsigned before = catalog->count;
    bool parsed = true;
    for (unsigned i = tree.items[0].first; i && parsed; i = tree.items[i].next) {
        const ScriptItem *entry = &tree.items[i];
        const ScriptItem *scope = script_field(&tree, entry, "scope");
        const ScriptItem *label = script_field(&tree, entry, "localisation");
        static const char *const keys[] = {"scope", "localisation"};
        if (catalog->count == PS_MAX_NATIVE_MODIFIERS)
            parsed = script_problem(&tree, entry, error, "too many native political modifier keys");
        else if (!native_key(entry->key) || find_native_modifier(catalog, entry->key))
            parsed = script_problem(&tree, entry, error, "expected a unique lowercase native political modifier key");
        else if (!script_keys(&tree, entry, keys, sizeof(keys) / sizeof(*keys), error))
            parsed = false;
        else if (!script_word(scope, "country") && !script_word(scope, "province"))
            parsed = script_problem(&tree, scope ? scope : entry, error, "modifier scope must be country or province");
        else if (label && (label->block || !ps_identifier(label->word)))
            parsed = script_problem(&tree, label, error, "expected a localisation key");
        if (!parsed) break;
        NativeModifier *item = &catalog->items[catalog->count++];
        *item = (NativeModifier){ .line = entry->line, .column = entry->column, .id = -1,
            .scope = script_word(scope, "country") ? NATIVE_MODIFIER_COUNTRY : NATIVE_MODIFIER_PROVINCE };
        strcpy(item->path, tree.path);
        strcpy(item->key, entry->key);
        strcpy(item->localisation, label ? label->word : entry->key);
    }
    if (!parsed) catalog->count = before;
    script_tree_free(&tree);
    return parsed;
}

bool register_native_modifier(const ModifierRegistration *api, NativeModifier *definition, ScriptError *error) {
    const ScriptTree source = {0};
    const ScriptItem entry = { .line = definition->line, .column = definition->column };
    int32_t id = -1;
    const AnvilResult lookup = api->lookup(definition->key, &id);
    if (lookup != ANVIL_ERROR_NOT_FOUND)
        return script_problem(&source, &entry, error, lookup == ANVIL_OK
            ? "modifier key already registered by EU4 or another plugin" : "native modifier registry unavailable");

    // Match the native percentage definitions for GLOBAL_TAX_MODIFIER and LOCAL_TAX_MODIFIER.
    enum { COUNTRY_PERCENT = 0x97, PROVINCE_PERCENT = 0x83 };
    _Alignas(16) NativeModifierRecord record;
    GameString key = { .capacity = 15 }, label = { .capacity = 15 };
    api->assign(&key, definition->key, strlen(definition->key));
    api->assign(&label, definition->localisation, strlen(definition->localisation));
    const int32_t token = api->intern(&key, false);
    if (token >= 0) {
        void *transformer = NULL;
        api->construct(&record, token, 0, &label, definition->scope == NATIVE_MODIFIER_COUNTRY
                       ? COUNTRY_PERCENT : PROVINCE_PERCENT, &transformer);
        id = api->append(&record); // EU4 takes care of the registry, name index and record destruction.
    }
    api->release(&label);
    api->release(&key);
    int32_t registered;
    if (token < 0 || id < 0 || api->lookup(definition->key, &registered) != ANVIL_OK || registered != id)
        return script_problem(&source, &entry, error, "native modifier registration could not be verified");
    definition->id = id;
    return true;
}
