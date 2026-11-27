#pragma once

#include "accession.h"
#include "script_tree.h"

#define PS_TEXT_DIRECTORY "common/political_systems/localisation"

typedef enum PoliticalTextKind { TEXT_NONE, TEXT_STRING, TEXT_INTEGER, TEXT_FIXED } PoliticalTextKind;
typedef struct TextValue {
    PoliticalTextKind kind;
    const char *text;
    int32_t number;
} TextValue;

typedef struct TextTemplate TextTemplate;
typedef struct TextTemplates {
    TextTemplate *items;
    size_t count;
} TextTemplates;

typedef struct TextEngine {
    GameString *(*localize)(GameString *, const char *, const void *, unsigned);
    void (*assign)(GameString *, const char *, size_t);
    void (*release)(GameString *);
    const char *(*lookup)(const char *key);
    void (*problem)(const char *path, unsigned line, const char *key, const char *parameter, const char *reason);
} TextEngine;

typedef bool (*TextSource)(void *context, const char *source, const char *id, TextValue *out);

bool collect_text_templates(TextTemplates *, const char *, size_t, const char *, ScriptError *);
void free_text_templates(TextTemplates *);
bool render_political_text(const TextEngine *, const TextTemplates *, const char *, TextSource, void *,
                           GameString *, bool *used_breakdown);

static inline TextValue text_value(const char *text) { return (TextValue){.kind = TEXT_STRING, .text = text}; }
static inline TextValue text_integer(int32_t value) { return (TextValue){.kind = TEXT_INTEGER, .number = value}; }
static inline TextValue text_fixed(int32_t value) { return (TextValue){.kind = TEXT_FIXED, .number = value}; }
