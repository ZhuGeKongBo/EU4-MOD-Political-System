#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <string.h>

#include "native_passives.h"
#include "symbols.gen.h"

static void (*collect_country)(const void *);
static void (*collect_province)(const void *);
static void (*merge_modifier)(void *, const void *);
static SRWLOCK snapshot_lock = SRWLOCK_INIT;
static NativePassives active;
static bool country_installed, province_installed;

static int compare_recipient(const void *a, const void *b) {
    const NativePassive *first = a, *second = b;
    if (first->recipient != second->recipient) return first->recipient < second->recipient ? -1 : 1;
    const uintptr_t lhs = (uintptr_t)first->modifier, rhs = (uintptr_t)second->modifier;
    return lhs < rhs ? -1 : lhs > rhs;
}

static void merge_passives(void *totals, bool is_province, uintptr_t recipient) {
    AcquireSRWLockShared(&snapshot_lock);
    const NativePassive *items = is_province ? active.provinces : active.countries;
    const size_t count = is_province ? active.province_count : active.country_count;
    size_t lo = 0, hi = count;
    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        if (items[mid].recipient < recipient) lo = mid + 1;
        else hi = mid;
    }
    while (lo < count && items[lo].recipient == recipient)
        merge_modifier(totals, items[lo++].modifier);
    ReleaseSRWLockShared(&snapshot_lock);
}

static void collect_political_country_modifiers(const void *context) {
    collect_country(context);
    void *owner;
    const unsigned char *country;
    memcpy(&owner, context, sizeof(owner));
    memcpy(&country, (const unsigned char *)context + 8, sizeof(country));
    if (!owner || !country) return;
    uint64_t tag;
    memcpy(&tag, country + 0x20, sizeof(tag));
    if (tag) merge_passives((unsigned char *)owner + 8, false, (uintptr_t)tag);
}

static void collect_political_province_modifiers(const void *context) {
    collect_province(context);
    void *owner;
    const void *province;
    memcpy(&owner, context, sizeof(owner));
    memcpy(&province, (const unsigned char *)context + 0x20, sizeof(province));
    if (!owner || !province) return;
    unsigned id;
    memcpy(&id, (const unsigned char *)province + 0x20, sizeof(id));
    if (id) merge_passives((unsigned char *)owner + 0x10, true, id);
}

AnvilResult install_native_passives(const AnvilHost *host, AnvilPlugin *self) {
    merge_modifier = host->address_of(self, SYM_NATIVE_MODIFIER_MERGE);
    if (!merge_modifier) return ANVIL_ERROR_UNSUPPORTED;
    country_installed = host->hook_function(self, SYM_NATIVE_COUNTRY_IMPERIAL_MODIFIERS,
        (void *)collect_political_country_modifiers, (void **)&collect_country, NULL) == ANVIL_OK;
    province_installed = host->hook_function(self, SYM_NATIVE_PROVINCE_IMPERIAL_MODIFIERS,
        (void *)collect_political_province_modifiers, (void **)&collect_province, NULL) == ANVIL_OK;
    return country_installed && province_installed ? ANVIL_OK : ANVIL_ERROR_UNSUPPORTED;
}

bool native_passives_available(NativeClauseKind kind) {
    return kind == NATIVE_COUNTRY_MODIFIER ? country_installed : province_installed;
}

bool append_native_passive(NativePassives *state, bool province, uintptr_t recipient, const void *modifier) {
    if (!recipient || !modifier) return true;
    NativePassive **items = province ? &state->provinces : &state->countries;
    size_t *count = province ? &state->province_count : &state->country_count;
    size_t *capacity = province ? &state->province_capacity : &state->country_capacity;
    if (*count == *capacity) {
        if (*capacity > SIZE_MAX / (2 * sizeof(**items))) return false;
        const size_t next = *capacity ? *capacity * 2 : 16;
        NativePassive *array = realloc(*items, next * sizeof(*array));
        if (!array) return false;
        *items = array;
        *capacity = next;
    }
    (*items)[(*count)++] = (NativePassive){.recipient = recipient, .modifier = modifier};
    return true;
}

void discard_native_passives(NativePassives *state) {
    free(state->countries);
    free(state->provinces);
    *state = (NativePassives){0};
}

static void refresh_changed(const NativePassive *old, size_t old_count, const NativePassive *next, size_t count,
                            bool province, PassiveRefresh refresh) {
    size_t a = 0, b = 0;
    while (a < old_count || b < count) {
        const uintptr_t id = b == count || (a < old_count && old[a].recipient < next[b].recipient)
            ? old[a].recipient : next[b].recipient;
        const size_t start_a = a, start_b = b;
        while (a < old_count && old[a].recipient == id) ++a;
        while (b < count && next[b].recipient == id) ++b;
        if (a - start_a != b - start_b) {
            refresh(province, id);
        } else {
            for (size_t i = 0; i < a - start_a; ++i) {
                if (old[start_a + i].modifier == next[start_b + i].modifier) continue;
                refresh(province, id);
                break;
            }
        }
    }
}

void replace_native_passives(NativePassives state, PassiveRefresh refresh) {
    if (state.country_count > 1) qsort(state.countries, state.country_count, sizeof(*state.countries), compare_recipient);
    if (state.province_count > 1) qsort(state.provinces, state.province_count, sizeof(*state.provinces), compare_recipient);
    AcquireSRWLockExclusive(&snapshot_lock);
    NativePassives previous = active;
    active = state;
    ReleaseSRWLockExclusive(&snapshot_lock);
    if (refresh) {
        refresh_changed(previous.provinces, previous.province_count, state.provinces, state.province_count, true, refresh);
        refresh_changed(previous.countries, previous.country_count, state.countries, state.country_count, false, refresh);
    }
    discard_native_passives(&previous);
}
