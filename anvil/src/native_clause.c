#include "native_clause.h"
#include "script_tree.h"

#include <stdlib.h>
#include <string.h>

bool read_native_clause(const ScriptTree *tree, const ScriptItem *item, NativeClause *out, ScriptError *error) {
    if (!item) return true;
    *out = (NativeClause){0};
    if (script_word(item, "clear") || script_word(item, "null")) return true;
    if (!item->block) return script_problem(tree, item, error, "expected an EU4 script block");
    if (!item->first) return true;
    *out = (NativeClause){ .text = item->start, .path = tree->path,
        .length = (size_t)(item->end - item->start), .line = item->line, .column = item->column,
        .value_line = item->value_line };
    return true;
}

bool native_clause_present(const NativeClause *clause) {
    return clause->length != 0 || clause->program != NULL;
}

static void write16(unsigned char *out, uint16_t value) {
    out[0] = (unsigned char)value;
    out[1] = (unsigned char)(value >> 8);
}

static void write32(unsigned char *out, uint32_t value) {
    write16(out, (uint16_t)value);
    write16(out + 2, (uint16_t)(value >> 16));
}

static uint32_t zip_crc(const char *text, size_t length) {
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < length; ++i) {
        crc ^= (unsigned char)text[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (crc & 1 ? UINT32_C(0xEDB88320) : 0);
    }
    return ~crc;
}

unsigned char *native_script_zip(const char *path, const char *text, size_t length, size_t *archive_length) {
    const size_t path_length = strlen(path);
    if (!path_length || path_length > UINT16_MAX || length > UINT32_MAX - 2 * path_length - 98) return NULL;
    const size_t central = 30 + path_length + length;
    const size_t central_length = 46 + path_length;
    const size_t size = central + central_length + 22;
    unsigned char *archive = calloc(size, 1);
    if (!archive) return NULL;
    const uint32_t crc = zip_crc(text, length);
    write32(archive, 0x04034b50);
    write16(archive + 4, 20);
    write16(archive + 12, 33);
    write32(archive + 14, crc);
    write32(archive + 18, (uint32_t)length);
    write32(archive + 22, (uint32_t)length);
    write16(archive + 26, (uint16_t)path_length);
    memcpy(archive + 30, path, path_length);
    memcpy(archive + 30 + path_length, text, length);

    unsigned char *directory = archive + central;
    write32(directory, 0x02014b50);
    write16(directory + 4, 20);
    write16(directory + 6, 20);
    write16(directory + 14, 33);
    write32(directory + 16, crc);
    write32(directory + 20, (uint32_t)length);
    write32(directory + 24, (uint32_t)length);
    write16(directory + 28, (uint16_t)path_length);
    memcpy(directory + 46, path, path_length);

    unsigned char *end = directory + central_length;
    write32(end, 0x06054b50);
    write16(end + 8, 1);
    write16(end + 10, 1);
    write32(end + 12, (uint32_t)central_length);
    write32(end + 16, (uint32_t)central);
    *archive_length = size;
    return archive;
}
