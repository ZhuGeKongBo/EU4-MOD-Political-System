#pragma once

#include <anvil/anvil_plugin.h>

#include "save_state.h"

typedef struct KhanLoadedSave {
    bool loaded;
    NativeSaveFields fields;
} KhanLoadedSave;

AnvilResult register_native_save(const AnvilHost *host, AnvilPlugin *self);
void publish_native_save(const void *world, const NativeSaveFields *fields);
void take_native_load(const void *world, KhanLoadedSave *out);
