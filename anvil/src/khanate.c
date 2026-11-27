#include "khanate.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

uint32_t khanate_history_key(const char *filename, bool countries) {
    const size_t length = strlen(filename);
    if (length <= 4 || strcmp(filename + length - 4, ".txt") || strpbrk(filename, "/\\"))
        return 0;
    if (countries) {
        if (length < 7 || strspn(filename, "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") != 3 ||
            (length != 7 && strncmp(filename + 3, " - ", 3)))
            return 0;
        uint32_t key = 0;
        memcpy(&key, filename, 3);
        return key;
    }
    char *end = NULL;
    if (*filename < '0' || *filename > '9') return 0;
    const unsigned long province = strtoul(filename, &end, 10);
    return province > 0 && province <= INT_MAX &&
           (end == filename + length - 4 || !strncmp(end, " - ", 3)) ? (uint32_t)province : 0;
}

void release_history_flags(KhanateHistoryFiles *list) {
    for (size_t row = 0; row < list->count; ++row) free(list->items[row].timeline.entries);
    free(list->items);
    *list = (KhanateHistoryFiles){0};
}

void release_khanate_history(KhanateHistory *history) {
    release_history_flags(&history->provinces);
    release_history_flags(&history->countries);
}

static bool history_flag(const KhanateHistoryFiles *files, uint32_t date, uint32_t key) {
    for (size_t i = 0; i < files->count; ++i) {
        const KhanateFlag *row = &files->items[i];
        if (row->key == key) return flag_at(&row->timeline, date);
    }
    return false;
}

bool is_khanate_province(const KhanateHistory *history, uint32_t date, int32_t province) {
    return province > 0 && history_flag(&history->provinces, date, (uint32_t)province);
}

static bool marked_in_map(const KhanateFlag *row, uint32_t date, int32_t limit) {
    return limit > 0 && row->key > 0 && row->key < (uint32_t)limit && flag_at(&row->timeline, date);
}

size_t count_khanate_provinces(const KhanateHistory *history, uint32_t date, int32_t limit) {
    size_t count = 0;
    for (size_t i = 0; i < history->provinces.count; ++i)
        if (marked_in_map(&history->provinces.items[i], date, limit)) ++count;
    return count;
}

bool has_khan_tumen(const KhanateHistory *history, uint32_t date, const char tag[4]) {
    uint32_t key = 0;
    memcpy(&key, tag, 3);
    return history_flag(&history->countries, date, key);
}

static int compare_countries(const void *left, const void *right) {
    const AnvilGameTag *a = left, *b = right;
    return strcmp(a->text, b->text);
}

bool read_capital_members(const AnvilGameWorldService *world, const KhanateHistory *history, uint32_t date,
                          const AnvilGameTag *holder, Shogunate *out) {
    if (out->daimyo) return false;
    *out = (Shogunate){ .emperor = *holder };
    if (!history->provinces.count) return true;
    if (!world->province || !world->country || !world->province_limit) return false;
    const int32_t province_limit = world->province_limit();
    if (province_limit <= 0) return false;
    out->daimyo = calloc(history->provinces.count, sizeof(*out->daimyo));
    if (!out->daimyo) return false;
    for (size_t i = 0; i < history->provinces.count; ++i) {
        const KhanateFlag *row = &history->provinces.items[i];
        const int32_t id = (int32_t)row->key;
        if (!marked_in_map(row, date, province_limit)) continue;
        AnvilGameProvince province = { .size = sizeof(province) };
        if (world->province(id, &province) != ANVIL_OK || !province.owner.text[0]) continue;
        AnvilGameCountry country = { .size = sizeof(country) };
        if (world->country(province.owner.text, &country) == ANVIL_OK &&
            country.province_count > 0 && country.capital == id)
            out->daimyo[out->count++] = province.owner;
    }
    qsort(out->daimyo, out->count, sizeof(*out->daimyo), compare_countries);
    return true;
}
