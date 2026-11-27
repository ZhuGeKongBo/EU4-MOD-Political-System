#include "shogunate.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

bool read_subject_members(const AnvilGameWorldService *world, const AnvilGameTag *emperor,
                          const char *subject_type, Shogunate *out) {
    if (out->daimyo) return false;
    *out = (Shogunate){ .emperor = *emperor };
    if (!emperor->text[0]) return true;

    uint32_t total;
    AnvilResult result = world->country_subjects(emperor->text, NULL, 0, &total);
    if (result != ANVIL_OK) return false;
    if (!total) return true;
    if (total > INT16_MAX) return false;
    out->daimyo = calloc(total, sizeof(*out->daimyo));
    if (!out->daimyo) return false;
    uint32_t returned;
    result = world->country_subjects(emperor->text, out->daimyo, total, &returned);
    if (result != ANVIL_OK || returned > total) goto failed;

    out->direct_subjects = returned;
    if (!subject_type[0]) {
        out->count = returned;
        return true;
    }
    for (uint32_t i = 0; i < returned; ++i) {
        const AnvilGameTag subject = out->daimyo[i];
        char type[128];
        uint32_t length;
        result = world->subject_type(emperor->text, subject.text, type, sizeof(type), &length);
        if (result != ANVIL_OK && result != ANVIL_ERROR_NOT_FOUND) goto failed;
        if (result == ANVIL_OK && !strcmp(type, subject_type)) {
            out->daimyo[out->count++] = subject;
        }
    }
    return true;

failed:
    release_shogunate(out);
    return false;
}

void release_shogunate(Shogunate *state) {
    free(state->daimyo);
    *state = (Shogunate){0};
}
