#pragma once

#include "accession.h"
#include "script_tree.h"

#define PS_NATIVE_MODIFIER_DIRECTORY "common/political_modifiers"

typedef enum NativeModifierScope { NATIVE_MODIFIER_COUNTRY, NATIVE_MODIFIER_PROVINCE } NativeModifierScope;
enum { PS_MAX_NATIVE_MODIFIERS = 128 };

typedef struct NativeModifier {
    char key[GK_SCRIPT_WORD_SIZE], localisation[GK_SCRIPT_WORD_SIZE], path[PS_FILE_PATH_SIZE];
    unsigned line, column;
    NativeModifierScope scope;
    int32_t id;
} NativeModifier;

typedef struct NativeModifierCatalog {
    NativeModifier items[PS_MAX_NATIVE_MODIFIERS];
    unsigned count;
} NativeModifierCatalog;

typedef struct ModifierRegistration {
    int32_t (*intern)(const GameString *, bool);
    void *(*construct)(void *, int32_t, uint32_t, const GameString *, uint32_t, void **);
    int32_t (*append)(void *);
    void (*assign)(GameString *, const char *, size_t);
    void (*release)(GameString *);
    AnvilResult (*lookup)(const char *, int32_t *);
} ModifierRegistration;

bool collect_native_modifiers(NativeModifierCatalog *catalog, const char *text, size_t size,
                              const char *path, ScriptError *error);
const NativeModifier *find_native_modifier(const NativeModifierCatalog *catalog, const char *key);
AnvilResult startup_modifier_id(const void *begin, const void *end, const char *key, int32_t *id);
bool register_native_modifier(const ModifierRegistration *api, NativeModifier *definition, ScriptError *error);
