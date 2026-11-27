#include "political_resources.h"

bool growth_modifier_is_local(const ResourceDefinition *definition) {
    return definition->modifier_source == GROWTH_LEADER_CAPITAL || definition->modifier_source == GROWTH_ACCOUNT_CAPITAL;
}

#include <limits.h>
#include <stdio.h>
#include <string.h>

bool read_fixed_number(const char *text, int32_t *out) {
    const bool negative = *text == '-';
    text += negative;
    if (*text < '0' || *text > '9') return false;
    int64_t whole = 0, fraction = 0;
    while (*text >= '0' && *text <= '9') {
        whole = whole * 10 + (*text++ - '0');
        if (whole > INT32_MAX / PS_RESOURCE_SCALE) return false;
    }
    if (*text == '.') {
        ++text;
        int factor = PS_RESOURCE_SCALE / 10;
        if (*text < '0' || *text > '9') return false;
        while (*text >= '0' && *text <= '9' && factor) {
            fraction += (*text++ - '0') * factor;
            factor /= 10;
        }
    }
    const int64_t magnitude = whole * PS_RESOURCE_SCALE + fraction;
    if (*text || magnitude > INT32_MAX) return false;
    *out = (int32_t)(negative ? -magnitude : magnitude);
    return true;
}

void format_fixed_number(int32_t value, unsigned precision, char text[24]) {
    static const int divisor[] = {1000, 100, 10, 1};
    const int64_t magnitude = value < 0 ? -(int64_t)value : value;
    const char *sign = value < 0 ? "-" : "";
    if (precision) snprintf(text, 24, "%s%lld.%0*d", sign, (long long)(magnitude / PS_RESOURCE_SCALE),
                            (int)precision, (int)(magnitude % PS_RESOURCE_SCALE / divisor[precision]));
    else snprintf(text, 24, "%s%lld", sign, (long long)(magnitude / PS_RESOURCE_SCALE));
}

int32_t resource_limit(const ResourceDefinition *definition, int64_t value) {
    static const int step[] = {1000, 100, 10, 1};
    const int64_t unit = step[definition->precision];
    value = value < 0 ? -((-value + unit / 2) / unit * unit) : (value + unit / 2) / unit * unit;
    if (value < definition->minimum) return definition->minimum;
    if (value > definition->maximum) return definition->maximum;
    return (int32_t)value;
}

unsigned resource_find(const ResourceDefinition *definitions, unsigned count, const char *id) {
    unsigned i = 0;
    while (i < count && strcmp(definitions[i].id, id)) ++i;
    return i;
}

unsigned calculation_find(const CalculationDefinition *definitions, unsigned count, const char *id) {
    unsigned i = 0;
    while (i < count && strcmp(definitions[i].id, id)) ++i;
    return i;
}

int32_t resource_balance(const ResourceDefinition *definition, const ResourceLedger *ledger, unsigned country) {
    for (unsigned i = 0; i < ledger->count; ++i)
        if (ledger->accounts[i].country == country) return ledger->accounts[i].balance;
    return definition->initial;
}

bool store_balance(ResourceLedger *ledger, unsigned country, int32_t balance) {
    for (unsigned i = 0; i < ledger->count; ++i) {
        ResourceAccount *account = &ledger->accounts[i];
        if (account->country != country) continue;
        account->balance = balance;
        return true;
    }
    if (ledger->count == PS_MAX_EXPLICIT_MEMBERS) return false;
    ledger->accounts[ledger->count++] = (ResourceAccount){country, balance};
    return true;
}

unsigned crossed_thresholds(const ResourceDefinition *definition, int32_t before, int32_t after) {
    unsigned crossed = 0;
    for (unsigned i = 0; i < definition->threshold_count; ++i) {
        const ResourceThreshold *t = &definition->thresholds[i];
        if (t->falling ? before > t->at && after <= t->at : before < t->at && after >= t->at)
            crossed |= 1u << i;
    }
    return crossed;
}
