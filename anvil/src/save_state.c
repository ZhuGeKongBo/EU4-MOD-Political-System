#include "save_state.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>

static const char country_tag_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
static const char identifier_start[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_";
static const char identifier_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_";
static const char namespace_prefix[] = PS_LEGACY_TYPE "|" PS_LEGACY_INSTANCE "|";

static bool valid_id(const char *id, size_t size) {
    return size > 0 && size < GK_SCRIPT_WORD_SIZE && strchr(identifier_start, id[0]) &&
           strspn(id, identifier_chars) == size;
}

static bool valid_history_date(uint32_t date) {
    static const uint32_t month_days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    const uint32_t month = date / 100 % 100, day = date % 100;
    return date >= 10101 && date <= 99991231 && month >= 1 && month <= 12 && day >= 1 && day <= month_days[month - 1];
}

static bool encode_record(const char *version, const char *type, const char *id,
                          uint32_t history_date, const KhanSaveRecord *record, char value[GK_SAVE_VALUE_SIZE]) {
    const char *leader = record->leader;
    const char *passed = record->reform.passed_id;
    if ((leader[0] && (leader[3] || strspn(leader, country_tag_chars) != 3)) ||
        record->reform.authority < 0 || record->reform.authority > GK_MAX_AUTHORITY ||
        (passed[0] && (!memchr(passed, 0, GK_SCRIPT_WORD_SIZE) || !valid_id(passed, strlen(passed)))))
        return false;
    char date[12] = {0};
    if (history_date) {
        if (!valid_history_date(history_date)) return false;
        snprintf(date, sizeof(date), "%u|", history_date);
    }
    const int size = snprintf(value, GK_SAVE_VALUE_SIZE, "%s|%s|%s|%s%s|%d|%s", version, type, id, date,
                              leader[0] ? leader : "none", record->reform.authority, passed[0] ? passed : "-");
    return size > 0 && size < GK_SAVE_VALUE_SIZE;
}

static KhanLoadStatus decode_record(const char *leader, KhanSaveRecord *record) {
    KhanSaveRecord parsed = {0};
    if (!strncmp(leader, "none|", 5)) {
        leader += 4;
    } else {
        if (strspn(leader, country_tag_chars) != 3 || leader[3] != '|') return GK_LOAD_INVALID;
        memcpy(parsed.leader, leader, 3);
        leader += 3;
    }
    const char *authority = leader + 1;
    const size_t digits = strspn(authority, "0123456789");
    if (!digits || digits > 3 || authority[digits] != '|') return GK_LOAD_INVALID;
    for (size_t i = 0; i < digits; ++i)
        parsed.reform.authority = parsed.reform.authority * 10 + (authority[i] - '0');
    if (parsed.reform.authority > GK_MAX_AUTHORITY) return GK_LOAD_INVALID;

    const char *passed = authority + digits + 1;
    if (strcmp(passed, "-")) {
        const size_t size = strlen(passed);
        if (!valid_id(passed, size)) return GK_LOAD_INVALID;
        memcpy(parsed.reform.passed_id, passed, size);
    }
    *record = parsed;
    return GK_LOAD_VALID;
}

static KhanLoadStatus decode_version(const char **value, char version) {
    const char *start = *value;
    const char *end = strchr(start, '|');
    if (!end || end == start || start + strspn(start, "0123456789") != end) return GK_LOAD_INVALID;
    if (end != start + 1 || start[0] != version) return GK_LOAD_UNSUPPORTED;
    *value = end + 1;
    return GK_LOAD_VALID;
}

bool encode_khan_state(const KhanSaveRecord *record, char value[GK_SAVE_VALUE_SIZE]) {
    return encode_record("1", PS_LEGACY_TYPE, PS_LEGACY_INSTANCE, 0, record, value);
}

KhanLoadStatus decode_khan_state(const char *value, KhanSaveRecord *record) {
    const KhanLoadStatus version = decode_version(&value, '1');
    if (version != GK_LOAD_VALID) return version;
    if (strncmp(value, namespace_prefix, sizeof(namespace_prefix) - 1)) return GK_LOAD_UNSUPPORTED;
    return decode_record(value + sizeof(namespace_prefix) - 1, record);
}

bool encode_political_state(const PoliticalSaveRecord *record, char value[GK_SAVE_VALUE_SIZE]) {
    const char *type_end = memchr(record->type, 0, sizeof(record->type));
    const char *instance_end = memchr(record->instance, 0, sizeof(record->instance));
    if (!type_end || !instance_end || !valid_id(record->type, type_end - record->type) ||
        !valid_id(record->instance, instance_end - record->instance)) return false;
    return encode_record(record->history_date ? "3" : "2", record->type, record->instance,
                         record->history_date, &record->data, value);
}

static bool read_id(const char **value, char id[GK_SCRIPT_WORD_SIZE]) {
    const char *end = strchr(*value, '|');
    if (!end || !valid_id(*value, end - *value)) return false;
    const size_t length = (size_t)(end - *value);
    memcpy(id, *value, length);
    id[length] = 0;
    *value = end + 1;
    return true;
}

KhanLoadStatus decode_political_state(const NativeSaveField *field, PoliticalSaveRecord *record) {
    PoliticalSaveRecord parsed = {0};
    if (!memchr(field->value, 0, sizeof(field->value))) return GK_LOAD_INVALID;
    const char *value = field->value;
    KhanLoadStatus status;
    if (field->legacy) {
        strcpy(parsed.type, PS_LEGACY_TYPE);
        strcpy(parsed.instance, PS_LEGACY_INSTANCE);
        status = decode_khan_state(value, &parsed.data);
    } else {
        const bool has_date = value[0] == '3';
        status = decode_version(&value, has_date ? '3' : '2');
        if (status == GK_LOAD_VALID) {
            if (!read_id(&value, parsed.type) || !read_id(&value, parsed.instance)) return GK_LOAD_INVALID;
            if (has_date) {
                const size_t digits = strspn(value, "0123456789");
                if (digits < 5 || digits > 8 || value[digits] != '|') return GK_LOAD_INVALID;
                for (size_t i = 0; i < digits; ++i)
                    parsed.history_date = parsed.history_date * 10 + (value[i] - '0');
                if (!valid_history_date(parsed.history_date)) return GK_LOAD_INVALID;
                value += digits + 1;
            }
            status = decode_record(value, &parsed.data);
        }
    }
    if (status == GK_LOAD_VALID) *record = parsed;
    return status;
}

KhanLoadStatus decode_saved_instances(const NativeSaveFields *fields, PoliticalSaveRecord *out) {
    if (fields->incomplete || fields->count > PS_MAX_SAVE_RECORDS) return GK_LOAD_INVALID;
    for (size_t i = 0; i < fields->count; ++i) {
        const KhanLoadStatus status = decode_political_state(&fields->entries[i], &out[i]);
        if (status != GK_LOAD_VALID) return status;
        for (size_t previous = 0; previous < i; ++previous)
            if (!_stricmp(out[previous].instance, out[i].instance)) return GK_LOAD_INVALID;
    }
    return fields->count ? GK_LOAD_VALID : GK_LOAD_MISSING;
}

static bool read_unsigned(const char **cursor, uint64_t maximum, uint64_t *out) {
    const char *start = *cursor;
    char *end;
    if (*start < '0' || *start > '9') return false;
    errno = 0;
    unsigned long long value = strtoull(start, &end, 10);
    if (errno || value > maximum || *end != '|') return false;
    *cursor = end + 1;
    *out = value;
    return true;
}

void append_leader_reign(LeaderHistory *history, LeaderReign reign) {
    if (history->count == PS_LEADER_REIGNS) {
        --history->count;
        memmove(history->items, history->items + 1, history->count * sizeof(*history->items));
    }
    history->items[history->count++] = reign;
}

static char *write_hex(char *out, const char *text) {
    static const char digits[] = "0123456789ABCDEF";
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        *out++ = digits[*p >> 4];
        *out++ = digits[*p & 15];
    }
    *out = '\0';
    return out;
}

static bool read_hex(const char **value, char *text, size_t capacity, char terminator) {
    static const char digits[] = "0123456789ABCDEF";
    while (**value && **value != terminator) {
        const char *high = strchr(digits, (*value)[0]);
        const char *low = strchr(digits, (*value)[1]);
        if (!high || !low || !(*value)[1] || capacity <= 1 || (high == digits && low == digits))
            return false;
        *text++ = (char)((high - digits) * 16 + (low - digits));
        *value += 2;
        --capacity;
    }
    *text = '\0';
    if (**value != terminator) return false;
    if (terminator) ++*value;
    return true;
}

bool encode_leader_reign(const char *instance, const LeaderReign *reign, char value[GK_SAVE_VALUE_SIZE]) {
    _Static_assert(GK_SCRIPT_WORD_SIZE + 64 + 2 * (PS_RULER_NAME_SIZE + PS_DYNASTY_NAME_SIZE) <=
                   GK_SAVE_VALUE_SIZE, "leader history fields must fit their save value");
    char tag[4];
    election_text(reign->country, tag);
    if (!valid_id(instance, strlen(instance)) || strspn(tag, country_tag_chars) != 3 ||
        !valid_history_date(reign->start) ||
        (reign->end && (!valid_history_date(reign->end) || reign->end <= reign->start)) ||
        !memchr(reign->name, 0, sizeof(reign->name)) || !memchr(reign->dynasty, 0, sizeof(reign->dynasty))) return false;
    const int size = snprintf(value, GK_SAVE_VALUE_SIZE, "N1|%s|%s|%u|%u|%" PRIu64 "|",
                              instance, tag, reign->start, reign->end, reign->ruler);
    if (size < 0 || size >= GK_SAVE_VALUE_SIZE) return false;
    char *next = write_hex(value + size, reign->name);
    *next++ = '|';
    write_hex(next, reign->dynasty);
    return true;
}

bool decode_leader_reign(const char *value, char instance[GK_SCRIPT_WORD_SIZE], LeaderReign *reign) {
    uint64_t start, end, ruler;
    if (strncmp(value, "N1|", 3)) return false;
    value += 3;
    if (!read_id(&value, instance) || strspn(value, country_tag_chars) != 3 || value[3] != '|') return false;
    char tag[4] = {value[0], value[1], value[2], 0};
    value += 4;
    if (!read_unsigned(&value, 99991231, &start) || !valid_history_date((unsigned)start) ||
        !read_unsigned(&value, 99991231, &end) ||
        (end && (end <= start || !valid_history_date((unsigned)end))) ||
        !read_unsigned(&value, UINT64_MAX, &ruler)) return false;
    LeaderReign parsed = {.country = election_tag(tag), .start = (unsigned)start, .end = (unsigned)end, .ruler = ruler};
    if (!read_hex(&value, parsed.name, sizeof(parsed.name), '|') ||
        !read_hex(&value, parsed.dynasty, sizeof(parsed.dynasty), '\0')) return false;
    *reign = parsed;
    return true;
}

_Static_assert(2 * (GK_SCRIPT_WORD_SIZE - 1) + sizeof("P1||||999912") + 11 * PS_RESOURCE_CHUNK <= GK_SAVE_VALUE_SIZE,
               "resource account chunk fits in one native field");
_Static_assert(2 * (GK_SCRIPT_WORD_SIZE - 1) + sizeof("J1|||99991231|99991231|1|TAG|") + 11 * PS_CONTEST_CHUNK <= GK_SAVE_VALUE_SIZE,
               "contest bid chunk fits in one native field");

static bool append_account_tokens(char value[GK_SAVE_VALUE_SIZE], int offset, const ResourceAccount *accounts, unsigned count) {
    if (offset < 0 || offset >= GK_SAVE_VALUE_SIZE) return false;
    for (unsigned i = 0; i < count; ++i) {
        char tag[4];
        const char *country = accounts[i].country ? election_text(accounts[i].country, tag) : "---";
        if (accounts[i].country && strspn(country, country_tag_chars) != 3) return false;
        const int size = snprintf(value + offset, GK_SAVE_VALUE_SIZE - offset, "%s%08" PRIX32, country, (uint32_t)accounts[i].balance);
        if (size != 11 || offset + size >= GK_SAVE_VALUE_SIZE) return false;
        offset += size;
    }
    return true;
}

static bool decode_account_tokens(const char *p, unsigned limit, bool bids, ResourceLedger *ledger) {
    ledger->count = 0;
    size_t bytes = strlen(p);
    if (bytes % 11 || bytes / 11 > limit) return false;
    for (size_t i = 0; i < bytes; i += 11) {
        const char *tag = p + i;
        char text[4] = {0};
        if (memcmp(tag, "---", 3)) {
            if (strspn(tag, country_tag_chars) < 3) return false;
            memcpy(text, tag, 3);
        }
        const unsigned country = election_tag(text);
        for (unsigned j = 0; j < ledger->count; ++j) if (ledger->accounts[j].country == country) return false;
        uint32_t number = 0;
        for (unsigned digit = 3; digit < 11; ++digit) {
            const char ch = tag[digit];
            if (ch >= '0' && ch <= '9') number = number * 16 + (unsigned)(ch - '0');
            else if (ch >= 'A' && ch <= 'F') number = number * 16 + (unsigned)(ch - 'A' + 10);
            else return false;
        }
        int32_t signed_number;
        memcpy(&signed_number, &number, sizeof(signed_number));
        if (bids && (!country || signed_number < 0)) return false;
        ledger->accounts[ledger->count++] = (ResourceAccount){country, signed_number};
    }
    return true;
}

static bool valid_month(unsigned month) { return month >= 101 && month <= 999912 && month % 100 >= 1 && month % 100 <= 12; }

bool encode_resource_accounts(const char *instance, const char *resource, unsigned month,
                              const ResourceAccount *accounts, unsigned count, char value[GK_SAVE_VALUE_SIZE]) {
    if (!valid_id(instance, strlen(instance)) || !valid_id(resource, strlen(resource)) ||
        !valid_month(month) || count > PS_RESOURCE_CHUNK) return false;
    const int offset = snprintf(value, GK_SAVE_VALUE_SIZE, "P1|%s|%s|%u|", instance, resource, month);
    return append_account_tokens(value, offset, accounts, count);
}

bool decode_resource_accounts(const char value[GK_SAVE_VALUE_SIZE], char instance[GK_SCRIPT_WORD_SIZE],
                              char resource[GK_SCRIPT_WORD_SIZE], unsigned *month, ResourceLedger *ledger) {
    if (!memchr(value, 0, GK_SAVE_VALUE_SIZE) || strncmp(value, "P1|", 3)) return false;
    const char *p = value + 3;
    uint64_t period;
    if (!read_id(&p, instance) || !read_id(&p, resource) ||
        !read_unsigned(&p, 999912, &period) || !valid_month((unsigned)period)) return false;
    *month = (unsigned)period;
    return decode_account_tokens(p, PS_RESOURCE_CHUNK, false, ledger);
}

bool encode_contest(const char *instance, const char *office, const PoliticalContest *contest,
                    unsigned offset, char value[GK_SAVE_VALUE_SIZE]) {
    if (!valid_id(instance, strlen(instance)) || (*office && !valid_id(office, strlen(office))) ||
        offset > contest->bids.count || !valid_history_date(contest->opened) || !valid_history_date(contest->due) ||
        contest->due <= contest->opened) return false;
    char tag[4];
    const char *winner = contest->winner ? election_text(contest->winner, tag) : "---";
    const int bytes = snprintf(value, GK_SAVE_VALUE_SIZE, "J1|%s|%s|%u|%u|%u|%s|", instance, *office ? office : "---",
                               contest->opened, contest->due, contest->settled ? 1u : 0u, winner);
    unsigned count = contest->bids.count - offset;
    if (count > PS_CONTEST_CHUNK) count = PS_CONTEST_CHUNK;
    return append_account_tokens(value, bytes, contest->bids.accounts + offset, count);
}

bool decode_contest(const char value[GK_SAVE_VALUE_SIZE], char instance[GK_SCRIPT_WORD_SIZE],
                    char office[GK_SCRIPT_WORD_SIZE], PoliticalContest *contest) {
    if (!memchr(value, 0, GK_SAVE_VALUE_SIZE) || strncmp(value, "J1|", 3)) return false;
    const char *p = value + 3;
    if (!read_id(&p, instance)) return false;
    office[0] = 0;
    if (!strncmp(p, "---|", 4)) p += 4;
    else if (!read_id(&p, office)) return false;
    uint64_t opened, due, settled;
    if (!read_unsigned(&p, 99991231, &opened) || !valid_history_date((unsigned)opened) ||
        !read_unsigned(&p, 99991231, &due) || !valid_history_date((unsigned)due) || due <= opened ||
        !read_unsigned(&p, 1, &settled)) return false;
    char tag[4] = {0};
    if (strncmp(p, "---|", 4)) {
        if (!settled || strspn(p, country_tag_chars) != 3 || p[3] != '|') return false;
        memcpy(tag, p, 3);
    }
    *contest = (PoliticalContest){ .opened = (unsigned)opened, .due = (unsigned)due, .winner = election_tag(tag), .settled = settled != 0 };
    return decode_account_tokens(p + 4, PS_CONTEST_CHUNK, true, &contest->bids);
}

bool encode_action_run(const char *instance, const PoliticalAction *action, const PoliticalActionRun *run,
                       char value[GK_SAVE_VALUE_SIZE]) {
    if (!valid_id(instance, strlen(instance)) || !valid_id(action->id, strlen(action->id)) || !run->actor) return false;
    char actor[4], target[4];
    const char *from = election_text(run->actor, actor);
    const char *to = run->target ? election_text(run->target, target) : "---";
    if (strspn(from, country_tag_chars) != 3 || (run->target && strspn(to, country_tag_chars) != 3)) return false;
    const int size = snprintf(value, GK_SAVE_VALUE_SIZE, "A1|%s|%s|%u|%s|%s|%u|%u|%u|", instance, action->id,
                              action->kind, from, to, run->performed, run->expires, run->cooldown_until);
    return size > 0 && size < GK_SAVE_VALUE_SIZE;
}

bool decode_action_run(const char value[GK_SAVE_VALUE_SIZE], char instance[GK_SCRIPT_WORD_SIZE],
                       char action[GK_SCRIPT_WORD_SIZE], ActionKind *kind, PoliticalActionRun *run) {
    if (!memchr(value, 0, GK_SAVE_VALUE_SIZE) || strncmp(value, "A1|", 3)) return false;
    const char *p = value + 3;
    uint64_t mode, date, expires, cooldown;
    if (!read_id(&p, instance) || !read_id(&p, action) || !read_unsigned(&p, ACTION_DECREE, &mode) || !mode) return false;
    if (strspn(p, country_tag_chars) != 3 || p[3] != '|') return false;
    char actor[4] = {0}, target[4] = {0};
    memcpy(actor, p, 3);
    p += 4;
    if (!strncmp(p, "---|", 4)) p += 4;
    else {
        if (strspn(p, country_tag_chars) != 3 || p[3] != '|') return false;
        memcpy(target, p, 3);
        p += 4;
    }
    if (!read_unsigned(&p, 99991231, &date) || !read_unsigned(&p, 99991231, &expires) ||
        !read_unsigned(&p, 99991231, &cooldown) || *p || !valid_history_date((unsigned)date) ||
        ((mode == ACTION_DECREE) != (expires != 0)) ||
        (expires && (!valid_history_date((unsigned)expires) || expires <= date)) ||
        (cooldown && (!valid_history_date((unsigned)cooldown) || cooldown <= date))) return false;
    *kind = (ActionKind)mode;
    *run = (PoliticalActionRun){ .actor = election_tag(actor), .target = election_tag(target),
        .performed = (unsigned)date, .expires = (unsigned)expires, .cooldown_until = (unsigned)cooldown };
    return true;
}

static bool tag3(const char *p) { return strspn(p, country_tag_chars) >= 3; }

bool encode_lifecycle(const PoliticalInstanceSpec *spec, const PoliticalLifecycle *state, char value[GK_SAVE_VALUE_SIZE]) {
    char founder[4];
    const char *tag = state->founder ? election_text(state->founder, founder) : "---";
    const char *name = state->dynamic && spec->title[0] ? spec->title : "-";
    const int length = snprintf(value, GK_SAVE_VALUE_SIZE, "Y1|%s|%s|%u|%u|%u|%u|%s|%s|", spec->id, spec->type,
        state->mode, state->dynamic, state->automatic, state->created, tag, name);
    return length > 0 && length < GK_SAVE_VALUE_SIZE;
}

bool decode_lifecycle(const char value[GK_SAVE_VALUE_SIZE], PoliticalLifecycleRecord *record) {
    if (!memchr(value, 0, GK_SAVE_VALUE_SIZE) || strncmp(value, "Y1|", 3)) return false;
    const char *p = value + 3;
    PoliticalLifecycleRecord incoming = {0};
    uint64_t mode, dynamic, automatic, created;
    if (!read_id(&p, incoming.instance) || !read_id(&p, incoming.type) ||
        !read_unsigned(&p, POLITICAL_DISSOLVED, &mode) || !mode || !read_unsigned(&p, 1, &dynamic) ||
        !read_unsigned(&p, 1, &automatic) || (automatic && mode != POLITICAL_SUSPENDED) ||
        !read_unsigned(&p, 99991231, &created) || (dynamic ? !valid_history_date((unsigned)created) : created != 0) ||
        (!dynamic && mode == POLITICAL_ACTIVE)) return false;
    if (dynamic) {
        if (!tag3(p) || p[3] != '|') return false;
        char tag[4] = {0};
        memcpy(tag, p, 3);
        incoming.lifecycle.founder = election_tag(tag);
    } else if (strncmp(p, "---|", 4)) return false;
    p += 4;
    if (!strncmp(p, "-|", 2)) p += 2;
    else if (!dynamic || !read_id(&p, incoming.name)) return false;
    if (*p) return false;
    incoming.lifecycle.mode = (PoliticalMode)mode;
    incoming.lifecycle.dynamic = dynamic != 0;
    incoming.lifecycle.automatic = automatic != 0;
    incoming.lifecycle.created = (unsigned)created;
    *record = incoming;
    return true;
}

bool encode_process(const char *instance, const PoliticalAction *action, const PoliticalProcess *process,
                    char value[GK_SAVE_VALUE_SIZE]) {
    if (!ps_identifier(instance) || !ps_identifier(action->id) || !process->issuer || !process->round) return false;
    char issuer[4], target[4];
    const char *from = election_text(process->issuer, issuer);
    const char *to = process->target ? election_text(process->target, target) : "---";
    const int size = snprintf(value, GK_SAVE_VALUE_SIZE, "Q2|%s|%s|%u|%u|%s|%s|%u|%u|%u|%u|%s|%u|",
        instance, action->id, action->kind, process->round, from, to, process->opened,
        process->deadline, process->resolved, process->status, process->option[0] ? process->option : "-", process->notified);
    return size > 0 && size < GK_SAVE_VALUE_SIZE;
}

bool decode_process(const char value[GK_SAVE_VALUE_SIZE], char instance[GK_SCRIPT_WORD_SIZE],
                    char action[GK_SCRIPT_WORD_SIZE], ActionKind *kind, PoliticalProcess *process) {
    if (!memchr(value, 0, GK_SAVE_VALUE_SIZE)) return false;
    const bool v2 = !strncmp(value, "Q2|", 3);
    if (!v2 && strncmp(value, "Q1|", 3)) return false;
    const char *p = value + 3;
    uint64_t mode, round, opened, deadline, resolved, status;
    if (!read_id(&p, instance) || !read_id(&p, action) || !read_unsigned(&p, ACTION_INCIDENT, &mode) || mode < ACTION_INVITATION ||
        !read_unsigned(&p, INT_MAX, &round) || !round || !tag3(p) || p[3] != '|') return false;
    char from[4] = {0}, to[4] = {0};
    memcpy(from, p, 3);
    p += 4;
    if (mode == ACTION_INVITATION) {
        if (!tag3(p) || p[3] != '|') return false;
        memcpy(to, p, 3);
    } else if (strncmp(p, "---|", 4)) return false;
    p += 4;
    if (!read_unsigned(&p, 99991231, &opened) || !read_unsigned(&p, 99991231, &deadline) ||
        !read_unsigned(&p, 99991231, &resolved) || !read_unsigned(&p, PROCESS_SETTLED, &status) || !status ||
        !valid_history_date((unsigned)opened) || !valid_history_date((unsigned)deadline) || deadline <= opened ||
        ((status == PROCESS_PENDING) != (resolved == 0)) ||
        (resolved && (!valid_history_date((unsigned)resolved) || resolved < opened)) ||
        (resolved && ((status == PROCESS_EXPIRED) != (resolved >= deadline)))) return false;
    char option[GK_SCRIPT_WORD_SIZE] = {0};
    if (mode == ACTION_INVITATION && status == PROCESS_SETTLED) return false;
    if (mode == ACTION_INCIDENT && status != PROCESS_PENDING && status != PROCESS_EXPIRED && status != PROCESS_SETTLED) return false;
    if (status == PROCESS_SETTLED) {
        if (!read_id(&p, option)) return false;
    } else {
        if (strncmp(p, "-|", 2)) return false;
        p += 2;
    }
    uint64_t notified = 0;
    if ((v2 && !read_unsigned(&p, 1, &notified)) || *p || (notified && mode != ACTION_INVITATION)) return false;
    *kind = (ActionKind)mode;
    *process = (PoliticalProcess){ .round = (unsigned)round, .issuer = election_tag(from), .target = election_tag(to),
        .opened = (unsigned)opened, .deadline = (unsigned)deadline, .resolved = (unsigned)resolved,
        .status = (ProcessStatus)status, .notified = notified != 0 };
    strcpy(process->option, option);
    return true;
}

_Static_assert(PS_MAX_EXPLICIT_MEMBERS * 3 + GK_SCRIPT_WORD_SIZE + sizeof("M1|") < GK_SAVE_VALUE_SIZE,
               "member record fits in the native save field");

static bool write_tags(char *p, const unsigned *tags, unsigned count) {
    for (unsigned i = 0; i < count; ++i) {
        char tag[4];
        if (!tag3(election_text(tags[i], tag))) return false;
        memcpy(p, tag, 3);
        p += 3;
    }
    *p = 0;
    return true;
}

bool encode_explicit_members(const char *instance, const MemberTags *members, char value[GK_SAVE_VALUE_SIZE]) {
    if (!valid_id(instance, strlen(instance)) || members->count > PS_MAX_EXPLICIT_MEMBERS) return false;
    const int prefix = snprintf(value, GK_SAVE_VALUE_SIZE, "M1|%s|", instance);
    return write_tags(value + prefix, members->tags, members->count);
}

static bool read_tags(const char *p, unsigned *tags, unsigned *count, unsigned capacity) {
    const size_t length = strlen(p);
    if (length % 3 || length / 3 > capacity || strspn(p, country_tag_chars) != length) return false;
    *count = 0;
    for (; *p; p += 3) {
        const unsigned tag = election_tag(p);
        for (unsigned i = 0; i < *count; ++i)
            if (tags[i] == tag) return false;
        tags[(*count)++] = tag;
    }
    return true;
}

bool decode_explicit_members(const char value[GK_SAVE_VALUE_SIZE], char instance[GK_SCRIPT_WORD_SIZE], MemberTags *members) {
    if (!memchr(value, 0, GK_SAVE_VALUE_SIZE) || strncmp(value, "M1|", 3)) return false;
    const char *p = value + 3;
    char id[GK_SCRIPT_WORD_SIZE] = {0};
    if (!read_id(&p, id)) return false;
    MemberTags parsed = {0};
    if (!read_tags(p, parsed.tags, &parsed.count, PS_MAX_EXPLICIT_MEMBERS)) return false;
    strcpy(instance, id);
    *members = parsed;
    return true;
}

_Static_assert(PS_TERRITORY_CHUNK * 7 + GK_SCRIPT_WORD_SIZE + sizeof("T2|1023|1024|") < GK_SAVE_VALUE_SIZE,
               "territory fits in the native save field");
_Static_assert(PS_MAX_ROLE_HOLDERS * 3 + 2 * GK_SCRIPT_WORD_SIZE + sizeof("R1|") < GK_SAVE_VALUE_SIZE,
               "role appointments fit in the native save field");

static bool write_provinces(char value[GK_SAVE_VALUE_SIZE], size_t used, const unsigned *ids, unsigned count) {
    for (unsigned i = 0; i < count; ++i) {
        if (!ids[i] || ids[i] > PS_MAX_PROVINCE_ID) return false;
        const int written = snprintf(value + used, GK_SAVE_VALUE_SIZE - used, "%u|", ids[i]);
        if (written < 0 || (size_t)written >= GK_SAVE_VALUE_SIZE - used) return false;
        used += (size_t)written;
    }
    return true;
}

static bool read_provinces(const char *p, unsigned *ids, unsigned *count) {
    while (*p) {
        uint64_t id;
        if (*count == PS_TERRITORY_CHUNK || !read_unsigned(&p, PS_MAX_PROVINCE_ID, &id) || !id) return false;
        for (unsigned i = 0; i < *count; ++i)
            if (ids[i] == id) return false;
        ids[(*count)++] = (unsigned)id;
    }
    return true;
}

bool encode_territory(const char *instance, const ProvinceList *territory, char value[GK_SAVE_VALUE_SIZE]) {
    if (!valid_id(instance, strlen(instance)) || territory->count > PS_TERRITORY_CHUNK) return false;
    const int used = snprintf(value, GK_SAVE_VALUE_SIZE, "T1|%s|", instance);
    return used > 0 && used < GK_SAVE_VALUE_SIZE && write_provinces(value, (size_t)used, territory->ids, territory->count);
}

bool decode_territory(const char value[GK_SAVE_VALUE_SIZE], char instance[GK_SCRIPT_WORD_SIZE], ProvinceList *territory) {
    if (!memchr(value, 0, GK_SAVE_VALUE_SIZE) || strncmp(value, "T1|", 3)) return false;
    const char *p = value + 3;
    ProvinceList parsed = {0};
    if (!read_id(&p, instance) || !read_provinces(p, parsed.ids, &parsed.count)) return false;
    *territory = parsed;
    return true;
}

bool encode_territory_chunk(const char *instance, const ProvinceList *territory, unsigned offset,
                            char value[GK_SAVE_VALUE_SIZE]) {
    const unsigned total = territory->count;
    if (!valid_id(instance, strlen(instance)) || total <= PS_TERRITORY_CHUNK || total > PS_MAX_TERRITORY ||
        offset >= total || offset % PS_TERRITORY_CHUNK) return false;
    const unsigned count = total - offset > PS_TERRITORY_CHUNK ? PS_TERRITORY_CHUNK : total - offset;
    const int used = snprintf(value, GK_SAVE_VALUE_SIZE, "T2|%s|%u|%u|", instance, offset, total);
    return used > 0 && used < GK_SAVE_VALUE_SIZE && write_provinces(value, (size_t)used, territory->ids + offset, count);
}

bool decode_territory_chunk(const char value[GK_SAVE_VALUE_SIZE], char instance[GK_SCRIPT_WORD_SIZE], TerritorySaveChunk *chunk) {
    if (!memchr(value, 0, GK_SAVE_VALUE_SIZE) || strncmp(value, "T2|", 3)) return false;
    const char *p = value + 3;
    uint64_t offset, total;
    if (!read_id(&p, instance) || !read_unsigned(&p, PS_MAX_TERRITORY - 1, &offset) ||
        !read_unsigned(&p, PS_MAX_TERRITORY, &total) || offset >= total || offset % PS_TERRITORY_CHUNK ||
        total <= PS_TERRITORY_CHUNK) return false;
    TerritorySaveChunk parsed = {.offset = (unsigned)offset, .total = (unsigned)total};
    const unsigned expected = total - offset > PS_TERRITORY_CHUNK ? PS_TERRITORY_CHUNK : (unsigned)(total - offset);
    if (!read_provinces(p, parsed.ids, &parsed.count) || parsed.count != expected) return false;
    *chunk = parsed;
    return true;
}

bool encode_role(const char *instance, const char *role, const RoleHolders *holders, char value[GK_SAVE_VALUE_SIZE]) {
    if (!valid_id(instance, strlen(instance)) || !valid_id(role, strlen(role)) || holders->count > PS_MAX_ROLE_HOLDERS)
        return false;
    const int prefix = snprintf(value, GK_SAVE_VALUE_SIZE, "R1|%s|%s|", instance, role);
    return write_tags(value + prefix, holders->countries, holders->count);
}

bool decode_role(const char value[GK_SAVE_VALUE_SIZE], char instance[GK_SCRIPT_WORD_SIZE], char role[GK_SCRIPT_WORD_SIZE], RoleHolders *holders) {
    if (!memchr(value, 0, GK_SAVE_VALUE_SIZE) || strncmp(value, "R1|", 3)) return false;
    const char *p = value + 3;
    RoleHolders parsed = {0};
    if (!read_id(&p, instance) || !read_id(&p, role) ||
        !read_tags(p, parsed.countries, &parsed.count, PS_MAX_ROLE_HOLDERS)) return false;
    *holders = parsed;
    return true;
}

bool encode_election_state(const char *instance, const ElectionState *s, char value[GK_SAVE_VALUE_SIZE]) {
    if (!valid_id(instance, strlen(instance)) || !s->round || s->round > INT_MAX || s->count > ELECTION_OVERRIDES)
        return false;
    char winner[4];
    int written = snprintf(value, GK_SAVE_VALUE_SIZE, "E1|%s|%u|%" PRIu64 "|%s|%u|%u|", instance, s->round,
                           s->ruler, s->last_winner ? election_text(s->last_winner, winner) : "---", s->last_votes, s->last_ties);
    if (written < 0 || (size_t)written + 8 * s->count >= GK_SAVE_VALUE_SIZE) return false;
    char *p = value + written;
    for (unsigned i = 0; i < s->count; ++i) {
        const ElectionPreference *e = &s->entries[i];
        char voter[4], target[4];
        if (!tag3(election_text(e->voter, voter)) || e->weight < -1 || e->weight > ELECTION_MAX_WEIGHT) return false;
        const char *choice = e->choice ? election_text(e->choice, target) : "---";
        if (e->choice && !tag3(choice)) return false;
        memcpy(p, voter, 3);
        p[3] = e->weight < 0 ? '-' : (char)('0' + e->weight / 10);
        p[4] = e->weight < 0 ? '-' : (char)('0' + e->weight % 10);
        memcpy(p + 5, choice, 3);
        p += 8;
    }
    *p = 0;
    return true;
}

bool decode_election_state(const char value[GK_SAVE_VALUE_SIZE], char instance[GK_SCRIPT_WORD_SIZE], ElectionState *out) {
    if (!memchr(value, 0, GK_SAVE_VALUE_SIZE) || strncmp(value, "E1|", 3)) return false;
    const char *p = value + 3;
    ElectionState state = {0};
    uint64_t round, votes, ties;
    char id[GK_SCRIPT_WORD_SIZE] = {0};
    if (!read_id(&p, id) || !read_unsigned(&p, INT_MAX, &round) || !round ||
        !read_unsigned(&p, UINT64_MAX, &state.ruler)) return false;
    if (strlen(p) < 4 || p[3] != '|') return false;
    if (strncmp(p, "---", 3)) {
        if (!tag3(p)) return false;
        state.last_winner = election_tag(p);
    }
    p += 4;
    if (!read_unsigned(&p, UINT_MAX, &votes) || !read_unsigned(&p, UINT_MAX, &ties) ||
        (!!state.last_winner != !!votes) || (!!votes != !!ties)) return false;
    state.round = (uint32_t)round;
    state.last_votes = (uint32_t)votes;
    state.last_ties = (uint32_t)ties;
    size_t size = strlen(p);
    if (size % 8 || size / 8 > ELECTION_OVERRIDES) return false;
    for (; *p; p += 8) {
        if (!tag3(p)) return false;
        int weight = -1;
        if (strncmp(p + 3, "--", 2)) {
            if (strspn(p + 3, "0123456789") < 2) return false;
            weight = (p[3] - '0') * 10 + p[4] - '0';
        }
        unsigned choice = 0;
        if (!strncmp(p + 5, "___", 3)) choice = ABSTAIN_VOTE;
        else if (strncmp(p + 5, "---", 3)) {
            if (!tag3(p + 5)) return false;
            choice = election_tag(p + 5);
        }
        const unsigned before = state.count;
        if (election_prefer(&state, election_tag(p), choice, weight) || state.count != before + 1) return false;
    }
    *out = state;
    strcpy(instance, id);
    return true;
}

bool encode_election_cooldowns(const char *instance, const ElectionState *s, char value[GK_SAVE_VALUE_SIZE]) {
    if (!valid_id(instance, strlen(instance)) || s->cooldown_count > ELECTION_OVERRIDES) return false;
    int n = snprintf(value, GK_SAVE_VALUE_SIZE, "C1|%s|", instance);
    if (n < 0 || (size_t)n + 11*s->cooldown_count >= GK_SAVE_VALUE_SIZE) return false;
    for (unsigned i=0; i<s->cooldown_count; ++i) {
        char tag[4];
        const struct ElectionCooldown *c = &s->cooldowns[i];
        if (!tag3(election_text(c->voter, tag)) || !valid_history_date(c->until)) return false;
        snprintf(value+n, GK_SAVE_VALUE_SIZE-(size_t)n, "%s%08u", tag, c->until);
        n += 11;
    }
    return true;
}

bool decode_election_cooldowns(const char value[GK_SAVE_VALUE_SIZE], char instance[GK_SCRIPT_WORD_SIZE], ElectionState *out) {
    if (!memchr(value, 0, GK_SAVE_VALUE_SIZE) || strncmp(value, "C1|", 3)) return false;
    const char *p = value+3;
    char id[GK_SCRIPT_WORD_SIZE] = {0};
    if (!read_id(&p,id)) return false;
    const size_t size = strlen(p);
    if (!size || size%11 || size/11 > ELECTION_OVERRIDES) return false;
    ElectionState tmp = {0};
    for (; *p; p += 11) {
        if (!tag3(p) || strspn(p+3,"0123456789") < 8) return false;
        unsigned date = 0, voter = election_tag(p);
        for (unsigned j=3; j<11; ++j) date = 10*date + (unsigned)(p[j]-'0');
        if (!valid_history_date(date) || election_locked_until(&tmp, voter, 0)) return false;
        tmp.cooldowns[tmp.cooldown_count++] = (struct ElectionCooldown){voter,date};
    }
    memcpy(out->cooldowns,tmp.cooldowns,sizeof(tmp.cooldowns));
    out->cooldown_count = tmp.cooldown_count;
    strcpy(instance,id);
    return true;
}
