#pragma once

#include <anvil/anvil_plugin.h>
#include "native_clause.h"

typedef struct NativePassive {
    uintptr_t recipient;
    const void *modifier;
} NativePassive;

typedef struct NativePassives {
    NativePassive *countries, *provinces;
    size_t country_count, province_count;
    size_t country_capacity, province_capacity;
} NativePassives;
typedef void (*PassiveRefresh)(bool province, uintptr_t recipient);

AnvilResult install_native_passives(const AnvilHost *host, AnvilPlugin *self);
bool native_passives_available(NativeClauseKind kind);
bool append_native_passive(NativePassives *state, bool province, uintptr_t recipient, const void *modifier);
void discard_native_passives(NativePassives *state);
/* Transfers ownership. Modifier objects belong to the current catalogue. */
void replace_native_passives(NativePassives state, PassiveRefresh refresh);
