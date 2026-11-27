#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "content_layout.h"
#include "political_text.h"

typedef struct Entry {
    char kind, name[PS_FILE_PATH_SIZE], local[40];
    struct Entry *next;
} Entry;

static Entry *catalog;
static ContentTypes types;
static ContentLayouts layouts;
static TextTemplates texts;
static PoliticalRoster roster;
static unsigned errors;

static void report(const ScriptError *error) {
    printf("error\t%s\t%u\t%s\n", error->path, error->line, error->reason);
    ++errors;
}

static void issue(const char *path, unsigned line, const char *detail) {
    ScriptError error = {.line = line};
    snprintf(error.path, sizeof(error.path), "%s", path);
    snprintf(error.reason, sizeof(error.reason), "%s", detail);
    report(&error);
}

static bool require_game_check(NativeClause *clause, NativeClauseKind kind, ScriptError *unused) {
    (void)kind;
    (void)unused;
    if (clause->text)
        printf("warning\t%s\t%u\tEU4 engine clause needs an in-game check\n", clause->path, clause->line);
    return true;
}

static const Entry *entry(const char *name, char kind) {
    for (const Entry *r = catalog; r; r = r->next)
        if ((!kind || r->kind == kind) && !_stricmp(name,r->name)) return r;
    return NULL;
}

static bool mounted(const char *name) { return entry(name, 'T') != NULL; }

static char *read(const Entry *entry, size_t *size) {
    FILE *file = entry ? fopen(entry->local, "rb") : NULL;
    if (!file) return NULL;
    fseek(file,0,SEEK_END);
    long length = ftell(file);
    rewind(file);
    if (length < 0 || length > 32*1024*1024) { fclose(file); return NULL; }
    char *bytes = malloc((size_t)length+1);
    if (!bytes) { fclose(file); return NULL; }
    *size = fread(bytes,1,(size_t)length,file);
    fclose(file);
    if (*size != (size_t)length) { free(bytes); return NULL; }
    bytes[*size] = 0;
    return bytes;
}

static bool tree(const char *path, ScriptTree *tree, ScriptError *error) {
    size_t size;
    char *bytes = read(entry(path,'G'), &size);
    if (!bytes) {
        snprintf(error->path,sizeof(error->path),"%s",path);
        strcpy(error->reason,"missing mounted definition");
        return false;
    }
    bool ok = script_tree_read(tree,bytes,size,path,error);
    if (ok) tree->owned_text = bytes;
    else free(bytes);
    return ok;
}

static bool map_for(const char *id) {
    ScriptError error = {.line = 1};
    const KhanDefinition *def = resolve_content_type(&types,id,&error);
    if (!def) { report(&error); return false; }
    if (def->abstract) return true;
    visit_native_clauses((KhanDefinition *)def, require_game_check, NULL);
    const EntryButton *binding = &def->entry;
    if (binding->layout[0]) {
        int idx = layout_index(&layouts,binding->layout);
        bool actions = false;
        for (unsigned i=0; i<def->action_count; ++i) if (def->actions[i].shown) actions = true;
        const PoliticalLayout *layout = idx < 0 ? NULL : layouts.items+idx;
        if (!layout || !layout->valid)
            issue(def->source_path,def->source_line,"missing or invalid interface.layout");
        else if ((binding->close_button && !layout->close[0]) || (def->leader_enabled && !layout->leader[0]) ||
                 (def->authority_enabled && !layout->authority[0] && !layout->resources.rows) ||
                 (def->member_source != MEMBERS_UNSET && !layout->members.rows) || (actions && !layout->actions.rows))
            issue(def->source_path,def->source_line,"layout is missing a control required by an enabled module");
    }
    if (binding->defined) {
        int idx = hud_index(&layouts,binding->region);
        if (idx < 0 || !layouts.hud[idx].valid || hud_icon_index(layouts.hud+idx,binding->icon) < 0)
            issue(def->source_path,def->source_line,"HUD region or button sprite could not be bound");
    }
    return true;
}

static void check_type(const char *id, void *previous) {
    if (strcmp(id, previous)) map_for(id);
}

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    FILE *manifest = fopen("catalog.tsv","rb");
    if (!manifest) return 2;
    char line[400];
    Entry **tail = &catalog;
    while (fgets(line,sizeof(line),manifest)) {
        char *a = strtok(line,"\t\r\n"), *b = strtok(NULL,"\t\r\n"), *c = strtok(NULL,"\t\r\n");
        if (!a || !b || !c || strlen(b) >= PS_FILE_PATH_SIZE || strlen(c) >= sizeof((*tail)->local))
            { fclose(manifest); return 2; }
        *tail = calloc(1,sizeof(**tail));
        if (!*tail) { fclose(manifest); return 2; }
        (*tail)->kind = a[0]; strcpy((*tail)->name,b); strcpy((*tail)->local,c); tail = &(*tail)->next;
    }
    fclose(manifest);
    for (const Entry *item = catalog; item; item=item->next) {
        if (item->kind == 'G' || item->kind == 'T') continue;
        ScriptError error = {.line = 1};
        size_t size;
        char *bytes = read(item,&size);
        if (!bytes) { issue(item->name,1,"cannot read validation input"); continue; }
        bool success;
        switch (item->kind) {
            case 'D': success=collect_content_types(&types,bytes,size,item->name,&error); break;
            case 'L': success=collect_layouts(&layouts,bytes,size,item->name,&error); break;
            case 'P': success=collect_text_templates(&texts,bytes,size,item->name,&error); break;
            case 'R': success=append_roster(bytes,size,&roster,&error); break;
            default: success=append_content_history(bytes,size,item->name,&roster,&error); break;
        }
        if (!success) {
            if (!error.path[0]) snprintf(error.path,sizeof(error.path),"%s",item->name);
            report(&error);
        }
        free(bytes);
    }
    for (size_t i=0; i<layouts.count; ++i) {
        PoliticalLayout *layout = layouts.items+i;
        ScriptTree gui = {0};
        ScriptError error = {.line = 1};
        layout->valid = tree(layout->gui_file,&gui,&error) && check_layout_gui(layout,&gui,&error);
        if (!layout->valid) report(&error);
        script_tree_free(&gui);
    }
    for (size_t i=0; i<layouts.hud_count; ++i) {
        HudRegion *hud = layouts.hud+i;
        ScriptTree gui = {0};
        ScriptError error = {.line = 1};
        bool parsed = tree(hud->gui_file,&gui,&error) && check_hud_gui(hud,&gui,&error);
        script_tree_free(&gui);
        hud->valid = parsed && tree(hud->gfx_file,&gui,&error) && check_hud_gfx(hud,&gui,mounted,&error);
        if (!hud->valid) report(&error);
        script_tree_free(&gui);
    }
    if (layouts.hud_count) {
        ScriptTree gui = {0};
        ScriptError error = {.line = 1};
        if (!tree(PS_SELECTOR_GUI,&gui,&error) || !check_gui_window_tooltips(&gui,PS_SELECTOR_WINDOW,&error)) report(&error);
        script_tree_free(&gui);
    }
    if (strcmp(argv[1],"-")) map_for(argv[1]);
    validate_unused_types(&types,report);
    visit_concrete_types(&types,check_type,argv[1]);
    for (size_t i=0; i<roster.count; ++i) {
        if (!has_content_type(&types,roster.items[i].type)) issue(roster.items[i].path,roster.items[i].line,"historical instance refers to an unregistered type");
        free(roster.items[i].changes);
    }
    printf("checked\tlayouts=%zu\thud_regions=%zu\tinstances=%zu\terrors=%u\n",layouts.count,layouts.hud_count,roster.count,errors);
    free_content_types(&types);
    free_text_templates(&texts);
    for (Entry *next; catalog; catalog=next) { next=catalog->next; free(catalog); }
    return errors ? 1 : 0;
}
