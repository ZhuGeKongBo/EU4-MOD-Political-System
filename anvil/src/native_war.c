#include "native_war.h"
#include "symbols.gen.h"
#include <string.h>

typedef struct JoinWarCommand {
    const void *vtable;
    uint32_t command;
    uint64_t defender, issuer, member;
    int32_t sent, resolved, notification, state;
    uint16_t scripted;
    int32_t cooldown;
    uint64_t opponent;
    uint8_t include_defense, show_result;
    uint32_t kind;
    uint8_t rules[7];
    void *additional[3];
} JoinWarCommand;

typedef struct DiplomaticEnvelope {
    const void *vtable;
    uint32_t command;
    uint32_t reserved_header;
    uint8_t reserved;
    GameString label;
    int32_t notification, resolution;
    uint16_t flags;
    uint32_t command_type;
    uint64_t context;
    void *payload;
} DiplomaticEnvelope;

_Static_assert(offsetof(JoinWarCommand, defender) == 0x10 && offsetof(JoinWarCommand, opponent) == 0x40 &&
               offsetof(JoinWarCommand, kind) == 0x4C && offsetof(JoinWarCommand, additional) == 0x58 &&
               sizeof(JoinWarCommand) == 0x70, "EU4 call to arms command");
_Static_assert(offsetof(DiplomaticEnvelope, reserved) == 0x10 && offsetof(DiplomaticEnvelope, label) == 0x18 &&
               offsetof(DiplomaticEnvelope, payload) == 0x50 &&
               sizeof(DiplomaticEnvelope) == 0x58, "EU4 diplomatic envelope");

static const void *read_vtable_site(const void *site) {
    if (!site) return NULL;
    int32_t offset;
    memcpy(&offset, (const unsigned char *)site + 3, sizeof(offset));
    return (const unsigned char *)site + 7 + offset;
}

bool bind_native_defense(NativeDefense *api, const AnvilHost *host, AnvilPlugin *self) {
    api->join_vtable = read_vtable_site(host->address_of(self, SYM_DEFENSIVE_JOIN_VTABLE));
    api->envelope_vtable = read_vtable_site(host->address_of(self, SYM_DIPLOMATIC_COMMAND_VTABLE));
    api->check = host->address_of(self, SYM_DEFENSIVE_JOIN_CHECK);
    api->clone = host->address_of(self, SYM_DEFENSIVE_JOIN_CLONE);
    api->submit = host->address_of(self, SYM_DIPLOMATIC_COMMAND_SUBMIT);
    api->dispose = host->address_of(self, SYM_DIPLOMATIC_COMMAND_DISPOSE);
    api->free_string = host->address_of(self, SYM_FREE_STRING);
    return api->join_vtable && api->envelope_vtable && api->check && api->clone &&
           api->submit && api->dispose && api->free_string;
}

bool summon_defender(const NativeDefense *api, uint64_t defender, uint64_t member, uint64_t opponent, int32_t date) {
    if (!api->check || !defender || !member || !opponent || defender == member || defender == opponent || member == opponent)
        return false;
    // Stock effect at 0x6CE1DD. The opposing tag selects one war through
    // the two leaders' relation (0x53D776); the final flag selects that branch.
    JoinWarCommand request = {
        .vtable = api->join_vtable, .command = 0x165, .defender = defender, .issuer = defender,
        .member = member, .sent = date, .resolved = date, .notification = -1, .scripted = 1,
        .opponent = opponent, .include_defense = 1, .show_result = 1, .kind = 0x13,
        .rules = {1, 1, 1, 1, 0, 1, 1},
    };
    GameString description = {.capacity = 15};
    const bool eligible = api->check(&request, &description, false);
    api->free_string(&description);
    if (!eligible) return false;
    // This is an automatic obligation, matching the stock effect's accepted response.
    request.state = 2;
    void *payload = api->clone(&request);
    if (!payload) return false;
    DiplomaticEnvelope envelope = {
        .vtable = api->envelope_vtable, .command = 0x165, .label.capacity = 15,
        .notification = -1, .command_type = 0x2934, .context = 0xFFFF, .payload = payload,
    };
    api->submit(&envelope);
    api->dispose(&envelope, 0);
    return true;
}
