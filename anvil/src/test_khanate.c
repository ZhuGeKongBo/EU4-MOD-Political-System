#include "khanate.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int oirat_capital = 718;
static const char *boduna_owner = "KRC";

static int32_t province_limit(void) { return 5000; }

static AnvilResult read_province(int32_t id, AnvilGameProvince *out) {
    const char *owner;
    switch (id) {
        case 718: owner = "OIR"; break;
        case 706: owner = "SYG"; break;
        case 2109: owner = boduna_owner; break;
        default: return ANVIL_ERROR_NOT_FOUND;
    }
    strcpy(out->owner.text, owner);
    return ANVIL_OK;
}

static AnvilResult read_country(const char *tag, AnvilGameCountry *out) {
    if (!strcmp(tag, "OIR")) out->capital = oirat_capital;
    else if (!strcmp(tag, "SYG")) out->capital = 706;
    else if (!strcmp(tag, "KRC")) out->capital = 2109;
    else return ANVIL_ERROR_NOT_FOUND;
    out->province_count = 1;
    return ANVIL_OK;
}

int main(void) {
    ScriptError error = {0};
    const char leader[] = "1399.1.1 = { khan_emperor = KHA }";
    KhanTimeline holders = {0};
    assert(read_holder_history(leader, sizeof(leader) - 1, KHAN_HOLDER_FLAG, &holders, &error));
    assert(!history_at(&holders, 13981231).tag[0]);
    assert(!strcmp(history_at(&holders, 14441111).tag, "KHA"));
    free(holders.entries);

    const char marks[] = "1445.1.1 = { is_part_of_khanate = no }\n1444.1.1 = { is_part_of_khanate = yes }\n";
    KhanFlagTimeline province = {0}, role = {0};
    assert(read_flag_history(marks, sizeof(marks) - 1, KHAN_PROVINCE_FLAG, &province, &error));
    assert(!flag_at(&province, 14431231) && flag_at(&province, 14441111) && !flag_at(&province, 14450101));
    const char tumen[] = "khan_tumen = yes\n1450.1.1 = { khan_tumen = no }";
    assert(read_flag_history(tumen, sizeof(tumen) - 1, KHAN_TUMEN_FLAG, &role, &error));
    const char bad[] = "1444.1.1 = { is_part_of_khanate = maybe }";
    KhanFlagTimeline invalid = {0};
    assert(!read_flag_history(bad, sizeof(bad) - 1, KHAN_PROVINCE_FLAG, &invalid, &error));
    assert(!invalid.count && !invalid.entries && error.line == 1);

    assert(khanate_history_key("2109 - Boduna.txt", false) == 2109);
    assert(khanate_history_key("706.txt", false) == 706);
    assert(!khanate_history_key("2109 - ../anything.txt", false));
    assert(!khanate_history_key("KRCmore.txt", true));
    const uint32_t oir_key = khanate_history_key("OIR - OiratHorde.txt", true);
    assert(oir_key == khanate_history_key("OIR.txt", true));

    KhanateFlag provinces[] = { {718, province}, {706, province}, {2109, province} };
    KhanateFlag countries[] = { {oir_key, role} };
    const KhanateHistory data = { .provinces = {provinces, 3}, .countries = {countries, 1} };
    const AnvilGameWorldService world = {
        .province_limit = province_limit, .province = read_province, .country = read_country,
    };
    const AnvilGameTag holder = { .text = "KHA" };
    Shogunate members = {0};
    assert(read_capital_members(&world, &data, 14441111, &holder, &members));
    assert(members.count == 3 && !strcmp(members.emperor.text, "KHA"));
    assert(!strcmp(members.daimyo[0].text, "KRC") && !strcmp(members.daimyo[1].text, "OIR") &&
           !strcmp(members.daimyo[2].text, "SYG"));
    assert(has_khan_tumen(&data, 14441111, "OIR") && !has_khan_tumen(&data, 14441111, "KRC"));
    assert(!has_khan_tumen(&data, 14500101, "OIR"));
    assert(is_khanate_province(&data, 14441111, 718));
    assert(!is_khanate_province(&data, 14441111, 100));
    assert(!is_khanate_province(&data, 14431231, 718) && !is_khanate_province(&data, 14450101, 718));
    assert(count_khanate_provinces(&data, 14441111, 5000) == 3);
    assert(count_khanate_provinces(&data, 14441111, 2109) == 2);
    assert(!count_khanate_provinces(&data, 14441111, 0));
    release_shogunate(&members);

    oirat_capital = 100;
    boduna_owner = "OIR";
    assert(read_capital_members(&world, &data, 14441111, &holder, &members));
    assert(members.count == 1 && !strcmp(members.daimyo[0].text, "SYG"));
    assert(is_khanate_province(&data, 14441111, 718) && is_khanate_province(&data, 14441111, 2109));
    release_shogunate(&members);
    assert(read_capital_members(&world, &data, 14431231, &holder, &members) && !members.count);
    release_shogunate(&members);
    free(province.entries);
    free(role.entries);
    puts("Khanate tests passed: native history keys, dates, capital membership, ownership changes and Tumen.");
    return 0;
}
