#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { GK_SCRIPT_WORD_SIZE = 128, GK_MAX_AUTHORITY = 100, GK_DEFAULT_AUTHORITY = 50 };
enum { PS_FILE_PATH_SIZE = 260, PS_MAX_EXPLICIT_MEMBERS = 96 };
enum { PS_MAX_PROVINCE_ID = 999999 };
enum { PS_MESSAGE_BODY_LINES = 4 };
typedef struct ScriptError {
    unsigned line, column;
    char reason[192];
    char path[PS_FILE_PATH_SIZE];
} ScriptError;

typedef struct MemberTags {
    unsigned count;
    unsigned tags[PS_MAX_EXPLICIT_MEMBERS];
} MemberTags;
