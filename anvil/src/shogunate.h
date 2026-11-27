#pragma once

#include <anvil/anvil_game.h>
#include <stdbool.h>

typedef struct Shogunate {
    AnvilGameTag emperor;
    AnvilGameTag *daimyo;
    uint32_t count;
    uint32_t direct_subjects;
} Shogunate;

bool read_subject_members(const AnvilGameWorldService *world, const AnvilGameTag *emperor,
                          const char *subject_type, Shogunate *out);
void release_shogunate(Shogunate *state);
