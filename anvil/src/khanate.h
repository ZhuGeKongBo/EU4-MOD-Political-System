#pragma once

#include "political_script.h"
#include "shogunate.h"

#define KHAN_PROVINCE_FLAG "is_part_of_khanate"
#define KHAN_TUMEN_FLAG "khan_tumen"
#define KHAN_HOLDER_FLAG "khan_emperor"
#define KHAN_DIPLOMACY_PATH "history/diplomacy/" GK_KHANATE_HISTORY ".txt"

typedef struct KhanateFlag {
    uint32_t key;
    KhanFlagTimeline timeline;
} KhanateFlag;

typedef struct KhanateHistoryFiles {
    KhanateFlag *items;
    size_t count;
} KhanateHistoryFiles;

typedef struct KhanateHistory {
    KhanateHistoryFiles provinces;
    KhanateHistoryFiles countries;
} KhanateHistory;

uint32_t khanate_history_key(const char *filename, bool countries);
void release_history_flags(KhanateHistoryFiles *list);
void release_khanate_history(KhanateHistory *history);
bool is_khanate_province(const KhanateHistory *history, uint32_t date, int32_t province);
size_t count_khanate_provinces(const KhanateHistory *history, uint32_t date, int32_t limit);
bool has_khan_tumen(const KhanateHistory *history, uint32_t date, const char tag[4]);
bool read_capital_members(const AnvilGameWorldService *world, const KhanateHistory *history, uint32_t date,
                          const AnvilGameTag *holder, Shogunate *out);
