#include "political_text.h"
#include "ps_script.h"

#include <stdlib.h>
#include <string.h>

enum { MAX_TEXT_PARAMETERS = 32, TEXT_OUTPUT_LIMIT = 32768 };

typedef struct TextBinding {
    char name[GK_SCRIPT_WORD_SIZE], source[GK_SCRIPT_WORD_SIZE], id[GK_SCRIPT_WORD_SIZE];
} TextBinding;

struct TextTemplate {
    char key[GK_SCRIPT_WORD_SIZE], target[GK_SCRIPT_WORD_SIZE], path[PS_FILE_PATH_SIZE];
    unsigned line, count;
    TextBinding bindings[MAX_TEXT_PARAMETERS];
};

/* Verified against 0x14B2CC0, 0x1AA605, 0x244DD4 and 0x12D2F59 in Steam 1.37.5. */
typedef struct NativeTextParameter {
    const char *name;
    GameString owned_name;
    uint32_t kind;
    GameString text;
    int32_t fixed;
    uint32_t padding[3];
    int32_t integer;
    uint32_t reserved;
} NativeTextParameter;

_Static_assert(sizeof(NativeTextParameter) == 0x68 && offsetof(NativeTextParameter, kind) == 0x28 &&
    offsetof(NativeTextParameter, text) == 0x30 && offsetof(NativeTextParameter, fixed) == 0x50 &&
    offsetof(NativeTextParameter, integer) == 0x60, "EU4 localisation parameter layout");

static const TextTemplate *find_template(const TextTemplates *templates, const char *key) {
    for (size_t i = 0; i < templates->count; ++i)
        if (!strcmp(templates->items[i].key, key)) return &templates->items[i];
    return NULL;
}

static bool read_bindings(const ScriptTree *tree, const ScriptItem *block, TextTemplate *result, ScriptError *error) {
    if (!block->block) return script_problem(tree, block, error, "parameters expects a block");
    for (unsigned i = block->first; i; i = tree->items[i].next) {
        const ScriptItem *item = &tree->items[i], *source = item, *id = NULL;
        if (!ps_identifier(item->key)) return script_problem(tree, item, error, "invalid localisation parameter name");
        if (result->count == MAX_TEXT_PARAMETERS) return script_problem(tree, item, error, "too many localisation parameters");
        for (unsigned j = 0; j < result->count; ++j)
            if (!strcmp(result->bindings[j].name, item->key))
                return script_problem(tree, item, error, "duplicate localisation parameter");
        if (item->block) {
            static const char *const keys[] = {"source", "id"};
            if (!script_keys(tree, item, keys, 2, error)) return false;
            source = script_field(tree, item, "source");
            id = script_field(tree, item, "id");
        }
        if (!source || source->block || !ps_identifier(source->word) ||
            (id && (id->block || !ps_identifier(id->word))))
            return script_problem(tree, item, error, "expected a named source and optional id");
        TextBinding *binding = &result->bindings[result->count++];
        strcpy(binding->name, item->key);
        strcpy(binding->source, source->word);
        if (id) strcpy(binding->id, id->word);
    }
    return true;
}

bool collect_text_templates(TextTemplates *templates, const char *text, size_t length, const char *path, ScriptError *error) {
    ScriptTree tree = {0};
    if (!script_tree_read(&tree, text, length, path, error)) return false;
    const size_t previous = templates->count;
    for (unsigned i = tree.items[0].first; i; i = tree.items[i].next) {
        const ScriptItem *item = &tree.items[i];
        static const char *const keys[] = {"template", "parameters"};
        if (!ps_identifier(item->key) || find_template(templates, item->key)) {
            script_problem(&tree, item, error, "invalid or duplicate localisation template key");
            goto failed;
        }
        if (!script_keys(&tree, item, keys, 2, error)) goto failed;
        TextTemplate *items = realloc(templates->items, (templates->count + 1) * sizeof(*items));
        if (!items) {
            script_problem(&tree, item, error, "cannot allocate localisation template");
            goto failed;
        }
        templates->items = items;
        TextTemplate *target = &items[templates->count++];
        *target = (TextTemplate){.line = item->line};
        strcpy(target->key, item->key);
        strcpy(target->path, tree.path);
        const ScriptItem *alias = script_field(&tree, item, "template");
        const ScriptItem *bindings = script_field(&tree, item, "parameters");
        if (alias && (alias->block || !ps_identifier(alias->word) || !strcmp(item->key, alias->word))) {
            script_problem(&tree, alias, error, "invalid localisation template reference");
            goto failed;
        }
        if (alias) strcpy(target->target, alias->word);
        if (bindings && !read_bindings(&tree, bindings, target, error)) goto failed;
    }
    script_tree_free(&tree);
    return true;
failed:
    templates->count = previous;
    script_tree_free(&tree);
    return false;
}

void free_text_templates(TextTemplates *templates) {
    free(templates->items);
    *templates = (TextTemplates){0};
}

static const TextBinding *find_binding(const TextTemplate *template, const char *name) {
    for (unsigned i = 0; template && i < template->count; ++i)
        if (!strcmp(template->bindings[i].name, name)) return &template->bindings[i];
    return NULL;
}

bool render_political_text(const TextEngine *engine, const TextTemplates *templates, const char *key,
                           TextSource source, void *context, GameString *out, bool *used_breakdown) {
    if (used_breakdown) *used_breakdown = false;
    if (!engine->lookup || !key || !*key) return false;
    const TextTemplate *original = find_template(templates, key), *template = original;
    const char *resolved = key, *problem = NULL;
    for (size_t depth = 0; template && template->target[0]; ++depth) {
        if (depth >= templates->count) {
            if (engine->problem) engine->problem(original->path, original->line, key, "", "template reference cycle");
            return false;
        }
        resolved = template->target;
        template = find_template(templates, resolved);
    }
    const char *raw = engine->lookup(resolved);
    if (!raw) {
        if (original && engine->problem) engine->problem(original->path, original->line, key, resolved, "missing localisation");
        return false;
    }
    char names[MAX_TEXT_PARAMETERS][GK_SCRIPT_WORD_SIZE], name[GK_SCRIPT_WORD_SIZE] = "";
    NativeTextParameter params[MAX_TEXT_PARAMETERS] = {0};
    unsigned count = 0;
    size_t size = strlen(raw);
    bool breakdown = false;
    for (const char *open = strchr(raw, '$'); open && !problem; ) {
        const char *close = strchr(open + 1, '$');
        if (!close) { problem = "unclosed parameter"; break; }
        const char *format = memchr(open + 1, '|', (size_t)(close - open - 1));
        const size_t length = (size_t)((format ? format : close) - open - 1);
        if (!length || length >= sizeof(name)) { problem = "invalid parameter name"; break; }
        memcpy(name, open + 1, length);
        name[length] = '\0';
        if (!ps_identifier(name)) { problem = "invalid parameter name"; break; }
        unsigned i = 0;
        while (i < count && strcmp(names[i], name)) ++i;
        if (i == count) {
            if (count == MAX_TEXT_PARAMETERS) { problem = "too many parameters"; break; }
            const TextBinding *binding = NULL;
            for (const TextTemplate *entry = original; entry && !binding;
                 entry = entry->target[0] ? find_template(templates, entry->target) : NULL)
                binding = find_binding(entry, name);
            const char *kind = binding ? binding->source : name;
            TextValue value = {0};
            if (!source(context, kind, binding ? binding->id : "", &value) || value.kind == TEXT_NONE) {
                problem = "parameter unavailable in this context";
                break;
            }
            breakdown |= !_stricmp(kind, "breakdown") || !_stricmp(kind, "why");
            strcpy(names[count++], name);
            NativeTextParameter *param = &params[i];
            param->name = names[i];
            param->owned_name.capacity = 15;
            if (value.kind == TEXT_STRING) {
                if (!value.text || strlen(value.text) > TEXT_OUTPUT_LIMIT) { problem = "parameter text too long"; break; }
                param->kind = 1;
                param->text.capacity = 15;
                engine->assign(&param->text, value.text, strlen(value.text));
            } else if (value.kind == TEXT_INTEGER) {
                param->kind = 5;
                param->integer = value.number;
            } else {
                param->kind = 7;
                param->fixed = value.number;
            }
        }
        size += params[i].kind == 1 ? params[i].text.size + 8 : 64;
        if (size > TEXT_OUTPUT_LIMIT) problem = "rendered localisation too long";
        open = strchr(close + 1, '$');
    }
    if (!problem && size > TEXT_OUTPUT_LIMIT) problem = "localisation too long";
    if (!problem) {
        engine->localize(out, resolved, count ? params : NULL, count);
        if (used_breakdown) *used_breakdown = breakdown;
    } else if (engine->problem) {
        const TextTemplate *where = original ? original : template;
        engine->problem(where ? where->path : "localisation", where ? where->line : 0, key, name, problem);
    }
    for (unsigned i = 0; i < count; ++i)
        if (params[i].kind == 1) engine->release(&params[i].text);
    return !problem;
}
