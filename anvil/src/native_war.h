#pragma once

#include "accession.h"

typedef struct NativeDefense {
    const void *join_vtable, *envelope_vtable;
    bool (*check)(const void *, GameString *, bool);
    void *(*clone)(const void *);
    void (*submit)(void *);
    void *(*dispose)(void *, unsigned);
    void (*free_string)(GameString *);
} NativeDefense;

bool bind_native_defense(NativeDefense *api, const AnvilHost *host, AnvilPlugin *self);
bool summon_defender(const NativeDefense *api, uint64_t defender, uint64_t member, uint64_t opponent, int32_t date);
